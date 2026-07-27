#ifndef IMAGEEDITOR_POINTERTOOLINTERACTION_H
#define IMAGEEDITOR_POINTERTOOLINTERACTION_H

#include "IDrawingInteraction.h"
#include <QObject>

namespace Editor::Interactions {

/**
 * @brief Interaction for pointer/selection tool
 *
 * The pointer tool is used for selecting and interacting with existing items.
 * It doesn't create new items, so it implements IDrawingInteraction directly.
 */
class PointerToolInteraction : public QObject, public IDrawingInteraction
{
    Q_OBJECT

public:
    explicit PointerToolInteraction(QObject *parent = nullptr);

    // IDrawingInteraction interface
    bool onMousePress(const QPointF &scenePos, QGraphicsScene *scene) override;
    bool onMouseMove(const QPointF &scenePos, QGraphicsScene *scene) override;
    bool onMouseRelease(const QPointF &scenePos, QGraphicsScene *scene) override;
    Qt::CursorShape getCursor() const override { return Qt::ArrowCursor; }
    void cleanup(QGraphicsScene *scene) override;
    bool isDrawing() const override { return false; }

signals:
    /**
     * @brief Emitted when an item is clicked
     * @param item The clicked graphics item
     */
    void itemClicked(QGraphicsItem *item);
};

} // namespace Editor::Interactions

#endif // IMAGEEDITOR_POINTERTOOLINTERACTION_H
