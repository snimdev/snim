#include "ArrowTool.h"
#include "ArrowHandleTool.h"
#include <QPainter>
#include <QPainterPath>
#include <QPainterPathStroker>
#include <QStyleOptionGraphicsItem>
#include <cmath>

namespace Editor::Tools {

ArrowTool::ArrowTool(const QPointF &start, const QPointF &end, QGraphicsItem *parent)
    : QGraphicsObject(parent)
    , m_startPoint(start)
    , m_endPoint(end)
    , m_pen(Qt::red, 3, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin)
    , m_arrowHeadType(Outlined)
    , m_startHandle(nullptr)
    , m_endHandle(nullptr)
{
    setFlags(QGraphicsItem::ItemIsSelectable |
             QGraphicsItem::ItemIsMovable |  // Make the arrow movable
             QGraphicsItem::ItemSendsGeometryChanges |
             QGraphicsItem::ItemIsFocusable);

    setAcceptHoverEvents(true);
    setCursor(Qt::SizeAllCursor);  // Show move cursor when hovering

    // Create handles
    m_startHandle = new ArrowHandleTool(ArrowHandleTool::StartHandle, this, this);
    m_endHandle = new ArrowHandleTool(ArrowHandleTool::EndHandle, this, this);

    // Initially hide handles
    m_startHandle->setVisible(false);
    m_endHandle->setVisible(false);

    updateGeometry();
}

void ArrowTool::updateGeometry()
{
    prepareGeometryChange();

    // Create arrow path (the main line)
    m_arrowPath = createArrowPath();

    // Create arrow head path
    m_arrowHeadPath = createArrowHeadPath();

    // Create stroke path for interaction
    m_strokePath = createStrokePath();

    // Calculate bounding rect
    QRectF bounds = m_strokePath.boundingRect();

    // Add some padding for selection handles
    qreal padding = 20.0;
    bounds.adjust(-padding, -padding, padding, padding);
    m_boundingRect = bounds;

    // Update handle positions
    updateHandles();

    update();
}

QPainterPath ArrowTool::createArrowPath() const
{
    QPainterPath path;
    path.moveTo(m_startPoint);
    path.lineTo(m_endPoint);

    // Ensure we have at least a tiny line
    if (path.isEmpty() || (m_startPoint - m_endPoint).manhattanLength() < 0.01) {
        path.moveTo(m_startPoint);
        path.lineTo(m_startPoint.x() + 0.0001, m_startPoint.y());
    }

    return path;
}

QPainterPath ArrowTool::createArrowHeadPath() const
{
    QPainterPath headPath;

    QLineF mainLine(m_startPoint, m_endPoint);
    qreal length = mainLine.length();

    if (length < 10) {
        return headPath; // Too short for arrowhead
    }

    const qreal headLength = qMax(8.0, m_pen.widthF() * 3.0);
    const qreal angle = mainLine.angle() + 180;

    QLineF headLine1 = QLineF::fromPolar(headLength, angle + 30).translated(m_endPoint);
    QLineF headLine2 = QLineF::fromPolar(headLength, angle - 30).translated(m_endPoint);

    headPath.moveTo(headLine1.p2());
    headPath.lineTo(m_endPoint);
    headPath.lineTo(headLine2.p2());

    return headPath;
}

QPainterPath ArrowTool::createStrokePath() const
{
    QPainterPathStroker stroker;
    stroker.setCapStyle(m_pen.capStyle());
    stroker.setJoinStyle(m_pen.joinStyle());
    stroker.setWidth(m_pen.widthF());

    QPainterPath combinedPath = stroker.createStroke(m_arrowPath);

    if (!m_arrowHeadPath.isEmpty()) {
        combinedPath = combinedPath.united(stroker.createStroke(m_arrowHeadPath));
    }

    return combinedPath;
}

void ArrowTool::updateHandles()
{
    if (m_startHandle) {
        m_startHandle->updatePosition(m_startPoint);
    }
    if (m_endHandle) {
        m_endHandle->updatePosition(m_endPoint);
    }
}

QRectF ArrowTool::boundingRect() const
{
    return m_boundingRect;
}

QPainterPath ArrowTool::shape() const
{
    // Return stroke path for better mouse interaction
    return m_strokePath;
}

void ArrowTool::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    Q_UNUSED(widget)

    painter->setRenderHint(QPainter::Antialiasing);

    // Draw the main line
    painter->setPen(m_pen);
    painter->drawPath(m_arrowPath);

    // Draw arrow head
    if (!m_arrowHeadPath.isEmpty()) {
        if (m_arrowHeadType == Filled) {
            painter->setBrush(QBrush(m_pen.color()));
            painter->drawPath(m_arrowHeadPath);
        } else {
            painter->setBrush(Qt::NoBrush);
            painter->drawPath(m_arrowHeadPath);
        }
    }

    if (option->state & QStyle::State_Selected) {
        QPen selectionPen(Qt::blue, 1.0, Qt::DashLine);
        painter->setPen(selectionPen);
        painter->setBrush(Qt::NoBrush);
        painter->drawRect(m_strokePath.boundingRect());
    }
}

void ArrowTool::setStartPoint(const QPointF &point)
{
    if (m_startPoint != point) {
        m_startPoint = point;
        updateGeometry();
    }
}

void ArrowTool::setEndPoint(const QPointF &point)
{
    if (m_endPoint != point) {
        m_endPoint = point;
        updateGeometry();
    }
}

void ArrowTool::setPen(const QPen &pen)
{
    if (m_pen != pen) {
        m_pen = pen;
        updateGeometry();
    }
}

void ArrowTool::setArrowHeadType(ArrowHeadType type)
{
    if (m_arrowHeadType != type) {
        m_arrowHeadType = type;
        update();
    }
}

QVariant ArrowTool::itemChange(GraphicsItemChange change, const QVariant &value)
{
    if (change == ItemSelectedChange) {
        bool selected = value.toBool();
        // Show/hide handles based on selection
        if (m_startHandle) {
            m_startHandle->setVisible(selected);
        }
        if (m_endHandle) {
            m_endHandle->setVisible(selected);
        }
    }

    return QGraphicsObject::itemChange(change, value);
}

QList<ToolProperty> ArrowTool::getProperties() const
{
    QList<ToolProperty> properties;

    ToolProperty colorProp;
    colorProp.id = "color";
    colorProp.name = "Color";
    colorProp.value = m_pen.color();
    colorProp.controlType = "color";
    properties.append(colorProp);

    ToolProperty widthProp;
    widthProp.id = "width";
    widthProp.name = "Width";
    widthProp.value = m_pen.widthF();
    widthProp.controlType = "slider";
    widthProp.options["min"] = 1;
    widthProp.options["max"] = 20;
    properties.append(widthProp);

    ToolProperty headTypeProp;
    headTypeProp.id = "headType";
    headTypeProp.name = "Arrow Head";
    headTypeProp.value = m_arrowHeadType == Filled ? "Filled" : "Outlined";
    headTypeProp.controlType = "dropdown";
    headTypeProp.options["items"] = QStringList{"Outlined", "Filled"};
    properties.append(headTypeProp);

    return properties;
}

QGraphicsItem* ArrowTool::clone() const
{
    auto* copy = new ArrowTool(m_startPoint, m_endPoint);
    copy->applyStyleFrom(this);
    return copy;
}

void ArrowTool::applyStyleFrom(const ITool* other)
{
    if (const auto* o = dynamic_cast<const ArrowTool*>(other)) {
        setPen(o->pen());
        setArrowHeadType(o->arrowHeadType());
    }
}

void ArrowTool::setProperty(const QString& propertyId, const QVariant& value)
{
    if (propertyId == "color" && value.canConvert<QColor>()) {
        QPen newPen = m_pen;
        newPen.setColor(value.value<QColor>());
        setPen(newPen);
    } else if (propertyId == "width" && value.canConvert<qreal>()) {
        QPen newPen = m_pen;
        newPen.setWidthF(value.toReal());
        setPen(newPen);
    } else if (propertyId == "headType" && value.canConvert<QString>()) {
        QString type = value.toString();
        if (type == "Filled") {
            setArrowHeadType(Filled);
        } else {
            setArrowHeadType(Outlined);
        }
    }
}

} // namespace Editor::Tools
