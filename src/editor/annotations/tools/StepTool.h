#ifndef IMAGEEDITOR_STEPTOOL_H
#define IMAGEEDITOR_STEPTOOL_H

#include "ToolItem.h"
#include <QColor>
#include <QPainterPath>
#include <QRectF>

namespace Editor::Tools {

/**
 * A numbered badge: a filled disc with a numeral, stamped one per click for
 * step-by-step annotations. The item position is the badge centre.
 */
class StepTool : public ToolItem
{
    Q_OBJECT

public:
    explicit StepTool(QGraphicsItem *parent = nullptr);
    ~StepTool() override = default;

    // QGraphicsItem interface
    QRectF boundingRect() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget = nullptr) override;
    QPainterPath shape() const override;

    // ITool interface for dynamic properties
    [[nodiscard]] QList<ToolProperty> getProperties() const override;
    void setProperty(const QString& propertyId, const QVariant& value) override;
    [[nodiscard]] QGraphicsItem* clone() const override;
    void applyStyleFrom(const ITool* other) override;   // copies color + diameter, never the number

    // Styling / content
    void setNumber(int number);
    [[nodiscard]] int number() const { return m_number; }
    void setColor(const QColor &color);
    [[nodiscard]] QColor color() const { return m_color; }
    void setDiameter(qreal diameter);
    [[nodiscard]] qreal diameter() const { return m_diameter; }

    // Template mode: the panel's number field becomes a one-shot override of the
    // number the next stamp would otherwise derive from the layer stack.
    void setTemplateMode(bool templateMode);
    bool takePendingOverride(int *out);

    static constexpr qreal DEFAULT_DIAMETER = 28.0;

private:
    [[nodiscard]] QRectF discRect() const;

    int m_number = 1;
    QColor m_color = Qt::red;
    qreal m_diameter = DEFAULT_DIAMETER;
    bool m_templateMode = false;
    bool m_overridePending = false;
};

} // namespace Editor::Tools

#endif // IMAGEEDITOR_STEPTOOL_H
