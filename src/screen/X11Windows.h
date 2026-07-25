#ifndef SCREEN_X11WINDOWS_H
#define SCREEN_X11WINDOWS_H

#include <QRect>
#include <QVector>
#include <QtGlobal>

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

} // namespace Screen::X11Windows

#endif // SCREEN_X11WINDOWS_H
