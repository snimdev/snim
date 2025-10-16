#ifndef IMAGEEDITOR_HIGHLIGHTDRAWINGINTERACTION_H
#define IMAGEEDITOR_HIGHLIGHTDRAWINGINTERACTION_H

#include "BaseDrawingInteraction.h"
#include <QColor>
#include <QList>
#include <QPointF>

namespace ImageEditor {
    namespace Tools {
        class HighlightTool;
    }
}

namespace ImageEditor::Interactions {

/**
 * @brief Interaction for highlight drawing tool
 *
 * Similar to freehand but for highlighting with fixed width and opacity.
 * Handles continuous path drawing by adding points as the mouse moves.
 */
class HighlightDrawingInteraction : public BaseDrawingInteraction
{
    Q_OBJECT

public:
    explicit HighlightDrawingInteraction(QObject *parent = nullptr);

    // IDrawingInteraction interface
    Qt::CursorShape getCursor() const override { return Qt::CrossCursor; }

    // Configure the color and width for highlighting
    void setColor(const QColor &color) { m_color = color; }
    QColor color() const { return m_color; }
    void setWidth(qreal width) { m_width = width; }
    qreal width() const { return m_width; }

signals:
    /**
     * @brief Emitted when highlight drawing is complete
     * @param points The list of points that make up the path
     * @param color The highlight color (without opacity)
     * @param width The highlight width
     */
    void highlightDrawn(const QList<QPointF> &points, const QColor &color, qreal width);

protected:
    // BaseDrawingInteraction interface
    QGraphicsItem* createPreview(const QPointF &startPos) override;
    void updatePreview(const QPointF &currentPos) override;
    bool finalizeDrawing() override;

private:
    QColor m_color;
    qreal m_width;
};

} // namespace ImageEditor::Interactions

#endif // IMAGEEDITOR_HIGHLIGHTDRAWINGINTERACTION_H
