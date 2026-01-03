#ifndef IMAGEEDITOR_DRAWINGGRAPHICSVIEW_H
#define IMAGEEDITOR_DRAWINGGRAPHICSVIEW_H

#include <QGraphicsView>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QGraphicsItem>
#include <QRect>

namespace ImageEditor {
    namespace Interactions {
        class IDrawingInteraction;
    }
    namespace Tools {
        class ITool;
    }
}

namespace ImageEditor {

class DrawingGraphicsView : public QGraphicsView
{
    Q_OBJECT

public:
    explicit DrawingGraphicsView(QWidget *parent = nullptr);

    /**
     * @brief Set the current drawing interaction
     * @param interaction The interaction to use (ownership is NOT transferred)
     */
    void setDrawingStrategy(Interactions::IDrawingInteraction *interaction);

    void setImageBounds(const QRect &bounds) { m_imageBounds = bounds; }

    /// Fit the whole scene in the view, but never zoom past 100% (small captures
    /// stay crisp and centered rather than being upscaled).
    void fitContent();
    /// Reset to 100% zoom, centered on the content.
    void zoomActual();

signals:
    /**
     * @brief Emitted when an item is clicked with pointer tool
     */
    void itemClicked(QGraphicsItem *item);

    /**
     * @brief ⌘/Ctrl+wheel over (or with a selected) text item requests a font-size
     * change of `steps` notches. ImageEditor turns this into an undoable command;
     * the view never mutates the model itself.
     */
    void adjustTextSizeRequested(Tools::ITool *tool, int steps);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;

private:
    bool isWithinImageBounds(const QPointF &point) const;
    void updateCursor();

    Interactions::IDrawingInteraction *m_currentStrategy; // Not owned
    QRect m_imageBounds;
};

} // namespace ImageEditor

#endif // IMAGEEDITOR_DRAWINGGRAPHICSVIEW_H
