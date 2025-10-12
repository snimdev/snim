#ifndef IMAGEEDITOR_DRAWINGGRAPHICSVIEW_H
#define IMAGEEDITOR_DRAWINGGRAPHICSVIEW_H

#include <QGraphicsView>
#include <QMouseEvent>
#include <QGraphicsLineItem>
#include "ArrowItem.h"

namespace ImageEditor {

class DrawingGraphicsView : public QGraphicsView
{
    Q_OBJECT

public:
    enum Tool {
        None,
        Pointer,
        Arrow,
        Text
    };

    explicit DrawingGraphicsView(QWidget *parent = nullptr);

    void setCurrentTool(Tool tool) { m_currentTool = tool; }
    void setImageBounds(const QRect &bounds) { m_imageBounds = bounds; }

signals:
    void arrowDrawn(const QPoint &start, const QPoint &end);
    void textRequested(const QPoint &position);
    void itemClicked(QGraphicsItem *item);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;

private:
    bool isWithinImageBounds(const QPoint &point) const;
    QPoint clampToImageBounds(const QPoint &point) const;

    Tool m_currentTool;
    bool m_drawing;
    QPoint m_startPoint;
    QPoint m_endPoint;
    QRect m_imageBounds;
    ArrowItem *m_currentArrow;
};

} // namespace ImageEditor

#endif // IMAGEEDITOR_DRAWINGGRAPHICSVIEW_H
