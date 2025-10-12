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

private slots:
    void processFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void onPortalResponse(QDBusPendingCallWatcher *watcher);
    void handleScreenshotResponse(uint response, const QVariantMap &results);

private:
    bool usePortalCapture();
    bool useFallbackCapture();
    bool hasAvailableFallbackTools() const;
    bool executeScreenshotTool(const QString &tool);
    void cleanupTempFile();
    void connectToPortalSignals();
    void connectToScreenshotToolSignals();

    static void showScreenshotDialog(const QPixmap &screenshot);

    static QString generateSessionToken();

    static void onScreenshotReady(const QPixmap &screenshot) ;
    static void onScreenshotFailed(const QString &error) ;

    QDBusInterface *m_portalInterface;
    QDBusInterface *m_sessionInterface;
    QString m_sessionHandle;
    QString m_requestToken;
    QTemporaryFile *m_tempFile;
    QProcess *m_fallbackProcess;
    bool m_captureArea;
    QDBusConnection m_sessionBus;
};

} // namespace Capture

#endif // CAPTURE_WAYLANDCAPTURESTRATEGY_H
