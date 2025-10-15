#include "PointerToolStrategy.h"
#include <QGraphicsScene>
#include <QTransform>

namespace ImageEditor::Strategies {

PointerToolStrategy::PointerToolStrategy(QObject *parent)
    : QObject(parent)
{
}

bool PointerToolStrategy::onMousePress(const QPointF &scenePos, QGraphicsScene *scene)
{
    if (!scene) return false;

    // Find item at click position
    QGraphicsItem *item = scene->itemAt(scenePos, QTransform());
    if (item) {
        // Get the top-level item (not child items of a group)
        item = getTopLevelItem(item);
        emit itemClicked(item);
    }

    // Always return false to let Qt's default selection/dragging behavior work
    // The pointer tool should not intercept mouse events, just observe them
    return false;
}

bool PointerToolStrategy::onMouseMove(const QPointF &scenePos, QGraphicsScene *scene)
{
    Q_UNUSED(scenePos)
    Q_UNUSED(scene)
    // Pointer tool doesn't handle move events (Qt handles dragging automatically)
    return false;
}

bool PointerToolStrategy::onMouseRelease(const QPointF &scenePos, QGraphicsScene *scene)
{
    Q_UNUSED(scenePos)
    Q_UNUSED(scene)
    // Pointer tool doesn't need release events
    return false;
}

void PointerToolStrategy::cleanup(QGraphicsScene *scene)
{
    Q_UNUSED(scene)
    // Pointer tool has nothing to clean up
}

QGraphicsItem* PointerToolStrategy::getTopLevelItem(QGraphicsItem *item) const
{
    if (!item) return nullptr;

    // Traverse up to find the top-level parent
    while (item->parentItem() != nullptr) {
        item = item->parentItem();
    }

    return item;
}

} // namespace ImageEditor::Strategies
