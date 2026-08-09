#include "ToolItem.h"
#include <QPainter>
#include <QStyleOptionGraphicsItem>

namespace Editor::Tools {

ToolItem::ToolItem(QGraphicsItem *parent)
    : QGraphicsObject(parent)
{
    setFlags(QGraphicsItem::ItemIsSelectable |
             QGraphicsItem::ItemIsMovable |
             QGraphicsItem::ItemSendsGeometryChanges |
             QGraphicsItem::ItemIsFocusable);
    setAcceptHoverEvents(true);
    setCursor(Qt::SizeAllCursor);
}

void ToolItem::paintSelection(QPainter *painter, const QStyleOptionGraphicsItem *option,
                              const QRectF &rect)
{
    if (!(option->state & QStyle::State_Selected))
        return;
    painter->setPen(QPen(Qt::blue, 1.0, Qt::DashLine));
    painter->setBrush(Qt::NoBrush);
    painter->drawRect(rect);
}

} // namespace Editor::Tools
