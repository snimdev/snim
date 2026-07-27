#include "WaylandCaptureStrategy.h"

#include <QDebug>

#include "screen/sources/PortalFrameSource.h"

namespace Capture {
    WaylandCaptureStrategy::WaylandCaptureStrategy(QObject *parent)
        : CaptureStrategy(parent)
          , m_portalSource(new Screen::PortalFrameSource(this)) {
        connect(m_portalSource, &Screen::DesktopFrameSource::frameReady, this,
                [this](const QPixmap &frame, const QRect &virtualGeometry) {
                    deliverFrame(frame, virtualGeometry, m_portalShowsSelector);
                });
        connect(m_portalSource, &Screen::DesktopFrameSource::frameFailed, this,
                [this](const QString &reason, bool cancelled) {
                    qDebug() << "Portal screenshot failed:" << reason;
                    if (cancelled)
                        emit screenshotCancelled();
                    else
                        emit screenshotFailed(QStringLiteral("Portal screenshot failed: %1").arg(reason));
                });
    }

    void WaylandCaptureStrategy::captureFullScreen() {
        grabThroughPortal(false);
    }

    void WaylandCaptureStrategy::captureArea() {
        grabThroughPortal(true);
    }

    void WaylandCaptureStrategy::captureWindow() {
        // Wayland lists no windows: the user draws around the one they want.
        captureArea();
    }

    bool WaylandCaptureStrategy::isAvailable() const {
        return Screen::isWaylandSession() && Screen::PortalFrameSource::isPortalReachable();
    }

    void WaylandCaptureStrategy::grabThroughPortal(bool showSelector) {
        m_portalShowsSelector = showSelector;
        if (Screen::PortalFrameSource::isPortalReachable()) {
            m_portalSource->grab();
            return;
        }
        emit screenshotFailed(QStringLiteral(
            "No screenshot method available: the Screenshot portal needs xdg-desktop-portal "
            "and a backend for this desktop."));
    }
} // namespace Capture
