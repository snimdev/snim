#ifndef SCREEN_WINDOWENUMERATOR_H
#define SCREEN_WINDOWENUMERATOR_H

#include <QVector>
#include <QRect>
#include <QtGlobal>

namespace Screen {

/**
 * One on-screen, user-visible window: its rectangle in VIRTUAL-DESKTOP LOGICAL
 * coordinates plus the platform window id (macOS CGWindowID, Windows HWND; 0 where
 * unavailable).
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
 * Platform-specific: implemented natively on macOS (CGWindowList) and Windows
 * (EnumWindows). On platforms
 * without an implementation it returns an empty list, and callers fall back to
 * highlighting the screen under the cursor.
 */
QVector<WindowInfo> enumerateWindowInfos();

// Rect-only convenience for the screenshot window-pick path (which doesn't need
// the id). Single source of truth: derived from enumerateWindowInfos().
inline QVector<QRect> enumerateWindows()
{
    QVector<QRect> rects;
    const QVector<WindowInfo> infos = enumerateWindowInfos();
    rects.reserve(infos.size());
    for (const WindowInfo &w : infos)
        rects.append(w.rect);
    return rects;
}

} // namespace Screen

#endif // SCREEN_WINDOWENUMERATOR_H
