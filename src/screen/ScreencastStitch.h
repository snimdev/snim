#ifndef SCREEN_SCREENCASTSTITCH_H
#define SCREEN_SCREENCASTSTITCH_H

#include <QImage>
#include <QList>
#include <QPainter>
#include <QRect>
#include <QtMath>

namespace Screen {

/**
 * Pure stitching of ScreenCast portal frames into one virtual-desktop image, the same
 * shape every other strategy hands AreaSelector: a physical-pixel image tagged with
 * the highest DPR, whose origin is the virtual desktop's logical top-left.
 */

// One stream's frame and where the portal says it sits, in logical compositor pixels.
struct StreamFrame {
    QRect logical;   // null when the portal sent no position or size
    QImage image;
};

struct StitchedDesktop {
    QImage image;            // devicePixelRatio set
    QRect virtualGeometry;   // logical union of the streams
};

// Streams without geometry: a lone one is taken to be the whole desktop, several are
// laid out left to right at DPR 1 after the known ones.
inline QList<StreamFrame> resolveStreamGeometry(QList<StreamFrame> frames,
                                                const QRect &fallbackDesktop)
{
    if (frames.size() == 1 && frames.first().logical.isEmpty() && !fallbackDesktop.isEmpty()) {
        frames.first().logical = fallbackDesktop;
        return frames;
    }

    QRect known;
    for (const StreamFrame &frame : std::as_const(frames))
        known = known.united(frame.logical);

    int nextX = known.isEmpty() ? 0 : known.right() + 1;
    const int top = known.isEmpty() ? 0 : known.top();
    for (StreamFrame &frame : frames) {
        if (!frame.logical.isEmpty())
            continue;
        frame.logical = QRect(QPoint(nextX, top), frame.image.size());
        nextX += frame.image.width();
    }
    return frames;
}

inline StitchedDesktop stitchStreams(const QList<StreamFrame> &input, const QRect &fallbackDesktop)
{
    const QList<StreamFrame> frames = resolveStreamGeometry(input, fallbackDesktop);

    QRect virtualGeometry;
    qreal dpr = 1.0;
    for (const StreamFrame &frame : frames) {
        if (frame.image.isNull() || frame.logical.isEmpty())
            continue;
        virtualGeometry = virtualGeometry.united(frame.logical);
        dpr = qMax(dpr, qreal(frame.image.width()) / frame.logical.width());
    }
    if (virtualGeometry.isEmpty())
        return {};

    const QSize physical(qCeil(virtualGeometry.width() * dpr - 0.001),
                         qCeil(virtualGeometry.height() * dpr - 0.001));
    QImage result(physical, QImage::Format_RGB32);
    result.fill(Qt::black);

    QPainter painter(&result);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    for (const StreamFrame &frame : frames) {
        if (frame.image.isNull() || frame.logical.isEmpty())
            continue;
        const QRectF target((frame.logical.x() - virtualGeometry.x()) * dpr,
                            (frame.logical.y() - virtualGeometry.y()) * dpr,
                            frame.logical.width() * dpr, frame.logical.height() * dpr);
        // Same-DPR frames land 1:1; only a lower-DPR monitor gets scaled up.
        if (target.size().toSize() == frame.image.size())
            painter.drawImage(target.topLeft(), frame.image);
        else
            painter.drawImage(target, frame.image);
    }
    painter.end();

    result.setDevicePixelRatio(dpr);
    return {result, virtualGeometry};
}

} // namespace Screen

#endif // SCREEN_SCREENCASTSTITCH_H
