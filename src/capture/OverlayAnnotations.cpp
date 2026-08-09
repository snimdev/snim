#include "capture/OverlayAnnotations.h"

#include "capture/CaptureGeometry.h"
#include "editor/annotations/AnnotationBuilder.h"
#include "editor/annotations/StepNumbering.h"
#include "editor/annotations/ToolRegistry.h"
#include "editor/annotations/commands/EditorCommands.h"
#include "editor/annotations/interactions/IDrawingInteraction.h"
#include "editor/annotations/tools/TextTool.h"

#include <QCoreApplication>
#include <QGraphicsScene>
#include <QInputMethodEvent>
#include <QKeyEvent>
#include <QKeySequence>
#include <QLatin1StringView>
#include <QPainter>
#include <QTransform>
#include <QUndoStack>
#include <algorithm>

namespace Capture {

using Editor::Commands::AddItemCommand;
using Screen::SelectionContext;

namespace {
    // Tools the overlay offers, in strip order.
    constexpr QLatin1StringView kOverlayTools[] = {
        QLatin1StringView("arrow"), QLatin1StringView("rectangle"),
        QLatin1StringView("ellipse"), QLatin1StringView("freehand"),
        QLatin1StringView("highlight"), QLatin1StringView("text"),
        QLatin1StringView("step"), QLatin1StringView("blur"),
    };
    const QString kUndoSlot = QStringLiteral("undo");
    const QString kRedoSlot = QStringLiteral("redo");

    QString keyText(QKeySequence::StandardKey k)
    {
        return QKeySequence(k).toString(QKeySequence::NativeText);
    }

    // Hook arrow in the SVG's 24-unit box, mirrored for redo.
    QPainterPath historyGlyph(bool redo)
    {
        QPainterPath path;
        path.moveTo(9, 7);
        path.lineTo(4, 12);
        path.lineTo(9, 17);
        path.moveTo(4, 12);
        path.lineTo(15, 12);
        path.cubicTo(17.76, 12, 20, 14.24, 20, 17);
        path.lineTo(20, 18);
        return redo ? QTransform(-1, 0, 0, 1, 24, 0).map(path) : path;
    }
}

void OverlayAnnotations::UndoStackSink::commit(QGraphicsItem *item, const QString &toolId)
{
    const Editor::ToolSpec *spec = Editor::ToolRegistry::find(toolId);
    const QString text = QStringLiteral("Add %1").arg(spec ? spec->namePrefix : toolId);
    m_session->m_stack->push(new AddItemCommand(m_session->m_scene.get(), item, toolId, text));
    m_session->m_builder->rememberStyle(item, toolId);
}

OverlayAnnotations::OverlayAnnotations(const QPixmap &frame, const QRect &virtualGeometry,
                                       QObject *parent)
    : Screen::SelectionLayer(parent)
    , m_frame(frame)
    , m_virtualGeometry(virtualGeometry)
    , m_scene(std::make_unique<QGraphicsScene>())
    , m_stack(std::make_unique<QUndoStack>())
    , m_sink(this)
{
    m_scene->setSceneRect(QRectF(QPointF(0, 0), virtualGeometry.size()));
    // Text focus only lands in an active scene, and there is no view to activate it.
    QEvent activate(QEvent::WindowActivate);
    QCoreApplication::sendEvent(m_scene.get(), &activate);

    m_builder = new Editor::AnnotationBuilder(m_scene.get(), &m_sink, this);
    m_builder->setSourcePixmap(frame);
    m_builder->setImageBounds(m_scene->sceneRect().toRect());
    m_builder->setStepNumberProvider([this] { return Editor::nextStepNumber(committedItems()); });
    connect(m_builder, &Editor::AnnotationBuilder::textPlaced, this,
            [this](Editor::Tools::TextTool *item) { m_placedText = item; });

    connect(m_scene.get(), &QGraphicsScene::changed, this, &OverlayAnnotations::changed);
    connect(m_stack.get(), &QUndoStack::indexChanged, this, &OverlayAnnotations::changed);
}

OverlayAnnotations::~OverlayAnnotations()
{
    m_scene->disconnect(this);
    m_stack->disconnect(this);
    cancelStroke();
    // The builder reaches the sink and scene, so it must go before either.
    delete m_builder;
    m_builder = nullptr;
}

QPoint OverlayAnnotations::toScene(const QPoint &virt) const
{
    return virt - m_virtualGeometry.topLeft();
}

Editor::Interactions::IDrawingInteraction *OverlayAnnotations::activeInteraction() const
{
    return m_activeTool.isEmpty() ? nullptr : m_builder->interaction(m_activeTool);
}

QList<QGraphicsItem*> OverlayAnnotations::committedItems() const
{
    QList<QGraphicsItem*> items;
    for (int i = 0; i < m_stack->index(); ++i)
        if (auto *cmd = dynamic_cast<const AddItemCommand*>(m_stack->command(i)))
            items.append(cmd->item());
    return items;
}

void OverlayAnnotations::setActiveTool(const QString &id)
{
    commitPendingText();
    if (auto *previous = activeInteraction())
        previous->cleanup(m_scene.get());

    const QString previousId = m_activeTool;
    const Editor::ToolSpec *spec = Editor::ToolRegistry::find(id);
    const bool drawable = spec && spec->isDrawingTool && m_builder->interaction(id);
    m_activeTool = drawable ? id : QString();
    // Every overlay shows the armed tool, so they all repaint.
    if (m_activeTool != previousId)
        emit changed();
}

bool OverlayAnnotations::press(const QPoint &virt)
{
    // A click while typing ends the text box rather than starting a new shape.
    if (isEditingText()) {
        commitPendingText();
        return true;
    }
    auto *interaction = activeInteraction();
    const QPoint p = toScene(virt);
    if (!interaction || (!m_selection.isEmpty() && !m_selection.contains(p)))
        return false;
    return interaction->onMousePress(p, m_scene.get());
}

bool OverlayAnnotations::move(const QPoint &virt)
{
    auto *interaction = activeInteraction();
    if (!interaction || !interaction->isDrawing())
        return false;
    return interaction->onMouseMove(toScene(virt), m_scene.get());
}

bool OverlayAnnotations::release(const QPoint &virt)
{
    auto *interaction = activeInteraction();
    if (!interaction || !interaction->isDrawing())
        return false;
    return interaction->onMouseRelease(toScene(virt), m_scene.get());
}

bool OverlayAnnotations::isDrawing() const
{
    auto *interaction = activeInteraction();
    return interaction && interaction->isDrawing();
}

void OverlayAnnotations::cancelStroke()
{
    if (auto *interaction = activeInteraction())
        if (interaction->isDrawing())
            interaction->cleanup(m_scene.get());
}

QCursor OverlayAnnotations::cursor() const
{
    auto *interaction = activeInteraction();
    if (!interaction)
        return QCursor(Qt::ArrowCursor);
    return QCursor(interaction->isDrawing() ? interaction->getDrawingCursor()
                                            : interaction->getCursor());
}

void OverlayAnnotations::setSelection(const QRect &virt)
{
    m_selection = virt.isEmpty() ? QRect() : QRect(toScene(virt.topLeft()), virt.size());
    m_builder->setImageBounds(m_selection.isEmpty() ? m_scene->sceneRect().toRect() : m_selection);
}

bool OverlayAnnotations::isEditingText() const
{
    return m_builder->isEditingText();
}

void OverlayAnnotations::forwardKey(QKeyEvent *event)
{
    QCoreApplication::sendEvent(m_scene.get(), event);
}

void OverlayAnnotations::forwardInputMethod(QInputMethodEvent *event)
{
    QCoreApplication::sendEvent(m_scene.get(), event);
}

QVariant OverlayAnnotations::inputMethodQuery(Qt::InputMethodQuery query) const
{
    const QVariant value = m_scene->inputMethodQuery(query);
    if (value.typeId() == QMetaType::QRectF)
        return value.toRectF().translated(m_virtualGeometry.topLeft());
    return value;
}

void OverlayAnnotations::commitPendingText()
{
    m_builder->commitPendingText();
}

void OverlayAnnotations::undo()
{
    commitPendingText();
    cancelStroke();
    m_stack->undo();
}

void OverlayAnnotations::redo()
{
    commitPendingText();
    cancelStroke();
    m_stack->redo();
}

bool OverlayAnnotations::canUndo() const
{
    return m_stack->canUndo();
}

bool OverlayAnnotations::canRedo() const
{
    return m_stack->canRedo();
}

bool OverlayAnnotations::hasItems() const
{
    // A placed text box sits on the scene before it is committed to the stack.
    return m_stack->index() > 0 || (m_placedText && m_placedText->scene());
}

void OverlayAnnotations::clear()
{
    cancelStroke();
    // Routing pending text through the stack lets the unwind below own and free it.
    commitPendingText();
    m_stack->setIndex(0);   // every item off the scene, so the commands delete them
    m_stack->clear();
}

void OverlayAnnotations::render(QPainter *painter, const QRectF &target, const QRect &virtSource)
{
    const QRectF source(toScene(virtSource.topLeft()), virtSource.size());
    m_scene->render(painter, target, source, Qt::IgnoreAspectRatio);
}

QPixmap OverlayAnnotations::flattenedCrop(const QRect &virtArea)
{
    commitPendingText();
    cancelStroke();

    const QRect area = virtArea.intersected(m_virtualGeometry);
    const ScreenGrab *grab = screenGrabFor(m_screenGrabs, area);
    QPixmap out = grab ? cropVirtualArea(grab->pixmap, grab->geometry, area)
                       : cropVirtualArea(m_frame, m_virtualGeometry, area);
    if (out.isNull() || !hasItems())
        return out;

    // The painter maps logical coords onto the DPR-tagged pixmap, so this is native resolution.
    QPainter p(&out);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::SmoothPixmapTransform);
    render(&p, QRectF(QPointF(0, 0), out.deviceIndependentSize()), area);
    return out;
}

Editor::AnnotationSet OverlayAnnotations::snapshot(const QRect &virtArea)
{
    commitPendingText();
    cancelStroke();

    Editor::AnnotationSet set;
    const QRect area = virtArea.intersected(m_virtualGeometry);
    if (area.isEmpty())
        return set;
    const QRectF sceneArea(toScene(area.topLeft()), area.size());
    // Stack order is paint order: every item shares z 0.
    for (int i = 0; i < m_stack->index(); ++i) {
        const auto *cmd = dynamic_cast<const AddItemCommand*>(m_stack->command(i));
        if (!cmd || !cmd->item() || !cmd->item()->sceneBoundingRect().intersects(sceneArea))
            continue;
        set.add(cmd->toolId(), *cmd->item(), cmd->item()->pos() - sceneArea.topLeft());
    }
    return set;
}

// ---- Screen::SelectionLayer ------------------------------------------------

std::optional<QString> OverlayAnnotations::hint() const
{
    if (isEditingText())
        return QStringLiteral("Type your text  ·  Esc to finish");
    if (!isArmed())
        return std::nullopt;
    const QString verb = m_activeTool == QLatin1String("text") ? QStringLiteral("Click to type")
                       : m_activeTool == QLatin1String("step") ? QStringLiteral("Click to place a step")
                       : QStringLiteral("Drag to draw");
    return QStringLiteral("%1  ·  %2 to undo  ·  Esc to stop drawing").arg(verb, keyText(QKeySequence::Undo));
}

QVector<Screen::SelectionLayer::ToolbarSlot> OverlayAnnotations::toolbarSlots() const
{
    QVector<ToolbarSlot> out;
    for (QLatin1StringView id : kOverlayTools) {
        ToolbarSlot slot;
        slot.id = QString(id);
        if (const Editor::ToolSpec *spec = Editor::ToolRegistry::find(slot.id)) {
            slot.iconPath = spec->iconPath();
            slot.glyph = QString(spec->shortcut);
            slot.tooltip = QStringLiteral("%1 (%2)").arg(spec->tooltip, QString(spec->shortcut));
        }
        slot.checked = slot.id == m_activeTool;
        out.append(slot);
    }

    ToolbarSlot undo;
    undo.id = kUndoSlot;
    undo.iconPath = QStringLiteral(":/icons/icons/undo.svg");
    undo.glyphPath = historyGlyph(false);
    undo.tooltip = QStringLiteral("Undo (%1)").arg(keyText(QKeySequence::Undo));
    undo.group = 1;
    undo.enabled = canUndo();
    out.append(undo);

    ToolbarSlot redo;
    redo.id = kRedoSlot;
    redo.iconPath = QStringLiteral(":/icons/icons/redo.svg");
    redo.glyphPath = historyGlyph(true);
    redo.tooltip = QStringLiteral("Redo (%1)").arg(keyText(QKeySequence::Redo));
    redo.group = 1;
    redo.enabled = canRedo();
    out.append(redo);
    return out;
}

void OverlayAnnotations::activateSlot(const QString &id)
{
    if (id == kUndoSlot)
        undo();
    else if (id == kRedoSlot)
        redo();
    else
        toggleTool(id);
}

void OverlayAnnotations::toggleTool(const QString &id)
{
    setActiveTool(m_activeTool == id ? QString() : id);
}

bool OverlayAnnotations::handleKey(QKeyEvent *event, SelectionContext &context)
{
    // While text is being typed every key belongs to it; Esc commits the text.
    if (isEditingText()) {
        if (event->key() == Qt::Key_Escape)
            commitPendingText();
        else
            forwardKey(event);
        return true;
    }
    if (event->key() == Qt::Key_Escape && isDrawing()) {
        cancelStroke();
        return true;
    }
    return handleToolKey(event, context);
}

bool OverlayAnnotations::handleToolKey(QKeyEvent *event, SelectionContext &context)
{
    if (!context.actionsAvailable()) return false;

    if (event->matches(QKeySequence::Undo)) {
        undo();
        return true;
    }
    if (event->matches(QKeySequence::Redo)) {
        redo();
        return true;
    }

    if (event->key() == Qt::Key_Escape) {
        if (m_activeTool.isEmpty()) return false;
        setActiveTool({});
    } else {
        if (event->modifiers() != Qt::NoModifier || event->key() < Qt::Key_A || event->key() > Qt::Key_Z)
            return false;
        const Editor::ToolSpec *spec = Editor::ToolRegistry::findByShortcut(QChar(event->key()));
        if (!spec || std::find(std::begin(kOverlayTools), std::end(kOverlayTools), spec->id)
                         == std::end(kOverlayTools))
            return false;
        if (event->isAutoRepeat()) return true;
        toggleTool(spec->id);
    }
    context.refreshCursor();
    return true;
}

bool OverlayAnnotations::handleMouse(MouseAction action, const QPoint &virt, Qt::MouseButton button,
                                     SelectionContext &context)
{
    const bool left = button == Qt::LeftButton;
    // With a tool armed, presses inside the selection draw instead of moving or committing.
    const bool armed = isArmed() && context.adjusting();
    switch (action) {
        case MouseAction::Press: {
            if (!left || !armed) return false;
            const SelectionContext::Hit hit = context.hitTest(virt);
            if (hit == SelectionContext::Hit::Handle) return false;   // handles still resize
            // Outside the selection the press only ends typing: no fresh selection while drawing.
            const bool reachesSession = hit == SelectionContext::Hit::Inside || isEditingText();
            if (reachesSession && press(virt))
                context.setCursor(cursor());
            return true;
        }
        case MouseAction::Move:
            if (!isDrawing()) return false;
            move(virt);
            return true;
        case MouseAction::Release:
            if (!left || !isDrawing()) return false;
            release(virt);
            return true;
        case MouseAction::DoubleClick:
            // The platform already delivered this click's press, so only keep it from committing.
            return left && armed;
    }
    return false;
}

} // namespace Capture
