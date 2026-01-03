#ifndef CAPTURE_MACOVERLAY_H
#define CAPTURE_MACOVERLAY_H

#include <QtGlobal>

class QWidget;

namespace Capture {

/**
 * macOS only: turn an already-shown top-level widget into an instant, non-animated
 * capture overlay. Raises it above the menu bar/Dock, disables the window
 * appear animation (so there's no Spaces transition or menu-bar slide), makes it
 * span all Spaces, and gives it keyboard focus. Call AFTER show().
 */
void configureOverlayWindow(QWidget *widget);

/**
 * macOS only: turn an already-shown top-level widget into a persistent floating HUD
 * (e.g. the recording Stop control). Unlike configureOverlayWindow it does NOT
 * activate the app or take key focus, and it sets the window to stay visible when
 * the app is deactivated, so it remains on screen while the user clicks other apps
 * during a recording. Spans all Spaces. Call AFTER show().
 */
void configureRecordingHud(QWidget *widget);

/**
 * macOS only: float an already-shown control widget ABOVE the selection overlay
 * (which sits at the shielding window level), without taking key focus from it —
 * the recording options bar lives here: clickable while Enter/Esc still go to the
 * overlay. Spans all Spaces, no animation. Call AFTER show(), after the overlay.
 * (On other platforms plain WindowStaysOnTopHint + show-order does the same job.)
 */
void configureSelectionHud(QWidget *widget);

/**
 * macOS only: the CGWindowID (NSWindow windowNumber) backing a shown top-level
 * widget, or 0 if unavailable. Used so the recorder can keep a specific own-app
 * window (the webcam bubble) in the capture while excluding the rest of the app.
 */
quint64 nativeWindowId(QWidget *widget);

} // namespace Capture

#endif // CAPTURE_MACOVERLAY_H
