#include "EllipseTool.h"
#include <QPainter>
#include <QPainterPath>
#include <QPainterPathStroker>

namespace ImageEditor::Tools {

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

void EllipseTool::paintShape(QPainter *painter)
{
    // Draw the ellipse
    painter->drawEllipse(m_shapeRect);
}

QPainterPath EllipseTool::createShapePath() const
{
    // Always include the filled interior so the whole shape is grabbable, not just
    // its outline (an unfilled ellipse would otherwise only respond on its border).
    QPainterPath path;
    path.addEllipse(m_shapeRect);

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

} // namespace ImageEditor::Tools
