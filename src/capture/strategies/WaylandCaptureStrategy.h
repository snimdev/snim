#ifndef CAPTURE_WAYLANDCAPTURESTRATEGY_H
#define CAPTURE_WAYLANDCAPTURESTRATEGY_H

#include "CaptureStrategy.h"
#include <QObject>
#include <QPixmap>
#include <QtDBus/QDBusInterface>
#include <QtDBus/QDBusReply>
#include <QtDBus/QDBusObjectPath>
#include <QtDBus/QDBusConnection>
#include <QtDBus/QDBusPendingCallWatcher>
#include <QTemporaryFile>
#include <QProcess>
#include <QVariantMap>

namespace Capture {

class PortalFrameSource;

/**
 * Wayland capture strategy using XDG Desktop Portal and fallback tools
 */
class WaylandCaptureStrategy : public CaptureStrategy
{
    Q_OBJECT

public:
    explicit WaylandCaptureStrategy(QObject *parent = nullptr);
    ~WaylandCaptureStrategy() override;

    void captureFullScreen() override;
    void captureArea() override;
    void captureWindow() override;

    bool isAvailable() const override;
    QString name() const override { return "Wayland Portal Capture"; }

    // Check if we're running on Wayland
    static bool isWaylandSession();

    // Check if XDG Desktop Portal is available
    bool isPortalAvailable() const;

protected:
    void showAreaSelector(const QPixmap &screenshot, const QRect &virtualGeometry);

private slots:
    void processFinished(int exitCode, QProcess::ExitStatus exitStatus);

private:
    bool usePortalCapture();
    bool useFallbackCapture();
    bool hasAvailableFallbackTools() const;
    bool executeScreenshotTool(const QString &tool);
    void cleanupTempFile();
    void connectToPortalSignals();

    static QPixmap cropToCurrentScreen(const QImage &fullImage);

    static QString generateSessionToken();

    QDBusInterface *m_portalInterface;
    QDBusInterface *m_sessionInterface;
    QString m_sessionHandle;
    QTemporaryFile *m_tempFile;
    QProcess *m_fallbackProcess;
    bool m_captureArea;
    QDBusConnection m_sessionBus;
    PortalFrameSource *m_portalSource;
};

} // namespace Capture

#endif // CAPTURE_WAYLANDCAPTURESTRATEGY_H
