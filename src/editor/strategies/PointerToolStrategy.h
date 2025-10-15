#ifndef IMAGEEDITOR_POINTERTOOLSTRATEGY_H
#define IMAGEEDITOR_POINTERTOOLSTRATEGY_H

#include "IDrawingToolStrategy.h"
#include <QObject>

namespace ImageEditor::Strategies {

/**
 * @brief Strategy for pointer/selection tool
 *
 * The pointer tool is used for selecting and interacting with existing items.
 * It doesn't create new items, so it implements IDrawingToolStrategy directly.
 */
class PointerToolStrategy : public QObject, public IDrawingToolStrategy
{
    Q_OBJECT

public:
    explicit PointerToolStrategy(QObject *parent = nullptr);

    // IDrawingToolStrategy interface
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

} // namespace ImageEditor::Strategies

#endif // IMAGEEDITOR_POINTERTOOLSTRATEGY_H
