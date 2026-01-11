#include "StepDrawingInteraction.h"

namespace Editor::Interactions {

StepDrawingInteraction::StepDrawingInteraction(QObject *parent)
    : QObject(parent)
{
}

bool StepDrawingInteraction::onMousePress(const QPointF &scenePos, QGraphicsScene *scene)
{
    Q_UNUSED(scene)
    // Emit signal to request a badge at the click point
    emit stepRequested(scenePos);
    return true;
}

bool StepDrawingInteraction::onMouseMove(const QPointF &scenePos, QGraphicsScene *scene)
{
    Q_UNUSED(scenePos)
    Q_UNUSED(scene)
    // Step tool doesn't need move events
    return false;
}

bool StepDrawingInteraction::onMouseRelease(const QPointF &scenePos, QGraphicsScene *scene)
{
    Q_UNUSED(scenePos)
    Q_UNUSED(scene)
    // Step tool doesn't need release events
    return false;
}

void StepDrawingInteraction::cleanup(QGraphicsScene *scene)
{
    Q_UNUSED(scene)
    // Step tool has no preview to clean up
}

} // namespace Editor::Interactions
