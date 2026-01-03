#ifndef CAPTURE_WINDOWENUMERATOR_H
#define CAPTURE_WINDOWENUMERATOR_H

#include <QVector>
#include <QRect>

namespace Capture {

/**
 * Returns the on-screen, user-visible window rectangles in VIRTUAL-DESKTOP
 * LOGICAL coordinates, ordered front-to-back (topmost first) so the first rect
 * containing a point is the window under the cursor.
 *
 * Platform-specific: implemented natively on macOS (CGWindowList). On platforms
 * without an implementation it returns an empty list, and callers fall back to
 * highlighting the screen under the cursor.
 */
QVector<QRect> enumerateWindows();

} // namespace Capture

#endif // CAPTURE_WINDOWENUMERATOR_H
