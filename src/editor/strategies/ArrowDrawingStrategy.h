#ifndef IMAGEEDITOR_ARROWDRAWINGSTRATEGY_H
#define IMAGEEDITOR_ARROWDRAWINGSTRATEGY_H

#include "BaseDrawingStrategy.h"
#include <QPen>

namespace ImageEditor {
    namespace Tools {
        class ArrowTool;
    }
}

namespace ImageEditor::Strategies {

/**
 * @brief Strategy for arrow drawing tool
 *
 * Handles arrow creation from start point to end point.
 * The preview is recreated on each mouse move to show the updated arrow.
 */
class ArrowDrawingStrategy : public BaseDrawingStrategy
{
    Q_OBJECT

public:
    explicit ArrowDrawingStrategy(QObject *parent = nullptr);

    // IDrawingToolStrategy interface
    Qt::CursorShape getCursor() const override { return Qt::ArrowCursor; }
    Qt::CursorShape getDrawingCursor() const override { return Qt::CrossCursor; }

signals:
    /**
     * @brief Emitted when arrow drawing is complete
     * @param start The starting point of the arrow
     * @param end The ending point of the arrow
     */
    void arrowDrawn(const QPoint &start, const QPoint &end);

protected:
    // BaseDrawingStrategy interface
    QGraphicsItem* createPreview(const QPointF &startPos) override;
    void updatePreview(const QPointF &currentPos) override;
    bool finalizeDrawing() override;

private:
    static constexpr qreal MIN_ARROW_LENGTH = 10.0;
};

} // namespace ImageEditor::Strategies

#endif // IMAGEEDITOR_ARROWDRAWINGSTRATEGY_H
