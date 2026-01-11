#include "HighlightDrawingInteraction.h"
#include "../tools/HighlightTool.h"

namespace Editor::Interactions {

HighlightDrawingInteraction::HighlightDrawingInteraction(QObject *parent)
    : BaseDrawingInteraction(parent)
    , m_color(QColor("#b3ff61")) // Default highlight color
    , m_width(Tools::HighlightTool::HIGHLIGHT_WIDTH_MEDIUM) // Default to medium width
{
}

QGraphicsItem* HighlightDrawingInteraction::createPreview(const QPointF &startPos)
{
    auto *highlightPreview = new Tools::HighlightTool();
    highlightPreview->setColor(m_color);
    highlightPreview->setWidth(m_width);
    highlightPreview->addPoint(startPos);
    return highlightPreview;
}

void HighlightDrawingInteraction::updatePreview(const QPointF &currentPos)
{
    if (m_previewItem) {
        auto *highlight = dynamic_cast<Tools::HighlightTool*>(m_previewItem);
        if (highlight) {
            highlight->addPoint(currentPos);
        }
    }
}

bool HighlightDrawingInteraction::finalizeDrawing()
{
    if (!m_previewItem) return false;

    auto *highlight = dynamic_cast<Tools::HighlightTool*>(m_previewItem);
    if (highlight && !highlight->isEmpty()) {
        // Extract points and emit signal with color and width
        QList<QPointF> points = highlight->points();
        emit highlightDrawn(points, m_color, m_width);
        return true;
    }

    return false;
}

} // namespace Editor::Interactions
