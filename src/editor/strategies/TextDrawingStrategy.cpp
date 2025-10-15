#include "TextDrawingStrategy.h"

namespace ImageEditor::Strategies {

TextDrawingStrategy::TextDrawingStrategy(QObject *parent)
    : QObject(parent)
{
}

bool TextDrawingStrategy::onMousePress(const QPointF &scenePos, QGraphicsScene *scene)
{
    Q_UNUSED(scene)
    // Emit signal to request text input from user
    emit textRequested(scenePos.toPoint());
    return true;
}

bool TextDrawingStrategy::onMouseMove(const QPointF &scenePos, QGraphicsScene *scene)
{
    Q_UNUSED(scenePos)
    Q_UNUSED(scene)
    // Text tool doesn't need move events
    return false;
}

bool TextDrawingStrategy::onMouseRelease(const QPointF &scenePos, QGraphicsScene *scene)
{
    Q_UNUSED(scenePos)
    Q_UNUSED(scene)
    // Text tool doesn't need release events
    return false;
}

void TextDrawingStrategy::cleanup(QGraphicsScene *scene)
{
    Q_UNUSED(scene)
    // Text tool has no preview to clean up
}

} // namespace ImageEditor::Strategies
