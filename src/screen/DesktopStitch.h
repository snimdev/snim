#ifndef SCREEN_DESKTOPSTITCH_H
#define SCREEN_DESKTOPSTITCH_H

#include <QImage>
#include <QList>
#include <QPainter>
#include <QRect>
#include <QtMath>

namespace Screen {

/**
 * Pure stitching of per-screen frames into the one virtual-desktop image every frame
 * source hands the selectors: physical pixels tagged with the densest frame's DPR,
 * whose origin is the virtual desktop's logical top-left. Header-only and free of
 * platform code, so KWin, ScreenCast, screencopy and the tests share one copy.
 */

// One screen's frame and the logical rect it shows.
struct PlacedFrame {
    QRect logical;   // empty when the source sent no geometry
    QImage image;
};

// Frames a source sent without geometry: a lone one is taken to be the whole desktop,
// several are laid out left to right at DPR 1 after the known ones.
inline QList<PlacedFrame> placeFramesWithoutGeometry(QList<PlacedFrame> frames,
                                                     const QRect &fallbackDesktop)
{
    if (frames.size() == 1 && frames.first().logical.isEmpty() && !fallbackDesktop.isEmpty()) {
        frames.first().logical = fallbackDesktop;
        return frames;
    }

    QRect known;
    for (const PlacedFrame &frame : std::as_const(frames))
        known = known.united(frame.logical);

    int nextX = known.isEmpty() ? 0 : known.right() + 1;
    const int top = known.isEmpty() ? 0 : known.top();
    for (PlacedFrame &frame : frames) {
        if (!frame.logical.isEmpty())
            continue;
        frame.logical = QRect(QPoint(nextX, top), frame.image.size());
        nextX += frame.image.width();
    }
    return frames;
}

// The logical union of the frames that have pixels.
inline QRect coveredGeometry(const QList<PlacedFrame> &frames)
{
    QRect covered;
    for (const PlacedFrame &frame : frames) {
        if (!frame.image.isNull())
            covered = covered.united(frame.logical);
    }
    return covered;
}

// A frame's own DPR. The smaller axis wins: compositors truncate fractional logical sizes.
inline qreal frameDevicePixelRatio(const QImage &image, const QRect &logical)
{
    if (image.isNull() || logical.width() <= 0 || logical.height() <= 0)
        return 0.0;
    return qMin(image.width() / qreal(logical.width()), image.height() / qreal(logical.height()));
}

/**
 * Every frame at its logical place, at the densest frame's DPR, uncovered areas black.
 * Frames at that DPR are copied pixel for pixel, sparser ones scaled up. Null when no
 * frame has both pixels and a geometry, or virtualGeometry is empty.
 */
inline QImage stitchDesktop(const QList<PlacedFrame> &frames, const QRect &virtualGeometry)
{
    qreal dpr = 0.0;
    for (const PlacedFrame &frame : frames)
        dpr = qMax(dpr, frameDevicePixelRatio(frame.image, frame.logical));
    if (dpr <= 0.0 || virtualGeometry.isEmpty())
        return {};

    struct Placement {
        QRectF target;
        const QImage *image;
    };
    QList<Placement> placements;
    QRectF extent(0, 0, virtualGeometry.width() * dpr, virtualGeometry.height() * dpr);
    for (const PlacedFrame &frame : frames) {
        if (frame.image.isNull() || frame.logical.isEmpty())
            continue;
        const QPointF origin((frame.logical.x() - virtualGeometry.x()) * dpr,
                             (frame.logical.y() - virtualGeometry.y()) * dpr);
        const bool native = qAbs(frameDevicePixelRatio(frame.image, frame.logical) - dpr) < 0.01;
        const QRectF target = native ? QRectF(origin.toPoint(), QSizeF(frame.image.size()))
                                     : QRectF(origin, QSizeF(frame.logical.size()) * dpr);
        placements.append({target, &frame.image});
        extent |= target;
    }

    const QSize size(qCeil(extent.right() - 0.001), qCeil(extent.bottom() - 0.001));
    // A lone native frame that is the whole canvas needs no repaint.
    if (placements.size() == 1 && placements.first().target == QRectF(QPointF(), QSizeF(size))) {
        QImage whole = *placements.first().image;
        whole.setDevicePixelRatio(dpr);
        return whole;
    }

    QImage canvas(size, QImage::Format_RGB32);
    canvas.fill(Qt::black);
    QPainter painter(&canvas);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    for (const Placement &placement : std::as_const(placements)) {
        if (placement.target.size() == QSizeF(placement.image->size()))
            painter.drawImage(placement.target.topLeft().toPoint(), *placement.image);
        else
            painter.drawImage(placement.target, *placement.image);
    }
    painter.end();

    canvas.setDevicePixelRatio(dpr);
    return canvas;
}

} // namespace Screen

#endif // SCREEN_DESKTOPSTITCH_H
