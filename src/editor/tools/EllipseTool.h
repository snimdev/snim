#ifndef IMAGEEDITOR_ELLIPSETOOL_H
#define IMAGEEDITOR_ELLIPSETOOL_H

#include "ShapeTool.h"

namespace ImageEditor::Tools {

/**
 * @brief Ellipse shape tool
 *
 * Draws an ellipse (or circle) with configurable fill and stroke.
 * Supports resizing via 8 handles (corners and edges of bounding rect).
 */
class EllipseTool : public ShapeTool
{
    Q_OBJECT

public:
    explicit EllipseTool(const QRectF &rect, QGraphicsItem *parent = nullptr);
    ~EllipseTool() override = default;

protected:
    void paintShape(QPainter *painter) override;
    QPainterPath createShapePath() const override;
};

} // namespace ImageEditor::Tools

#endif // IMAGEEDITOR_ELLIPSETOOL_H
