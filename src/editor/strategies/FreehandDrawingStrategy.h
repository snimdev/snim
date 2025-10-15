#ifndef IMAGEEDITOR_FREEHANDDRAWINGSTRATEGY_H
#define IMAGEEDITOR_FREEHANDDRAWINGSTRATEGY_H

#include "BaseDrawingStrategy.h"
#include <QPen>
#include <QList>
#include <QPointF>

namespace ImageEditor {
    namespace Tools {
        class FreehandTool;
    }
}

namespace ImageEditor::Strategies {

/**
 * @brief Strategy for freehand drawing tool
 *
 * Handles continuous path drawing by adding points as the mouse moves.
 * Unlike other tools, freehand doesn't recreate the preview on each move,
 * but incrementally adds points to the same path.
 */
class FreehandDrawingStrategy : public BaseDrawingStrategy
{
    Q_OBJECT

public:
    explicit FreehandDrawingStrategy(QObject *parent = nullptr);

    // IDrawingToolStrategy interface
    Qt::CursorShape getCursor() const override { return Qt::CrossCursor; }

    // Configure the pen for drawing
    void setPen(const QPen &pen) { m_pen = pen; }
    QPen pen() const { return m_pen; }

signals:
    /**
     * @brief Emitted when freehand drawing is complete
     * @param points The list of points that make up the path
     */
    void freehandDrawn(const QList<QPointF> &points);

protected:
    // BaseDrawingStrategy interface
    QGraphicsItem* createPreview(const QPointF &startPos) override;
    void updatePreview(const QPointF &currentPos) override;
    bool finalizeDrawing() override;

private:
    QPen m_pen;
};

} // namespace ImageEditor::Strategies

#endif // IMAGEEDITOR_FREEHANDDRAWINGSTRATEGY_H
