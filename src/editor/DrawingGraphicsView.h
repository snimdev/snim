#ifndef IMAGEEDITOR_DRAWINGGRAPHICSVIEW_H
#define IMAGEEDITOR_DRAWINGGRAPHICSVIEW_H

#include <QGraphicsView>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QGraphicsItem>
#include <QRect>

#include "tools/ArrowTool.h"

namespace ImageEditor {

class DrawingGraphicsView : public QGraphicsView
{
    Q_OBJECT

public:
    enum ToolType {
        None,
        Pointer,
        Arrow,
        Text,
        Rectangle,
        Ellipse,
        Freehand
    };

    explicit DrawingGraphicsView(QWidget *parent = nullptr);

    void setCurrentTool(ToolType tool);
    void setImageBounds(const QRect &bounds) { m_imageBounds = bounds; }
    void setFreehandPen(const QPen &pen) { m_freehandPen = pen; }

signals:
    void arrowDrawn(const QPoint &start, const QPoint &end);
    void textRequested(const QPoint &position);
    void rectangleDrawn(const QRect &rect);
    void ellipseDrawn(const QRect &rect);
    void freehandDrawn(const QList<QPointF> &points);
    void itemClicked(QGraphicsItem *item);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;

private:
    bool isWithinImageBounds(const QPoint &point) const;
    QPoint clampToImageBounds(const QPoint &point) const;
    void updateCursor();

    ToolType m_currentTool;
    bool m_drawing;
    QPoint m_startPoint;
    QPoint m_endPoint;
    QRect m_imageBounds;
    QGraphicsItem *m_previewItem;  // Preview item while drawing (arrow, shape, etc.)
    QPen m_freehandPen;  // Current pen settings for freehand tool
};

} // namespace ImageEditor

#endif // IMAGEEDITOR_DRAWINGGRAPHICSVIEW_H
