#ifndef RECORDING_RECORDINGGEOMETRY_H
#define RECORDING_RECORDINGGEOMETRY_H

#include "screen/X11ScreenMap.h"

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
 * The (up to four) strips of `outer` not covered by `hole`: top and bottom bands at
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
 * (fractional under Wayland; on X11 the screen's Qt scale, once the root-window rect the
 * portal reports is mapped to logical). Both sizes are rounded down to even
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

struct StreamSource {
    QRect rectLogical;   // screen or virtual desktop rect in logical coords
    QSize sizePx;        // its pixel size (logical size * device pixel ratio)
};

/**
 * Work out which display a portal ScreenCast stream shows, from its pixel size alone.
 * Some portals (KDE's) never send position/size stream metadata, so the negotiated
 * PipeWire caps are the only clue about what was shared. Pure, so it's unit-tested.
 */
inline QRect resolveStreamRect(const QSize &capsPx,
                               const QVector<StreamSource> &screens,
                               const StreamSource &virtualDesktop,
                               const QRect &regionVirtual)
{
    if (capsPx.isEmpty())
        return {};

    if (capsPx == virtualDesktop.sizePx)
        return virtualDesktop.rectLogical;

    QVector<QRect> matches;
    for (const StreamSource &screen : screens) {
        if (screen.sizePx == capsPx)
            matches.append(screen.rectLogical);
    }

    // Identical monitors are indistinguishable by size, but the user just picked a source
    // in the portal dialog for the selection they made, so the screen holding it wins.
    if (!matches.isEmpty()) {
        for (const QRect &rect : matches) {
            if (rect.contains(regionVirtual.center()))
                return rect;
        }
        return matches.constFirst();
    }

    for (const StreamSource &screen : screens) {
        if (screen.rectLogical.contains(regionVirtual.center()))
            return screen.rectLogical;
    }
    return {};
}

using X11Screen = Screen::X11ScreenMap::Screen;

struct X11Grab {
    QRect rootPx;     // area of the X11 root window to read, in physical pixels
    QSize outputPx;   // final encoded size
    bool  valid = false;
};

/**
 * Map a virtual-desktop logical rect onto the X11 root window for ximagesrc, the forward
 * direction of Screen::X11ScreenMap: every screen the rect touches maps its own piece
 * with its own ratio and the grab is their bounding box. Without retinaCapture the output keeps the
 * logical size. Sizes round down to even for H.264 4:2:0. Pure, so it's unit-tested.
 */
inline X11Grab x11Grab(const QRect &regionVirtual, const QVector<X11Screen> &screens,
                       bool retinaCapture)
{
    X11Grab out;
    QRect rootPx;
    QRect logical;
    for (const X11Screen &screen : screens) {
        const QRect inter = regionVirtual.intersected(screen.geometry);
        if (inter.isEmpty() || screen.dpr <= 0.0)
            continue;

        const QPoint origin = screen.geometry.topLeft();
        const QRect local = inter.translated(-origin);
        // Edges, not sizes, are scaled so neighbouring pieces meet without a seam.
        const int left = qRound(local.left() * screen.dpr);
        const int top = qRound(local.top() * screen.dpr);
        const int right = qRound((local.left() + local.width()) * screen.dpr);
        const int bottom = qRound((local.top() + local.height()) * screen.dpr);
        const QRect piece = QRect(origin + QPoint(left, top), QSize(right - left, bottom - top))
                                .intersected(Screen::X11ScreenMap::nativeRect(screen));
        if (piece.isEmpty())
            continue;
        rootPx = rootPx.united(piece);
        logical = logical.united(inter);
    }

    QSize outputPx = retinaCapture ? rootPx.size() : logical.size();
    rootPx.setWidth(rootPx.width() & ~1);
    rootPx.setHeight(rootPx.height() & ~1);
    outputPx.setWidth(outputPx.width() & ~1);
    outputPx.setHeight(outputPx.height() & ~1);

    if (rootPx.width() < 2 || rootPx.height() < 2
        || outputPx.width() < 2 || outputPx.height() < 2)
        return out;

    out.rootPx = rootPx;
    out.outputPx = outputPx;
    out.valid = true;
    return out;
}

struct FrameRing {
    QRect window;            // ring window, in the coordinates of the inputs
    QVector<QRect> strips;   // the border itself, local to window
    bool  valid = false;
};

/**
 * The X11 recording frame: a window just large enough for a @p borderWidth border
 * around @p hole, clamped to @p screen, and the strips of it the border covers (the
 * window minus the hole, window-local), which the window's shape is cut to. Invalid
 * when no border fits, or when the window would cover the whole screen: window
 * managers unredirect full-screen windows from the compositor. Pure, so it's unit-tested.
 */
inline FrameRing x11FrameRing(const QRect &hole, const QRect &screen, int borderWidth)
{
    FrameRing out;
    const QRect inner = hole.intersected(screen);
    if (inner.isEmpty() || borderWidth <= 0)
        return out;

    const QRect window = inner.adjusted(-borderWidth, -borderWidth, borderWidth, borderWidth)
                             .intersected(screen);
    if (window == inner || window == screen)
        return out;

    out.window = window;
    out.strips = surroundingRects(QRect(QPoint(0, 0), window.size()),
                                  inner.translated(-window.topLeft()));
    out.valid = true;
    return out;
}

/**
 * Keep the webcam bubble wholly inside @p bounds: clamp @p proposedTopLeft for a bubble
 * of @p bubbleSize. Both are in the same coordinate space (screen-local on Wayland,
 * where the bubble is a circle inside a full-screen layer surface). A bubble larger
 * than the bounds pins to their top-left instead of taking a negative position.
 * Pure, so it's unit-tested without a compositor.
 */
inline QPoint clampBubbleTopLeft(const QRect &bounds, const QPoint &proposedTopLeft,
                                 const QSize &bubbleSize)
{
    QPoint p = proposedTopLeft;
    p.setX(qBound(bounds.left(), p.x(),
                  qMax(bounds.left(), bounds.left() + bounds.width() - bubbleSize.width())));
    p.setY(qBound(bounds.top(), p.y(),
                  qMax(bounds.top(), bounds.top() + bounds.height() - bubbleSize.height())));
    return p;
}

/**
 * Where the webcam bubble parks for a recorded region: the bottom-left corner inside
 * the region, expressed in coordinates local to @p screenGeometry and clamped so the
 * bubble stays wholly on that screen. Screen-local because a Wayland layer surface is
 * positioned by margins against its own output, never in virtual-desktop coordinates.
 * Pure, so it's unit-tested without a compositor.
 */
inline QPoint bubbleParkPos(const QRect &screenGeometry, const QRect &regionVirtual,
                            const QSize &bubbleSize, int margin)
{
    // qMax keeps a region shorter than the bubble pinned to its own top edge rather
    // than pushing the bubble out above it.
    const int x = regionVirtual.left() + margin;
    const int y = qMax(regionVirtual.top(),
                       regionVirtual.bottom() - bubbleSize.height() - margin);
    const QPoint local = QPoint(x, y) - screenGeometry.topLeft();
    return clampBubbleTopLeft(QRect(QPoint(0, 0), screenGeometry.size()), local, bubbleSize);
}

} // namespace Recording

#endif // RECORDING_RECORDINGGEOMETRY_H
