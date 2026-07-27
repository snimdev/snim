#ifndef IMAGEEDITOR_SHAPEHANDLETOOL_H
#define IMAGEEDITOR_SHAPEHANDLETOOL_H

#include <QGraphicsEllipseItem>
#include <QPointF>

namespace Editor::Tools {

class ShapeTool;

class ShapeHandleTool : public QGraphicsEllipseItem
{
public:
    // Import HandlePosition from ShapeTool to avoid duplication
    using HandlePosition = int;

    explicit ShapeHandleTool(HandlePosition position, ShapeTool *shapeTool, QGraphicsItem *parent = nullptr);
    ~ShapeHandleTool() override = default;

    void updatePosition(const QPointF &point);

protected:
    void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;
    void hoverEnterEvent(QGraphicsSceneHoverEvent *event) override;
    void hoverLeaveEvent(QGraphicsSceneHoverEvent *event) override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget = nullptr) override;

private:
    Qt::CursorShape getCursorForPosition() const;

    HandlePosition m_position;
    ShapeTool *m_shapeTool;
    bool m_dragging;
    bool m_hovered;

    static constexpr qreal HANDLE_SIZE = 8.0;
    static constexpr qreal HANDLE_HOVER_SIZE = 10.0;
};

} // namespace Editor::Tools

#endif // IMAGEEDITOR_SHAPEHANDLETOOL_H
