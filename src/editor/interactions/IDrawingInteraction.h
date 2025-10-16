#ifndef IMAGEEDITOR_IDRAWINGINTERACTION_H
#define IMAGEEDITOR_IDRAWINGINTERACTION_H

#include <QMouseEvent>
#include <QGraphicsScene>
#include <QGraphicsItem>
#include <QPointF>
#include <QRect>
#include <QPen>

namespace ImageEditor::Interactions {

/**
 * @brief Interface for drawing tool interactions
 *
 * This interface defines the contract for tool-specific drawing behavior.
 * Each tool (Arrow, Rectangle, Freehand, etc.) implements this interface
 * to encapsulate its own interaction logic.
 *
 * Benefits:
 * - Single Responsibility: Each interaction handles one tool's behavior
 * - Open/Closed: Add new tools without modifying existing code
 * - Reduced coupling: DrawingGraphicsView doesn't need to know tool details
 */
class IDrawingInteraction
{
public:
    virtual ~IDrawingInteraction() = default;

    /**
     * @brief Handle mouse press event
     * @param scenePos The position in scene coordinates
     * @param scene The graphics scene to add preview items to
     * @return true if the event was handled, false otherwise
     */
    virtual bool onMousePress(const QPointF &scenePos, QGraphicsScene *scene) = 0;

    /**
     * @brief Handle mouse move event (for preview updates)
     * @param scenePos The current position in scene coordinates
     * @param scene The graphics scene
     * @return true if the event was handled, false otherwise
     */
    virtual bool onMouseMove(const QPointF &scenePos, QGraphicsScene *scene) = 0;

    /**
     * @brief Handle mouse release event (finalize drawing)
     * @param scenePos The position in scene coordinates
     * @param scene The graphics scene
     * @return true if a valid shape was created, false otherwise
     */
    virtual bool onMouseRelease(const QPointF &scenePos, QGraphicsScene *scene) = 0;

    /**
     * @brief Get the cursor to use for this tool
     */
    virtual Qt::CursorShape getCursor() const = 0;

    /**
     * @brief Get the cursor to use while actively drawing
     */
    virtual Qt::CursorShape getDrawingCursor() const { return getCursor(); }

    /**
     * @brief Clean up any preview items
     * Called when switching tools or canceling drawing
     */
    virtual void cleanup(QGraphicsScene *scene) = 0;

    /**
     * @brief Check if this strategy is currently drawing
     */
    virtual bool isDrawing() const = 0;
};

} // namespace ImageEditor::Interactions

#endif // IMAGEEDITOR_IDRAWINGINTERACTION_H
