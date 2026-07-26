#ifndef SCREEN_X11SCREENMAP_H
#define SCREEN_X11SCREENMAP_H

#include <QGuiApplication>
#include <QPoint>
#include <QRect>
#include <QScreen>
#include <QVector>
#include <QtGlobal>

namespace Screen::X11ScreenMap {

/**
 * Maps between X11 root-window pixels and Qt's VIRTUAL-DESKTOP LOGICAL space. Qt's xcb
 * scaling (Xft.dpi, QT_SCALE_FACTOR) keeps each screen's top-left at its native position
 * and scales only sizes and offsets within it, so every point goes through the screen it
 * lies on. Edges round to the nearest pixel both ways, as Qt rounds a screen's size.
 * The mapping is pure, so the scaled cases are tested on every host.
 */
struct Screen {
    QRect geometry;   // QScreen::geometry(), logical
    qreal dpr = 1.0;  // QScreen::devicePixelRatio()
};

// The root-window pixels a screen covers.
inline QRect nativeRect(const Screen &screen)
{
    return {screen.geometry.topLeft(), QSize(qRound(screen.geometry.width() * screen.dpr),
                                             qRound(screen.geometry.height() * screen.dpr))};
}

namespace detail {

inline int screenAt(const QPoint &p, const QVector<Screen> &screens)
{
    for (int i = 0; i < screens.size(); ++i)
        if (nativeRect(screens.at(i)).contains(p))
            return i;
    return -1;
}

inline QPoint mapEdge(int x, int y, const Screen &s)
{
    const QPoint origin = s.geometry.topLeft();
    return {origin.x() + qRound((x - origin.x()) / s.dpr),
            origin.y() + qRound((y - origin.y()) / s.dpr)};
}

} // namespace detail

/**
 * The logical rect for a root-window one, or an empty rect when it touches no screen.
 * Each corner maps through the screen it lies on; a corner off every screen uses the
 * screen holding most of the rect.
 */
inline QRect toLogical(const QRect &rootPx, const QVector<Screen> &screens)
{
    if (rootPx.isEmpty() || screens.isEmpty())
        return {};

    int dominant = -1;
    qint64 bestArea = 0;
    for (int i = 0; i < screens.size(); ++i) {
        if (screens.at(i).dpr <= 0.0)
            continue;
        const QRect overlap = nativeRect(screens.at(i)).intersected(rootPx);
        const qint64 area = qint64(overlap.width()) * overlap.height();
        if (area > bestArea) {
            bestArea = area;
            dominant = i;
        }
    }
    if (dominant < 0)
        return {};

    // QRect::bottomRight() is inclusive: look up the last pixel, map the exclusive edge.
    int first = detail::screenAt(rootPx.topLeft(), screens);
    int last = detail::screenAt(rootPx.bottomRight(), screens);
    if (first < 0 || screens.at(first).dpr <= 0.0)
        first = dominant;
    if (last < 0 || screens.at(last).dpr <= 0.0)
        last = dominant;

    const QPoint topLeft = detail::mapEdge(rootPx.x(), rootPx.y(), screens.at(first));
    const QPoint bottomRight = detail::mapEdge(rootPx.x() + rootPx.width(),
                                               rootPx.y() + rootPx.height(), screens.at(last));
    if (bottomRight.x() <= topLeft.x() || bottomRight.y() <= topLeft.y())
        return {};
    return {topLeft, QSize(bottomRight.x() - topLeft.x(), bottomRight.y() - topLeft.y())};
}

// Every screen Qt knows, as the mapping takes them. GUI thread only.
inline QVector<Screen> currentScreens()
{
    QVector<Screen> screens;
    const QList<QScreen *> all = QGuiApplication::screens();
    for (const QScreen *screen : all)
        screens.append(Screen{screen->geometry(), screen->devicePixelRatio()});
    return screens;
}

} // namespace Screen::X11ScreenMap

#endif // SCREEN_X11SCREENMAP_H
