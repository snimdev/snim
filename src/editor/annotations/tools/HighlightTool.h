#ifndef IMAGEEDITOR_HIGHLIGHTTOOL_H
#define IMAGEEDITOR_HIGHLIGHTTOOL_H

#include "PathTool.h"

namespace Editor::Tools {

class HighlightTool : public PathTool
{
    Q_OBJECT

public:
    explicit HighlightTool(QGraphicsItem *parent = nullptr);

    // ITool interface for dynamic properties
    [[nodiscard]] QList<ToolProperty> getProperties() const override;
    void setProperty(const QString& propertyId, const QVariant& value) override;
    [[nodiscard]] QGraphicsItem* clone() const override;
    void applyStyleFrom(const ITool* other) override;   // copies color + width

    // Styling
    void setColor(const QColor &color);
    [[nodiscard]] QColor color() const;
    void setWidth(qreal width);
    [[nodiscard]] qreal width() const { return m_pen.widthF(); }
    [[nodiscard]] QPen pen() const { return m_pen; }

    static constexpr qreal HIGHLIGHT_WIDTH_SMALL = 15.0;
    static constexpr qreal HIGHLIGHT_WIDTH_MEDIUM = 25.0;
    static constexpr qreal HIGHLIGHT_WIDTH_LARGE = 35.0;
    static constexpr qreal HIGHLIGHT_OPACITY = 0.3;

protected:
    [[nodiscard]] QPen strokePen() const override { return m_pen; }
    void paintPath(QPainter *painter) override;

private:
    QPen m_pen;
};

} // namespace Editor::Tools

#endif // IMAGEEDITOR_HIGHLIGHTTOOL_H
