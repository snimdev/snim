#ifndef CAPTURE_KWINCAPTURESTRATEGY_H
#define CAPTURE_KWINCAPTURESTRATEGY_H

#include "CaptureStrategy.h"
#include <QObject>
#include <QPixmap>
#include <QtDBus/QDBusConnection>
#include <QtDBus/QDBusPendingCallWatcher>
#include <QVariantMap>
#include <functional>

namespace Screen {
class AreaSelector;
} // namespace Screen

namespace Capture {

/**
 * KDE Plasma capture strategy using KWin's org.kde.KWin.ScreenShot2 D-Bus interface.
 *
 * This provides direct compositor-level screenshot capture without going through
 * xdg-desktop-portal. Data transfer uses pipe2() for raw pixel streaming.
 *
 * Permission model:
 * - Most methods require the app's .desktop file to declare
 *   X-KDE-DBUS-Restricted-Interfaces=org.kde.KWin.ScreenShot2
 * - CaptureInteractive requires no permissions (user click = consent)
 * - On permission error, falls back to CaptureInteractive, unless an authorization
 *   gate is installed: then the gate's answer decides between a retry and the fallback
 */
class KWinCaptureStrategy : public CaptureStrategy
{
    Q_OBJECT

public:
    explicit KWinCaptureStrategy(QObject *parent = nullptr);
    ~KWinCaptureStrategy() override = default;

    void captureFullScreen() override;
    void captureArea() override;
    void captureWindow() override;

    [[nodiscard]] bool isAvailable() const override;
    [[nodiscard]] QString name() const override { return "KWin ScreenShot2"; }

    /// Check if the KWin ScreenShot2 D-Bus service is registered
    static bool isKWinAvailable();

    /// Called by the gate once it has an answer: true retries the same capture, false
    /// takes the CaptureInteractive fallback.
    using AuthorizationResume = std::function<void(bool retryFast)>;
    /// Asked once per run when KWin refuses for lack of an authorizing desktop entry.
    /// While a gate is installed the strategy launches no fallback on its own: it hands
    /// the pending capture to the gate and waits for the resume callback.
    using AuthorizationGate = std::function<void(AuthorizationResume)>;
    void setAuthorizationGate(AuthorizationGate gate);

private:
    /// D-Bus method call with pipe-based data transfer
    void callScreenShotMethod(const QString &method, const QVariantList &args,
                              bool showAreaSelector, int timeout = 4000);

    /// Handle the async D-Bus reply and read image from pipe. Keeps the request's own
    /// arguments so a denied call can be re-issued identically after the gate answers.
    void handleReply(QDBusPendingCallWatcher *watcher, int readFd,
                     bool showAreaSelector, const QString &method,
                     const QVariantList &args, int timeout);

    /// Hand a denied capture to the gate. Returns false when there is no gate left to
    /// consult, meaning the caller must fall back itself.
    bool requestAuthorization(AuthorizationResume resume);

    /// Read raw image data from pipe FD using metadata from D-Bus reply
    static QImage readImageFromPipe(int fd, const QVariantMap &metadata);

    /// Composite per-screen images into a single workspace image
    static QImage compositeScreenImages(const QList<QImage> &images);

    /// Show AreaSelector overlay on captured screenshot
    void showAreaSelector(const QPixmap &screenshot, const QRect &virtualGeometry);

    /// Build the options map for KWin calls
    static QVariantMap buildOptions(bool includeCursor = true, bool nativeResolution = true);

    /// Capture all screens by calling CaptureScreen per screen
    void captureWorkspace(bool showSelector);

    /// Fallback to CaptureInteractive when permission is denied
    void fallbackToInteractive(bool showSelector, int kind = 1);

    quint32 m_apiVersion = 0;
    AuthorizationGate m_authGate;
    bool m_gateConsumed = false;   // the gate is asked once per run, never per capture
};

} // namespace Capture

#endif // CAPTURE_KWINCAPTURESTRATEGY_H
