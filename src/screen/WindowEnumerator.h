#ifndef SCREEN_WINDOWENUMERATOR_H
#define SCREEN_WINDOWENUMERATOR_H

#include <QVector>
#include <QRect>
#include <QtGlobal>

namespace Screen {

/**
 * One on-screen, user-visible window: its rectangle in VIRTUAL-DESKTOP LOGICAL
 * coordinates plus the platform window id (macOS CGWindowID, Windows HWND, the X11
 * client window; 0 where unavailable).
 * The id lets a recorder target that exact window (true window capture).
 */
struct WindowInfo {
    QRect   rect;
    quint64 id = 0;
};

/**
 * The on-screen windows ordered front-to-back (topmost first), so the first one
 * containing a point is the window under the cursor.
 *
 * Platform-specific: implemented natively on macOS (CGWindowList), Windows
 * (EnumWindows) and X11 (the window manager's client list, over XCB). Where there is
 * none, a Wayland session included, it returns an empty list, and callers fall back
 * to highlighting the screen under the cursor.
 */
QVector<WindowInfo> enumerateWindowInfos();

} // namespace Screen

#endif // SCREEN_WINDOWENUMERATOR_H
