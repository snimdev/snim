#include "DrawingGraphicsView.h"
#include "interactions/IDrawingInteraction.h"
#include "tools/TextTool.h"
#include <QGraphicsScene>
#include <QTransform>
#include <QPainter>
#include <QWheelEvent>
#include <QtMath>

namespace Editor {

DrawingGraphicsView::DrawingGraphicsView(QWidget *parent)
    : QGraphicsView(parent)
    , m_currentStrategy(nullptr)
{
    setDragMode(QGraphicsView::NoDrag);
    setRenderHint(QPainter::Antialiasing);
    setMouseTracking(true);

    // Neutral gray viewport background (visible around the canvas)
    setBackgroundRole(QPalette::Dark);
    setAutoFillBackground(true);
    setFrameStyle(QFrame::NoFrame);
}

void DrawingGraphicsView::fitContent()
{
    if (!scene())
        return;
    const QRectF r = scene()->sceneRect();
    if (r.isEmpty())
        return;

    // Leave room for the canvas shadow; never upscale, so small captures stay crisp.
    constexpr qreal pad = 24;
    const qreal s = qMin(qMin((viewport()->width() - 2 * pad) / r.width(),
                              (viewport()->height() - 2 * pad) / r.height()), 1.0);
    if (s <= 0)
        return;
    setTransform(QTransform::fromScale(s, s));
    centerOn(r.center());
}

void DrawingGraphicsView::zoomActual()
{
    // Show the capture at 100% (no scaling), centered. Larger-than-viewport
    // captures are then scrollable (ScrollBarAsNeeded), not shrunk to fit.
    resetTransform();
    if (scene())
        centerOn(scene()->sceneRect().center());
}

void DrawingGraphicsView::setDarkTheme(bool dark)
{
    m_canvasShadow.setDark(dark);
    viewport()->update();
}

void DrawingGraphicsView::drawBackground(QPainter *painter, const QRectF &rect)
{
    QGraphicsView::drawBackground(painter, rect);
    // The scene rect is the canvas: the screenshot, or the backdrop around it.
    if (scene())
        m_canvasShadow.paint(painter, scene()->sceneRect(), rect);
}

void DrawingGraphicsView::setDrawingStrategy(Interactions::IDrawingInteraction *strategy)
{
    // Clean up previous strategy if it was drawing
    if (m_currentStrategy) {
        m_currentStrategy->cleanup(scene());
    }

    m_currentStrategy = strategy;
    updateCursor();
}

void DrawingGraphicsView::updateCursor()
{
    if (m_currentStrategy) {
        // Use drawing cursor if actively drawing, otherwise use normal cursor
        if (m_currentStrategy->isDrawing()) {
            setCursor(m_currentStrategy->getDrawingCursor());
        } else {
            setCursor(m_currentStrategy->getCursor());
        }
    } else {
        setCursor(Qt::ArrowCursor);
    }
}

void DrawingGraphicsView::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && m_currentStrategy) {
        QPointF scenePos = mapToScene(event->pos());

        // Check if the click is within image boundaries
        if (isWithinImageBounds(scenePos)) {
            bool handled = m_currentStrategy->onMousePress(scenePos, scene());
            if (handled) {
                updateCursor(); // Update cursor after press
                event->accept();
                return;
            }
        }
    }

    // Let base class handle if strategy didn't handle it
    QGraphicsView::mousePressEvent(event);
}

void DrawingGraphicsView::mouseMoveEvent(QMouseEvent *event)
{
    if (m_currentStrategy && m_currentStrategy->isDrawing()) {
        QPointF scenePos = mapToScene(event->pos());
        bool handled = m_currentStrategy->onMouseMove(scenePos, scene());

        if (handled) {
            event->accept();
            return;
        }
    }

    // Let base class handle if strategy didn't handle it
    QGraphicsView::mouseMoveEvent(event);
}

void DrawingGraphicsView::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && m_currentStrategy && m_currentStrategy->isDrawing()) {
        QPointF scenePos = mapToScene(event->pos());
        bool handled = m_currentStrategy->onMouseRelease(scenePos, scene());

        updateCursor(); // Restore cursor after drawing

        if (handled) {
            event->accept();
            return;
        }
    }

    // Let base class handle if strategy didn't handle it
    QGraphicsView::mouseReleaseEvent(event);
}

void DrawingGraphicsView::wheelEvent(QWheelEvent *event)
{
    // Only handle zoom when Ctrl key is pressed
    if (event->modifiers() & Qt::ControlModifier) {
        // ⌘/Ctrl+wheel resizes a text item: the one under the cursor, else the
        // currently-selected text item. Emit a request that Editor turns into an
        // undoable PropertyChangeCommand("fontSize"); the view never mutates the model.
        auto *target = dynamic_cast<Tools::TextTool*>(
            scene()->itemAt(mapToScene(event->position().toPoint()), QTransform()));
        if (!target) {
            const auto selected = scene()->selectedItems();
            for (QGraphicsItem *it : selected)
                if (auto *t = dynamic_cast<Tools::TextTool*>(it)) { target = t; break; }
        }
        if (target) {
            emit adjustTextSizeRequested(target, event->angleDelta().y() / 120);
            event->accept();
            return;
        }

        // Get the angle delta from the wheel event
        const int numDegrees = event->angleDelta().y() / 8;
        const int numSteps = numDegrees / 15;

        // Calculate zoom factor (negative steps zoom out)
        const double scaleFactor = 1.15;
        double factor = qPow(scaleFactor, numSteps);

        // Apply zoom with limits
        QTransform currentTransform = transform();
        double currentScale = currentTransform.m11(); // Get current scale factor

        // Set zoom limits (10% to 500%)
        const double minScale = 0.1;
        const double maxScale = 5.0;

        double newScale = currentScale * factor;
        if (newScale < minScale) {
            factor = minScale / currentScale;
        } else if (newScale > maxScale) {
            factor = maxScale / currentScale;
        }

        // Zoom centered on mouse position; scrollbars then allow panning.
        setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
        scale(factor, factor);
        setTransformationAnchor(QGraphicsView::AnchorViewCenter);

        event->accept();
    } else {
        // Pass event to parent for normal scroll behavior
        QGraphicsView::wheelEvent(event);
    }
}

bool DrawingGraphicsView::isWithinImageBounds(const QPointF &point) const
{
    return m_imageBounds.contains(point.toPoint());
}

} // namespace Editor
