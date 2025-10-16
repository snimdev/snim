#ifndef IMAGEEDITOR_BLURDRAWINGINTERACTION_H
#define IMAGEEDITOR_BLURDRAWINGINTERACTION_H

#include "BaseDrawingInteraction.h"
#include <QList>
#include <QPointF>
#include <QPixmap>

namespace ImageEditor {
    namespace Tools {
        class BlurTool;
    }
}

namespace ImageEditor::Interactions {

/**
 * @brief Interaction for blur tool
 *
 * Handles blur painting by adding points as the mouse moves, similar to freehand.
 * Applies blur effect to the underlying image along the painted path.
 */
class BlurDrawingInteraction : public BaseDrawingInteraction
{
    Q_OBJECT

public:
    explicit BlurDrawingInteraction(QObject *parent = nullptr);

    // IDrawingInteraction interface
    Qt::CursorShape getCursor() const override { return Qt::CrossCursor; }

    // Configure the blur parameters
    void setBlurRadius(qreal radius) { m_blurRadius = radius; }
    qreal blurRadius() const { return m_blurRadius; }

    void setBrushWidth(qreal width) { m_brushWidth = width; }
    qreal brushWidth() const { return m_brushWidth; }

    // Set the source image for blur effect
    void setSourcePixmap(const QPixmap &pixmap) { m_sourcePixmap = pixmap; }

signals:
    /**
     * @brief Emitted when blur drawing is complete
     * @param points The list of points that make up the blur path
     */
    void blurDrawn(const QList<QPointF> &points);

protected:
    // BaseDrawingInteraction interface
    QGraphicsItem* createPreview(const QPointF &startPos) override;
    void updatePreview(const QPointF &currentPos) override;
    bool finalizeDrawing() override;

private:
    qreal m_blurRadius;
    qreal m_brushWidth;
    QPixmap m_sourcePixmap;
};

} // namespace ImageEditor::Interactions

#endif // IMAGEEDITOR_BLURDRAWINGINTERACTION_H
