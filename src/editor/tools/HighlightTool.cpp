#include "HighlightTool.h"
#include <QPainter>
#include <QPainterPath>
#include <QPainterPathStroker>
#include <QStyleOptionGraphicsItem>

namespace ImageEditor::Tools {

HighlightTool::HighlightTool(QGraphicsItem *parent)
    : QGraphicsObject(parent)
    , m_width(HIGHLIGHT_WIDTH_MEDIUM) // Default to medium width
{
    // Initialize with default highlight color at 30% opacity
    QColor highlightColor(Qt::yellow);
    highlightColor.setAlphaF(HIGHLIGHT_OPACITY);

    m_pen = QPen(highlightColor, m_width, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);

    setFlags(QGraphicsItem::ItemIsSelectable |
             QGraphicsItem::ItemIsMovable |
             QGraphicsItem::ItemSendsGeometryChanges |
             QGraphicsItem::ItemIsFocusable);

    setAcceptHoverEvents(true);
    setCursor(Qt::SizeAllCursor);
}

void HighlightTool::addPoint(const QPointF &point)
{
    m_points.append(point);

    if (m_points.size() == 1) {
        m_path.moveTo(point);
    } else {
        m_path.lineTo(point);
    }

    updateGeometry();
}

void HighlightTool::finishPath()
{
    // Path is already complete, just update final geometry
    updateGeometry();
}

void HighlightTool::updateGeometry()
{
    prepareGeometryChange();

    // Create stroke path for interaction
    m_strokePath = createStrokePath();

    // Calculate bounding rect
    QRectF bounds = m_strokePath.boundingRect();

    // Add some padding
    qreal padding = 10.0;
    bounds.adjust(-padding, -padding, padding, padding);
    m_boundingRect = bounds;

    update();
}

QPainterPath HighlightTool::createStrokePath() const
{
    if (m_path.isEmpty()) {
        return QPainterPath();
    }

    QPainterPathStroker stroker;
    stroker.setCapStyle(m_pen.capStyle());
    stroker.setJoinStyle(m_pen.joinStyle());
    stroker.setWidth(m_width);

    return stroker.createStroke(m_path);
}

QRectF HighlightTool::boundingRect() const
{
    return m_boundingRect;
}

QPainterPath HighlightTool::shape() const
{
    return m_strokePath;
}

void HighlightTool::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    Q_UNUSED(widget)

    if (m_path.isEmpty()) {
        return;
    }

    painter->setRenderHint(QPainter::Antialiasing);

    // Draw the highlight path
    painter->setPen(m_pen);
    painter->setBrush(Qt::NoBrush);
    painter->drawPath(m_path);

    // Draw selection indicator
    if (option->state & QStyle::State_Selected) {
        QPen selectionPen(Qt::blue, 1.0, Qt::DashLine);
        painter->setPen(selectionPen);
        painter->setBrush(Qt::NoBrush);
        painter->drawRect(m_path.boundingRect());
    }
}

void HighlightTool::setColor(const QColor &color)
{
    QColor highlightColor = color;
    highlightColor.setAlphaF(HIGHLIGHT_OPACITY);

    QPen newPen = m_pen;
    newPen.setColor(highlightColor);
    m_pen = newPen;
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
    if (m_width != width) {
        m_width = width;
        QPen newPen = m_pen;
        newPen.setWidthF(width);
        m_pen = newPen;
        updateGeometry();
    }
}

QGraphicsItem* HighlightTool::clone() const
{
    auto* copy = new HighlightTool();
    copy->applyStyleFrom(this);
    for (const QPointF &p : m_points)
        copy->addPoint(p);
    copy->finishPath();
    return copy;
}

void HighlightTool::applyStyleFrom(const ITool* other)
{
    if (const auto* o = dynamic_cast<const HighlightTool*>(other)) {
        setColor(o->color());
        setWidth(o->width());
    }
}

QVariant HighlightTool::itemChange(GraphicsItemChange change, const QVariant &value)
{
    if (change == ItemSelectedChange) {
        // Handle selection change if needed
    } else if (change == ItemPositionHasChanged) {
        emit pathChanged();
    }

    return QGraphicsObject::itemChange(change, value);
}

QList<ToolProperty> HighlightTool::getProperties() const
{
    QList<ToolProperty> properties;

    ToolProperty colorProp;
    colorProp.id = "color";
    colorProp.name = "Highlight Color";
    colorProp.value = color(); // Get the base color without alpha
    colorProp.controlType = "color";
    properties.append(colorProp);

    // Add size property as dropdown
    ToolProperty sizeProp;
    sizeProp.id = "size";
    sizeProp.name = "Size";

    // Determine current size
    QString currentSize;
    if (qAbs(m_width - HIGHLIGHT_WIDTH_SMALL) < 0.1) {
        currentSize = "Small";
    } else if (qAbs(m_width - HIGHLIGHT_WIDTH_LARGE) < 0.1) {
        currentSize = "Large";
    } else {
        currentSize = "Medium";
    }

    sizeProp.value = currentSize;
    sizeProp.controlType = "dropdown";
    sizeProp.options["items"] = QStringList{"Small", "Medium", "Large"};
    properties.append(sizeProp);

    return properties;
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

} // namespace ImageEditor::Tools
