#ifndef IMAGEEDITOR_POINTERTOOLINTERACTION_H
#define IMAGEEDITOR_POINTERTOOLINTERACTION_H

#include "IDrawingInteraction.h"
#include <QObject>

namespace ImageEditor::Interactions {

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

private:
    /**
     * @brief Find the top-level parent of an item
     * Used to select the main item rather than child items (e.g., handles)
     */
    QGraphicsItem* getTopLevelItem(QGraphicsItem *item) const;
};

} // namespace ImageEditor::Interactions

#endif // IMAGEEDITOR_POINTERTOOLINTERACTION_H
