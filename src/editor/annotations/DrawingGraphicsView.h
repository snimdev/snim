#ifndef IMAGEEDITOR_DRAWINGGRAPHICSVIEW_H
#define IMAGEEDITOR_DRAWINGGRAPHICSVIEW_H

#include <QGraphicsView>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QGraphicsItem>
#include <QRect>
#include "CanvasShadow.h"

namespace Editor::Interactions { class IDrawingInteraction; }
namespace Editor::Tools { class ITool; }

namespace Editor {

class DrawingGraphicsView : public QGraphicsView
{
    Q_OBJECT

public:
    explicit DrawingGraphicsView(QWidget *parent = nullptr);

    void setDrawingStrategy(Interactions::IDrawingInteraction *interaction);   // not owned

    void setImageBounds(const QRect &bounds) { m_imageBounds = bounds; }

    /// Match the canvas shadow to a dark or light editor background.
    void setDarkTheme(bool dark);

    /// Fit the whole scene in the view with room for its shadow, but never zoom past
    /// 100% (small captures stay crisp and centered rather than being upscaled).
    void fitContent();
    /// Reset to 100% zoom, centered on the content.
    void zoomActual();

signals:
    // ⌘/Ctrl+wheel over (or with a selected) text item: a font-size change of `steps`
    // notches, which the editor makes undoable; the view never edits the model itself.
    void adjustTextSizeRequested(Tools::ITool *tool, int steps);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void drawBackground(QPainter *painter, const QRectF &rect) override;

private:
    bool isWithinImageBounds(const QPointF &point) const;
    void updateCursor();

    Interactions::IDrawingInteraction *m_currentStrategy; // Not owned
    QRect m_imageBounds;
    CanvasShadow m_canvasShadow;
};

} // namespace Editor

#endif // IMAGEEDITOR_DRAWINGGRAPHICSVIEW_H
