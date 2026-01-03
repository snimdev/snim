#ifndef IMAGEEDITOR_SHAPETOOL_H
#define IMAGEEDITOR_SHAPETOOL_H

#include "ITool.h"
#include <QGraphicsObject>
#include <QPen>
#include <QBrush>
#include <QRectF>

namespace Editor::Tools {

class ShapeHandleTool;

class ShapeTool : public QGraphicsObject, public ITool
{
    Q_OBJECT

public:
    enum HandlePosition {
        TopLeft,
        TopCenter,
        TopRight,
        MiddleLeft,
        MiddleRight,
        BottomLeft,
        BottomCenter,
        BottomRight
    };

    explicit ShapeTool(const QRectF &rect, QGraphicsItem *parent = nullptr);
    ~ShapeTool() override = default;

    // QGraphicsItem interface
    QRectF boundingRect() const override;
    QPainterPath shape() const override;

    // ITool interface for dynamic properties
    [[nodiscard]] QList<ToolProperty> getProperties() const override;
    void setProperty(const QString& propertyId, const QVariant& value) override;
    void applyStyleFrom(const ITool* other) override;   // copies pen/brush/opacity

    // Shape manipulation
    void setShapeRect(const QRectF &rect);
    [[nodiscard]] QRectF shapeRect() const { return m_shapeRect; }

    // Styling
    void setPen(const QPen &pen);
    [[nodiscard]] QPen pen() const { return m_pen; }

    void setBrush(const QBrush &brush);
    [[nodiscard]] QBrush brush() const { return m_brush; }

    void setOpacity(qreal opacity);
    [[nodiscard]] qreal getOpacity() const { return m_opacity; }

    // Handle management
    void updateHandlePosition(HandlePosition position, const QPointF &scenePos);

signals:
    void shapeChanged();

protected:
    QVariant itemChange(GraphicsItemChange change, const QVariant &value) override;
    void updateGeometry();
    void updateHandles();

    // Virtual method for derived classes to implement specific shape painting
    virtual void paintShape(QPainter *painter) = 0;

    // Virtual method for derived classes to provide shape-specific interaction path
    virtual QPainterPath createShapePath() const = 0;

    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget = nullptr) override;

    QRectF m_shapeRect;
    QPen m_pen;
    QBrush m_brush;
    qreal m_opacity;

private:
    void createHandles();
    QPointF getHandlePosition(HandlePosition position) const;

    // Interactive handles (8 resize handles)
    ShapeHandleTool *m_handles[8];
    QRectF m_boundingRect;
};

} // namespace Editor::Tools

#endif // IMAGEEDITOR_SHAPETOOL_H
