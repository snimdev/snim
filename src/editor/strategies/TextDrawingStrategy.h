#ifndef IMAGEEDITOR_TEXTDRAWINGSTRATEGY_H
#define IMAGEEDITOR_TEXTDRAWINGSTRATEGY_H

#include "IDrawingToolStrategy.h"
#include <QObject>

namespace ImageEditor::Strategies {

/**
 * @brief Strategy for text tool
 *
 * Text tool doesn't follow the typical drawing pattern - it places text on click.
 * Therefore, it implements IDrawingToolStrategy directly rather than extending BaseDrawingStrategy.
 */
class TextDrawingStrategy : public QObject, public IDrawingToolStrategy
{
    Q_OBJECT

public:
    explicit TextDrawingStrategy(QObject *parent = nullptr);

    // IDrawingToolStrategy interface
    bool onMousePress(const QPointF &scenePos, QGraphicsScene *scene) override;
    bool onMouseMove(const QPointF &scenePos, QGraphicsScene *scene) override;
    bool onMouseRelease(const QPointF &scenePos, QGraphicsScene *scene) override;
    Qt::CursorShape getCursor() const override { return Qt::IBeamCursor; }
    void cleanup(QGraphicsScene *scene) override;
    bool isDrawing() const override { return false; }

signals:
    /**
     * @brief Emitted when user clicks to place text
     * @param position The position where text should be placed
     */
    void textRequested(const QPoint &position);
};

} // namespace ImageEditor::Strategies

#endif // IMAGEEDITOR_TEXTDRAWINGSTRATEGY_H
