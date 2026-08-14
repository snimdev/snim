#ifndef SCREEN_X11SCREENMAP_H
#define SCREEN_X11SCREENMAP_H

#include "screen/ScreenMap.h"

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

/**
 * The logical rect for a root-window one, or an empty rect when it touches no screen.
 * Each corner maps through the screen it lies on; a corner off every screen uses the
 * screen holding most of the rect.
 */
inline QRect toLogical(const QRect &rootPx, const QVector<Screen> &screens)
{
    QVector<ScreenMap::Screen> mapped;
    mapped.reserve(screens.size());
    for (const Screen &screen : screens)
        mapped.append({nativeRect(screen), screen.geometry.topLeft(), screen.dpr});
    return ScreenMap::toLogical(rootPx, mapped, ScreenMap::Rounding::Nearest);
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
