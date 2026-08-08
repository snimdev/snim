#include "ShapeTool.h"
#include "HandleItem.h"
#include <QPainter>
#include <QPainterPath>
#include <QStyleOptionGraphicsItem>

namespace Editor::Tools {

ShapeTool::ShapeTool(const QRectF &rect, QGraphicsItem *parent)
    : QGraphicsObject(parent)
    , m_shapeRect(rect)
    , m_pen(Qt::black, 2, Qt::SolidLine, Qt::SquareCap, Qt::MiterJoin)
    , m_brush(Qt::NoBrush)
    , m_opacity(1.0)
{
    setFlags(QGraphicsItem::ItemIsSelectable |
             QGraphicsItem::ItemIsMovable |
             QGraphicsItem::ItemSendsGeometryChanges |
             QGraphicsItem::ItemIsFocusable);

    setAcceptHoverEvents(true);
    setCursor(Qt::SizeAllCursor);

    // Create resize handles
    createHandles();

    updateGeometry();
}

void ShapeTool::createHandles()
{
    for (int i = 0; i < 8; ++i) {
        const auto position = static_cast<HandlePosition>(i);
        m_handles[i] = new HandleItem(cursorFor(position), [this, position](const QPointF &p) {
            updateHandlePosition(position, p);
        }, this);
    }
}

Qt::CursorShape ShapeTool::cursorFor(HandlePosition position)
{
    switch (position) {
    case TopLeft:
    case BottomRight:
        return Qt::SizeFDiagCursor;
    case TopCenter:
    case BottomCenter:
        return Qt::SizeVerCursor;
    case TopRight:
    case BottomLeft:
        return Qt::SizeBDiagCursor;
    case MiddleLeft:
    case MiddleRight:
        return Qt::SizeHorCursor;
    }
    return Qt::SizeAllCursor;
}

void ShapeTool::updateGeometry()
{
    prepareGeometryChange();

    // Calculate bounding rect with padding for stroke and handles
    qreal halfPen = m_pen.widthF() / 2.0;
    qreal padding = 20.0;
    m_boundingRect = m_shapeRect.adjusted(-halfPen - padding, -halfPen - padding,
                                           halfPen + padding, halfPen + padding);

    // Update handle positions
    updateHandles();

    update();
}

void ShapeTool::updateHandles()
{
    for (int i = 0; i < 8; ++i)
        m_handles[i]->setPos(getHandlePosition(static_cast<HandlePosition>(i)));
}

QPointF ShapeTool::getHandlePosition(HandlePosition position) const
{
    switch (position) {
    case TopLeft:
        return m_shapeRect.topLeft();
    case TopCenter:
        return QPointF(m_shapeRect.center().x(), m_shapeRect.top());
    case TopRight:
        return m_shapeRect.topRight();
    case MiddleLeft:
        return QPointF(m_shapeRect.left(), m_shapeRect.center().y());
    case MiddleRight:
        return QPointF(m_shapeRect.right(), m_shapeRect.center().y());
    case BottomLeft:
        return m_shapeRect.bottomLeft();
    case BottomCenter:
        return QPointF(m_shapeRect.center().x(), m_shapeRect.bottom());
    case BottomRight:
        return m_shapeRect.bottomRight();
    }
    return QPointF();
}

void ShapeTool::updateHandlePosition(HandlePosition position, const QPointF &scenePos)
{
    // Convert scene position to local coordinates
    QPointF localPos = mapFromScene(scenePos);

    QRectF newRect = m_shapeRect;

    // Update the rectangle based on which handle is being dragged
    switch (position) {
    case TopLeft:
        newRect.setTopLeft(localPos);
        break;
    case TopCenter:
        newRect.setTop(localPos.y());
        break;
    case TopRight:
        newRect.setTopRight(localPos);
        break;
    case MiddleLeft:
        newRect.setLeft(localPos.x());
        break;
    case MiddleRight:
        newRect.setRight(localPos.x());
        break;
    case BottomLeft:
        newRect.setBottomLeft(localPos);
        break;
    case BottomCenter:
        newRect.setBottom(localPos.y());
        break;
    case BottomRight:
        newRect.setBottomRight(localPos);
        break;
    }

    // Normalize the rectangle (handle negative width/height from dragging)
    setShapeRect(newRect.normalized());
}

QRectF ShapeTool::boundingRect() const
{
    return m_boundingRect;
}

QPainterPath ShapeTool::shape() const
{
    return createShapePath();
}

void ShapeTool::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    Q_UNUSED(widget)

    painter->setRenderHint(QPainter::Antialiasing);

    // Apply opacity
    painter->setOpacity(m_opacity);

    // Set pen and brush
    painter->setPen(m_pen);
    painter->setBrush(m_brush);

    // Let derived class paint the specific shape
    paintShape(painter);

    // Draw selection indicator (always full opacity)
    if (option->state & QStyle::State_Selected) {
        painter->setOpacity(1.0);
        QPen selectionPen(Qt::blue, 1.0, Qt::DashLine);
        painter->setPen(selectionPen);
        painter->setBrush(Qt::NoBrush);
        painter->drawRect(m_shapeRect);
    }
}

void ShapeTool::setShapeRect(const QRectF &rect)
{
    if (m_shapeRect != rect) {
        m_shapeRect = rect;
        updateGeometry();
    }
}

void ShapeTool::setPen(const QPen &pen)
{
    if (m_pen != pen) {
        m_pen = pen;
        updateGeometry();
    }
}

void ShapeTool::setBrush(const QBrush &brush)
{
    if (m_brush != brush) {
        m_brush = brush;
        update();
    }
}

void ShapeTool::setOpacity(qreal opacity)
{
    // Clamp opacity between 0.0 and 1.0
    opacity = qBound(0.0, opacity, 1.0);
    if (!qFuzzyCompare(m_opacity, opacity)) {
        m_opacity = opacity;
        update();
    }
}

QVariant ShapeTool::itemChange(GraphicsItemChange change, const QVariant &value)
{
    if (change == ItemSelectedChange) {
        const bool selected = value.toBool();
        for (HandleItem *handle : m_handles)
            handle->setVisible(selected);
    }

    return QGraphicsObject::itemChange(change, value);
}

QList<ToolProperty> ShapeTool::getProperties() const
{
    QList<ToolProperty> properties;

    ToolProperty strokeColorProp;
    strokeColorProp.id = "strokeColor";
    strokeColorProp.name = "Stroke Color";
    strokeColorProp.value = m_pen.color();
    strokeColorProp.controlType = "color";
    properties.append(strokeColorProp);

    ToolProperty strokeWidthProp;
    strokeWidthProp.id = "strokeWidth";
    strokeWidthProp.name = "Stroke Width";
    strokeWidthProp.value = m_pen.widthF();
    strokeWidthProp.controlType = "slider";
    strokeWidthProp.options["min"] = 0;
    strokeWidthProp.options["max"] = 20;
    properties.append(strokeWidthProp);

    ToolProperty fillColorProp;
    fillColorProp.id = "fillColor";
    fillColorProp.name = "Fill Color";
    fillColorProp.value = m_brush.color();
    fillColorProp.controlType = "color";
    properties.append(fillColorProp);

    ToolProperty opacityProp;
    opacityProp.id = "opacity";
    opacityProp.name = "Opacity";
    opacityProp.value = m_opacity * 100; // Convert to percentage for display
    opacityProp.controlType = "slider";
    opacityProp.options["min"] = 0;
    opacityProp.options["max"] = 100;
    properties.append(opacityProp);

    return properties;
}

void ShapeTool::applyStyleFrom(const ITool* other)
{
    if (const auto* o = dynamic_cast<const ShapeTool*>(other)) {
        setPen(o->pen());
        setBrush(o->brush());
        setOpacity(o->getOpacity());
    }
}

void ShapeTool::setProperty(const QString& propertyId, const QVariant& value)
{
    if (propertyId == "strokeColor" && value.canConvert<QColor>()) {
        QPen newPen = m_pen;
        newPen.setColor(value.value<QColor>());
        setPen(newPen);
    } else if (propertyId == "strokeWidth" && value.canConvert<qreal>()) {
        QPen newPen = m_pen;
        newPen.setWidthF(value.toReal());
        setPen(newPen);
    } else if (propertyId == "fillColor" && value.canConvert<QColor>()) {
        QColor color = value.value<QColor>();
        QBrush newBrush = m_brush;
        newBrush.setColor(color);
        newBrush.setStyle(color.alpha() > 0 ? Qt::SolidPattern : Qt::NoBrush);
        setBrush(newBrush);
    } else if (propertyId == "opacity" && value.canConvert<qreal>()) {
        // Convert from percentage (0-100) to opacity (0.0-1.0)
        qreal opacityValue = value.toReal() / 100.0;
        setOpacity(opacityValue);
    }
}

} // namespace Editor::Tools
