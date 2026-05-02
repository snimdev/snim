#ifndef CORE_ICONUTIL_H
#define CORE_ICONUTIL_H

#include <QIcon>
#include <QString>
#include <QColor>

namespace Core {

/**
 * Load an SVG that uses `currentColor` for its strokes/fills, recolour it to
 * `color`, and rasterise it at the highest screen device-pixel-ratio (so it stays
 * crisp on Retina menu bars / toolbars instead of being a 1x bitmap the OS
 * upscales). `logicalSize` is the device-independent edge length in points.
 *
 * Single icon loader shared by the tray (App::TrayMenu) and the editor toolbar
 * (ImageEditor) so HiDPI handling lives in exactly one place.
 */
QIcon themedSvgIcon(const QString &svgPath, const QColor &color, int logicalSize);

} // namespace Core

#endif // CORE_ICONUTIL_H
