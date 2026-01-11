#ifndef IMAGEEDITOR_HIGHLIGHTTOOL_H
#define IMAGEEDITOR_HIGHLIGHTTOOL_H

#include "ITool.h"
#include <QGraphicsObject>
#include <QPen>
#include <QPainterPath>
#include <QList>
#include <QPointF>

namespace Editor::Tools {


class HighlightTool : public QGraphicsObject, public ITool
{
    Q_OBJECT

public:
    explicit HighlightTool(QGraphicsItem *parent = nullptr);
    ~HighlightTool() override = default;

    // QGraphicsItem interface
    QRectF boundingRect() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget = nullptr) override;
    QPainterPath shape() const override;

    // ITool interface for dynamic properties
    [[nodiscard]] QList<ToolProperty> getProperties() const override;
    void setProperty(const QString& propertyId, const QVariant& value) override;
    [[nodiscard]] QGraphicsItem* clone() const override;
    void applyStyleFrom(const ITool* other) override;   // copies color + width

    // Path manipulation
    void addPoint(const QPointF &point);
    void finishPath();
    [[nodiscard]] bool isEmpty() const { return m_points.isEmpty(); }
    [[nodiscard]] QList<QPointF> points() const { return m_points; }

    // Styling
    void setColor(const QColor &color);
    [[nodiscard]] QColor color() const;
    void setWidth(qreal width);
    [[nodiscard]] qreal width() const { return m_width; }
    [[nodiscard]] QPen pen() const { return m_pen; }

    static constexpr qreal HIGHLIGHT_WIDTH_SMALL = 15.0;
    static constexpr qreal HIGHLIGHT_WIDTH_MEDIUM = 25.0;
    static constexpr qreal HIGHLIGHT_WIDTH_LARGE = 35.0;
    static constexpr qreal HIGHLIGHT_OPACITY = 0.3;

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
    qreal m_width;
    QRectF m_boundingRect;
};

} // namespace Editor::Tools

#endif // IMAGEEDITOR_HIGHLIGHTTOOL_H
