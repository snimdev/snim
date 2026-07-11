#include "KWinCaptureStrategy.h"
#include "screen/sources/KWinFrameSource.h"

#include <QDebug>
#include <QPointer>
#include <QTimer>
#include <utility>

namespace Capture {

// --- Construction & availability ---

KWinCaptureStrategy::KWinCaptureStrategy(QObject *parent)
    : CaptureStrategy(parent)
    , m_workspace(new Screen::KWinFrameSource(this))
{
    connect(m_workspace, &Screen::DesktopFrameSource::frameReady, this,
            [this](const QPixmap &frame, const QRect &virtualGeometry) {
                m_workspaceBusy = false;
                deliverFrame(frame, virtualGeometry, m_workspaceSelector);
            });
    connect(m_workspace, &Screen::DesktopFrameSource::frameFailed, this,
            [this](const QString &reason, bool cancelled) {
                m_workspaceBusy = false;
                workspaceFailed(reason, cancelled);
            });
}

bool KWinCaptureStrategy::isAvailable() const
{
    return Screen::KWinFrameSource::isServiceRegistered() && m_workspace->apiVersion() > 0;
}

// --- Authorization gate ---

void KWinCaptureStrategy::setAuthorizationGate(AuthorizationGate gate)
{
    m_authGate = std::move(gate);
}

bool KWinCaptureStrategy::requestAuthorization(AuthorizationResume resume)
{
    if (!m_authGate || m_gateConsumed) {
        return false;
    }
    m_gateConsumed = true;

    QPointer<KWinCaptureStrategy> alive(this);
    AuthorizationResume guarded = [alive, resume = std::move(resume)](bool retryFast) {
        if (alive) {
            resume(retryFast);
        }
    };

    // Queued: the gate opens a modal dialog, which must not run inside the D-Bus reply handler.
    QTimer::singleShot(0, this, [this, guarded = std::move(guarded)]() mutable {
        m_authGate(std::move(guarded));
    });
    return true;
}

// --- Public capture methods ---

void KWinCaptureStrategy::captureFullScreen()
{
    captureWorkspace(false);
}

void KWinCaptureStrategy::captureArea()
{
    captureWorkspace(true);
}

void KWinCaptureStrategy::captureWindow()
{
    if (m_workspace->apiVersion() >= 2) {
        QVariantList args;
        args << QVariant::fromValue(Screen::KWinFrameSource::buildOptions());
        callScreenShotMethod(QStringLiteral("CaptureActiveWindow"), args, false);
    } else {
        // Fallback: interactive window pick
        fallbackToInteractive(false, 0); // kind=0 = window
    }
}

// --- Workspace capture (per-screen, then composite) ---

void KWinCaptureStrategy::captureWorkspace(bool showSelector)
{
    if (m_workspaceBusy) {
        qDebug() << "KWin workspace capture already in progress";
        return;
    }
    m_workspaceBusy = true;
    m_workspaceSelector = showSelector;
    // A full-screen shot keeps the pointer; the selector's frozen frame must not.
    m_workspace->setIncludeCursor(!showSelector);
    m_workspace->grab();
}

void KWinCaptureStrategy::workspaceFailed(const QString &reason, bool cancelled)
{
    const bool showSelector = m_workspaceSelector;
    if (m_workspace->wasDenied()) {
        if (requestAuthorization([this, showSelector](bool retryFast) {
                if (retryFast)
                    captureWorkspace(showSelector);
                else
                    fallbackToInteractive(showSelector, 1);
            })) {
            qDebug() << "Permission denied, asking before any fallback";
            return;
        }
        qDebug() << "Permission denied, falling back to CaptureInteractive";
        fallbackToInteractive(showSelector, 1);
        return;
    }
    if (cancelled) {
        emit screenshotCancelled();
        return;
    }
    emit screenshotFailed(QStringLiteral("KWin screenshot failed: %1").arg(reason));
}

// --- Single calls: the active window and the interactive fallback ---

void KWinCaptureStrategy::callScreenShotMethod(const QString &method, const QVariantList &args,
                                                 bool showAreaSel, int timeout)
{
    using Shot = Screen::KWinFrameSource::Shot;
    Screen::KWinFrameSource::call(method, args, timeout, this,
                                  [this, method, args, showAreaSel, timeout](const Shot &shot) {
        // Permission denied: ask the gate first, else fall back to CaptureInteractive
        if (shot.denied) {
            const int kind = (method == QLatin1String("CaptureActiveWindow")) ? 0 : 1;
            if (requestAuthorization([this, method, args, showAreaSel, timeout, kind](bool retryFast) {
                    if (retryFast)
                        callScreenShotMethod(method, args, showAreaSel, timeout);
                    else
                        fallbackToInteractive(showAreaSel, kind);
                })) {
                qDebug() << "Permission denied, asking before any fallback";
                return;
            }
            qDebug() << "Permission denied, falling back to CaptureInteractive";
            fallbackToInteractive(showAreaSel, kind);
            return;
        }
        if (shot.cancelled) {
            emit screenshotCancelled();
            return;
        }
        if (shot.image.isNull()) {
            emit screenshotFailed(QStringLiteral("KWin screenshot failed: %1").arg(shot.error));
            return;
        }

        QPixmap screenshot = QPixmap::fromImage(shot.image);
        screenshot.setDevicePixelRatio(shot.image.devicePixelRatio());
        if (!showAreaSel) {
            emit screenshotReady(screenshot);
            return;
        }
        // CaptureInteractive hands back the one screen the user clicked.
        deliverFrame(screenshot, Screen::qtVirtualDesktop(), true);
    });
}

// --- CaptureInteractive fallback ---

void KWinCaptureStrategy::fallbackToInteractive(bool showSelector, int kind)
{
    qDebug() << "Using CaptureInteractive (kind:" << kind << "), no .desktop permissions needed";

    QVariantList args;
    args << quint32(kind) << QVariant::fromValue(Screen::KWinFrameSource::buildOptions());
    callScreenShotMethod(QStringLiteral("CaptureInteractive"), args, showSelector, 60000);
}

} // namespace Capture
