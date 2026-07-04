#ifndef CAPTURE_SCREENCOPYCAPTURESTRATEGY_H
#define CAPTURE_SCREENCOPYCAPTURESTRATEGY_H

#include "WaylandCaptureStrategy.h"

namespace Capture {

/**
 * Native Wayland capture on wlroots-family compositors (Sway, Hyprland, river, niri,
 * labwc, Wayfire, COSMIC): every output is copied straight through
 * ext-image-copy-capture-v1 or wlr-screencopy, like grim, with no portal and no prompt.
 * Extends the portal strategy, which keeps the area selector and takes over whenever
 * the compositor refuses or times out a copy.
 */
class ScreencopyCaptureStrategy : public WaylandCaptureStrategy
{
    Q_OBJECT

public:
    explicit ScreencopyCaptureStrategy(QObject *parent = nullptr);

    void captureFullScreen() override;
    void captureArea() override;

    [[nodiscard]] bool isAvailable() const override;
    [[nodiscard]] QString name() const override { return "Wayland Screencopy"; }

private:
    /// Captures and stitches every output; false leaves the request to the portal.
    bool captureOutputs(bool showSelector);
};

} // namespace Capture

#endif // CAPTURE_SCREENCOPYCAPTURESTRATEGY_H
