#include "ShapeHandleTool.h"
#include "ShapeTool.h"
#include <QPainter>
#include <QGraphicsSceneMouseEvent>
#include <QCursor>

namespace ImageEditor::Tools {

ShapeHandleTool::ShapeHandleTool(HandlePosition position, ShapeTool *shapeTool, QGraphicsItem *parent)
    : QGraphicsEllipseItem(parent)
    , m_position(position)
    , m_shapeTool(shapeTool)
    , m_dragging(false)
    , m_hovered(false)
{
    // Set up the handle appearance
    setPen(QPen(Qt::blue, 2));
    setBrush(QBrush(Qt::white));

    // Make it interactive
    setFlags(QGraphicsItem::ItemSendsGeometryChanges);

    setAcceptHoverEvents(true);
    setCursor(getCursorForPosition());

    // Ensure handles are drawn on top
    setZValue(1000);

    // Set initial size
    setRect(-HANDLE_SIZE/2, -HANDLE_SIZE/2, HANDLE_SIZE, HANDLE_SIZE);
}

void ShapeHandleTool::updatePosition(const QPointF &point)
{
    // Position handle at the given point (centered)
    setPos(point);
}

Qt::CursorShape ShapeHandleTool::getCursorForPosition() const
{
    // Set appropriate cursor based on handle position
    switch (m_position) {
    case 0: // TopLeft
    case 7: // BottomRight
        return Qt::SizeFDiagCursor;
    case 1: // TopCenter
    case 6: // BottomCenter
        return Qt::SizeVerCursor;
    case 2: // TopRight
    case 5: // BottomLeft
        return Qt::SizeBDiagCursor;
    case 3: // MiddleLeft
    case 4: // MiddleRight
        return Qt::SizeHorCursor;
    default:
        return Qt::SizeAllCursor;
    }
}

void ShapeHandleTool::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_dragging = true;
        event->accept();
    } else {
        QGraphicsEllipseItem::mousePressEvent(event);
    }
}

void ShapeHandleTool::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    if (m_dragging && m_shapeTool) {
        // Update the shape's rectangle based on the new handle position
        m_shapeTool->updateHandlePosition(
            static_cast<ShapeTool::HandlePosition>(m_position),
            event->scenePos()
        );

        event->accept();
    } else {
        QGraphicsEllipseItem::mouseMoveEvent(event);
    }
}

void ShapeHandleTool::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && m_dragging) {
        m_dragging = false;
        event->accept();
    } else {
        QGraphicsEllipseItem::mouseReleaseEvent(event);
    }
}

void ShapeHandleTool::hoverEnterEvent(QGraphicsSceneHoverEvent *event)
{
    m_hovered = true;

    // Grow handle on hover for better visibility
    setRect(-HANDLE_HOVER_SIZE/2, -HANDLE_HOVER_SIZE/2,
            HANDLE_HOVER_SIZE, HANDLE_HOVER_SIZE);

    setPen(QPen(Qt::blue, 2.5));

    QGraphicsEllipseItem::hoverEnterEvent(event);
    update();
}

void ShapeHandleTool::hoverLeaveEvent(QGraphicsSceneHoverEvent *event)
{
    m_hovered = false;

    // Return to normal size
    setRect(-HANDLE_SIZE/2, -HANDLE_SIZE/2, HANDLE_SIZE, HANDLE_SIZE);

    setPen(QPen(Qt::blue, 2));

    QGraphicsEllipseItem::hoverLeaveEvent(event);
    update();
}

void ShapeHandleTool::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    Q_UNUSED(option)
    Q_UNUSED(widget)

    painter->setRenderHint(QPainter::Antialiasing);

    // Draw the handle
    if (m_hovered) {
        // Highlighted appearance when hovered
        painter->setPen(QPen(Qt::blue, 2.5));
        painter->setBrush(QBrush(QColor(200, 220, 255)));
    } else {
        // Normal appearance
        painter->setPen(QPen(Qt::blue, 2));
        painter->setBrush(QBrush(Qt::white));
    }

    painter->drawEllipse(rect());

    // Draw a small center dot for better visibility
    painter->setPen(Qt::NoPen);
    painter->setBrush(QBrush(Qt::blue));
    qreal dotSize = 2.0;
    painter->drawEllipse(QRectF(-dotSize/2, -dotSize/2, dotSize, dotSize));
}

} // namespace ImageEditor::Tools
