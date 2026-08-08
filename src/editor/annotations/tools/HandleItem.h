#ifndef IMAGEEDITOR_HANDLEITEM_H
#define IMAGEEDITOR_HANDLEITEM_H

#include <QGraphicsEllipseItem>
#include <QPointF>
#include <functional>

namespace Editor::Tools {

// A round grab handle on a selected tool, hidden until the owner shows it.
// Dragging it reports the scene position to the owner's callback.
class HandleItem : public QGraphicsEllipseItem
{
public:
    using DragCallback = std::function<void(const QPointF &scenePos)>;

    HandleItem(Qt::CursorShape cursor, DragCallback onDrag, QGraphicsItem *parent);

protected:
    void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;
    void hoverEnterEvent(QGraphicsSceneHoverEvent *event) override;
    void hoverLeaveEvent(QGraphicsSceneHoverEvent *event) override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget = nullptr) override;

private:
    void setHovered(bool hovered);

    DragCallback m_onDrag;
    bool m_dragging = false;
    bool m_hovered = false;
};

} // namespace Editor::Tools

#endif // IMAGEEDITOR_HANDLEITEM_H
