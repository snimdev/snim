#include "ScreencastCaptureStrategy.h"

#include "capture/ScreencastStitch.h"
#include "recording/strategies/LinuxRecorderModule.h"
#include "recording/strategies/ScreenCastPortalSession.h"

#include <QDebug>
#include <QGuiApplication>
#include <QScreen>
#include <QThread>
#include <QTimer>

#include <memory>
#include <unistd.h>

namespace Capture {

namespace {

// Long enough for a first-time pick in the portal's dialog.
constexpr int kSessionTimeoutMs = 60000;
constexpr int kFrameTimeoutMs = 3000;
constexpr uint kFirstPersistingVersion = 4;

using Session = Recording::ScreenCastPortalSession;

QRect qtVirtualDesktop()
{
    QRect desktop;
    for (const QScreen *screen : QGuiApplication::screens())
        desktop = desktop.united(screen->geometry());
    return desktop;
}

struct GrabResult {
    QList<Session::Stream> streams;
    QList<QImage> frames;
    QString error;
    bool ok = false;
};

} // namespace

ScreencastCaptureStrategy::ScreencastCaptureStrategy(QObject *parent)
    : WaylandCaptureStrategy(parent)
    , m_session(new Session(this))
    , m_timeout(new QTimer(this))
{
    m_timeout->setSingleShot(true);
    m_timeout->setInterval(kSessionTimeoutMs);
    connect(m_timeout, &QTimer::timeout, this, [this] {
        m_session->close();
        fallBack(QStringLiteral("the ScreenCast session timed out"));
    });

    connect(m_session, &Session::ready, this,
            [this](quint32, const QRect &, int fd) { handleReady(fd); });
    connect(m_session, &Session::failed, this, &ScreencastCaptureStrategy::handleFailed);
    connect(m_session, &Session::sessionClosed, this, [this] {
        if (m_busy && m_timeout->isActive())
            handleFailed(QStringLiteral("the portal closed the session"));
    });
}

ScreencastCaptureStrategy::~ScreencastCaptureStrategy() = default;

void ScreencastCaptureStrategy::captureFullScreen()
{
    begin(false);
}

void ScreencastCaptureStrategy::captureArea()
{
    begin(true);
}

void ScreencastCaptureStrategy::captureWindow()
{
    begin(false);
}

bool ScreencastCaptureStrategy::isAvailable() const
{
    return isSupported();
}

bool ScreencastCaptureStrategy::isSupported()
{
    return isWaylandSession() && Session::portalVersion() >= kFirstPersistingVersion
           && Recording::LinuxRecorderModule::canGrabFrames();
}

bool ScreencastCaptureStrategy::hasRestoreToken()
{
    return !Session::restoreToken(QString::fromLatin1(kRestoreTokenKey)).isEmpty();
}

void ScreencastCaptureStrategy::begin(bool showSelector)
{
    if (m_busy) {
        qDebug() << "ScreenCast capture already in progress";
        return;
    }
    m_busy = true;
    m_showSelector = showSelector;
    m_clock.start();

    if (!hasRestoreToken()) {
        qInfo() << "ScreenCast capture: no screens remembered yet, the portal will ask once "
                   "which screens Snim may capture";
        emit sourcePickerExpected();
    }

    Session::Options options;
    options.multiple = true;
    options.restoreTokenKey = QString::fromLatin1(kRestoreTokenKey);
    m_timeout->start();
    m_session->open(options);
}

void ScreencastCaptureStrategy::handleReady(int pipewireFd)
{
    m_timeout->stop();
    if (!m_busy) {
        ::close(pipewireFd);
        m_session->close();
        return;
    }

    auto result = std::make_shared<GrabResult>();
    result->streams = m_session->streams();
    qDebug() << "ScreenCast session ready after" << m_clock.elapsed() << "ms with"
             << result->streams.size() << "stream(s)";

    QList<quint32> nodes;
    for (const Session::Stream &stream : std::as_const(result->streams))
        nodes.append(stream.nodeId);

    // pipewiresrc blocks until the first frame, so keep it off the GUI thread.
    QThread *worker = QThread::create([result, nodes, pipewireFd] {
        result->ok = Recording::LinuxRecorderModule::grabFrames(pipewireFd, nodes, kFrameTimeoutMs,
                                                                &result->frames, &result->error);
        ::close(pipewireFd);
    });
    connect(worker, &QThread::finished, worker, &QObject::deleteLater);
    connect(worker, &QThread::finished, this, [this, result] {
        // Closing at once is what takes the screen-sharing indicator down.
        m_session->close();
        const qint64 grabbedMs = m_clock.elapsed();
        if (!result->ok) {
            fallBack(result->error);
            return;
        }

        QList<StreamFrame> frames;
        for (qsizetype i = 0; i < result->streams.size(); ++i)
            frames.append({result->streams.at(i).rectLogical, result->frames.value(i)});
        const StitchedDesktop desktop = stitchStreams(frames, qtVirtualDesktop());
        if (desktop.image.isNull()) {
            fallBack(QStringLiteral("the ScreenCast frames were empty"));
            return;
        }

        qInfo() << "ScreenCast capture:" << desktop.image.size() << "at DPR"
                << desktop.image.devicePixelRatio() << "frames after" << grabbedMs
                << "ms, total" << m_clock.elapsed() << "ms";

        QPixmap pixmap = QPixmap::fromImage(desktop.image);
        pixmap.setDevicePixelRatio(desktop.image.devicePixelRatio());
        const bool showSelector = m_showSelector;
        finish();
        if (showSelector)
            showAreaSelector(pixmap, desktop.virtualGeometry);
        else
            emit screenshotReady(pixmap);
    });
    worker->start();
}

void ScreencastCaptureStrategy::handleFailed(const QString &error)
{
    m_timeout->stop();
    if (!m_busy)
        return;
    fallBack(error);
}

void ScreencastCaptureStrategy::fallBack(const QString &reason)
{
    const bool showSelector = m_showSelector;
    finish();
    qWarning() << "ScreenCast capture failed (" << reason << "), using the Screenshot portal";
    if (showSelector)
        WaylandCaptureStrategy::captureArea();
    else
        WaylandCaptureStrategy::captureFullScreen();
}

void ScreencastCaptureStrategy::finish()
{
    m_timeout->stop();
    m_busy = false;
}

} // namespace Capture
