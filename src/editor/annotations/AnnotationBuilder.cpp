#include "AnnotationBuilder.h"

#include "ToolRegistry.h"
#include "tools/ITool.h"
#include "interactions/BaseDrawingInteraction.h"
#include "interactions/BlurDrawingInteraction.h"

namespace Editor {

using namespace Interactions;

AnnotationBuilder::AnnotationBuilder(QObject *parent)
    : QObject(parent)
{
    // One interaction per tool, plus one template per drawing tool, used both to drive
    // the Properties panel and to style freshly-drawn items. Templates are never added
    // to a scene. Each tool's default styling lives in its ToolSpec::makeTemplate closure.
    for (const ToolSpec &spec : ToolRegistry::tools()) {
        m_interactions.insert(spec.id, spec.makeInteraction(this));
        if (spec.makeTemplate)
            m_templates.insert(spec.id, spec.makeTemplate());
    }
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

void AnnotationBuilder::rememberStyle(QGraphicsItem *item, const QString &toolId)
{
    if (Tools::ITool *tmpl = templateFor(toolId))
        if (auto *asTool = dynamic_cast<Tools::ITool*>(item))
            tmpl->applyStyleFrom(asTool);
}

} // namespace Editor
