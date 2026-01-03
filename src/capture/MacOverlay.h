#ifndef CAPTURE_MACOVERLAY_H
#define CAPTURE_MACOVERLAY_H

class QWidget;

namespace Capture {

/**
 * macOS only: turn an already-shown top-level widget into an instant, non-animated
 * capture overlay. Raises it above the menu bar/Dock, disables the window
 * appear animation (so there's no Spaces transition or menu-bar slide), makes it
 * span all Spaces, and gives it keyboard focus. Call AFTER show().
 */
void configureOverlayWindow(QWidget *widget);

} // namespace Capture

#endif // CAPTURE_MACOVERLAY_H
