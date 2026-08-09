#include "HighlightTool.h"
#include <QPainter>

namespace Editor::Tools {

HighlightTool::HighlightTool(QGraphicsItem *parent)
    : PathTool(parent)
{
    QColor highlightColor(Qt::yellow);
    highlightColor.setAlphaF(HIGHLIGHT_OPACITY);
    m_pen = QPen(highlightColor, HIGHLIGHT_WIDTH_MEDIUM, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
}

void HighlightTool::paintPath(QPainter *painter)
{
    painter->setPen(m_pen);
    painter->setBrush(Qt::NoBrush);
    painter->drawPath(m_path);
}

void HighlightTool::setColor(const QColor &color)
{
    QColor highlightColor = color;
    highlightColor.setAlphaF(HIGHLIGHT_OPACITY);
    m_pen.setColor(highlightColor);
    updateGeometry();
}

QColor HighlightTool::color() const
{
    QColor c = m_pen.color();
    // Return the color without the alpha channel (return the base color)
    c.setAlphaF(1.0);
    return c;
}

void HighlightTool::setWidth(qreal width)
{
    if (m_pen.widthF() != width) {
        m_pen.setWidthF(width);
        updateGeometry();
    }
}

QGraphicsItem* HighlightTool::clone() const
{
    auto* copy = new HighlightTool();
    copy->applyStyleFrom(this);
    copy->addPoints(points());
    return copy;
}

void HighlightTool::applyStyleFrom(const ITool* other)
{
    if (const auto* o = dynamic_cast<const HighlightTool*>(other)) {
        setColor(o->color());
        setWidth(o->width());
    }
}

QList<ToolProperty> HighlightTool::getProperties() const
{
    const QString size = qAbs(width() - HIGHLIGHT_WIDTH_SMALL) < 0.1   ? QStringLiteral("Small")
                         : qAbs(width() - HIGHLIGHT_WIDTH_LARGE) < 0.1 ? QStringLiteral("Large")
                                                                       : QStringLiteral("Medium");
    return {
        {"color", "Highlight Color", color(), "color"},   // without the alpha
        {"size", "Size", size, "dropdown", {{"items", QStringList{"Small", "Medium", "Large"}}}},
    };
}

void HighlightTool::setProperty(const QString& propertyId, const QVariant& value)
{
    if (propertyId == "color" && value.canConvert<QColor>()) {
        setColor(value.value<QColor>());
    } else if (propertyId == "size" && value.canConvert<QString>()) {
        QString size = value.toString();
        if (size == "Small") {
            setWidth(HIGHLIGHT_WIDTH_SMALL);
        } else if (size == "Large") {
            setWidth(HIGHLIGHT_WIDTH_LARGE);
        } else {
            setWidth(HIGHLIGHT_WIDTH_MEDIUM);
        }
    }
}

} // namespace Editor::Tools
