#include "screen/OverlayWindows.h"

namespace Screen {

// Platforms whose overlays need nothing beyond Qt's own window flags.
void configureOverlayWindow(QWidget *) {}
void configureRecordingHud(QWidget *) {}
void configureSelectionHud(QWidget *) {}
void excludeFromCapture(QWidget *) {}

quint64 nativeWindowId(QWidget *)
{
    return 0;
}

} // namespace Screen
