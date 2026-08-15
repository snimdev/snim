#include "KWinCaptureStrategy.h"
#include "screen/sources/KWinFrameSource.h"

#include <QDebug>
#include <QPointer>
#include <QTimer>
#include <utility>

namespace Capture {

// --- Construction ---

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

// --- Authorization gate ---

void KWinCaptureStrategy::setAuthorizationGate(AuthorizationGate gate)
{
    m_authGate = std::move(gate);
}

void KWinCaptureStrategy::handleDenied(std::function<void()> retry, bool showSelector, int kind)
{
    if (!m_authGate || m_gateConsumed) {
        qDebug() << "Permission denied, falling back to CaptureInteractive";
        fallbackToInteractive(showSelector, kind);
        return;
    }
    m_gateConsumed = true;
    qDebug() << "Permission denied, asking before any fallback";

    QPointer<KWinCaptureStrategy> alive(this);
    AuthorizationResume resume = [this, alive, retry = std::move(retry), showSelector,
                                  kind](bool retryFast) {
        if (!alive)
            return;
        if (retryFast)
            retry();
        else
            fallbackToInteractive(showSelector, kind);
    };

    // Queued: the gate opens a modal dialog, which must not run inside the D-Bus reply handler.
    QTimer::singleShot(0, this, [this, resume = std::move(resume)]() mutable {
        m_authGate(std::move(resume));
    });
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
        handleDenied([this, showSelector] { captureWorkspace(showSelector); }, showSelector, 1);
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
        if (shot.denied) {
            const int kind = (method == QLatin1String("CaptureActiveWindow")) ? 0 : 1;
            handleDenied([this, method, args, showAreaSel, timeout] {
                callScreenShotMethod(method, args, showAreaSel, timeout);
            }, showAreaSel, kind);
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
