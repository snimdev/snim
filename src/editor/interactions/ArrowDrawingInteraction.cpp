#include "ArrowDrawingInteraction.h"
#include "../tools/ArrowTool.h"
#include <QGraphicsScene>
#include <cmath>

namespace ImageEditor::Interactions {

ArrowDrawingInteraction::ArrowDrawingInteraction(QObject *parent)
    : BaseDrawingInteraction(parent)
{
}

QGraphicsItem* ArrowDrawingInteraction::createPreview(const QPointF &startPos)
{
    auto *arrowPreview = new Tools::ArrowTool(startPos.toPoint(), startPos.toPoint());
    arrowPreview->setPen(QPen(Qt::red, 3));
    return arrowPreview;
}

void ArrowDrawingInteraction::updatePreview(const QPointF &currentPos)
{
    if (!m_previewItem) return;

    // For arrows, we need to recreate the preview with new end point
    // Remove old preview
    if (auto *scene = m_previewItem->scene()) {
        scene->removeItem(m_previewItem);
        delete m_previewItem;

        // Create new preview with updated end point
        m_previewItem = new Tools::ArrowTool(m_startPoint.toPoint(), currentPos.toPoint());
        auto *arrow = static_cast<Tools::ArrowTool*>(m_previewItem);
        arrow->setPen(QPen(Qt::red, 3));
        scene->addItem(m_previewItem);
    }
}

bool ArrowDrawingInteraction::finalizeDrawing()
{
    // Calculate arrow length
    qreal dx = m_endPoint.x() - m_startPoint.x();
    qreal dy = m_endPoint.y() - m_startPoint.y();
    qreal length = std::sqrt(dx*dx + dy*dy);

    if (length < MIN_ARROW_LENGTH) {
        return false; // Too short to be meaningful
    }

    emit arrowDrawn(m_startPoint.toPoint(), m_endPoint.toPoint());
    return true;
}

} // namespace ImageEditor::Interactions
