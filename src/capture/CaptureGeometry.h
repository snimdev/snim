#ifndef CAPTURE_CAPTUREGEOMETRY_H
#define CAPTURE_CAPTUREGEOMETRY_H

#include "screen/DesktopStitch.h"

#include <QList>
#include <QPixmap>
#include <QRect>
#include <QSize>

namespace Capture {

/**
 * The one copy of the virtual-desktop crop math shared by every capture strategy.
 * Header-only and widget-free (no platform code, no QWidget) so the macOS/X11
 * native path, the portal-Wayland path, the KWin path and the headless unit tests
 * all agree on the riskiest arithmetic: AreaSelector reports the selection in
 * VIRTUAL-DESKTOP LOGICAL coordinates, while the frozen frame is a physical-pixel
 * pixmap whose origin is the virtual desktop's top-left.
 */

using Screen::physicalCropRect;

// Crops and restores the DPR on the result.
inline QPixmap cropVirtualArea(const QPixmap &shot, const QRect &virtualGeometry,
                               const QRect &area)
{
    if (shot.isNull() || area.isEmpty())
        return QPixmap();

    const qreal dpr = shot.devicePixelRatio();
    const QRect physical = physicalCropRect(area, virtualGeometry, dpr, shot.size());
    // copy() reads a null rect as "the whole pixmap", so a selection that clamps away
    // to nothing has to be rejected here or it silently returns the entire desktop.
    if (physical.isEmpty())
        return QPixmap();

    QPixmap cropped = shot.copy(physical);
    cropped.setDevicePixelRatio(dpr);
    return cropped;
}

using Screen::ScreenGrab;

// The grab whose screen wholly holds area, so it crops at that screen's own DPR with
// no resampling. Null when area spans screens or a grab does not cover its screen.
inline const ScreenGrab *screenGrabFor(const QList<ScreenGrab> &grabs, const QRect &area)
{
    if (area.isEmpty())
        return nullptr;
    for (const ScreenGrab &grab : grabs) {
        if (!grab.geometry.contains(area))
            continue;
        // Fractional DPRs round the logical screen size, so allow under a pixel.
        const QSizeF logical = grab.pixmap.deviceIndependentSize();
        const bool covers = !grab.pixmap.isNull()
                            && qAbs(logical.width() - grab.geometry.width()) < 1.0
                            && qAbs(logical.height() - grab.geometry.height()) < 1.0;
        return covers ? &grab : nullptr;
    }
    return nullptr;
}

} // namespace Capture

#endif // CAPTURE_CAPTUREGEOMETRY_H
