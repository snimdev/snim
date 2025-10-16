#include "BaseDrawingInteraction.h"
#include <QGraphicsScene>
#include <QtMath>

namespace ImageEditor::Interactions {

BaseDrawingInteraction::BaseDrawingInteraction(QObject *parent)
    : QObject(parent)
    , m_isDrawing(false)
    , m_previewItem(nullptr)
{
}

BaseDrawingInteraction::~BaseDrawingInteraction()
{
    // Preview item should be owned by scene and cleaned up there
    m_previewItem = nullptr;
}

bool BaseDrawingInteraction::onMousePress(const QPointF &scenePos, QGraphicsScene *scene)
{
    if (!scene) return false;

    m_startPoint = scenePos;
    m_endPoint = scenePos;
    m_isDrawing = true;

    // Let subclass create the preview
    m_previewItem = createPreview(scenePos);
    if (m_previewItem) {
        scene->addItem(m_previewItem);
    }

    return true;
}

bool BaseDrawingInteraction::onMouseMove(const QPointF &scenePos, QGraphicsScene *scene)
{
    if (!m_isDrawing || !scene) return false;

    // Clamp to image boundaries
    m_endPoint = clampToImageBounds(scenePos);

    // Let subclass update the preview
    updatePreview(m_endPoint);

    return true;
}

bool BaseDrawingInteraction::onMouseRelease(const QPointF &scenePos, QGraphicsScene *scene)
{
    if (!m_isDrawing || !scene) return false;

    m_isDrawing = false;
    m_endPoint = clampToImageBounds(scenePos);

    // Let subclass validate and finalize
    bool success = finalizeDrawing();

    // Clean up preview
    cleanup(scene);

    return success;
}

void BaseDrawingInteraction::cleanup(QGraphicsScene *scene)
{
    if (m_previewItem && scene) {
        scene->removeItem(m_previewItem);
        delete m_previewItem;
        m_previewItem = nullptr;
    }
    m_isDrawing = false;
}

QPointF BaseDrawingInteraction::clampToImageBounds(const QPointF &point) const
{
    if (m_imageBounds.isNull()) {
        return point;
    }

    qreal clampedX = qMax(static_cast<qreal>(m_imageBounds.left()),
                          qMin(static_cast<qreal>(m_imageBounds.right()), point.x()));
    qreal clampedY = qMax(static_cast<qreal>(m_imageBounds.top()),
                          qMin(static_cast<qreal>(m_imageBounds.bottom()), point.y()));

    return QPointF(clampedX, clampedY);
}

} // namespace ImageEditor::Interactions
