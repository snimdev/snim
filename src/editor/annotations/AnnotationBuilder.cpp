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
#include "interactions/BaseDrawingInteraction.h"
#include "interactions/ArrowDrawingInteraction.h"
#include "interactions/RectangleDrawingInteraction.h"
#include "interactions/EllipseDrawingInteraction.h"
#include "interactions/FreehandDrawingInteraction.h"
#include "interactions/HighlightDrawingInteraction.h"
#include "interactions/BlurDrawingInteraction.h"
#include "interactions/StepDrawingInteraction.h"
#include "interactions/TextDrawingInteraction.h"

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
    // One interaction per tool, plus one template per drawing tool, used both to drive
    // the Properties panel and to style freshly-drawn items. Templates are never added
    // to a scene. Each tool's default styling lives in its ToolSpec::makeTemplate closure.
    for (const ToolSpec &spec : ToolRegistry::tools()) {
        m_interactions.insert(spec.id, spec.makeInteraction(this));
        if (spec.makeTemplate)
            m_templates.insert(spec.id, spec.makeTemplate());
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
    // All gesture tools derive BaseDrawingInteraction and clamp to these bounds.
    for (IDrawingInteraction *i : std::as_const(m_interactions))
        if (auto *b = dynamic_cast<BaseDrawingInteraction*>(i))
            b->setImageBounds(bounds);
}

void AnnotationBuilder::setSourcePixmap(const QPixmap &pixmap)
{
    m_sourcePixmap = pixmap;
    if (auto *blur = dynamic_cast<BlurDrawingInteraction*>(interaction("blur")))
        blur->setSourcePixmap(pixmap);
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

    // Each interaction's (type-specific) completion signal funnels into the sink. The
    // freshly-drawn item is styled from the tool template first.

    connectFactory("arrow", &ArrowDrawingInteraction::arrowDrawn,
                   [](ITool *tmpl, const QPoint &s, const QPoint &e) -> QGraphicsItem* {
        const double dx = e.x() - s.x(), dy = e.y() - s.y();
        if (std::sqrt(dx * dx + dy * dy) < 10.0) return nullptr;   // too short to be meaningful
        auto *it = new ArrowTool(s, e);
        it->applyStyleFrom(tmpl);
        return it;
    });

    if (auto *t = dynamic_cast<TextDrawingInteraction*>(interaction("text")))
        connect(t, &TextDrawingInteraction::textRequested, this, [this](const QPoint &position) {
            // Inline creation: drop an empty text box and edit it live (no popup).
            commitPendingText();   // a second box must not orphan the first
            auto *item = new TextTool("");
            item->setPos(position);
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

    connectFactory("rectangle", &RectangleDrawingInteraction::rectangleDrawn,
                   [](ITool *tmpl, const QRect &rect) -> QGraphicsItem* {
        auto *it = new RectangleTool(rect);
        it->applyStyleFrom(tmpl);
        return it;
    });

    connectFactory("ellipse", &EllipseDrawingInteraction::ellipseDrawn,
                   [](ITool *tmpl, const QRect &rect) -> QGraphicsItem* {
        auto *it = new EllipseTool(rect);
        it->applyStyleFrom(tmpl);
        return it;
    });

    connectFactory("freehand", &FreehandDrawingInteraction::freehandDrawn,
                   [](ITool *tmpl, const QList<QPointF> &pts) -> QGraphicsItem* {
        if (pts.isEmpty()) return nullptr;
        auto *it = new FreehandTool();
        it->applyStyleFrom(tmpl);
        for (const QPointF &p : pts) it->addPoint(p);
        it->finishPath();
        return it;
    });

    connectFactory("highlight", &HighlightDrawingInteraction::highlightDrawn,
                   [](ITool *tmpl, const QList<QPointF> &pts, const QColor &, qreal) -> QGraphicsItem* {
        if (pts.isEmpty()) return nullptr;
        auto *it = new HighlightTool();
        it->applyStyleFrom(tmpl);   // color/width carried by the template
        for (const QPointF &p : pts) it->addPoint(p);
        it->finishPath();
        return it;
    });

    connectFactory("blur", &BlurDrawingInteraction::blurDrawn,
                   [this](ITool *tmpl, const QList<QPointF> &pts) -> QGraphicsItem* {
        if (pts.isEmpty()) return nullptr;
        auto *it = new BlurTool();
        it->setSourcePixmap(m_sourcePixmap);
        it->applyStyleFrom(tmpl);
        for (const QPointF &p : pts) it->addPoint(p);
        it->finishPath();
        return it;
    });

    connectFactory("step", &StepDrawingInteraction::stepRequested,
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
