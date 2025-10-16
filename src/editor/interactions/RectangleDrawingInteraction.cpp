#include "RectangleDrawingInteraction.h"
#include <QGraphicsScene>
#include <QPen>

namespace ImageEditor::Interactions {

RectangleDrawingInteraction::RectangleDrawingInteraction(QObject *parent)
    : BaseDrawingInteraction(parent)
{
}

QGraphicsItem* RectangleDrawingInteraction::createPreview(const QPointF &startPos)
{
    QRectF rect(startPos, startPos);
    auto *rectPreview = new QGraphicsRectItem(rect);
    rectPreview->setPen(QPen(Qt::red, 2, Qt::DashLine));
    rectPreview->setBrush(Qt::NoBrush);
    return rectPreview;
}

void RectangleDrawingInteraction::updatePreview(const QPointF &currentPos)
{
    if (!m_previewItem) return;

    auto *rectItem = dynamic_cast<QGraphicsRectItem*>(m_previewItem);
    if (rectItem) {
        QRectF rect(m_startPoint, currentPos);
        rectItem->setRect(rect.normalized());
    }
}

bool RectangleDrawingInteraction::finalizeDrawing()
{
    QRect rect = QRect(m_startPoint.toPoint(), m_endPoint.toPoint()).normalized();

    // Check minimum size
    if (rect.width() < MIN_SIZE || rect.height() < MIN_SIZE) {
        return false;
    }

    emit rectangleDrawn(rect);
    return true;
}

} // namespace ImageEditor::Interactions
