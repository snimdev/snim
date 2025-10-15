#include "FreehandDrawingStrategy.h"
#include "../tools/FreehandTool.h"

namespace ImageEditor::Strategies {

FreehandDrawingStrategy::FreehandDrawingStrategy(QObject *parent)
    : BaseDrawingStrategy(parent)
    , m_pen(Qt::red, 3, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin)
{
}

QGraphicsItem* FreehandDrawingStrategy::createPreview(const QPointF &startPos)
{
    auto *freehandPreview = new Tools::FreehandTool();
    freehandPreview->setPen(m_pen);
    freehandPreview->addPoint(startPos);
    return freehandPreview;
}

void FreehandDrawingStrategy::updatePreview(const QPointF &currentPos)
{
    if (m_previewItem) {
        auto *freehand = dynamic_cast<Tools::FreehandTool*>(m_previewItem);
        if (freehand) {
            freehand->addPoint(currentPos);
        }
    }
}

bool FreehandDrawingStrategy::finalizeDrawing()
{
    if (!m_previewItem) return false;

    auto *freehand = dynamic_cast<Tools::FreehandTool*>(m_previewItem);
    if (freehand && !freehand->isEmpty()) {
        // Extract points and emit signal
        QList<QPointF> points = freehand->points();
        emit freehandDrawn(points);
        return true;
    }

    return false;
}

} // namespace ImageEditor::Strategies
