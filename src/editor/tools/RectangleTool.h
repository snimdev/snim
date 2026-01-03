#ifndef IMAGEEDITOR_RECTANGLETOOL_H
#define IMAGEEDITOR_RECTANGLETOOL_H

#include "ShapeTool.h"

namespace ImageEditor::Tools {

class RectangleTool : public ShapeTool
{
    Q_OBJECT

public:
    explicit RectangleTool(const QRectF &rect, QGraphicsItem *parent = nullptr);
    ~RectangleTool() override = default;

    [[nodiscard]] QGraphicsItem* clone() const override;

protected:
    void paintShape(QPainter *painter) override;
    QPainterPath createShapePath() const override;
};

} // namespace ImageEditor::Tools

#endif // IMAGEEDITOR_RECTANGLETOOL_H
