#ifndef IMAGEEDITOR_FREEHANDTOOL_H
#define IMAGEEDITOR_FREEHANDTOOL_H

#include "ITool.h"
#include <QGraphicsObject>
#include <QPen>
#include <QPainterPath>
#include <QList>
#include <QPointF>

namespace Editor::Tools {

class FreehandTool : public QGraphicsObject, public ITool
{
    Q_OBJECT

public:
    explicit FreehandTool(QGraphicsItem *parent = nullptr);
    ~FreehandTool() override = default;

    // QGraphicsItem interface
    QRectF boundingRect() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget = nullptr) override;
    QPainterPath shape() const override;

    // ITool interface for dynamic properties
    [[nodiscard]] QList<ToolProperty> getProperties() const override;
    void setProperty(const QString& propertyId, const QVariant& value) override;
    [[nodiscard]] QGraphicsItem* clone() const override;
    void applyStyleFrom(const ITool* other) override;   // copies pen

    // Path manipulation
    void addPoint(const QPointF &point);
    void finishPath();
    [[nodiscard]] bool isEmpty() const { return m_points.isEmpty(); }
    [[nodiscard]] QList<QPointF> points() const { return m_points; }

    // Styling
    void setPen(const QPen &pen);
    [[nodiscard]] QPen pen() const { return m_pen; }

signals:
    void pathChanged();

protected:
    QVariant itemChange(GraphicsItemChange change, const QVariant &value) override;

private:
    void updateGeometry();
    QPainterPath createStrokePath() const;

    QList<QPointF> m_points;
    QPainterPath m_path;
    QPainterPath m_strokePath;
    QPen m_pen;
    QRectF m_boundingRect;
};

} // namespace Editor::Tools

#endif // IMAGEEDITOR_FREEHANDTOOL_H
