#include "AnnotationBuilder.h"

#include "IAnnotationSink.h"
#include "ToolRegistry.h"
#include "tools/ITool.h"
#include "tools/ArrowTool.h"
#include "tools/RectangleTool.h"
#include "tools/EllipseTool.h"
#include "tools/FreehandTool.h"
#include "tools/HighlightTool.h"
#include "tools/BlurTool.h"
#include "tools/StepTool.h"
#include "interactions/DrawingInteractions.h"

#include <QGraphicsScene>
#include <QTimer>
#include <cmath>

namespace Editor {

using namespace Interactions;

AnnotationBuilder::AnnotationBuilder(QGraphicsScene *scene, IAnnotationSink *sink, QObject *parent)
    : QObject(parent)
    , m_scene(scene)
    , m_sink(sink)
{
    // One interaction per tool, plus one template per drawing tool, used to drive the
    // Properties panel and to style previews and freshly drawn items. Templates are never
    // added to a scene. Each tool's default styling lives in its ToolSpec::makeTemplate.
    for (const ToolSpec &spec : ToolRegistry::tools()) {
        Tools::ITool *tmpl = spec.makeTemplate ? spec.makeTemplate() : nullptr;
        IDrawingInteraction *interaction = spec.makeInteraction(this);
        if (auto *drag = dynamic_cast<DragInteraction*>(interaction))
            drag->setTemplate(tmpl);
        m_interactions.insert(spec.id, interaction);
        if (tmpl)
            m_templates.insert(spec.id, tmpl);
    }
    registerFactories();
}

AnnotationBuilder::~AnnotationBuilder()
{
    qDeleteAll(m_templates);
}

IDrawingInteraction *AnnotationBuilder::interaction(const QString &id) const
{
    return m_interactions.value(id);
}

Tools::ITool *AnnotationBuilder::templateFor(const QString &id) const
{
    return m_templates.value(id);
}

void AnnotationBuilder::setImageBounds(const QRect &bounds)
{
    for (IDrawingInteraction *i : std::as_const(m_interactions))
        if (auto *drag = dynamic_cast<DragInteraction*>(i))
            drag->setImageBounds(bounds);
}

void AnnotationBuilder::setSourcePixmap(const QPixmap &pixmap)
{
    m_sourcePixmap = pixmap;
}

void AnnotationBuilder::setStepNumberProvider(std::function<int()> provider)
{
    m_stepNumberProvider = std::move(provider);
}

void AnnotationBuilder::rememberStyle(QGraphicsItem *item, const QString &toolId)
{
    if (Tools::ITool *tmpl = templateFor(toolId))
        if (auto *asTool = dynamic_cast<Tools::ITool*>(item))
            tmpl->applyStyleFrom(asTool);
}

bool AnnotationBuilder::isEditingText() const
{
    if (!m_scene)
        return false;
    auto *t = dynamic_cast<Tools::TextTool*>(m_scene->focusItem());
    return t && t->textInteractionFlags() != Qt::NoTextInteraction;
}

void AnnotationBuilder::commitPendingText()
{
    if (Tools::TextTool *item = m_pendingTextItem) {
        item->clearFocus();
        finalizePendingText(item);   // the deferred focus-out call then finds nothing pending
    }
}

void AnnotationBuilder::finalizePendingText(Tools::TextTool *item)
{
    // Runs once per pending text box (guard against a double-scheduled deferral).
    if (!item || item != m_pendingTextItem)
        return;
    m_pendingTextItem = nullptr;
    disconnect(item, &Tools::TextTool::editingFinished, this, nullptr);

    // It's currently a bare scene item; take it off so the sink can add it back
    // through its own (undoable) path.
    m_scene->removeItem(item);

    if (item->toPlainText().trimmed().isEmpty()) {
        delete item;                 // empty -> create nothing (no layer, no undo entry)
        return;
    }
    m_sink->commit(item, "text");   // e.g. an undoable "Add Text" layer, named from its content
}

template <typename Factory, typename Interaction, typename... Args>
void AnnotationBuilder::connectFactory(const QString &id, void (Interaction::*signal)(Args...),
                                       Factory factory)
{
    if (auto *i = dynamic_cast<Interaction*>(interaction(id)))
        connect(i, signal, this, [this, id, factory](Args... args) {
            if (QGraphicsItem *item = factory(templateFor(id), args...))
                m_sink->commit(item, id);
        });
}

void AnnotationBuilder::registerFactories()
{
    using namespace Tools;

    // Each interaction's completion signal funnels into the sink. The freshly drawn
    // item is styled from the tool template first.

    connectFactory("arrow", &ArrowInteraction::arrowDrawn,
                   [](ITool *tmpl, const QPoint &s, const QPoint &e) -> QGraphicsItem* {
        // The interaction checked the unrounded drag; rounding to whole pixels can still
        // leave it under the minimum.
        const double dx = e.x() - s.x(), dy = e.y() - s.y();
        if (std::sqrt(dx * dx + dy * dy) < 10.0) return nullptr;
        auto *it = new ArrowTool(s, e);
        it->applyStyleFrom(tmpl);
        return it;
    });

    if (auto *t = dynamic_cast<ClickInteraction*>(interaction("text")))
        connect(t, &ClickInteraction::clicked, this, [this](const QPointF &position) {
            // Inline creation: drop an empty text box and edit it live (no popup).
            commitPendingText();   // a second box must not orphan the first
            auto *item = new TextTool("");
            item->setPos(position.toPoint());
            item->applyStyleFrom(templateFor("text"));
            m_scene->addItem(item);          // must be in the scene to take edit focus
            m_pendingTextItem = item;
            connect(item, &TextTool::editingFinished, this, [this, item]() {
                // editingFinished fires inside focusOutEvent, so defer commit/discard a tick.
                // Guarded: a discarded box's address can come back as the next pending one.
                QTimer::singleShot(0, this, [this, guard = QPointer<TextTool>(item)] {
                    finalizePendingText(guard);
                });
            });
            emit textPlaced(item);
            item->startEditing();            // caret appears at the click point; type directly
        });

    connectFactory("rectangle", &RectDragInteraction::rectDrawn,
                   [](ITool *tmpl, const QRect &rect) -> QGraphicsItem* {
        auto *it = new RectangleTool(rect);
        it->applyStyleFrom(tmpl);
        return it;
    });

    connectFactory("ellipse", &RectDragInteraction::rectDrawn,
                   [](ITool *tmpl, const QRect &rect) -> QGraphicsItem* {
        auto *it = new EllipseTool(rect);
        it->applyStyleFrom(tmpl);
        return it;
    });

    // The stroke tools share one recipe: the template's style, then the points (never
    // empty: the drag interaction drops a stroke without any).
    const auto stroke = [](PathTool *it, ITool *tmpl, const QList<QPointF> &pts) -> QGraphicsItem* {
        it->applyStyleFrom(tmpl);
        it->addPoints(pts);
        return it;
    };
    connectFactory("freehand", &PathDragInteraction::pathDrawn,
                   [stroke](ITool *tmpl, const QList<QPointF> &pts) {
        return stroke(new FreehandTool(), tmpl, pts);
    });
    connectFactory("highlight", &PathDragInteraction::pathDrawn,
                   [stroke](ITool *tmpl, const QList<QPointF> &pts) {
        return stroke(new HighlightTool(), tmpl, pts);
    });
    connectFactory("blur", &PathDragInteraction::pathDrawn,
                   [this, stroke](ITool *tmpl, const QList<QPointF> &pts) {
        auto *it = new BlurTool();
        it->setSourcePixmap(m_sourcePixmap);
        return stroke(it, tmpl, pts);
    });

    connectFactory("step", &ClickInteraction::clicked,
                   [this](ITool *t, const QPointF &pos) -> QGraphicsItem* {
        auto *tmpl = dynamic_cast<StepTool*>(t);
        auto *it = new StepTool();
        it->applyStyleFrom(tmpl);
        int n = 0;
        if (!tmpl || !tmpl->takePendingOverride(&n))
            n = m_stepNumberProvider ? m_stepNumberProvider() : 1;
        it->setNumber(n);
        it->setPos(pos);
        // Advance before the commit so a sink refreshing its panel shows the next number.
        if (tmpl)
            tmpl->setNumber(n + 1);
        return it;
    });
}

} // namespace Editor
