#ifndef CAPTURE_SCREENCOPYCLIENT_H
#define CAPTURE_SCREENCOPYCLIENT_H

#include "capture/ScreencopyGeometry.h"
#include <QList>
#include <QString>

namespace Capture::Screencopy {

/**
 * The Wayland side of native screencopy, the way grim does it: one shm frame per
 * wl_output through ext-image-copy-capture-v1 or zwlr_screencopy_manager_v1. It rides
 * on Qt's own wl_display with a private event queue, so it needs the wayland platform
 * plugin and never touches Qt's objects or queue.
 */

struct Globals {
    bool ext = false;   // both ext image-copy-capture managers
    bool wlr = false;
};

// What the compositor advertises to this app, probed once; all false off Wayland.
Globals advertisedGlobals();

// One upright frame per output; empty with *error set when any output fails.
QList<OutputFrame> captureOutputs(Protocol protocol, QString *error, int timeoutMs = 3000);

} // namespace Capture::Screencopy

#endif // CAPTURE_SCREENCOPYCLIENT_H
