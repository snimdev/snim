#include "DrawingGraphicsView.h"
#include "interactions/IDrawingInteraction.h"
#include <QGraphicsScene>
#include <QTransform>
#include <QPainter>
#include <QWheelEvent>
#include <QtMath>

namespace ImageEditor {

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
        // Check if there's a text item under the mouse cursor that should handle font size changes
        QGraphicsItem *itemUnderMouse = scene()->itemAt(mapToScene(event->position().toPoint()), QTransform());
        if (itemUnderMouse && itemUnderMouse->type() == QGraphicsTextItem::Type && itemUnderMouse->isSelected()) {
            // Let the text item handle the font size change
            QGraphicsView::wheelEvent(event);
            return;
        }

        // Get the angle delta from the wheel event
        const int numDegrees = event->angleDelta().y() / 8;
        const int numSteps = numDegrees / 15;

        // Calculate zoom factor
        const double scaleFactor = 1.15;
        double factor = 1.0;

        if (numSteps > 0) {
            // Zoom in
            factor = qPow(scaleFactor, numSteps);
        } else if (numSteps < 0) {
            // Zoom out
            factor = qPow(scaleFactor, numSteps);
        }

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

        // Zoom centered on mouse position
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

} // namespace ImageEditor
