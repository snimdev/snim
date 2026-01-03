#ifndef IMAGEEDITOR_TEXTDRAWINGINTERACTION_H
#define IMAGEEDITOR_TEXTDRAWINGINTERACTION_H

#include "IDrawingInteraction.h"
#include <QObject>

namespace Editor::Interactions {

/**
 * @brief Interaction for text tool
 *
 * Text tool doesn't follow the typical drawing pattern - it places text on click.
 * Therefore, it implements IDrawingInteraction directly rather than extending BaseDrawingInteraction.
 */
class TextDrawingInteraction : public QObject, public IDrawingInteraction
{
    Q_OBJECT

public:
    explicit TextDrawingInteraction(QObject *parent = nullptr);

    // IDrawingInteraction interface
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

} // namespace Editor::Interactions

#endif // IMAGEEDITOR_TEXTDRAWINGINTERACTION_H
