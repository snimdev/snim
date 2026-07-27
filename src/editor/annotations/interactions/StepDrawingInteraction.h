#ifndef IMAGEEDITOR_STEPDRAWINGINTERACTION_H
#define IMAGEEDITOR_STEPDRAWINGINTERACTION_H

#include "IDrawingInteraction.h"
#include <QObject>

namespace Editor::Interactions {

/**
 * @brief Interaction for the step-numbers tool
 *
 * Like the text tool, this stamps on click rather than dragging a shape.
 * Therefore, it implements IDrawingInteraction directly rather than extending BaseDrawingInteraction.
 */
class StepDrawingInteraction : public QObject, public IDrawingInteraction
{
    Q_OBJECT

public:
    explicit StepDrawingInteraction(QObject *parent = nullptr);

    // IDrawingInteraction interface
    bool onMousePress(const QPointF &scenePos, QGraphicsScene *scene) override;
    bool onMouseMove(const QPointF &scenePos, QGraphicsScene *scene) override;
    bool onMouseRelease(const QPointF &scenePos, QGraphicsScene *scene) override;
    Qt::CursorShape getCursor() const override { return Qt::CrossCursor; }
    void cleanup(QGraphicsScene *scene) override;
    bool isDrawing() const override { return false; }

signals:
    /**
     * @brief Emitted when the user clicks to stamp a step badge
     * @param scenePos The badge centre
     */
    void stepRequested(const QPointF &scenePos);
};

} // namespace Editor::Interactions

#endif // IMAGEEDITOR_STEPDRAWINGINTERACTION_H
