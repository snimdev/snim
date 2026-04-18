#ifndef CAPTURE_OVERLAYWINDOWS_H
#define CAPTURE_OVERLAYWINDOWS_H

#include <QtGlobal>

class QWidget;

namespace Capture {

/**
 * Facade over the native window-manager tweaks the capture overlays and recording
 * HUDs need beyond what Qt's window flags express. Callers make plain calls on every
 * platform; each platform supplies its own translation unit: OverlayWindows_mac.mm
 * (AppKit), OverlayWindows_win.cpp (topmost, DWM transitions, foreground) and
 * OverlayWindows_stub.cpp (no-ops where Qt's flags already suffice).
 */

/**
 * Turn an already-shown top-level widget into an instant, non-animated capture
 * overlay. macOS: raises it above the menu bar/Dock, disables the window appear
 * animation (so there's no Spaces transition or menu-bar slide), makes it span
 * all Spaces, and gives it keyboard focus. Windows: re-asserts topmost, turns off
 * DWM transitions and takes the foreground, even when opened from a global hotkey.
 * Call AFTER show().
 */
void configureOverlayWindow(QWidget *widget);

/**
 * Turn an already-shown top-level widget into a persistent floating HUD
 * (e.g. the recording Stop control). Unlike configureOverlayWindow it does NOT
 * activate the app or take key focus. macOS: sets the window to stay visible when
 * the app is deactivated, so it remains on screen while the user clicks other apps
 * during a recording, and spans all Spaces. Windows: re-asserts topmost without
 * activating. Call AFTER show().
 */
void configureRecordingHud(QWidget *widget);

/**
 * Float an already-shown control widget ABOVE the selection overlay without taking
 * key focus from it: the recording options bar lives here, clickable while Enter/Esc
 * still go to the overlay. macOS: shielding window level, spans all Spaces, no
 * animation. Call AFTER show(), after the overlay.
 * (On other platforms plain WindowStaysOnTopHint + show-order does the same job.)
 */
void configureSelectionHud(QWidget *widget);

/**
 * Keep an own top-level window out of screen recordings (and other apps' screen
 * captures) while it stays visible on screen. Windows: WDA_EXCLUDEFROMCAPTURE,
 * Windows 10 2004 or later. A no-op elsewhere: the macOS recorder already filters
 * out the whole app. The webcam bubble must never get this, so it is recorded.
 */
void excludeFromCapture(QWidget *widget);

/**
 * The native window id backing a shown top-level widget (macOS: the CGWindowID,
 * i.e. NSWindow windowNumber; Windows: the HWND), or 0 if unavailable. Used so the recorder can keep a
 * specific own-app window (the webcam bubble) in the capture while excluding the
 * rest of the app.
 */
quint64 nativeWindowId(QWidget *widget);

} // namespace Capture

#endif // CAPTURE_OVERLAYWINDOWS_H
