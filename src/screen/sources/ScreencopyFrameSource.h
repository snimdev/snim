#ifndef SCREEN_SCREENCOPYFRAMESOURCE_H
#define SCREEN_SCREENCOPYFRAMESOURCE_H

#include "DesktopFrameSource.h"

namespace Screen {

/**
 * Every output copied through ext-image-copy-capture-v1 or wlr-screencopy and stitched
 * onto the virtual desktop, like grim: no portal, no prompt. The copy runs on a worker
 * thread on its own Wayland event queue, so a slow compositor never stalls the GUI.
 */
class ScreencopyFrameSource : public DesktopFrameSource
{
    Q_OBJECT

public:
    using DesktopFrameSource::DesktopFrameSource;

    void grab() override;

    // Whether the compositor advertises ext-image-copy-capture or wlr-screencopy.
    [[nodiscard]] static bool isAvailable();

private:
    bool m_busy = false;
};

} // namespace Screen

#endif // SCREEN_SCREENCOPYFRAMESOURCE_H
