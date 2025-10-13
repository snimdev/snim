#include "DrawingGraphicsView.h"
#include "tools/ArrowTool.h"
#include <QGraphicsScene>
#include <QPen>
#include <QTransform>
#include <QPainter>
#include <QWheelEvent>
#include <QtMath>


namespace ImageEditor {

DrawingGraphicsView::DrawingGraphicsView(QWidget *parent)
    : QGraphicsView(parent)
    , m_currentTool(None)
    , m_drawing(false)
    , m_currentArrow(nullptr)
{
    setDragMode(QGraphicsView::NoDrag);
    setRenderHint(QPainter::Antialiasing);
}

void DrawingGraphicsView::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        QPoint scenePos = mapToScene(event->pos()).toPoint();

        // Check if the click is within image boundaries
        if (!isWithinImageBounds(scenePos)) {
            QGraphicsView::mousePressEvent(event);
            return;
        }

        if (m_currentTool == Pointer) {
            // Handle pointer tool - select items
            QGraphicsItem *item = scene()->itemAt(mapToScene(event->pos()), QTransform());
            if (item) {
                // Get the top-level item (not child items of a group)
                while (item->parentItem() != nullptr) {
                    item = item->parentItem();
                }
                emit itemClicked(item);
            }
            QGraphicsView::mousePressEvent(event);
            return;
        } else if (m_currentTool == Arrow) {
            m_startPoint = scenePos;
            m_drawing = true;
            event->accept();
            return;
        } else if (m_currentTool == Text) {
            emit textRequested(scenePos);
            event->accept();
            return;
        }
    }

    QGraphicsView::mousePressEvent(event);
}

void DrawingGraphicsView::mouseMoveEvent(QMouseEvent *event)
{
    if (m_drawing && m_currentTool == Arrow) {
        QPoint scenePos = mapToScene(event->pos()).toPoint();

        // Clamp the end point to image boundaries for arrows
        m_endPoint = clampToImageBounds(scenePos);

        // Remove previous temporary arrow
        if (m_currentArrow) {
            scene()->removeItem(m_currentArrow);
            delete m_currentArrow;
        }

        // Create new temporary arrow using ArrowTool for consistency
        QPen pen(Qt::red, 3);
        m_currentArrow = new Tools::ArrowTool(m_startPoint, m_endPoint);
        m_currentArrow->setPen(pen);
        scene()->addItem(m_currentArrow);

        event->accept();
        return;
    }

    QGraphicsView::mouseMoveEvent(event);
}

void DrawingGraphicsView::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && m_drawing && m_currentTool == Arrow) {
        m_drawing = false;

        // Remove the temporary arrow from scene
        if (m_currentArrow) {
            scene()->removeItem(m_currentArrow);
            delete m_currentArrow;
            m_currentArrow = nullptr;

            // Emit signal to create a proper arrow layer
            emit arrowDrawn(m_startPoint, m_endPoint);
        }
        event->accept();
        return;
    }

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

bool DrawingGraphicsView::isWithinImageBounds(const QPoint &point) const
{
    return m_imageBounds.contains(point);
}

QPoint DrawingGraphicsView::clampToImageBounds(const QPoint &point) const
{
    // Clamp the point to stay within image boundaries
    int clampedX = qMax(m_imageBounds.left(), qMin(m_imageBounds.right(), point.x()));
    int clampedY = qMax(m_imageBounds.top(), qMin(m_imageBounds.bottom(), point.y()));

    return QPoint(clampedX, clampedY);
}

} // namespace ImageEditor
