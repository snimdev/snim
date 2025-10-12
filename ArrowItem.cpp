#include "ArrowItem.h"
#include <QtMath>
#include <cmath>

ArrowItem::ArrowItem(const QPointF &start, const QPointF &end, QGraphicsItem *parent)
    : QGraphicsItemGroup(parent)
    , m_startPoint(start)
    , m_endPoint(end)
    , m_pen(Qt::red, 3)
    , m_mainLine(nullptr)
    , m_arrowHead1(nullptr)
    , m_arrowHead2(nullptr)
{
    setFlag(QGraphicsItem::ItemIsSelectable, true);
    setFlag(QGraphicsItem::ItemIsMovable, true);

    // Create the main line
    m_mainLine = new QGraphicsLineItem(0, 0, 0, 0);
    m_mainLine->setPen(m_pen);
    addToGroup(m_mainLine);

    // Create arrowhead lines
    m_arrowHead1 = new QGraphicsLineItem(0, 0, 0, 0);
    m_arrowHead1->setPen(m_pen);
    addToGroup(m_arrowHead1);

    m_arrowHead2 = new QGraphicsLineItem(0, 0, 0, 0);
    m_arrowHead2->setPen(m_pen);
    addToGroup(m_arrowHead2);

    // Update the arrow with the provided points
    updateArrow(start, end);
}

void ArrowItem::updateArrow(const QPointF &start, const QPointF &end)
{
    m_startPoint = start;
    m_endPoint = end;

    // Update main line
    m_mainLine->setLine(start.x(), start.y(), end.x(), end.y());

    // Calculate and update arrowhead
    createArrowHead();
}

void ArrowItem::createArrowHead()
{
    // Calculate arrow properties
    double dx = m_endPoint.x() - m_startPoint.x();
    double dy = m_endPoint.y() - m_startPoint.y();
    double length = std::sqrt(dx*dx + dy*dy);

    if (length < 10) {
        // Too short for arrowhead
        m_arrowHead1->setLine(0, 0, 0, 0);
        m_arrowHead2->setLine(0, 0, 0, 0);
        return;
    }

    // Calculate arrowhead
    double angle = std::atan2(dy, dx);
    double arrowLength = 15;
    double arrowAngle = M_PI / 6; // 30 degrees

    QPointF arrowP1 = m_endPoint + QPointF(
        -arrowLength * std::cos(angle - arrowAngle),
        -arrowLength * std::sin(angle - arrowAngle)
    );
    QPointF arrowP2 = m_endPoint + QPointF(
        -arrowLength * std::cos(angle + arrowAngle),
        -arrowLength * std::sin(angle + arrowAngle)
    );

    // Update arrowhead lines
    m_arrowHead1->setLine(m_endPoint.x(), m_endPoint.y(), arrowP1.x(), arrowP1.y());
    m_arrowHead2->setLine(m_endPoint.x(), m_endPoint.y(), arrowP2.x(), arrowP2.y());
}

void ArrowItem::setPen(const QPen &pen)
{
    m_pen = pen;
    if (m_mainLine) m_mainLine->setPen(pen);
    if (m_arrowHead1) m_arrowHead1->setPen(pen);
    if (m_arrowHead2) m_arrowHead2->setPen(pen);
}
