#ifndef IMAGEEDITOR_ARROWITEM_H
#define IMAGEEDITOR_ARROWITEM_H

#include <QGraphicsItemGroup>
#include <QGraphicsLineItem>
#include <QGraphicsRectItem>
#include <QGraphicsPolygonItem>
#include <QPen>
#include <QPointF>

namespace ImageEditor {

class ArrowItem : public QGraphicsItemGroup
{
public:
    enum ArrowHeadType {
        Outlined,  // Only sides filled (current default)
        Filled     // Completely filled
    };

    explicit ArrowItem(const QPointF &start, const QPointF &end, QGraphicsItem *parent = nullptr);

    void updateArrow(const QPointF &start, const QPointF &end);
    void setPen(const QPen &pen);
    void setArrowHeadType(ArrowHeadType type);

    [[nodiscard]] QPointF startPoint() const { return m_startPoint; }
    [[nodiscard]] QPointF endPoint() const { return m_endPoint; }
    [[nodiscard]] ArrowHeadType arrowHeadType() const { return m_arrowHeadType; }
    [[nodiscard]] QPen pen() const { return m_pen; }

protected:
    void createArrowHead();
    QVariant itemChange(GraphicsItemChange change, const QVariant &value) override;

private:
    void updateSelectionBorder();

    QPointF m_startPoint;
    QPointF m_endPoint;
    QPen m_pen;
    ArrowHeadType m_arrowHeadType;

    QGraphicsLineItem *m_mainLine;
    QGraphicsLineItem *m_arrowHead1;
    QGraphicsLineItem *m_arrowHead2;
    QGraphicsPolygonItem *m_filledArrowHead; // For filled arrow head
    QGraphicsRectItem *m_selectionBorder;
};

} // namespace ImageEditor

#endif // IMAGEEDITOR_ARROWITEM_H
