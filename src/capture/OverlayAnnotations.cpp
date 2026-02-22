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
#include <QKeyEvent>
#include <QPainter>
#include <QUndoStack>

namespace Capture {

using Editor::Commands::AddItemCommand;

void OverlayAnnotations::UndoStackSink::commit(QGraphicsItem *item, const QString &toolId)
{
    const Editor::ToolSpec *spec = Editor::ToolRegistry::find(toolId);
    const QString text = QStringLiteral("Add %1").arg(spec ? spec->namePrefix : toolId);
    m_session->m_stack->push(new AddItemCommand(m_session->m_scene.get(), item, toolId, text));
    m_session->m_builder->rememberStyle(item, toolId);
}

OverlayAnnotations::OverlayAnnotations(const QPixmap &frame, const QRect &virtualGeometry,
                                       QObject *parent)
    : QObject(parent)
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
    if (auto *previous = activeInteraction())
        previous->cleanup(m_scene.get());

    const QString previousId = m_activeTool;
    const Editor::ToolSpec *spec = Editor::ToolRegistry::find(id);
    if (!spec || !spec->isDrawingTool || !m_builder->interaction(id)) {
        m_activeTool.clear();
    } else {
        m_activeTool = id;
        Editor::Tools::ITool *tmpl = m_builder->templateFor(id);
        if (spec->syncStrategy && tmpl)
            spec->syncStrategy(m_builder->interaction(id), tmpl);
    }
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
    QPixmap out = cropVirtualArea(m_frame, m_virtualGeometry, area);
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

} // namespace Capture
