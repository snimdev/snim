#ifndef CAPTURE_KWINCAPTURESTRATEGY_H
#define CAPTURE_KWINCAPTURESTRATEGY_H

#include "CaptureStrategy.h"
#include <QObject>
#include <QPixmap>
#include <QVariantList>
#include <functional>

namespace Screen {
class KWinFrameSource;
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

    /// Called by the gate once it has an answer: true retries the same capture, false
    /// takes the CaptureInteractive fallback.
    using AuthorizationResume = std::function<void(bool retryFast)>;
    /// Asked once per run when KWin refuses for lack of an authorizing desktop entry.
    /// While a gate is installed the strategy launches no fallback on its own: it hands
    /// the pending capture to the gate and waits for the resume callback.
    using AuthorizationGate = std::function<void(AuthorizationResume)>;
    void setAuthorizationGate(AuthorizationGate gate);

private:
    /// One ScreenShot2 call through the frame source; a denied call can be re-issued
    /// identically after the gate answers.
    void callScreenShotMethod(const QString &method, const QVariantList &args,
                              bool showAreaSelector, int timeout = 4000);

    /// A denied capture: the gate, while unasked, chooses between retry and the
    /// CaptureInteractive fallback; without it the fallback runs at once.
    void handleDenied(std::function<void()> retry, bool showSelector, int kind);

    /// Every screen through the KWin frame source
    void captureWorkspace(bool showSelector);
    void workspaceFailed(const QString &reason, bool cancelled);

    /// Fallback to CaptureInteractive when permission is denied
    void fallbackToInteractive(bool showSelector, int kind = 1);

    Screen::KWinFrameSource *m_workspace;
    bool m_workspaceBusy = false;
    bool m_workspaceSelector = false;
    AuthorizationGate m_authGate;
    bool m_gateConsumed = false;   // the gate is asked once per run, never per capture
};

} // namespace Capture

#endif // CAPTURE_KWINCAPTURESTRATEGY_H
