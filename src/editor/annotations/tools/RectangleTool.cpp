#include "RectangleTool.h"
#include <QPainter>
#include <QPainterPath>

namespace Editor::Tools {

RectangleTool::RectangleTool(const QRectF &rect, QGraphicsItem *parent)
    : ShapeTool(rect, parent)
{
}

QGraphicsItem* RectangleTool::clone() const
{
    auto* copy = new RectangleTool(shapeRect());
    copy->applyStyleFrom(this);
    return copy;
}

QPainterPath RectangleTool::outline() const
{
    QPainterPath path;
    path.addRect(m_shapeRect);
    return path;
}

void RectangleTool::paintShape(QPainter *painter)
{
    painter->drawRect(m_shapeRect);
}

} // namespace Editor::Tools
