#include "AnnotationSet.h"

#include "tools/BlurTool.h"
#include "tools/ITool.h"

#include <QGraphicsItem>
#include <QPixmap>

namespace Editor {

namespace {

// Blur's clone() keeps the full source frame and re-blurs it; a prototype needs neither.
QGraphicsItem *makePrototype(const Tools::ITool &tool)
{
    if (const auto *blur = dynamic_cast<const Tools::BlurTool*>(&tool)) {
        auto *copy = new Tools::BlurTool();
        copy->applyStyleFrom(blur);
        for (const QPointF &p : blur->points())
            copy->addPoint(p);
        copy->finishPath();
        return copy;
    }
    return tool.clone();
}

} // namespace

void AnnotationSet::add(const QString &toolId, const QGraphicsItem &source, const QPointF &pos)
{
    const auto *tool = dynamic_cast<const Tools::ITool*>(&source);
    if (!tool)
        return;
    if (QGraphicsItem *prototype = makePrototype(*tool))
        m_entries.append({toolId, pos, std::shared_ptr<QGraphicsItem>(prototype)});
}

QList<AnnotationSet::Instance> AnnotationSet::instantiate(const QPixmap &blurSource) const
{
    QList<Instance> out;
    out.reserve(m_entries.size());
    for (const Entry &entry : m_entries) {
        const auto *tool = dynamic_cast<const Tools::ITool*>(entry.prototype.get());
        QGraphicsItem *item = tool ? tool->clone() : nullptr;
        if (!item)
            continue;
        item->setPos(entry.pos);
        // The patch is sampled at the item's pos, so the source goes in after setPos.
        if (auto *blur = dynamic_cast<Tools::BlurTool*>(item)) {
            blur->setSourcePixmap(blurSource);
            blur->finishPath();
        }
        out.append({entry.toolId, item});
    }
    return out;
}

} // namespace Editor
