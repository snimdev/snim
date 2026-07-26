#ifndef CAPTURE_WAYLANDCAPTURESTRATEGY_H
#define CAPTURE_WAYLANDCAPTURESTRATEGY_H

#include "CaptureStrategy.h"
#include <QObject>
#include <QPixmap>

namespace Screen {
class PortalFrameSource;
} // namespace Screen

namespace Capture {

/**
 * Wayland capture strategy using the XDG Desktop Portal's Screenshot interface
 */
class WaylandCaptureStrategy : public CaptureStrategy
{
    Q_OBJECT

public:
    explicit WaylandCaptureStrategy(QObject *parent = nullptr);

    void captureFullScreen() override;
    void captureArea() override;
    void captureWindow() override;

    bool isAvailable() const override;
    QString name() const override { return "Wayland Portal Capture"; }

private:
    void grabThroughPortal(bool showSelector);

    Screen::PortalFrameSource *m_portalSource;
    bool m_portalShowsSelector = false;
};

} // namespace Capture

#endif // CAPTURE_WAYLANDCAPTURESTRATEGY_H
