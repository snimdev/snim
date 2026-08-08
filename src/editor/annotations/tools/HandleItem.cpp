#include "HandleItem.h"
#include <QCursor>
#include <QGraphicsSceneMouseEvent>
#include <QPainter>

namespace Editor::Tools {

namespace {
constexpr qreal kSize = 8.0;
constexpr qreal kHoverSize = 10.0;
}

HandleItem::HandleItem(Qt::CursorShape cursor, DragCallback onDrag, QGraphicsItem *parent)
    : QGraphicsEllipseItem(parent)
    , m_onDrag(std::move(onDrag))
{
    setPen(QPen(Qt::blue, 2));
    setBrush(QBrush(Qt::white));
    // Not movable: a drag moves the owner's geometry, not the handle.
    setFlags(QGraphicsItem::ItemSendsGeometryChanges);
    setAcceptHoverEvents(true);
    setCursor(cursor);
    setZValue(1000);
    setRect(-kSize / 2, -kSize / 2, kSize, kSize);
    setVisible(false);
}

void HandleItem::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_dragging = true;
        event->accept();
    } else {
        QGraphicsEllipseItem::mousePressEvent(event);
    }
}

void HandleItem::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    if (m_dragging && m_onDrag) {
        m_onDrag(event->scenePos());
        event->accept();
    } else {
        QGraphicsEllipseItem::mouseMoveEvent(event);
    }
}

void HandleItem::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && m_dragging) {
        m_dragging = false;
        event->accept();
    } else {
        QGraphicsEllipseItem::mouseReleaseEvent(event);
    }
}

void HandleItem::setHovered(bool hovered)
{
    m_hovered = hovered;
    const qreal size = hovered ? kHoverSize : kSize;
    setRect(-size / 2, -size / 2, size, size);
    setPen(QPen(Qt::blue, hovered ? 2.5 : 2));
}

void HandleItem::hoverEnterEvent(QGraphicsSceneHoverEvent *event)
{
    setHovered(true);
    QGraphicsEllipseItem::hoverEnterEvent(event);
    update();
}

void HandleItem::hoverLeaveEvent(QGraphicsSceneHoverEvent *event)
{
    setHovered(false);
    QGraphicsEllipseItem::hoverLeaveEvent(event);
    update();
}

void HandleItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *)
{
    painter->setRenderHint(QPainter::Antialiasing);
    painter->setPen(pen());
    painter->setBrush(m_hovered ? QBrush(QColor(200, 220, 255)) : QBrush(Qt::white));
    painter->drawEllipse(rect());

    // Center dot, for visibility over busy backgrounds.
    painter->setPen(Qt::NoPen);
    painter->setBrush(QBrush(Qt::blue));
    painter->drawEllipse(QRectF(-1.0, -1.0, 2.0, 2.0));
}

} // namespace Editor::Tools
