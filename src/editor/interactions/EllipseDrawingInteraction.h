#ifndef IMAGEEDITOR_ELLIPSEDRAWINGINTERACTION_H
#define IMAGEEDITOR_ELLIPSEDRAWINGINTERACTION_H

#include "BaseDrawingInteraction.h"
#include <QGraphicsEllipseItem>

namespace Editor::Interactions {

/**
 * @brief Interaction for ellipse drawing tool
 *
 * Handles ellipse creation from start point to end point.
 * Shows a dashed preview while drawing.
 */
class EllipseDrawingInteraction : public BaseDrawingInteraction
{
    Q_OBJECT

public:
    explicit EllipseDrawingInteraction(QObject *parent = nullptr);

    // IDrawingInteraction interface
    Qt::CursorShape getCursor() const override { return Qt::CrossCursor; }

signals:
    /**
     * @brief Emitted when ellipse drawing is complete
     * @param rect The bounding rectangle of the ellipse
     */
    void ellipseDrawn(const QRect &rect);

protected:
    // BaseDrawingInteraction interface
    QGraphicsItem* createPreview(const QPointF &startPos) override;
    void updatePreview(const QPointF &currentPos) override;
    bool finalizeDrawing() override;

private:
    static constexpr int MIN_SIZE = 5; // Minimum width/height
};

} // namespace Editor::Interactions

#endif // IMAGEEDITOR_ELLIPSEDRAWINGINTERACTION_H
