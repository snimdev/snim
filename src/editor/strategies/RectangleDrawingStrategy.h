#ifndef IMAGEEDITOR_RECTANGLEDRAWINGSTRATEGY_H
#define IMAGEEDITOR_RECTANGLEDRAWINGSTRATEGY_H

#include "BaseDrawingStrategy.h"
#include <QGraphicsRectItem>

namespace ImageEditor::Strategies {

/**
 * @brief Strategy for rectangle drawing tool
 *
 * Handles rectangle creation from start point to end point.
 * Shows a dashed preview while drawing.
 */
class RectangleDrawingStrategy : public BaseDrawingStrategy
{
    Q_OBJECT

public:
    explicit RectangleDrawingStrategy(QObject *parent = nullptr);

    // IDrawingToolStrategy interface
    Qt::CursorShape getCursor() const override { return Qt::CrossCursor; }

signals:
    /**
     * @brief Emitted when rectangle drawing is complete
     * @param rect The drawn rectangle
     */
    void rectangleDrawn(const QRect &rect);

protected:
    // BaseDrawingStrategy interface
    QGraphicsItem* createPreview(const QPointF &startPos) override;
    void updatePreview(const QPointF &currentPos) override;
    bool finalizeDrawing() override;

private:
    static constexpr int MIN_SIZE = 5; // Minimum width/height
};

} // namespace ImageEditor::Strategies

#endif // IMAGEEDITOR_RECTANGLEDRAWINGSTRATEGY_H
