#include "StepTool.h"
#include <QPainter>
#include <QFont>
#include <QFontMetricsF>
#include <QStyleOptionGraphicsItem>

namespace Editor::Tools {

StepTool::StepTool(QGraphicsItem *parent)
    : QGraphicsObject(parent)
{
    setFlags(QGraphicsItem::ItemIsSelectable |
             QGraphicsItem::ItemIsMovable |
             QGraphicsItem::ItemSendsGeometryChanges |
             QGraphicsItem::ItemIsFocusable);

    setAcceptHoverEvents(true);
    setCursor(Qt::SizeAllCursor);
}

QRectF StepTool::discRect() const
{
    const qreal r = m_diameter / 2.0;
    return QRectF(-r, -r, m_diameter, m_diameter);
}

QRectF StepTool::boundingRect() const
{
    // Padded so the dashed selection rect isn't clipped.
    return discRect().adjusted(-2, -2, 2, 2);
}

QPainterPath StepTool::shape() const
{
    QPainterPath path;
    path.addEllipse(discRect());   // circular hit area, not the padded bounding rect
    return path;
}

void StepTool::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    Q_UNUSED(widget)

    painter->setRenderHint(QPainter::Antialiasing);

    const QRectF disc = discRect();

    painter->setPen(Qt::NoPen);
    painter->setBrush(m_color);
    painter->drawEllipse(disc);

    // Same contrast rule as the themed toolbar icons.
    const QColor textColor(m_color.lightness() < 128 ? "#ffffff" : "#202020");

    const QString text = QString::number(m_number);
    QFont font = painter->font();
    font.setBold(true);
    int pixelSize = qRound(m_diameter * 0.55);
    font.setPixelSize(pixelSize);
    while (pixelSize > 6 && QFontMetricsF(font).horizontalAdvance(text) > m_diameter * 0.75) {
        --pixelSize;
        font.setPixelSize(pixelSize);
    }
    painter->setFont(font);
    painter->setPen(textColor);
    painter->drawText(disc, Qt::AlignCenter, text);

    if (option->state & QStyle::State_Selected) {
        QPen selectionPen(Qt::blue, 1.0, Qt::DashLine);
        painter->setPen(selectionPen);
        painter->setBrush(Qt::NoBrush);
        painter->drawRect(disc);
    }
}

void StepTool::setNumber(int number)
{
    // Never touches the override flag; only setProperty() in template mode arms it.
    if (m_number != number) {
        m_number = number;
        update();
    }
}

void StepTool::setColor(const QColor &color)
{
    if (m_color != color) {
        m_color = color;
        update();
    }
}

void StepTool::setDiameter(qreal diameter)
{
    if (m_diameter != diameter) {
        prepareGeometryChange();
        m_diameter = diameter;
        update();
    }
}

void StepTool::setTemplateMode(bool templateMode)
{
    m_templateMode = templateMode;
}

bool StepTool::takePendingOverride(int *out)
{
    if (!m_overridePending)
        return false;
    if (out)
        *out = m_number;
    m_overridePending = false;
    return true;
}

QGraphicsItem* StepTool::clone() const
{
    auto* copy = new StepTool();
    copy->applyStyleFrom(this);
    copy->setNumber(m_number);   // a duplicate keeps its number
    return copy;
}

void StepTool::applyStyleFrom(const ITool* other)
{
    // Style only: commitDrawnItem() copies a committed badge's style back into the
    // template, and that must not clobber the template's next number.
    if (const auto* o = dynamic_cast<const StepTool*>(other)) {
        setColor(o->color());
        setDiameter(o->diameter());
    }
}

QList<ToolProperty> StepTool::getProperties() const
{
    QList<ToolProperty> properties;

    ToolProperty colorProp;
    colorProp.id = "color";
    colorProp.name = "Color";
    colorProp.value = m_color;
    colorProp.controlType = "color";
    properties.append(colorProp);

    ToolProperty sizeProp;
    sizeProp.id = "size";
    sizeProp.name = "Size";
    sizeProp.value = int(m_diameter);
    sizeProp.controlType = "slider";
    sizeProp.options["min"] = 16;
    sizeProp.options["max"] = 64;
    properties.append(sizeProp);

    ToolProperty numberProp;
    numberProp.id = "number";
    numberProp.name = m_templateMode ? "Next number" : "Number";
    numberProp.value = m_number;
    numberProp.controlType = "spinbox";
    numberProp.options["min"] = 1;
    numberProp.options["max"] = 999;
    properties.append(numberProp);

    return properties;
}

void StepTool::setProperty(const QString& propertyId, const QVariant& value)
{
    if (propertyId == "color" && value.canConvert<QColor>()) {
        setColor(value.value<QColor>());
    } else if (propertyId == "size" && value.canConvert<qreal>()) {
        setDiameter(value.toReal());
    } else if (propertyId == "number" && value.canConvert<int>()) {
        setNumber(value.toInt());
        if (m_templateMode)
            m_overridePending = true;   // one-shot: consumed by the next stamp
    }
}

} // namespace Editor::Tools
