#ifndef SCREEN_X11WINDOWS_H
#define SCREEN_X11WINDOWS_H

#include <QMargins>
#include <QRect>
#include <QSize>
#include <QVector>
#include <QtGlobal>

#include <optional>

namespace Screen::X11Windows {

/**
 * The X11 desktop's client windows, read over Qt's own XCB connection: nothing without
 * one (Wayland, offscreen). Root-window pixels throughout. GUI thread only.
 */
struct Window {
    quint64 id = 0;   // the client window
    QRect visible;    // what the user sees of it (X11WindowFilter::visibleRect)
};

// The windows the picker offers, topmost first.
[[nodiscard]] QVector<Window> pickableWindows();

// What recording a client window reads: the top-level holding it, its frame where the
// window manager adds one, so the decorations are recorded too.
struct Capture {
    quint64 window = 0;   // the top-level to read
    QSize size;           // its size
    QMargins crop;        // what of it is not the window: invisible borders, a drawn shadow
};

// Nothing when the client is gone or not shown.
[[nodiscard]] std::optional<Capture> capture(quint64 client);

// Whether the client is still open: listed by the window manager, which drops it once
// its program closes or withdraws it (a minimized window stays), or alive without one.
[[nodiscard]] bool isOpen(quint64 client);

} // namespace Screen::X11Windows

#endif // SCREEN_X11WINDOWS_H
