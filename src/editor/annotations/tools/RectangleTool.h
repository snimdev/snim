#ifndef IMAGEEDITOR_RECTANGLETOOL_H
#define IMAGEEDITOR_RECTANGLETOOL_H

#include "ShapeTool.h"

namespace Editor::Tools {

class RectangleTool : public ShapeTool
{
    Q_OBJECT

public:
    explicit RectangleTool(const QRectF &rect, QGraphicsItem *parent = nullptr);

    [[nodiscard]] QGraphicsItem* clone() const override;

protected:
    [[nodiscard]] QPainterPath outline() const override;
    // drawRect, as drawPath(outline()) antialiases a rectangle differently.
    void paintShape(QPainter *painter) override;
};

} // namespace Editor::Tools

#endif // IMAGEEDITOR_RECTANGLETOOL_H
