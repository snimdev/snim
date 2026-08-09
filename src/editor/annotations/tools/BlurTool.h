#ifndef IMAGEEDITOR_BLURTOOL_H
#define IMAGEEDITOR_BLURTOOL_H

#include "PathTool.h"
#include <QPixmap>
#include <QTimer>

namespace Editor::Tools {

// Blurs the screenshot under a painted stroke instead of drawing one.
class BlurTool : public PathTool
{
    Q_OBJECT

public:
    explicit BlurTool(QGraphicsItem *parent = nullptr);

    // ITool interface for dynamic properties
    [[nodiscard]] QList<ToolProperty> getProperties() const override;
    void setProperty(const QString& propertyId, const QVariant& value) override;
    [[nodiscard]] QGraphicsItem* clone() const override;
    void applyStyleFrom(const ITool* other) override;   // copies blur radius + brush width

    void addPoint(const QPointF &point) override;
    void finishPath() override;   // blurs the pixels under the stroke

    // Blur configuration
    void setBlurRadius(qreal radius);
    [[nodiscard]] qreal blurRadius() const { return m_blurRadius; }

    void setBrushWidth(qreal width);
    [[nodiscard]] qreal brushWidth() const { return m_brushWidth; }

    // Source image for blur effect
    void setSourcePixmap(const QPixmap &pixmap);

protected:
    QVariant itemChange(GraphicsItemChange change, const QVariant &value) override;
    [[nodiscard]] QPen strokePen() const override;
    [[nodiscard]] qreal boundsPadding() const override { return m_blurRadius + 10.0; }
    void paintPath(QPainter *painter) override;

private:
    void generateBlurredPixmap();
    void scheduleRegeneration();   // coalesced re-blur after the item is moved
    QImage applyBoxBlur(const QImage& source, int radius);

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
