#include "EllipseTool.h"
#include <QPainter>
#include <QPainterPath>
#include <QPainterPathStroker>

namespace ImageEditor::Tools {

EllipseTool::EllipseTool(const QRectF &rect, QGraphicsItem *parent)
    : ShapeTool(rect, parent)
{
}

void EllipseTool::paintShape(QPainter *painter)
{
    // Draw the ellipse
    painter->drawEllipse(m_shapeRect);
}

QPainterPath EllipseTool::createShapePath() const
{
    QPainterPath path;
    path.addEllipse(m_shapeRect);

    // Create a stroke path for better mouse interaction
    if (m_pen.widthF() > 0) {
        QPainterPathStroker stroker;
        stroker.setCapStyle(m_pen.capStyle());
        stroker.setJoinStyle(m_pen.joinStyle());
        stroker.setWidth(qMax(m_pen.widthF(), 5.0)); // Minimum 5px for easier clicking

        QPainterPath strokePath = stroker.createStroke(path);

        // If we have a fill, include the entire ellipse
        if (m_brush.style() != Qt::NoBrush) {
            return strokePath.united(path);
        }

        return strokePath;
    }

    return path;
}

} // namespace ImageEditor::Tools
