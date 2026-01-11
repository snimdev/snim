#ifndef IMAGEEDITOR_RECTANGLEDRAWINGINTERACTION_H
#define IMAGEEDITOR_RECTANGLEDRAWINGINTERACTION_H

#include "BaseDrawingInteraction.h"
#include <QGraphicsRectItem>

namespace Editor::Interactions {

/**
 * @brief Interaction for rectangle drawing tool
 *
 * Handles rectangle creation from start point to end point.
 * Shows a dashed preview while drawing.
 */
class RectangleDrawingInteraction : public BaseDrawingInteraction
{
    Q_OBJECT

public:
    explicit RectangleDrawingInteraction(QObject *parent = nullptr);

    // IDrawingInteraction interface
    Qt::CursorShape getCursor() const override { return Qt::CrossCursor; }

signals:
    /**
     * @brief Emitted when rectangle drawing is complete
     * @param rect The drawn rectangle
     */
    void rectangleDrawn(const QRect &rect);

protected:
    // BaseDrawingInteraction interface
    QGraphicsItem* createPreview(const QPointF &startPos) override;
    void updatePreview(const QPointF &currentPos) override;
    bool finalizeDrawing() override;

private:
    static constexpr int MIN_SIZE = 5; // Minimum width/height
};

} // namespace Editor::Interactions

#endif // IMAGEEDITOR_RECTANGLEDRAWINGINTERACTION_H
