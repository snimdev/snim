#include "FreehandTool.h"
#include <QPainter>
#include <QPainterPath>
#include <QPainterPathStroker>
#include <QStyleOptionGraphicsItem>

namespace ImageEditor::Tools {

FreehandTool::FreehandTool(QGraphicsItem *parent)
    : QGraphicsObject(parent)
    , m_pen(Qt::red, 3, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin)
{
    setFlags(QGraphicsItem::ItemIsSelectable |
             QGraphicsItem::ItemIsMovable |
             QGraphicsItem::ItemSendsGeometryChanges |
             QGraphicsItem::ItemIsFocusable);

    setAcceptHoverEvents(true);
    setCursor(Qt::SizeAllCursor);
}

void FreehandTool::addPoint(const QPointF &point)
{
    m_points.append(point);

    if (m_points.size() == 1) {
        m_path.moveTo(point);
    } else {
        m_path.lineTo(point);
    }

    updateGeometry();
}

void FreehandTool::finishPath()
{
    // Path is already complete, just update final geometry
    updateGeometry();
}

void FreehandTool::updateGeometry()
{
    prepareGeometryChange();

    // Create stroke path for interaction
    m_strokePath = createStrokePath();

    // Calculate bounding rect
    QRectF bounds = m_strokePath.boundingRect();

    // Add some padding
    qreal padding = 10.0;
    bounds.adjust(-padding, -padding, padding, padding);
    m_boundingRect = bounds;

    update();
}

QPainterPath FreehandTool::createStrokePath() const
{
    if (m_path.isEmpty()) {
        return QPainterPath();
    }

    QPainterPathStroker stroker;
    stroker.setCapStyle(m_pen.capStyle());
    stroker.setJoinStyle(m_pen.joinStyle());
    stroker.setWidth(qMax(m_pen.widthF(), 5.0)); // Minimum 5px for easier clicking

    return stroker.createStroke(m_path);
}

QRectF FreehandTool::boundingRect() const
{
    return m_boundingRect;
}

QPainterPath FreehandTool::shape() const
{
    return m_strokePath;
}

void FreehandTool::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    Q_UNUSED(widget)

    if (m_path.isEmpty()) {
        return;
    }

    painter->setRenderHint(QPainter::Antialiasing);

    // Draw the path
    painter->setPen(m_pen);
    painter->setBrush(Qt::NoBrush);
    painter->drawPath(m_path);

    // Draw selection indicator
    if (option->state & QStyle::State_Selected) {
        QPen selectionPen(Qt::blue, 1.0, Qt::DashLine);
        painter->setPen(selectionPen);
        painter->setBrush(Qt::NoBrush);
        painter->drawRect(m_path.boundingRect());
    }
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
    for (const QPointF &p : m_points)
        copy->addPoint(p);
    copy->finishPath();
    return copy;
}

void FreehandTool::applyStyleFrom(const ITool* other)
{
    if (const auto* o = dynamic_cast<const FreehandTool*>(other))
        setPen(o->pen());
}

QVariant FreehandTool::itemChange(GraphicsItemChange change, const QVariant &value)
{
    if (change == ItemSelectedChange) {
        // Handle selection change if needed
    } else if (change == ItemPositionHasChanged) {
        emit pathChanged();
    }

    return QGraphicsObject::itemChange(change, value);
}

QList<ToolProperty> FreehandTool::getProperties() const
{
    QList<ToolProperty> properties;

    ToolProperty colorProp;
    colorProp.id = "color";
    colorProp.name = "Stroke Color";
    colorProp.value = m_pen.color();
    colorProp.controlType = "color";
    properties.append(colorProp);

    ToolProperty widthProp;
    widthProp.id = "width";
    widthProp.name = "Stroke Width";
    widthProp.value = m_pen.widthF();
    widthProp.controlType = "slider";
    widthProp.options["min"] = 1;
    widthProp.options["max"] = 20;
    properties.append(widthProp);

    return properties;
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

} // namespace ImageEditor::Tools
