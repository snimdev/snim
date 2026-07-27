#include "BlurTool.h"
#include <QPainter>
#include <QPainterPath>
#include <QPainterPathStroker>
#include <QStyleOptionGraphicsItem>
#include <QGraphicsScene>
#include <QGraphicsBlurEffect>
#include <QImage>

namespace Editor::Tools {

BlurTool::BlurTool(QGraphicsItem *parent)
    : QGraphicsObject(parent)
    , m_blurRadius(10.0)
    , m_brushWidth(30.0)
    , m_needsUpdate(true)
{
    setFlags(QGraphicsItem::ItemIsSelectable |
             QGraphicsItem::ItemIsMovable |   // draggable like the other tools; it
             QGraphicsItem::ItemSendsGeometryChanges |  // re-blurs on move (see itemChange)
             QGraphicsItem::ItemIsFocusable);

    setAcceptHoverEvents(true);
    setCursor(Qt::SizeAllCursor);

    // When the item is dragged/nudged it must re-blur the pixels it now covers. The box
    // blur is expensive, so coalesce: restart this one-shot timer on every position change
    // and only regenerate once movement has settled (~50 ms idle).
    m_regenTimer = new QTimer(this);
    m_regenTimer->setSingleShot(true);
    m_regenTimer->setInterval(50);
    connect(m_regenTimer, &QTimer::timeout, this, [this] {
        if (!m_points.isEmpty() && !m_sourcePixmap.isNull()) {
            generateBlurredPixmap();
            update();
        }
    });
}

void BlurTool::scheduleRegeneration()
{
    if (m_regenTimer)
        m_regenTimer->start();   // (re)start; fires once the item stops moving
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
        m_blurredPixmap = QPixmap();
        m_needsUpdate = false;
        return;
    }

    // The source carries a devicePixelRatio (2x/3x on Retina) while the stroke path and the
    // scene are in *logical* coordinates, so map carefully between the two.
    const qreal dpr = m_sourcePixmap.devicePixelRatio() > 0.0
                          ? m_sourcePixmap.devicePixelRatio() : 1.0;
    // The item's origin in scene/source-logical coords. The screenshot sits at the scene
    // origin, so item-local L maps to source-logical (L + offset). Dragging the item shifts
    // `offset`, so the blur follows the pixels it now covers instead of relocating a stale patch.
    const QPointF offset = pos();

    // Region under the stroke in item-local logical coords, padded for the blur kernel.
    QRectF localRect = m_strokePath.boundingRect();
    const qreal padding = m_blurRadius * 3.0;
    localRect.adjust(-padding, -padding, padding, padding);

    // Same region in source-logical coords, clipped to the image.
    const QRectF srcLogical(0, 0, m_sourcePixmap.width() / dpr, m_sourcePixmap.height() / dpr);
    const QRectF sampleLogical = localRect.translated(offset).intersected(srcLogical);
    if (sampleLogical.isEmpty()) {
        m_blurredPixmap = QPixmap();
        m_needsUpdate = false;
        return;
    }

    // The device-pixel rect to copy out of the (HiDPI) source.
    const QRect sampleDevice =
        QRectF(sampleLogical.left()  * dpr, sampleLogical.top()    * dpr,
               sampleLogical.width() * dpr, sampleLogical.height() * dpr)
            .toRect()
            .intersected(QRect(QPoint(0, 0), m_sourcePixmap.size()));
    if (sampleDevice.isEmpty()) {
        m_blurredPixmap = QPixmap();
        m_needsUpdate = false;
        return;
    }

    // Extract the underlying pixels and blur them. Scale the kernel by dpr so the visual
    // blur strength is the same regardless of display density.
    QImage region = m_sourcePixmap.copy(sampleDevice).toImage()
                        .convertToFormat(QImage::Format_ARGB32);
    QImage blurred = applyBoxBlur(region, qMax(1, qRound(m_blurRadius * dpr)));
    blurred.setDevicePixelRatio(dpr);   // draw it back at logical size

    // Item-local top-left of the patch (where paint() blits it).
    m_blurPatchOffset = sampleLogical.topLeft() - offset;

    // Compose a DPR-tagged patch, masked to the stroke shape. The painter works in logical
    // coords (the image is DPR-tagged), so the logical stroke path clips it directly.
    QImage patch(sampleDevice.size(), QImage::Format_ARGB32_Premultiplied);
    patch.setDevicePixelRatio(dpr);
    patch.fill(Qt::transparent);
    {
        QPainter p(&patch);
        p.setRenderHint(QPainter::Antialiasing);
        p.translate(-m_blurPatchOffset);   // item-local logical -> patch-local logical
        p.setClipPath(m_strokePath);
        p.drawImage(m_blurPatchOffset, blurred);
    }

    m_blurredPixmap = QPixmap::fromImage(patch);
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
        painter->drawPixmap(m_blurPatchOffset, m_blurredPixmap);
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

QGraphicsItem* BlurTool::clone() const
{
    auto* copy = new BlurTool();
    copy->applyStyleFrom(this);
    copy->setSourcePixmap(m_sourcePixmap);
    for (const QPointF &p : m_points)
        copy->addPoint(p);
    copy->finishPath();
    return copy;
}

void BlurTool::applyStyleFrom(const ITool* other)
{
    if (const auto* o = dynamic_cast<const BlurTool*>(other)) {
        setBlurRadius(o->blurRadius());
        setBrushWidth(o->brushWidth());
    }
}

QVariant BlurTool::itemChange(GraphicsItemChange change, const QVariant &value)
{
    if (change == ItemPositionHasChanged) {
        // The blur sampled the pixels under its previous position; re-blur the pixels it
        // now covers so it keeps obscuring what's beneath it (coalesced via the timer).
        scheduleRegeneration();
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

} // namespace Editor::Tools
