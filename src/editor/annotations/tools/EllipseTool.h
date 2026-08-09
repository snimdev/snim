#ifndef IMAGEEDITOR_ELLIPSETOOL_H
#define IMAGEEDITOR_ELLIPSETOOL_H

#include "ShapeTool.h"

namespace Editor::Tools {

class EllipseTool : public ShapeTool
{
    Q_OBJECT

public:
    explicit EllipseTool(const QRectF &rect, QGraphicsItem *parent = nullptr);

    [[nodiscard]] QGraphicsItem* clone() const override;

protected:
    [[nodiscard]] QPainterPath outline() const override;
};

} // namespace Editor::Tools

#endif // IMAGEEDITOR_ELLIPSETOOL_H
