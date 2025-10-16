#include "EllipseDrawingInteraction.h"
#include <QGraphicsScene>
#include <QPen>

namespace ImageEditor::Interactions {

EllipseDrawingInteraction::EllipseDrawingInteraction(QObject *parent)
    : BaseDrawingInteraction(parent)
{
}

QGraphicsItem* EllipseDrawingInteraction::createPreview(const QPointF &startPos)
{
    QRectF rect(startPos, startPos);
    auto *ellipsePreview = new QGraphicsEllipseItem(rect);
    ellipsePreview->setPen(QPen(Qt::red, 2, Qt::DashLine));
    ellipsePreview->setBrush(Qt::NoBrush);
    return ellipsePreview;
}

void EllipseDrawingInteraction::updatePreview(const QPointF &currentPos)
{
    if (!m_previewItem) return;

    auto *ellipseItem = dynamic_cast<QGraphicsEllipseItem*>(m_previewItem);
    if (ellipseItem) {
        QRectF rect(m_startPoint, currentPos);
        ellipseItem->setRect(rect.normalized());
    }
}

bool EllipseDrawingInteraction::finalizeDrawing()
{
    QRect rect = QRect(m_startPoint.toPoint(), m_endPoint.toPoint()).normalized();

    // Check minimum size
    if (rect.width() < MIN_SIZE || rect.height() < MIN_SIZE) {
        return false;
    }

    emit ellipseDrawn(rect);
    return true;
}

} // namespace ImageEditor::Interactions
