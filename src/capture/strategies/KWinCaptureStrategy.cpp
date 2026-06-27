#include "KWinCaptureStrategy.h"
#include "screen/AreaSelector.h"
#include "capture/CaptureGeometry.h"
#include "screen/sources/KWinFrameSource.h"

#include <QApplication>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusInterface>
#include <QDBusMessage>
#include <QDBusPendingCall>
#include <QDBusPendingReply>
#include <QDBusUnixFileDescriptor>
#include <QDebug>
#include <QFuture>
#include <QFutureWatcher>
#include <QGuiApplication>
#include <QPointer>
#include <QScreen>
#include <QTimer>
#include <QWindow>
#include <QtConcurrentRun>
#include <utility>

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

namespace Capture {

static const QString kServiceName = QStringLiteral("org.kde.KWin.ScreenShot2");
static const QString kObjectPath = QStringLiteral("/org/kde/KWin/ScreenShot2");
static const QString kInterface = QStringLiteral("org.kde.KWin.ScreenShot2");

// --- Construction & availability ---

KWinCaptureStrategy::KWinCaptureStrategy(QObject *parent)
    : CaptureStrategy(parent)
    , m_workspace(new Screen::KWinFrameSource(this))
{
    connect(m_workspace, &Screen::DesktopFrameSource::frameReady, this,
            [this](const QPixmap &frame, const QRect &virtualGeometry) {
                m_workspaceBusy = false;
                if (m_workspaceSelector)
                    showAreaSelector(frame, virtualGeometry);
                else
                    emit screenshotReady(frame);
            });
    connect(m_workspace, &Screen::DesktopFrameSource::frameFailed, this,
            [this](const QString &reason, bool cancelled) {
                m_workspaceBusy = false;
                workspaceFailed(reason, cancelled);
            });
}

bool KWinCaptureStrategy::isKWinAvailable()
{
    return QDBusConnection::sessionBus().interface()->isServiceRegistered(kServiceName);
}

bool KWinCaptureStrategy::isAvailable() const
{
    return isKWinAvailable() && m_workspace->apiVersion() > 0;
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
    if (cancelled)
        return;   // user cancelled, silently ignore
    emit screenshotFailed(QStringLiteral("KWin screenshot failed: %1").arg(reason));
}

// --- Core D-Bus call with pipe ---

void KWinCaptureStrategy::callScreenShotMethod(const QString &method, const QVariantList &args,
                                                 bool showAreaSel, int timeout)
{
    int pipeFds[2]{-1, -1};
    if (pipe2(pipeFds, O_CLOEXEC) == -1) {
        qWarning() << "pipe2() failed:" << strerror(errno);
        emit screenshotFailed(QString("pipe2() failed: %1").arg(strerror(errno)));
        return;
    }

    QDBusMessage msg = QDBusMessage::createMethodCall(kServiceName, kObjectPath, kInterface, method);

    QVariantList fullArgs = args;
    fullArgs.append(QVariant::fromValue(QDBusUnixFileDescriptor(pipeFds[1])));
    msg.setArguments(fullArgs);

    QDBusPendingCall pending = QDBusConnection::sessionBus().asyncCall(msg, timeout);
    ::close(pipeFds[1]);

    auto *watcher = new QDBusPendingCallWatcher(pending, this);
    handleReply(watcher, pipeFds[0], showAreaSel, method, args, timeout);
}

// --- Async reply handler ---

void KWinCaptureStrategy::handleReply(QDBusPendingCallWatcher *watcher, int readFd,
                                       bool showAreaSel, const QString &method,
                                       const QVariantList &args, int timeout)
{
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this, readFd, showAreaSel, method, args, timeout](QDBusPendingCallWatcher *w) {
        w->deleteLater();
        const QDBusPendingReply<QVariantMap> reply = *w;

        if (reply.isError()) {
            ::close(readFd);
            const QString errorName = reply.error().name();
            qDebug() << "KWin" << method << "error:" << errorName << reply.error().message();

            // Permission denied: ask the gate first, else fall back to CaptureInteractive
            if (errorName.contains("NoAuthorized") || errorName.contains("AccessDenied")) {
                const int kind = (method == "CaptureActiveWindow") ? 0 : 1;

                if (requestAuthorization([this, method, args, showAreaSel, timeout, kind](bool retryFast) {
                        if (retryFast) {
                            callScreenShotMethod(method, args, showAreaSel, timeout);
                        } else {
                            fallbackToInteractive(showAreaSel, kind);
                        }
                    })) {
                    qDebug() << "Permission denied, asking before any fallback";
                    return;
                }

                qDebug() << "Permission denied, falling back to CaptureInteractive";
                fallbackToInteractive(showAreaSel, kind);
                return;
            }

            if (errorName.contains("Cancelled")) {
                return; // User cancelled, silently ignore
            }

            emit screenshotFailed(QString("KWin screenshot failed: %1").arg(reply.error().message()));
            return;
        }

        const QVariantMap &metadata = reply;
        const QString type = metadata.value("type").toString();
        if (type != "raw") {
            ::close(readFd);
            emit screenshotFailed(QString("Unsupported KWin screenshot type: %1").arg(type));
            return;
        }

        // Read image in background thread
        auto *futureWatcher = new QFutureWatcher<QImage>(this);
        connect(futureWatcher, &QFutureWatcher<QImage>::finished, this,
                [this, futureWatcher, showAreaSel]() {
            QImage image = futureWatcher->result();
            futureWatcher->deleteLater();

            if (image.isNull()) {
                emit screenshotFailed("Failed to read screenshot from KWin pipe");
                return;
            }

            QPixmap screenshot = QPixmap::fromImage(image);
            screenshot.setDevicePixelRatio(image.devicePixelRatio());

            if (showAreaSel) {
                QRect virtualDesktop;
                for (QScreen *s : QGuiApplication::screens()) {
                    virtualDesktop = virtualDesktop.united(s->geometry());
                }
                this->showAreaSelector(screenshot, virtualDesktop);
            } else {
                emit screenshotReady(screenshot);
            }
        });

        QFuture<QImage> future =
            QtConcurrent::run(Screen::KWinFrameSource::readImageFromPipe, readFd, metadata);
        futureWatcher->setFuture(future);
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

// --- Area selector (reused from WaylandCaptureStrategy pattern) ---

// Every terminal action (accept, cancel, copy, save) tears down ALL per-screen
// overlays first, so nothing is left covering the screen or the save dialog.
static void tearDownSelectors(QList<Screen::AreaSelector*> *selectors)
{
    for (auto *sel : *selectors) {
        sel->blockSignals(true);
        sel->close();
        sel->deleteLater();
    }
    selectors->clear();
    delete selectors;
}

void KWinCaptureStrategy::showAreaSelector(const QPixmap &screenshot, const QRect &virtualGeometry)
{
    qDebug() << "Showing area selector, virtual geometry:" << virtualGeometry
             << "screenshot:" << screenshot.size() << "DPR:" << screenshot.devicePixelRatio();

    QList<QScreen*> screens = QGuiApplication::screens();
    auto *selectors = new QList<Screen::AreaSelector*>();

    for (QScreen *screen : screens) {
        QRect screenGeometry = screen->geometry();
        auto *selector = new Screen::AreaSelector();
        selector->setScreenshot(screenshot);
        selector->setVirtualGeometry(virtualGeometry);
        selector->setScreenOffset(screenGeometry.topLeft());
        selector->setActionsEnabled(quickActionsEnabled());

        selector->setGeometry(screenGeometry);
        selector->setWindowState(Qt::WindowFullScreen);
        selector->winId(); // ensure the native window exists before placing it
        if (QWindow *wh = selector->windowHandle())
            wh->setScreen(screen);
        selector->showFullScreen();

        selectors->append(selector);
    }

    const auto annotations = attachAnnotations(*selectors, screenshot, virtualGeometry);

    for (auto *selector : *selectors) {
        // Each handler copies the session first, so it outlives the teardown.
        connect(selector, &Screen::AreaSelector::areaSelected,
                this, [this, selectors, screenshot, virtualGeometry, annotations](const QRect &area) {
            const auto session = annotations;
            tearDownSelectors(selectors);

            if (area.isEmpty()) {
                qDebug() << "Area selection cancelled";
                return;
            }

            emitSelection(screenshot, virtualGeometry, area, session);
        });
        connect(selector, &Screen::AreaSelector::copyRequested,
                this, [this, selectors, screenshot, virtualGeometry, annotations](const QRect &area) {
            const auto session = annotations;
            tearDownSelectors(selectors);
            copyAreaToClipboard(screenshot, virtualGeometry, area, session);
        });
        connect(selector, &Screen::AreaSelector::saveRequested,
                this, [this, selectors, screenshot, virtualGeometry, annotations](const QRect &area) {
            const auto session = annotations;
            // Teardown first, or the save dialog opens behind the overlay.
            tearDownSelectors(selectors);
            saveAreaToFile(screenshot, virtualGeometry, area, session);
        });
    }
}

} // namespace Capture
