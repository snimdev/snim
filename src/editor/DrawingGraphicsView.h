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
        Text
    };

    explicit DrawingGraphicsView(QWidget *parent = nullptr);

    void setCurrentTool(ToolType tool);
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
    void updateCursor();

    ToolType m_currentTool;
    bool m_drawing;
    QPoint m_startPoint;
    QPoint m_endPoint;
    QRect m_imageBounds;
    Tools::ArrowTool *m_currentArrow;
};

} // namespace ImageEditor

#endif // IMAGEEDITOR_DRAWINGGRAPHICSVIEW_H
