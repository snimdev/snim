#include "ScreencastFrameSource.h"

#include "screen/PipeWireFrames.h"
#include "screen/ScreenCastPortalSession.h"
#include "screen/DesktopStitch.h"

#include <QDebug>
#include <QGuiApplication>
#include <QSettings>
#include <QThread>
#include <QTimer>

#include <memory>
#include <unistd.h>

namespace Screen {

namespace {

// Long enough for a first-time pick in the portal's dialog.
constexpr int kSessionTimeoutMs = 60000;
constexpr int kFrameTimeoutMs = 3000;
// A silent restore answers in well under this; slower means the picker was on screen.
constexpr qint64 kInteractiveHandshakeMs = 1000;
// Lets the picker's close animation leave the screen before the frame is taken.
constexpr unsigned long kPickerSettleMs = 600;

using Session = ScreenCastPortalSession;

// Set when a remembered pick was dropped for missing a screen, until a pick covers them all.
bool s_pickMissedScreens = false;

struct GrabResult {
    QList<Session::Stream> streams;
    QList<QImage> frames;
    QString error;
    bool ok = false;
};

} // namespace

ScreencastFrameSource::ScreencastFrameSource(QObject *parent)
    : DesktopFrameSource(parent)
    , m_session(new Session(this))
    , m_timeout(new QTimer(this))
{
    m_timeout->setSingleShot(true);
    m_timeout->setInterval(kSessionTimeoutMs);
    connect(m_timeout, &QTimer::timeout, this, [this] {
        m_session->close();
        fail(QStringLiteral("the ScreenCast session timed out"));
    });

    connect(m_session, &Session::ready, this,
            [this](quint32, const QRect &, int fd) { handleReady(fd); });
    connect(m_session, &Session::failed, this, [this](const QString &error) {
        m_timeout->stop();
        if (m_busy)
            fail(error, m_session->wasCancelled());
    });
    connect(m_session, &Session::sessionClosed, this, [this] {
        if (m_busy && m_timeout->isActive()) {
            m_timeout->stop();
            fail(QStringLiteral("the portal closed the session"));
        }
    });
}

ScreencastFrameSource::~ScreencastFrameSource() = default;

bool ScreencastFrameSource::isSupported()
{
    return isWaylandSession()
           && Session::portalVersion() >= Session::kFirstPersistingVersion
           && PipeWireFrames::canGrab();
}

bool ScreencastFrameSource::hasRestoreToken()
{
    return !Session::restoreToken(QString::fromLatin1(kRestoreTokenKey)).isEmpty();
}

void ScreencastFrameSource::grab()
{
    if (m_busy) {
        qDebug() << "ScreenCast capture already in progress";
        return;
    }
    m_busy = true;
    m_clock.start();

    m_pickerExpected = !hasRestoreToken();
    if (m_pickerExpected) {
        qInfo() << "ScreenCast capture: no screens remembered yet, the portal will ask once "
                   "which screens Snim may capture";
        emit sourcePickerExpected(s_pickMissedScreens);
    }

    Session::Options options;
    options.multiple = true;
    options.restoreTokenKey = QString::fromLatin1(kRestoreTokenKey);
    m_timeout->start();
    m_session->open(options);
}

void ScreencastFrameSource::handleReady(int pipewireFd)
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
    const unsigned long settleMs =
        (m_pickerExpected || m_clock.elapsed() > kInteractiveHandshakeMs) ? kPickerSettleMs : 0;
    QThread *worker = QThread::create([result, nodes, pipewireFd, settleMs] {
        QThread::msleep(settleMs);
        result->ok = PipeWireFrames::grab(pipewireFd, nodes, kFrameTimeoutMs,
                                                  &result->frames, &result->error);
        ::close(pipewireFd);
    });
    connect(worker, &QThread::finished, worker, &QObject::deleteLater);
    connect(worker, &QThread::finished, this, [this, result] {
        // Closing at once is what takes the screen-sharing indicator down.
        m_session->close();
        const qint64 grabbedMs = m_clock.elapsed();
        if (!result->ok) {
            fail(result->error);
            return;
        }

        QList<PlacedFrame> frames;
        for (qsizetype i = 0; i < result->streams.size(); ++i)
            frames.append({result->streams.at(i).rectLogical, result->frames.value(i)});
        frames = placeFramesWithoutGeometry(frames, qtVirtualDesktop());
        const QRect covered = coveredGeometry(frames);
        QImage image = stitchDesktop(frames, covered);
        if (image.isNull()) {
            fail(QStringLiteral("the ScreenCast frames were empty"));
            return;
        }
        // Only a lone stream without geometry can miss: its true scale is unknown.
        const bool frameCovers = frameCoversGeometry(image.size(), image.devicePixelRatio(), covered);
        if (!frameCovers)
            image.setDevicePixelRatio(qGuiApp->devicePixelRatio());
        QList<QRect> streamRects;
        for (const PlacedFrame &frame : std::as_const(frames)) {
            if (!frame.image.isNull())
                streamRects.append(frame.logical);
        }
        checkPickCoversScreens(streamRects, frameCovers);

        qInfo() << "ScreenCast capture:" << image.size() << "at DPR" << image.devicePixelRatio()
                << "frames after" << grabbedMs << "ms, total" << m_clock.elapsed() << "ms";

        QPixmap pixmap = QPixmap::fromImage(image);
        pixmap.setDevicePixelRatio(image.devicePixelRatio());
        m_busy = false;
        emit frameReady(pixmap, covered);
    });
    worker->start();
}

void ScreencastFrameSource::checkPickCoversScreens(const QList<QRect> &streamRects,
                                                   bool frameCovers)
{
    QList<QRect> screens;
    for (const QScreen *screen : QGuiApplication::screens())
        screens.append(screen->geometry());
    if (frameCovers && coversEveryScreen(streamRects, screens)) {
        s_pickMissedScreens = false;
        return;
    }
    // A screen left out of the pick, or plugged in since: the next grab asks again.
    qInfo() << "ScreenCast capture: the picked screens" << streamRects << "miss part of"
            << qtVirtualDesktop() << "so the pick is forgotten and the next capture asks again";
    QSettings().remove(QString::fromLatin1(kRestoreTokenKey));
    s_pickMissedScreens = true;
}

void ScreencastFrameSource::fail(const QString &reason, bool cancelled)
{
    m_timeout->stop();
    m_busy = false;
    emit frameFailed(reason, cancelled);
}

} // namespace Screen
