#ifndef RECORDING_RECORDINGGEOMETRY_H
#define RECORDING_RECORDINGGEOMETRY_H

#include <QRect>
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

} // namespace Recording

#endif // RECORDING_RECORDINGGEOMETRY_H
