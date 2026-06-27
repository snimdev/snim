#ifndef SCREEN_QTSCREENSFRAMESOURCE_H
#define SCREEN_QTSCREENSFRAMESOURCE_H

#include "DesktopFrameSource.h"

namespace Screen {

/**
 * Every screen through QScreen::grabWindow, composited at the highest DPR. Synchronous;
 * right on macOS, Windows and X11, only black on Wayland.
 */
class QtScreensFrameSource : public DesktopFrameSource
{
    Q_OBJECT

public:
    using DesktopFrameSource::DesktopFrameSource;

    [[nodiscard]] QString name() const override { return QStringLiteral("Qt screen grab"); }
    void grab() override;

    // Null when there are no screens.
    [[nodiscard]] static QPixmap grabNow(QRect *virtualGeometryOut);
};

} // namespace Screen

#endif // SCREEN_QTSCREENSFRAMESOURCE_H
