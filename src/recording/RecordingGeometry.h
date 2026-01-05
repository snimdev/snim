#ifndef RECORDING_RECORDINGGEOMETRY_H
#define RECORDING_RECORDINGGEOMETRY_H

#include <QRect>
#include <QSize>
#include <QVector>

namespace Recording {

/**
 * Map a virtual-desktop logical rect to coordinates local to one display (origin at
 * that display's top-left). The macOS backend turns this into a ScreenCaptureKit
 * sourceRect. Kept here as a pure, platform-free function so it can be unit-tested
 * without ScreenCaptureKit.
 */
inline QRect displayLocalRect(const QRect &regionVirtual, const QRect &screenGeometry)
{
    return regionVirtual.translated(-screenGeometry.topLeft());
}

/**
 * The (up to four) strips of `outer` not covered by `hole` — top and bottom bands at
 * full width, left and right bands between them. They tile exactly: no overlaps, and
 * their union is outer minus hole. The recording-frame overlay fills them with the
 * dim color so the recorded region itself stays untouched. Pure, so it's unit-tested.
 */
inline QVector<QRect> surroundingRects(const QRect &outer, const QRect &holeIn)
{
    const QRect hole = holeIn.intersected(outer);
    if (hole.isEmpty())
        return outer.isEmpty() ? QVector<QRect>{} : QVector<QRect>{outer};

    QVector<QRect> strips;
    if (hole.top() > outer.top())                        // top band, full width
        strips.append(QRect(outer.left(), outer.top(),
                            outer.width(), hole.top() - outer.top()));
    if (hole.bottom() < outer.bottom())                  // bottom band, full width
        strips.append(QRect(outer.left(), hole.bottom() + 1,
                            outer.width(), outer.bottom() - hole.bottom()));
    if (hole.left() > outer.left())                      // left band, hole height
        strips.append(QRect(outer.left(), hole.top(),
                            hole.left() - outer.left(), hole.height()));
    if (hole.right() < outer.right())                    // right band, hole height
        strips.append(QRect(hole.right() + 1, hole.top(),
                            outer.right() - hole.right(), hole.height()));
    return strips;
}

struct StreamCrop {
    QRect cropPx;     // crop rect in stream pixels
    QSize outputPx;   // final encoded size
    bool  valid = false;
};

/**
 * Map a virtual-desktop logical rect onto one xdg-desktop-portal ScreenCast stream:
 * where to crop inside the stream's pixel buffer, and how large the encoded output
 * should be. `streamRectLogical` is the stream's place on the virtual desktop and
 * `streamSizePx` its real buffer size, so their ratio is the compositor's scale
 * (fractional under Wayland, 1.0 on X11 portals). Both sizes are rounded down to even
 * because H.264 4:2:0 cannot encode odd dimensions. Pure, so it's unit-tested without
 * a portal or GStreamer.
 */
inline StreamCrop portalStreamCrop(const QRect &regionVirtual,
                                   const QRect &streamRectLogical,
                                   const QSize &streamSizePx,
                                   bool retinaCapture)
{
    StreamCrop out;
    if (regionVirtual.isEmpty() || streamRectLogical.isEmpty()
        || streamSizePx.width() <= 0 || streamSizePx.height() <= 0)
        return out;

    const QRect inter = regionVirtual.intersected(streamRectLogical);
    if (inter.isEmpty())
        return out;

    const qreal scaleX = streamSizePx.width() / (qreal)streamRectLogical.width();
    const qreal scaleY = streamSizePx.height() / (qreal)streamRectLogical.height();

    const QRect local = inter.translated(-streamRectLogical.topLeft());
    QRect cropPx(qRound(local.x() * scaleX), qRound(local.y() * scaleY),
                 qRound(local.width() * scaleX), qRound(local.height() * scaleY));
    cropPx = cropPx.intersected(QRect(QPoint(0, 0), streamSizePx));

    QSize outputPx = retinaCapture ? cropPx.size() : inter.size();
    cropPx.setWidth(cropPx.width() & ~1);
    cropPx.setHeight(cropPx.height() & ~1);
    outputPx.setWidth(outputPx.width() & ~1);
    outputPx.setHeight(outputPx.height() & ~1);

    if (cropPx.width() < 2 || cropPx.height() < 2
        || outputPx.width() < 2 || outputPx.height() < 2)
        return out;

    out.cropPx = cropPx;
    out.outputPx = outputPx;
    out.valid = true;
    return out;
}

} // namespace Recording

#endif // RECORDING_RECORDINGGEOMETRY_H
