#include "ArrowHandleTool.h"
#include "ArrowTool.h"
#include <QPainter>
#include <QGraphicsSceneMouseEvent>
#include <QCursor>

namespace Editor::Tools {

ArrowHandleTool::ArrowHandleTool(HandleType type, ArrowTool *arrowTool, QGraphicsItem *parent)
    : QGraphicsEllipseItem(parent)
    , m_type(type)
    , m_arrowTool(arrowTool)
    , m_dragging(false)
    , m_hovered(false)
{
    // Set up the handle appearance
    setPen(QPen(Qt::blue, 2));
    setBrush(QBrush(Qt::white));

    // Make it interactive but NOT movable (we handle movement manually)
    setFlags(QGraphicsItem::ItemSendsGeometryChanges);

    setAcceptHoverEvents(true);
    setCursor(Qt::PointingHandCursor);

    // Ensure handles are drawn on top
    setZValue(1000);

    // Set initial size
    setRect(-HANDLE_SIZE/2, -HANDLE_SIZE/2, HANDLE_SIZE, HANDLE_SIZE);

    qDebug() << "ArrowHandleTool created, type:" << (type == StartHandle ? "Start" : "End");
}

void ArrowHandleTool::updatePosition(const QPointF &point)
{
    // Position handle at the given point (centered)
    setPos(point);
}

void ArrowHandleTool::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_dragging = true;
        m_dragStartPos = event->scenePos();
        qDebug() << "ArrowHandleTool: Mouse press, starting drag";
        event->accept();
        // Don't propagate to parent - we're handling this
    } else {
        QGraphicsEllipseItem::mousePressEvent(event);
    }
}

void ArrowHandleTool::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    if (m_dragging && m_arrowTool) {
        // Convert scene position to arrow's local coordinate system
        QPointF newPos = m_arrowTool->mapFromScene(event->scenePos());

        qDebug() << "ArrowHandleTool: Dragging to" << newPos;

        // Update the appropriate arrow endpoint
        if (m_type == StartHandle) {
            m_arrowTool->setStartPoint(newPos);
        } else {
            m_arrowTool->setEndPoint(newPos);
        }

        event->accept();
        // Don't propagate - we're handling the drag
    } else {
        QGraphicsEllipseItem::mouseMoveEvent(event);
    }
}

void ArrowHandleTool::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && m_dragging) {
        m_dragging = false;
        event->accept();
    } else {
        QGraphicsEllipseItem::mouseReleaseEvent(event);
    }
}

void ArrowHandleTool::hoverEnterEvent(QGraphicsSceneHoverEvent *event)
{
    m_hovered = true;

    // Grow handle on hover for better visibility
    setRect(-HANDLE_HOVER_SIZE/2, -HANDLE_HOVER_SIZE/2,
            HANDLE_HOVER_SIZE, HANDLE_HOVER_SIZE);

    setPen(QPen(Qt::blue, 2.5));

    QGraphicsEllipseItem::hoverEnterEvent(event);
    update();
}

void ArrowHandleTool::hoverLeaveEvent(QGraphicsSceneHoverEvent *event)
{
    m_hovered = false;

    // Return to normal size
    setRect(-HANDLE_SIZE/2, -HANDLE_SIZE/2, HANDLE_SIZE, HANDLE_SIZE);

    setPen(QPen(Qt::blue, 2));

    QGraphicsEllipseItem::hoverLeaveEvent(event);
    update();
}

void ArrowHandleTool::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
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

} // namespace Editor::Tools
