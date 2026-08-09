#ifndef IMAGEEDITOR_FREEHANDTOOL_H
#define IMAGEEDITOR_FREEHANDTOOL_H

#include "PathTool.h"

namespace Editor::Tools {

class FreehandTool : public PathTool
{
    Q_OBJECT

public:
    explicit FreehandTool(QGraphicsItem *parent = nullptr);

    // ITool interface for dynamic properties
    [[nodiscard]] QList<ToolProperty> getProperties() const override;
    void setProperty(const QString& propertyId, const QVariant& value) override;
    [[nodiscard]] QGraphicsItem* clone() const override;
    void applyStyleFrom(const ITool* other) override;   // copies pen

    void setPen(const QPen &pen);
    [[nodiscard]] QPen pen() const { return m_pen; }

protected:
    [[nodiscard]] QPen strokePen() const override;
    void paintPath(QPainter *painter) override;

private:
    QPen m_pen;
};

} // namespace Editor::Tools

#endif // IMAGEEDITOR_FREEHANDTOOL_H
