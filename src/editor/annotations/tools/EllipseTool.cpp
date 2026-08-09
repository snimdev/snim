#include "EllipseTool.h"
#include <QPainterPath>

namespace Editor::Tools {

EllipseTool::EllipseTool(const QRectF &rect, QGraphicsItem *parent)
    : ShapeTool(rect, parent)
{
}

QGraphicsItem* EllipseTool::clone() const
{
    auto* copy = new EllipseTool(shapeRect());
    copy->applyStyleFrom(this);
    return copy;
}

QPainterPath EllipseTool::outline() const
{
    QPainterPath path;
    path.addEllipse(m_shapeRect);
    return path;
}

} // namespace Editor::Tools
