#ifndef ARROWITEM_H
#define ARROWITEM_H

#include <QGraphicsItemGroup>
#include <QGraphicsLineItem>
#include <QPen>
#include <QPointF>

class ArrowItem : public QGraphicsItemGroup
{
public:
    explicit ArrowItem(const QPointF &start, const QPointF &end, QGraphicsItem *parent = nullptr);

    void updateArrow(const QPointF &start, const QPointF &end);
    void setPen(const QPen &pen);

    QPointF startPoint() const { return m_startPoint; }
    QPointF endPoint() const { return m_endPoint; }

protected:
    void createArrowHead();

private:
    QPointF m_startPoint;
    QPointF m_endPoint;
    QPen m_pen;

    QGraphicsLineItem *m_mainLine;
    QGraphicsLineItem *m_arrowHead1;
    QGraphicsLineItem *m_arrowHead2;
};

#endif // ARROWITEM_H
