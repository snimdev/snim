#ifndef IMAGEEDITOR_ARROWTOOL_H
#define IMAGEEDITOR_ARROWTOOL_H

#include "ITool.h"
#include <QGraphicsItemGroup>
#include <QGraphicsLineItem>
#include <QGraphicsRectItem>
#include <QGraphicsPolygonItem>
#include <QPen>
#include <QPointF>

namespace ImageEditor::Tools {

class ArrowTool : public QGraphicsItemGroup, public ITool
{
public:
    enum ArrowHeadType {
        Outlined,  // Only sides filled (current default)
        Filled     // Completely filled
    };

    explicit ArrowTool(const QPointF &start, const QPointF &end, QGraphicsItem *parent = nullptr);

    void updateArrow(const QPointF &start, const QPointF &end);
    void setPen(const QPen &pen);
    void setArrowHeadType(ArrowHeadType type);

    [[nodiscard]] QList<ToolProperty> getProperties() const override;
    void setProperty(const QString& propertyId, const QVariant& value) override;

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

#endif // IMAGEEDITOR_ARROWTOOL_H
