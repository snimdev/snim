#ifndef IMAGEEDITOR_BASEDRAWINGSTRATEGY_H
#define IMAGEEDITOR_BASEDRAWINGSTRATEGY_H

#include "IDrawingToolStrategy.h"
#include <QObject>
#include <QPointF>

namespace ImageEditor::Strategies {

/**
 * @brief Base class for drawing strategies providing common functionality
 *
 * This abstract class implements common behavior shared by most drawing tools:
 * - Start/end point tracking
 * - Drawing state management
 * - Preview item lifecycle
 * - Boundary clamping
 */
class BaseDrawingStrategy : public QObject, public IDrawingToolStrategy
{
    Q_OBJECT

public:
    explicit BaseDrawingStrategy(QObject *parent = nullptr);
    ~BaseDrawingStrategy() override;

    // IDrawingToolStrategy interface
    bool onMousePress(const QPointF &scenePos, QGraphicsScene *scene) override;
    bool onMouseMove(const QPointF &scenePos, QGraphicsScene *scene) override;
    bool onMouseRelease(const QPointF &scenePos, QGraphicsScene *scene) override;
    void cleanup(QGraphicsScene *scene) override;
    bool isDrawing() const override { return m_isDrawing; }

    // Boundary management
    void setImageBounds(const QRect &bounds) { m_imageBounds = bounds; }

signals:
    /**
     * @brief Emitted when a complete shape is drawn
     * Subclasses define what data is emitted (points, rect, etc.)
     */
    void drawingCompleted();

protected:
    /**
     * @brief Create and return the preview item for this tool
     * Called when drawing starts
     */
    virtual QGraphicsItem* createPreview(const QPointF &startPos) = 0;

    /**
     * @brief Update the preview item as mouse moves
     * Called on every mouse move during drawing
     */
    virtual void updatePreview(const QPointF &currentPos) = 0;

    /**
     * @brief Validate and emit the final result
     * Called on mouse release
     * @return true if a valid shape was created
     */
    virtual bool finalizeDrawing() = 0;

    /**
     * @brief Clamp a point to image boundaries
     */
    QPointF clampToImageBounds(const QPointF &point) const;

    // Protected state
    bool m_isDrawing;
    QPointF m_startPoint;
    QPointF m_endPoint;
    QGraphicsItem *m_previewItem;
    QRect m_imageBounds;
};

} // namespace ImageEditor::Strategies

#endif // IMAGEEDITOR_BASEDRAWINGSTRATEGY_H
