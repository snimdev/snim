#include "RectangleTool.h"
#include <QPainter>
#include <QPainterPath>
#include <QPainterPathStroker>

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

void RectangleTool::paintShape(QPainter *painter)
{
    // Draw the rectangle
    painter->drawRect(m_shapeRect);
}

QPainterPath RectangleTool::createShapePath() const
{
    // Always include the filled interior so the whole shape is grabbable, not just
    // its outline (an unfilled rectangle would otherwise only respond on its border).
    QPainterPath path;
    path.addRect(m_shapeRect);

    if (m_pen.widthF() > 0) {
        // Widen the outline a little (min 5px) so the edge is easy to click too.
        QPainterPathStroker stroker;
        stroker.setCapStyle(m_pen.capStyle());
        stroker.setJoinStyle(m_pen.joinStyle());
        stroker.setWidth(qMax(m_pen.widthF(), 5.0));
        return stroker.createStroke(path).united(path);
    }

    return path;
}

} // namespace Editor::Tools
