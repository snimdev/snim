#ifndef IMAGEEDITOR_ELLIPSEDRAWINGSTRATEGY_H
#define IMAGEEDITOR_ELLIPSEDRAWINGSTRATEGY_H

#include "BaseDrawingStrategy.h"
#include <QGraphicsEllipseItem>

namespace ImageEditor::Strategies {

/**
 * @brief Strategy for ellipse drawing tool
 *
 * Handles ellipse creation from start point to end point.
 * Shows a dashed preview while drawing.
 */
class EllipseDrawingStrategy : public BaseDrawingStrategy
{
    Q_OBJECT

public:
    explicit EllipseDrawingStrategy(QObject *parent = nullptr);

    // IDrawingToolStrategy interface
    Qt::CursorShape getCursor() const override { return Qt::CrossCursor; }

signals:
    /**
     * @brief Emitted when ellipse drawing is complete
     * @param rect The bounding rectangle of the ellipse
     */
    void ellipseDrawn(const QRect &rect);

protected:
    // BaseDrawingStrategy interface
    QGraphicsItem* createPreview(const QPointF &startPos) override;
    void updatePreview(const QPointF &currentPos) override;
    bool finalizeDrawing() override;

private:
    static constexpr int MIN_SIZE = 5; // Minimum width/height
};

} // namespace ImageEditor::Strategies

#endif // IMAGEEDITOR_ELLIPSEDRAWINGSTRATEGY_H
