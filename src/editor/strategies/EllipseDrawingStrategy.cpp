#include "EllipseDrawingStrategy.h"
#include <QGraphicsScene>
#include <QPen>

namespace ImageEditor::Strategies {

EllipseDrawingStrategy::EllipseDrawingStrategy(QObject *parent)
    : BaseDrawingStrategy(parent)
{
}

QGraphicsItem* EllipseDrawingStrategy::createPreview(const QPointF &startPos)
{
    QRectF rect(startPos, startPos);
    auto *ellipsePreview = new QGraphicsEllipseItem(rect);
    ellipsePreview->setPen(QPen(Qt::red, 2, Qt::DashLine));
    ellipsePreview->setBrush(Qt::NoBrush);
    return ellipsePreview;
}

void EllipseDrawingStrategy::updatePreview(const QPointF &currentPos)
{
    if (!m_previewItem) return;

    auto *ellipseItem = dynamic_cast<QGraphicsEllipseItem*>(m_previewItem);
    if (ellipseItem) {
        QRectF rect(m_startPoint, currentPos);
        ellipseItem->setRect(rect.normalized());
    }
}

bool EllipseDrawingStrategy::finalizeDrawing()
{
    QRect rect = QRect(m_startPoint.toPoint(), m_endPoint.toPoint()).normalized();

    // Check minimum size
    if (rect.width() < MIN_SIZE || rect.height() < MIN_SIZE) {
        return false;
    }

    emit ellipseDrawn(rect);
    return true;
}

} // namespace ImageEditor::Strategies
