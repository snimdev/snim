#include "PointerToolInteraction.h"
#include <QGraphicsScene>
#include <QTransform>

namespace Editor::Interactions {

PointerToolInteraction::PointerToolInteraction(QObject *parent)
    : QObject(parent)
{
}

bool PointerToolInteraction::onMousePress(const QPointF &scenePos, QGraphicsScene *scene)
{
    if (!scene) return false;

    // Find item at click position
    QGraphicsItem *item = scene->itemAt(scenePos, QTransform());
    if (item) {
        // The top-level item, not a child such as a handle
        emit itemClicked(item->topLevelItem());
    }

    // Always return false to let Qt's default selection/dragging behavior work
    // The pointer tool should not intercept mouse events, just observe them
    return false;
}

bool PointerToolInteraction::onMouseMove(const QPointF &scenePos, QGraphicsScene *scene)
{
    Q_UNUSED(scenePos)
    Q_UNUSED(scene)
    // Pointer tool doesn't handle move events (Qt handles dragging automatically)
    return false;
}

bool PointerToolInteraction::onMouseRelease(const QPointF &scenePos, QGraphicsScene *scene)
{
    Q_UNUSED(scenePos)
    Q_UNUSED(scene)
    // Pointer tool doesn't need release events
    return false;
}

void PointerToolInteraction::cleanup(QGraphicsScene *scene)
{
    Q_UNUSED(scene)
    // Pointer tool has nothing to clean up
}

} // namespace Editor::Interactions
