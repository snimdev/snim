#ifndef SCREEN_SCREENCOPYFRAMESOURCE_H
#define SCREEN_SCREENCOPYFRAMESOURCE_H

#include "DesktopFrameSource.h"

namespace Screen {

/**
 * Every output copied through ext-image-copy-capture-v1 or wlr-screencopy and stitched
 * onto the virtual desktop, like grim: no portal, no prompt. Synchronous.
 */
class ScreencopyFrameSource : public DesktopFrameSource
{
    Q_OBJECT

public:
    using DesktopFrameSource::DesktopFrameSource;

    [[nodiscard]] QString name() const override { return QStringLiteral("Wayland screencopy"); }
    void grab() override;

    // Whether the compositor advertises ext-image-copy-capture or wlr-screencopy.
    [[nodiscard]] static bool isAvailable();

    // Null with *error set when the compositor offers no protocol or refuses a copy.
    [[nodiscard]] static QPixmap grabNow(QRect *virtualGeometryOut, QString *error);
};

} // namespace Screen

#endif // SCREEN_SCREENCOPYFRAMESOURCE_H
