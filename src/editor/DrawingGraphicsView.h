#ifndef IMAGEEDITOR_DRAWINGGRAPHICSVIEW_H
#define IMAGEEDITOR_DRAWINGGRAPHICSVIEW_H

#include <QGraphicsView>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QGraphicsItem>
#include <QRect>

namespace ImageEditor {
    namespace Strategies {
        class IDrawingToolStrategy;
    }
}

namespace ImageEditor {

class DrawingGraphicsView : public QGraphicsView
{
    Q_OBJECT

public:
    explicit DrawingGraphicsView(QWidget *parent = nullptr);

    /**
     * @brief Set the current drawing strategy
     * @param strategy The strategy to use (ownership is NOT transferred)
     */
    void setDrawingStrategy(Strategies::IDrawingToolStrategy *strategy);

    void setImageBounds(const QRect &bounds) { m_imageBounds = bounds; }

signals:
    /**
     * @brief Emitted when an item is clicked with pointer tool
     */
    void itemClicked(QGraphicsItem *item);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;

private:
    bool isWithinImageBounds(const QPointF &point) const;
    void updateCursor();

    Strategies::IDrawingToolStrategy *m_currentStrategy; // Not owned
    QRect m_imageBounds;
};

} // namespace ImageEditor

#endif // IMAGEEDITOR_DRAWINGGRAPHICSVIEW_H
