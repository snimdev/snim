#ifndef IMAGEEDITOR_BLURTOOL_H
#define IMAGEEDITOR_BLURTOOL_H

#include "ITool.h"
#include <QGraphicsObject>
#include <QPainterPath>
#include <QList>
#include <QPointF>
#include <QPixmap>
#include <QTimer>

namespace Editor::Tools {

/**
 * @brief A tool for applying blur effects along a painted path
 *
 * Similar to FreehandTool, but applies blur to the underlying image
 * instead of drawing a stroke. The blur radius is configurable.
 */
class BlurTool : public QGraphicsObject, public ITool
{
    Q_OBJECT

public:
    explicit BlurTool(QGraphicsItem *parent = nullptr);
    ~BlurTool() override = default;

    // QGraphicsItem interface
    QRectF boundingRect() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget = nullptr) override;
    QPainterPath shape() const override;

    // ITool interface for dynamic properties
    [[nodiscard]] QList<ToolProperty> getProperties() const override;
    void setProperty(const QString& propertyId, const QVariant& value) override;
    [[nodiscard]] QGraphicsItem* clone() const override;
    void applyStyleFrom(const ITool* other) override;   // copies blur radius + brush width

    // Path manipulation
    void addPoint(const QPointF &point);
    void finishPath();
    [[nodiscard]] bool isEmpty() const { return m_points.isEmpty(); }
    [[nodiscard]] QList<QPointF> points() const { return m_points; }

    // Blur configuration
    void setBlurRadius(qreal radius);
    [[nodiscard]] qreal blurRadius() const { return m_blurRadius; }

    void setBrushWidth(qreal width);
    [[nodiscard]] qreal brushWidth() const { return m_brushWidth; }

    // Source image for blur effect
    void setSourcePixmap(const QPixmap &pixmap);

protected:
    QVariant itemChange(GraphicsItemChange change, const QVariant &value) override;

private:
    void updateGeometry();
    void generateBlurredPixmap();
    void scheduleRegeneration();   // coalesced re-blur after the item is moved
    QPainterPath createStrokePath() const;
    QImage applyBoxBlur(const QImage& source, int radius);

    QList<QPointF> m_points;
    QPainterPath m_path;
    QPainterPath m_strokePath;
    QRectF m_boundingRect;

    qreal m_blurRadius;     // Blur strength
    qreal m_brushWidth;     // Width of the blur brush

    QPixmap m_sourcePixmap;      // Original image
    QPixmap m_blurredPixmap;     // Blurred result to display (a patch, DPR-tagged)
    QPointF m_blurPatchOffset;   // item-local top-left where the patch is blitted
    bool m_needsUpdate;

    QTimer *m_regenTimer = nullptr;  // coalesces re-blurring while the item is dragged
};

} // namespace Editor::Tools

#endif // IMAGEEDITOR_BLURTOOL_H
