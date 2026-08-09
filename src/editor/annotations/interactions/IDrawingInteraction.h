#ifndef IMAGEEDITOR_IDRAWINGINTERACTION_H
#define IMAGEEDITOR_IDRAWINGINTERACTION_H

#include <QPointF>
#include <QtCore/qnamespace.h>

class QGraphicsScene;

namespace Editor::Interactions {

// Strategy: how a tool turns mouse input into an annotation. The editor view and the
// capture overlay forward scene positions to the active tool's interaction; each hook
// returns true when it consumed the event.
class IDrawingInteraction
{
public:
    virtual ~IDrawingInteraction() = default;

    virtual bool onMousePress(const QPointF &scenePos, QGraphicsScene *scene) = 0;
    virtual bool onMouseMove(const QPointF &, QGraphicsScene *) { return false; }
    virtual bool onMouseRelease(const QPointF &, QGraphicsScene *) { return false; }
    // Drops an unfinished preview, e.g. on a tool switch.
    virtual void cleanup(QGraphicsScene *) {}
    [[nodiscard]] virtual bool isDrawing() const { return false; }

    [[nodiscard]] virtual Qt::CursorShape getCursor() const = 0;
    [[nodiscard]] virtual Qt::CursorShape getDrawingCursor() const { return getCursor(); }
};

} // namespace Editor::Interactions

#endif // IMAGEEDITOR_IDRAWINGINTERACTION_H
