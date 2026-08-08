#ifndef IMAGEEDITOR_ARROWTOOL_H
#define IMAGEEDITOR_ARROWTOOL_H

#include "ITool.h"
#include <QGraphicsObject>
#include <QPen>
#include <QPointF>

namespace Editor::Tools {

class HandleItem;

class ArrowTool : public QGraphicsObject, public ITool
{
    Q_OBJECT

public:
    enum ArrowHeadType {
        Outlined,  // Only sides (two lines)
        Filled     // Completely filled triangle
    };

    explicit ArrowTool(const QPointF &start, const QPointF &end, QGraphicsItem *parent = nullptr);
    ~ArrowTool() override = default;

    // QGraphicsItem interface
    QRectF boundingRect() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget = nullptr) override;
    QPainterPath shape() const override;

    // ITool interface for dynamic properties
    [[nodiscard]] QList<ToolProperty> getProperties() const override;
    void setProperty(const QString& propertyId, const QVariant& value) override;
    [[nodiscard]] QGraphicsItem* clone() const override;
    void applyStyleFrom(const ITool* other) override;   // copies pen + head type

    // Arrow manipulation
    void setStartPoint(const QPointF &point);
    void setEndPoint(const QPointF &point);

    [[nodiscard]] QPointF startPoint() const { return m_startPoint; }
    [[nodiscard]] QPointF endPoint() const { return m_endPoint; }

    // Styling
    void setPen(const QPen &pen);
    [[nodiscard]] QPen pen() const { return m_pen; }

    void setArrowHeadType(ArrowHeadType type);
    [[nodiscard]] ArrowHeadType arrowHeadType() const { return m_arrowHeadType; }

protected:
    QVariant itemChange(GraphicsItemChange change, const QVariant &value) override;

private:
    void updateHandles();
    void updateGeometry();
    QPainterPath createArrowPath() const;
    QPainterPath createArrowHeadPath() const;
    QPainterPath createStrokePath() const;

    QPointF m_startPoint;
    QPointF m_endPoint;
    QPen m_pen;
    ArrowHeadType m_arrowHeadType;

    // Cached geometry
    QPainterPath m_arrowPath;
    QPainterPath m_arrowHeadPath;
    QPainterPath m_strokePath;
    QRectF m_boundingRect;

    HandleItem *m_startHandle = nullptr;
    HandleItem *m_endHandle = nullptr;
};

} // namespace Editor::Tools

#endif // IMAGEEDITOR_ARROWTOOL_H
