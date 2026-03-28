#ifndef CAPTURE_WINSCREENMAP_H
#define CAPTURE_WINSCREENMAP_H

#include <QPoint>
#include <QRect>
#include <QVector>
#include <QtMath>

namespace Capture::WinScreenMap {

/**
 * Maps Win32 physical-pixel rects (DWMWA_EXTENDED_FRAME_BOUNDS) into Qt's
 * VIRTUAL-DESKTOP LOGICAL space. Pure and platform-neutral, so the mixed-DPI
 * cases are tested on every host. Under per-monitor DPI awareness Qt keeps each
 * screen's origin and divides only its extent by that screen's DPR, so logical
 * space is not one uniform scale of physical space: each point has to go through
 * the screen it lies on.
 */
struct Screen {
    QRect physical;        // the monitor rect in physical pixels (rcMonitor)
    QPoint logicalOrigin;  // QScreen::geometry().topLeft()
    qreal dpr = 1.0;
};

namespace detail {

inline int screenAt(const QPoint &p, const QVector<Screen> &screens)
{
    for (int i = 0; i < screens.size(); ++i)
        if (screens.at(i).physical.contains(p))
            return i;
    return -1;
}

inline QPointF mapPoint(qreal x, qreal y, const Screen &s)
{
    return QPointF(s.logicalOrigin.x() + (x - s.physical.x()) / s.dpr,
                   s.logicalOrigin.y() + (y - s.physical.y()) / s.dpr);
}

} // namespace detail

/**
 * The logical rect for a physical one, or an empty rect when it touches no screen.
 * Each corner maps through the screen it lies on (so a window straddling a 100% and
 * a 150% monitor lines up on both); a corner off every screen uses the screen that
 * holds most of the rect. Rounded outward so the result never clips the window.
 */
inline QRect toLogical(const QRect &physical, const QVector<Screen> &screens)
{
    if (physical.isEmpty() || screens.isEmpty())
        return QRect();

    int dominant = -1;
    qint64 bestArea = 0;
    for (int i = 0; i < screens.size(); ++i) {
        const QRect overlap = screens.at(i).physical.intersected(physical);
        const qint64 area = qint64(overlap.width()) * overlap.height();
        if (area > bestArea) {
            bestArea = area;
            dominant = i;
        }
    }
    if (dominant < 0)
        return QRect();

    // QRect::bottomRight() is inclusive: look up the last pixel, map the exclusive edge.
    int first = detail::screenAt(physical.topLeft(), screens);
    int last = detail::screenAt(physical.bottomRight(), screens);
    if (first < 0)
        first = dominant;
    if (last < 0)
        last = dominant;

    const QPointF topLeft = detail::mapPoint(physical.x(), physical.y(), screens.at(first));
    const QPointF bottomRight = detail::mapPoint(physical.x() + physical.width(),
                                                 physical.y() + physical.height(),
                                                 screens.at(last));
    const QPoint tl(qFloor(topLeft.x()), qFloor(topLeft.y()));
    const QPoint br(qCeil(bottomRight.x()), qCeil(bottomRight.y()));
    if (br.x() <= tl.x() || br.y() <= tl.y())
        return QRect();
    return QRect(tl, QSize(br.x() - tl.x(), br.y() - tl.y()));
}

} // namespace Capture::WinScreenMap

#endif // CAPTURE_WINSCREENMAP_H
