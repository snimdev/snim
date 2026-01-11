#ifndef IMAGEEDITOR_FREEHANDDRAWINGINTERACTION_H
#define IMAGEEDITOR_FREEHANDDRAWINGINTERACTION_H

#include "BaseDrawingInteraction.h"
#include <QPen>
#include <QList>
#include <QPointF>

namespace Editor::Tools { class FreehandTool; }

namespace Editor::Interactions {

/**
 * @brief Interaction for freehand drawing tool
 *
 * Handles continuous path drawing by adding points as the mouse moves.
 * Unlike other tools, freehand doesn't recreate the preview on each move,
 * but incrementally adds points to the same path.
 */
class FreehandDrawingInteraction : public BaseDrawingInteraction
{
    Q_OBJECT

public:
    explicit FreehandDrawingInteraction(QObject *parent = nullptr);

    // IDrawingInteraction interface
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
    // BaseDrawingInteraction interface
    QGraphicsItem* createPreview(const QPointF &startPos) override;
    void updatePreview(const QPointF &currentPos) override;
    bool finalizeDrawing() override;

private:
    QPen m_pen;
};

} // namespace Editor::Interactions

#endif // IMAGEEDITOR_FREEHANDDRAWINGINTERACTION_H
