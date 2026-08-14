#ifndef SCREEN_WINSCREENMAP_H
#define SCREEN_WINSCREENMAP_H

#include "screen/ScreenMap.h"

#include <QRect>
#include <QVector>

namespace Screen::WinScreenMap {

/**
 * Maps Win32 physical-pixel rects (DWMWA_EXTENDED_FRAME_BOUNDS) into Qt's
 * VIRTUAL-DESKTOP LOGICAL space. Under per-monitor DPI awareness Qt keeps each screen's
 * origin and divides only its extent by that screen's DPR. physical is the monitor rect
 * in physical pixels (rcMonitor).
 */
using Screen = ScreenMap::Screen;

// Each corner through its own screen, rounded outward so the result never clips the window.
inline QRect toLogical(const QRect &physical, const QVector<Screen> &screens)
{
    return ScreenMap::toLogical(physical, screens, ScreenMap::Rounding::Outward);
}

#ifdef Q_OS_WIN
// The native lookups, defined in WindowEnumerator_win.cpp. Qt's screens with their
// monitor rects (none off the windows platform plugin), and a window's (HWND) visible
// frame in physical pixels.
QVector<Screen> currentScreens();
QRect frameBounds(void *window);
#endif

} // namespace Screen::WinScreenMap

#endif // SCREEN_WINSCREENMAP_H
