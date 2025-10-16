#include "BlurTool.h"
#include <QPainter>
#include <QPainterPath>
#include <QPainterPathStroker>
#include <QStyleOptionGraphicsItem>
#include <QGraphicsScene>
#include <QGraphicsBlurEffect>
#include <QImage>

namespace ImageEditor::Tools {

BlurTool::BlurTool(QGraphicsItem *parent)
    : QGraphicsObject(parent)
    , m_blurRadius(10.0)
    , m_brushWidth(30.0)
    , m_needsUpdate(true)
{
    setFlags(QGraphicsItem::ItemIsSelectable |
             QGraphicsItem::ItemSendsGeometryChanges |
             QGraphicsItem::ItemIsFocusable);

    setAcceptHoverEvents(true);
}

void BlurTool::addPoint(const QPointF &point)
{
    m_points.append(point);

    if (m_points.size() == 1) {
        m_path.moveTo(point);
    } else {
        m_path.lineTo(point);
    }

    m_needsUpdate = true;
    updateGeometry();
}

void BlurTool::finishPath()
{
    // Path is complete, generate final blurred pixmap
    if (!m_points.isEmpty()) {
        generateBlurredPixmap();
    }
    updateGeometry();
}

void BlurTool::updateGeometry()
{
    prepareGeometryChange();

    // Create stroke path for interaction
    m_strokePath = createStrokePath();

    // Calculate bounding rect
    QRectF bounds = m_strokePath.boundingRect();

    // Add padding for blur effect
    qreal padding = m_blurRadius + 10.0;
    bounds.adjust(-padding, -padding, padding, padding);
    m_boundingRect = bounds;

    update();
}

QPainterPath BlurTool::createStrokePath() const
{
    if (m_path.isEmpty()) {
        return QPainterPath();
    }

    QPainterPathStroker stroker;
    stroker.setCapStyle(Qt::RoundCap);
    stroker.setJoinStyle(Qt::RoundJoin);
    stroker.setWidth(qMax(m_brushWidth, 5.0));

    return stroker.createStroke(m_path);
}

void BlurTool::generateBlurredPixmap()
{
    if (m_sourcePixmap.isNull() || m_path.isEmpty()) {
        return;
    }

    // Get the bounding rect of the stroke path
    QRectF pathRect = m_strokePath.boundingRect();
    qreal padding = m_blurRadius * 3; // Extra padding for blur
    pathRect.adjust(-padding, -padding, padding, padding);

    // Ensure the rect is within the source pixmap bounds
    QRectF sourceRect = QRectF(0, 0, m_sourcePixmap.width(), m_sourcePixmap.height());
    pathRect = pathRect.intersected(sourceRect);

    if (pathRect.isEmpty()) {
        return;
    }

    // Convert to integer rect
    QRect intPathRect = pathRect.toRect();

    // Extract the region from source pixmap
    QPixmap regionPixmap = m_sourcePixmap.copy(intPathRect);
    QImage regionImage = regionPixmap.toImage();

    // Apply simple box blur
    QImage blurred = applyBoxBlur(regionImage, static_cast<int>(m_blurRadius));

    // Create a mask for the stroke path
    QImage mask(intPathRect.size(), QImage::Format_ARGB32_Premultiplied);
    mask.fill(Qt::transparent);

    QPainter maskPainter(&mask);
    maskPainter.setRenderHint(QPainter::Antialiasing);
    maskPainter.translate(-intPathRect.topLeft());
    maskPainter.fillPath(m_strokePath, Qt::white);
    maskPainter.end();

    // Apply the mask to the blurred image
    QImage maskedBlur(intPathRect.size(), QImage::Format_ARGB32_Premultiplied);
    maskedBlur.fill(Qt::transparent);

    for (int y = 0; y < maskedBlur.height(); ++y) {
        for (int x = 0; x < maskedBlur.width(); ++x) {
            QRgb maskPixel = mask.pixel(x, y);
            int alpha = qAlpha(maskPixel);
            if (alpha > 0) {
                QRgb blurPixel = blurred.pixel(x, y);
                maskedBlur.setPixel(x, y, blurPixel);
            }
        }
    }

    // Composite the blurred region onto the original image
    QImage result = m_sourcePixmap.toImage();
    QPainter resultPainter(&result);
    resultPainter.setRenderHint(QPainter::Antialiasing);
    resultPainter.drawImage(intPathRect.topLeft(), maskedBlur);
    resultPainter.end();

    m_blurredPixmap = QPixmap::fromImage(result);
    m_needsUpdate = false;
}

QImage BlurTool::applyBoxBlur(const QImage& source, int radius)
{
    if (radius <= 0) {
        return source;
    }

    QImage result = source.convertToFormat(QImage::Format_ARGB32);

    // Horizontal pass
    QImage temp(result.size(), QImage::Format_ARGB32);
    for (int y = 0; y < result.height(); ++y) {
        for (int x = 0; x < result.width(); ++x) {
            int r = 0, g = 0, b = 0, a = 0;
            int count = 0;

            for (int kx = -radius; kx <= radius; ++kx) {
                int px = qBound(0, x + kx, result.width() - 1);
                QRgb pixel = result.pixel(px, y);
                r += qRed(pixel);
                g += qGreen(pixel);
                b += qBlue(pixel);
                a += qAlpha(pixel);
                ++count;
            }

            temp.setPixel(x, y, qRgba(r / count, g / count, b / count, a / count));
        }
    }

    // Vertical pass
    for (int y = 0; y < temp.height(); ++y) {
        for (int x = 0; x < temp.width(); ++x) {
            int r = 0, g = 0, b = 0, a = 0;
            int count = 0;

            for (int ky = -radius; ky <= radius; ++ky) {
                int py = qBound(0, y + ky, temp.height() - 1);
                QRgb pixel = temp.pixel(x, py);
                r += qRed(pixel);
                g += qGreen(pixel);
                b += qBlue(pixel);
                a += qAlpha(pixel);
                ++count;
            }

            result.setPixel(x, y, qRgba(r / count, g / count, b / count, a / count));
        }
    }

    return result;
}

QRectF BlurTool::boundingRect() const
{
    return m_boundingRect;
}

QPainterPath BlurTool::shape() const
{
    return m_strokePath;
}

void BlurTool::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    Q_UNUSED(widget)

    if (m_path.isEmpty()) {
        return;
    }

    painter->setRenderHint(QPainter::Antialiasing);

    // If we have a blurred pixmap, draw it
    if (!m_blurredPixmap.isNull() && !m_needsUpdate) {
        painter->save();
        painter->setClipPath(m_strokePath);
        painter->drawPixmap(0, 0, m_blurredPixmap);
        painter->restore();
    } else {
        // During drawing, show a semi-transparent overlay to indicate blur area
        painter->fillPath(m_strokePath, QColor(128, 128, 128, 100));
    }

    // Draw selection indicator
    if (option->state & QStyle::State_Selected) {
        QPen selectionPen(Qt::blue, 1.0, Qt::DashLine);
        painter->setPen(selectionPen);
        painter->setBrush(Qt::NoBrush);
        painter->drawRect(m_path.boundingRect());
    }
}

void BlurTool::setBlurRadius(qreal radius)
{
    if (m_blurRadius != radius) {
        m_blurRadius = qBound(1.0, radius, 50.0);
        m_needsUpdate = true;
        if (!m_points.isEmpty()) {
            generateBlurredPixmap();
        }
        updateGeometry();
    }
}

void BlurTool::setBrushWidth(qreal width)
{
    if (m_brushWidth != width) {
        m_brushWidth = qBound(5.0, width, 100.0);
        updateGeometry();
    }
}

void BlurTool::setSourcePixmap(const QPixmap &pixmap)
{
    m_sourcePixmap = pixmap;
    m_needsUpdate = true;
}

QVariant BlurTool::itemChange(GraphicsItemChange change, const QVariant &value)
{
    if (change == ItemSelectedChange) {
        // Handle selection change if needed
    } else if (change == ItemPositionHasChanged) {
        emit pathChanged();
    }

    return QGraphicsObject::itemChange(change, value);
}

QList<ToolProperty> BlurTool::getProperties() const
{
    QList<ToolProperty> properties;

    ToolProperty radiusProp;
    radiusProp.id = "blurRadius";
    radiusProp.name = "Blur Strength";
    radiusProp.value = m_blurRadius;
    radiusProp.controlType = "slider";
    radiusProp.options["min"] = 1;
    radiusProp.options["max"] = 50;
    properties.append(radiusProp);

    ToolProperty widthProp;
    widthProp.id = "brushWidth";
    widthProp.name = "Brush Width";
    widthProp.value = m_brushWidth;
    widthProp.controlType = "slider";
    widthProp.options["min"] = 5;
    widthProp.options["max"] = 100;
    properties.append(widthProp);

    return properties;
}

void BlurTool::setProperty(const QString& propertyId, const QVariant& value)
{
    if (propertyId == "blurRadius" && value.canConvert<qreal>()) {
        setBlurRadius(value.toReal());
    } else if (propertyId == "brushWidth" && value.canConvert<qreal>()) {
        setBrushWidth(value.toReal());
    }
}

} // namespace ImageEditor::Tools
