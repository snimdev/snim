#include "TextDrawingInteraction.h"

namespace ImageEditor::Interactions {

TextDrawingInteraction::TextDrawingInteraction(QObject *parent)
    : QObject(parent)
{
}

bool TextDrawingInteraction::onMousePress(const QPointF &scenePos, QGraphicsScene *scene)
{
    Q_UNUSED(scene)
    // Emit signal to request text input from user
    emit textRequested(scenePos.toPoint());
    return true;
}

bool TextDrawingInteraction::onMouseMove(const QPointF &scenePos, QGraphicsScene *scene)
{
    Q_UNUSED(scenePos)
    Q_UNUSED(scene)
    // Text tool doesn't need move events
    return false;
}

bool TextDrawingInteraction::onMouseRelease(const QPointF &scenePos, QGraphicsScene *scene)
{
    Q_UNUSED(scenePos)
    Q_UNUSED(scene)
    // Text tool doesn't need release events
    return false;
}

void TextDrawingInteraction::cleanup(QGraphicsScene *scene)
{
    Q_UNUSED(scene)
    // Text tool has no preview to clean up
}

} // namespace ImageEditor::Interactions
