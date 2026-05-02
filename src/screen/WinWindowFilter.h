#ifndef SCREEN_WINWINDOWFILTER_H
#define SCREEN_WINWINDOWFILTER_H

#include <QRect>
#include <QString>
#include <QtGlobal>

namespace Screen::WinWindowFilter {

/**
 * What the Windows window picker knows about one top-level window, read from
 * Win32 and DWM by WindowEnumerator_win. Plain data, so the filter below is
 * tested on every host.
 */
struct Candidate {
    bool visible = false;      // IsWindowVisible
    bool minimised = false;    // IsIconic
    bool cloaked = false;      // DWMWA_CLOAKED: other virtual desktops, suspended UWP apps
    bool toolWindow = false;   // WS_EX_TOOLWINDOW
    QString className;
    quint32 processId = 0;
    QRect bounds;              // DWMWA_EXTENDED_FRAME_BOUNDS, physical pixels
};

// Whether the picker offers this window: a visible, on-screen app window of another process.
inline bool isPickable(const Candidate &w, quint32 ownProcessId)
{
    if (!w.visible || w.minimised || w.cloaked || w.toolWindow)
        return false;
    // The desktop icons and the wallpaper, which span every screen.
    if (w.className == QLatin1String("Progman") || w.className == QLatin1String("WorkerW"))
        return false;
    if (w.processId == ownProcessId)
        return false;
    return !w.bounds.isEmpty();
}

} // namespace Screen::WinWindowFilter

#endif // SCREEN_WINWINDOWFILTER_H
