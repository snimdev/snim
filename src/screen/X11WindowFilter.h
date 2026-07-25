#ifndef SCREEN_X11WINDOWFILTER_H
#define SCREEN_X11WINDOWFILTER_H

#include <QMargins>
#include <QRect>
#include <QtGlobal>

namespace Screen::X11WindowFilter {

/**
 * What the X11 window picker knows about one client window, read over XCB by
 * Screen::X11Windows. Plain data, so the filter below is tested on every host.
 */
struct Candidate {
    bool viewable = false;     // mapped, as are all its ancestors
    bool hidden = false;       // _NET_WM_STATE_HIDDEN: minimized or on another desktop
    bool desktopOrDock = false;   // _NET_WM_WINDOW_TYPE_DESKTOP or _DOCK: wallpaper, panels
    bool own = false;          // one of Snim's windows
    QRect bounds;              // visibleRect(), root-window pixels
};

// Whether the picker offers this window: a visible app window of another program.
inline bool isPickable(const Candidate &w)
{
    return w.viewable && !w.hidden && !w.desktopOrDock && !w.own && !w.bounds.isEmpty();
}

/**
 * The window as the user sees it, in root-window pixels: the client rect plus the
 * window manager's decorations (_NET_FRAME_EXTENTS), minus the invisible shadow a
 * client-side decorated window draws around itself (_GTK_FRAME_EXTENTS).
 */
inline QRect visibleRect(const QRect &client, const QMargins &frameExtents,
                         const QMargins &gtkFrameExtents)
{
    return client.marginsAdded(frameExtents).marginsRemoved(gtkFrameExtents);
}

// What to crop off a recorded window to keep only @p visible, both in root pixels.
inline QMargins cropMargins(const QRect &recorded, const QRect &visible)
{
    const QRect kept = visible.intersected(recorded);
    if (kept.isEmpty())
        return {};
    return {kept.left() - recorded.left(), kept.top() - recorded.top(),
            recorded.right() - kept.right(), recorded.bottom() - kept.bottom()};
}

} // namespace Screen::X11WindowFilter

#endif // SCREEN_X11WINDOWFILTER_H
