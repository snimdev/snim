#ifndef IMAGEEDITOR_TOOLITEM_H
#define IMAGEEDITOR_TOOLITEM_H

#include "ITool.h"
#include <QGraphicsObject>

class QStyleOptionGraphicsItem;

namespace Editor::Tools {

// Base of the drawn annotations: selectable, movable and focusable, with a move
// cursor on hover and a dashed outline while selected.
class ToolItem : public QGraphicsObject, public ITool
{
    Q_OBJECT

protected:
    explicit ToolItem(QGraphicsItem *parent);

    // Draws the dashed selection outline around rect when the item is selected.
    static void paintSelection(QPainter *painter, const QStyleOptionGraphicsItem *option,
                               const QRectF &rect);
};

} // namespace Editor::Tools

#endif // IMAGEEDITOR_TOOLITEM_H
