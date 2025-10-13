#include "ArrowTool.h"
#include <QtMath>
#include <cmath>
#include <QGraphicsPolygonItem>

namespace ImageEditor {

ArrowTool::ArrowTool(const QPointF &start, const QPointF &end, QGraphicsItem *parent)
    : QGraphicsItemGroup(parent)
    , m_startPoint(start)
    , m_endPoint(end)
    , m_pen(Qt::red, 3)
    , m_arrowHeadType(Outlined)
    , m_mainLine(nullptr)
    , m_arrowHead1(nullptr)
    , m_arrowHead2(nullptr)
    , m_filledArrowHead(nullptr)
    , m_selectionBorder(nullptr)
{
    setFlag(QGraphicsItem::ItemIsSelectable, true);
    setFlag(QGraphicsItem::ItemIsMovable, true);
    setFlag(QGraphicsItem::ItemSendsGeometryChanges, true);

    // Create the main line
    m_mainLine = new QGraphicsLineItem(0, 0, 0, 0);
    m_mainLine->setPen(m_pen);
    addToGroup(m_mainLine);

    // Create arrowhead lines (for outlined arrow)
    m_arrowHead1 = new QGraphicsLineItem(0, 0, 0, 0);
    m_arrowHead1->setPen(m_pen);
    addToGroup(m_arrowHead1);

    m_arrowHead2 = new QGraphicsLineItem(0, 0, 0, 0);
    m_arrowHead2->setPen(m_pen);
    addToGroup(m_arrowHead2);

    // Create filled arrowhead (initially hidden)
    m_filledArrowHead = new QGraphicsPolygonItem();
    m_filledArrowHead->setPen(m_pen);
    m_filledArrowHead->setBrush(QBrush(m_pen.color()));
    m_filledArrowHead->setVisible(false);
    addToGroup(m_filledArrowHead);

    // Create selection border (initially hidden)
    m_selectionBorder = new QGraphicsRectItem();
    QPen borderPen(Qt::blue, 1, Qt::DashLine);
    m_selectionBorder->setPen(borderPen);
    m_selectionBorder->setBrush(Qt::NoBrush);
    m_selectionBorder->setVisible(false);
    addToGroup(m_selectionBorder);

    // Update the arrow with the provided points
    updateArrow(start, end);
}

void ArrowTool::updateArrow(const QPointF &start, const QPointF &end)
{
    m_startPoint = start;
    m_endPoint = end;

    // Update main line
    m_mainLine->setLine(start.x(), start.y(), end.x(), end.y());

    // Calculate and update arrowhead
    createArrowHead();

    // Update selection border if visible
    updateSelectionBorder();
}

void ArrowTool::createArrowHead()
{
    // Calculate arrow properties
    double dx = m_endPoint.x() - m_startPoint.x();
    double dy = m_endPoint.y() - m_startPoint.y();
    double length = std::sqrt(dx*dx + dy*dy);

    if (length < 10) {
        // Too short for arrowhead
        m_arrowHead1->setLine(0, 0, 0, 0);
        m_arrowHead2->setLine(0, 0, 0, 0);
        m_filledArrowHead->setPolygon(QPolygonF());
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

    if (m_arrowHeadType == Outlined) {
        // Show outlined arrowhead, hide filled
        m_arrowHead1->setLine(m_endPoint.x(), m_endPoint.y(), arrowP1.x(), arrowP1.y());
        m_arrowHead2->setLine(m_endPoint.x(), m_endPoint.y(), arrowP2.x(), arrowP2.y());
        m_arrowHead1->setVisible(true);
        m_arrowHead2->setVisible(true);
        m_filledArrowHead->setVisible(false);
    } else {
        // Show filled arrowhead, hide outlined
        QPolygonF triangle;
        triangle << m_endPoint << arrowP1 << arrowP2;
        m_filledArrowHead->setPolygon(triangle);
        m_filledArrowHead->setVisible(true);
        m_arrowHead1->setVisible(false);
        m_arrowHead2->setVisible(false);
    }
}

void ArrowTool::setPen(const QPen &pen)
{
    m_pen = pen;
    if (m_mainLine) m_mainLine->setPen(pen);
    if (m_arrowHead1) m_arrowHead1->setPen(pen);
    if (m_arrowHead2) m_arrowHead2->setPen(pen);
    if (m_filledArrowHead) {
        m_filledArrowHead->setPen(pen);
        m_filledArrowHead->setBrush(QBrush(pen.color()));
    }
}

void ArrowTool::setArrowHeadType(ArrowHeadType type)
{
    m_arrowHeadType = type;
    createArrowHead();
}

QVariant ArrowTool::itemChange(GraphicsItemChange change, const QVariant &value)
{
    if (change == ItemSelectedChange) {
        updateSelectionBorder();
    }
    return QGraphicsItemGroup::itemChange(change, value);
}

void ArrowTool::updateSelectionBorder()
{
    if (!m_selectionBorder) return;

    if (isSelected()) {
        // Calculate bounding rectangle for the arrow
        QRectF arrowBounds = boundingRect();

        // Add some padding around the arrow
        qreal padding = 5.0;
        arrowBounds.adjust(-padding, -padding, padding, padding);

        // Update the selection border rectangle
        m_selectionBorder->setRect(arrowBounds);
        m_selectionBorder->setVisible(true);
    } else {
        m_selectionBorder->setVisible(false);
    }
}

} // namespace ImageEditor
