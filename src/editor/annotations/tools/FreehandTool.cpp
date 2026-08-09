#include "FreehandTool.h"
#include <QPainter>

namespace Editor::Tools {

FreehandTool::FreehandTool(QGraphicsItem *parent)
    : PathTool(parent)
    , m_pen(Qt::red, 3, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin)
{
}

QPen FreehandTool::strokePen() const
{
    QPen pen = m_pen;
    pen.setWidthF(qMax(m_pen.widthF(), 5.0));   // thin strokes stay easy to click
    return pen;
}

void FreehandTool::paintPath(QPainter *painter)
{
    painter->setPen(m_pen);
    painter->setBrush(Qt::NoBrush);
    painter->drawPath(m_path);
}

void FreehandTool::setPen(const QPen &pen)
{
    if (m_pen != pen) {
        m_pen = pen;
        updateGeometry();
    }
}

QGraphicsItem* FreehandTool::clone() const
{
    auto* copy = new FreehandTool();
    copy->applyStyleFrom(this);
    copy->addPoints(points());
    return copy;
}

void FreehandTool::applyStyleFrom(const ITool* other)
{
    if (const auto* o = dynamic_cast<const FreehandTool*>(other))
        setPen(o->pen());
}

QList<ToolProperty> FreehandTool::getProperties() const
{
    return {
        {"color", "Stroke Color", m_pen.color(), "color"},
        {"width", "Stroke Width", m_pen.widthF(), "slider", {{"min", 1}, {"max", 20}}},
    };
}

void FreehandTool::setProperty(const QString& propertyId, const QVariant& value)
{
    if (propertyId == "color" && value.canConvert<QColor>()) {
        QPen newPen = m_pen;
        newPen.setColor(value.value<QColor>());
        setPen(newPen);
    } else if (propertyId == "width" && value.canConvert<qreal>()) {
        QPen newPen = m_pen;
        newPen.setWidthF(value.toReal());
        setPen(newPen);
    }
}

} // namespace Editor::Tools
