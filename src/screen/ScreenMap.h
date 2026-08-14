#ifndef SCREEN_SCREENMAP_H
#define SCREEN_SCREENMAP_H

#include <QPoint>
#include <QRect>
#include <QVector>
#include <QtMath>

namespace Screen::ScreenMap {

/**
 * Maps native pixel rects into Qt's VIRTUAL-DESKTOP LOGICAL space: the one algorithm behind
 * X11ScreenMap and WinScreenMap. Qt keeps each screen's origin and scales only the extent
 * within it, so logical space is no single scale of native space and every point goes
 * through the screen it lies on. Pure, so the mixed-scale cases are tested on every host.
 */
struct Screen {
    QRect physical;        // the native pixels the screen covers
    QPoint logicalOrigin;  // QScreen::geometry().topLeft()
    qreal dpr = 1.0;
};

enum class Rounding {
    Nearest,   // X11: edges land where Qt rounds a screen's size
    Outward,   // Windows: the result never clips the window
};

namespace detail {

inline int screenAt(const QPoint &p, const QVector<Screen> &screens)
{
    for (int i = 0; i < screens.size(); ++i)
        if (screens.at(i).physical.contains(p))
            return i;
    return -1;
}

// Only the offset is rounded, so a negative origin rounds like a positive one.
inline QPoint mapCorner(const QPoint &native, const Screen &s, Rounding rounding, bool farCorner)
{
    const auto round = [rounding, farCorner](qreal offset) {
        if (rounding == Rounding::Nearest)
            return qRound(offset);
        return farCorner ? qCeil(offset) : qFloor(offset);
    };
    return s.logicalOrigin + QPoint(round((native.x() - s.physical.x()) / s.dpr),
                                    round((native.y() - s.physical.y()) / s.dpr));
}

} // namespace detail

/**
 * The logical rect for a native one, or an empty rect when it touches no screen. Each
 * corner maps through the screen it lies on; a corner off every screen uses the screen
 * holding most of the rect. Screens without a scale are skipped.
 */
inline QRect toLogical(const QRect &physical, const QVector<Screen> &screens, Rounding rounding)
{
    if (physical.isEmpty() || screens.isEmpty())
        return {};

    int dominant = -1;
    qint64 bestArea = 0;
    for (int i = 0; i < screens.size(); ++i) {
        if (screens.at(i).dpr <= 0.0)
            continue;
        const QRect overlap = screens.at(i).physical.intersected(physical);
        const qint64 area = qint64(overlap.width()) * overlap.height();
        if (area > bestArea) {
            bestArea = area;
            dominant = i;
        }
    }
    if (dominant < 0)
        return {};

    // QRect::bottomRight() is inclusive: look up the last pixel, map the exclusive edge.
    int first = detail::screenAt(physical.topLeft(), screens);
    int last = detail::screenAt(physical.bottomRight(), screens);
    if (first < 0 || screens.at(first).dpr <= 0.0)
        first = dominant;
    if (last < 0 || screens.at(last).dpr <= 0.0)
        last = dominant;

    const QPoint topLeft = detail::mapCorner(physical.topLeft(), screens.at(first), rounding, false);
    const QPoint bottomRight = detail::mapCorner(
        QPoint(physical.x() + physical.width(), physical.y() + physical.height()), screens.at(last),
        rounding, true);
    if (bottomRight.x() <= topLeft.x() || bottomRight.y() <= topLeft.y())
        return {};
    return {topLeft, QSize(bottomRight.x() - topLeft.x(), bottomRight.y() - topLeft.y())};
}

} // namespace Screen::ScreenMap

#endif // SCREEN_SCREENMAP_H
