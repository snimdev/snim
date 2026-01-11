#include "BlurDrawingInteraction.h"
#include "../tools/BlurTool.h"

namespace Editor::Interactions {

BlurDrawingInteraction::BlurDrawingInteraction(QObject *parent)
    : BaseDrawingInteraction(parent)
    , m_blurRadius(10.0)
    , m_brushWidth(30.0)
{
}

QGraphicsItem* BlurDrawingInteraction::createPreview(const QPointF &startPos)
{
    auto *blurPreview = new Tools::BlurTool();
    blurPreview->setBlurRadius(m_blurRadius);
    blurPreview->setBrushWidth(m_brushWidth);
    blurPreview->setSourcePixmap(m_sourcePixmap);
    blurPreview->addPoint(startPos);
    return blurPreview;
}

void BlurDrawingInteraction::updatePreview(const QPointF &currentPos)
{
    if (m_previewItem) {
        auto *blur = dynamic_cast<Tools::BlurTool*>(m_previewItem);
        if (blur) {
            blur->addPoint(currentPos);
        }
    }
}

bool BlurDrawingInteraction::finalizeDrawing()
{
    if (!m_previewItem) return false;

    auto *blur = dynamic_cast<Tools::BlurTool*>(m_previewItem);
    if (blur && !blur->isEmpty()) {
        // Finish the blur path and generate final blurred result
        blur->finishPath();

        // Extract points and emit signal
        QList<QPointF> points = blur->points();
        emit blurDrawn(points);
        return true;
    }

    return false;
}

} // namespace Editor::Interactions
