#ifndef IMAGEEDITOR_ARROWHANDLETOOL_H
#define IMAGEEDITOR_ARROWHANDLETOOL_H

#include <QGraphicsEllipseItem>
#include <QPointF>

namespace Editor::Tools {

class ArrowTool;


class ArrowHandleTool : public QGraphicsEllipseItem
{
public:
    enum HandleType {
        StartHandle,
        EndHandle
    };

    explicit ArrowHandleTool(HandleType type, ArrowTool *arrowTool, QGraphicsItem *parent = nullptr);
    ~ArrowHandleTool() override = default;

    void updatePosition(const QPointF &point);
    HandleType handleType() const { return m_type; }

protected:
    void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;
    void hoverEnterEvent(QGraphicsSceneHoverEvent *event) override;
    void hoverLeaveEvent(QGraphicsSceneHoverEvent *event) override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget = nullptr) override;

private:
    HandleType m_type;
    ArrowTool *m_arrowTool;
    bool m_dragging;
    QPointF m_dragStartPos;
    bool m_hovered;

    static constexpr qreal HANDLE_SIZE = 8.0;
    static constexpr qreal HANDLE_HOVER_SIZE = 10.0;
};

} // namespace Editor::Tools

#endif // IMAGEEDITOR_ARROWHANDLETOOL_H
