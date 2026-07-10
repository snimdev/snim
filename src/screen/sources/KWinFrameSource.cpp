#include "KWinFrameSource.h"

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusUnixFileDescriptor>
#include <QDBusVariant>
#include <QDataStream>
#include <QDebug>
#include <QFile>
#include <QFutureWatcher>
#include <QtConcurrentRun>

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

namespace Screen {

namespace {

const QString kServiceName = QStringLiteral("org.kde.KWin.ScreenShot2");
const QString kObjectPath = QStringLiteral("/org/kde/KWin/ScreenShot2");
const QString kInterface = QStringLiteral("org.kde.KWin.ScreenShot2");
constexpr int kCallTimeoutMs = 4000;

} // namespace

bool KWinFrameSource::isServiceRegistered()
{
    return QDBusConnection::sessionBus().interface()->isServiceRegistered(kServiceName);
}

KWinFrameSource::KWinFrameSource(QObject *parent)
    : DesktopFrameSource(parent)
{
    auto msg = QDBusMessage::createMethodCall(kServiceName, kObjectPath,
                                              QStringLiteral("org.freedesktop.DBus.Properties"),
                                              QStringLiteral("Get"));
    msg.setArguments({kInterface, QStringLiteral("Version")});

    const QDBusMessage reply = QDBusConnection::sessionBus().call(msg);
    if (reply.type() == QDBusMessage::ReplyMessage) {
        m_apiVersion = reply.arguments().constFirst().value<QDBusVariant>().variant().toUInt();
        qDebug() << "KWin ScreenShot2 API version:" << m_apiVersion;
    }
}

QVariantMap KWinFrameSource::buildOptions(bool includeCursor, bool nativeResolution)
{
    QVariantMap options;
    if (includeCursor)
        options.insert(QStringLiteral("include-cursor"), true);
    if (nativeResolution)
        options.insert(QStringLiteral("native-resolution"), true);
    options.insert(QStringLiteral("include-shadow"), false);
    return options;
}

void KWinFrameSource::grab()
{
    if (m_busy) {
        qDebug() << "KWin screen grab already in progress";
        return;
    }
    const auto screens = QGuiApplication::screens();
    m_failed = false;
    m_denied = false;
    m_cancelled = false;
    if (screens.isEmpty()) {
        emit frameFailed(QStringLiteral("no screens available"), false);
        return;
    }
    m_busy = true;
    m_frames.clear();
    m_lastError.clear();
    m_pending = static_cast<int>(screens.size());

    if (screens.size() == 1) {
        const QRect logical = screens.first()->geometry();
        if (m_apiVersion >= 2)
            captureScreen(QStringLiteral("CaptureActiveScreen"), {QVariant::fromValue(buildOptions())},
                          logical);
        else
            captureScreen(QStringLiteral("CaptureScreen"),
                          {screens.first()->name(), QVariant::fromValue(buildOptions())}, logical);
        return;
    }
    for (const QScreen *screen : screens)
        captureScreen(QStringLiteral("CaptureScreen"),
                      {screen->name(), QVariant::fromValue(buildOptions())}, screen->geometry());
}

void KWinFrameSource::call(const QString &method, const QVariantList &args, int timeoutMs,
                           QObject *context, ShotHandler done)
{
    int pipeFds[2]{-1, -1};
    if (pipe2(pipeFds, O_CLOEXEC) == -1) {
        Shot shot;
        shot.error = QStringLiteral("pipe2() failed: %1").arg(QString::fromLocal8Bit(strerror(errno)));
        qWarning() << shot.error;
        QMetaObject::invokeMethod(context, [done, shot] { done(shot); }, Qt::QueuedConnection);
        return;
    }

    QDBusMessage msg = QDBusMessage::createMethodCall(kServiceName, kObjectPath, kInterface, method);
    QVariantList fullArgs = args;
    fullArgs.append(QVariant::fromValue(QDBusUnixFileDescriptor(pipeFds[1])));
    msg.setArguments(fullArgs);

    const QDBusPendingCall pending = QDBusConnection::sessionBus().asyncCall(msg, timeoutMs);
    ::close(pipeFds[1]);

    const int readFd = pipeFds[0];
    auto *watcher = new QDBusPendingCallWatcher(pending, context);
    QObject::connect(watcher, &QDBusPendingCallWatcher::finished, context,
                     [context, readFd, method, done](QDBusPendingCallWatcher *w) {
        w->deleteLater();
        const QDBusPendingReply<QVariantMap> reply = *w;
        if (reply.isError()) {
            ::close(readFd);
            const QString errorName = reply.error().name();
            qDebug() << "KWin" << method << "error:" << errorName << reply.error().message();
            Shot shot;
            shot.denied = errorName.contains(QLatin1String("NoAuthorized"))
                          || errorName.contains(QLatin1String("AccessDenied"));
            shot.cancelled = !shot.denied && errorName.contains(QLatin1String("Cancelled"));
            shot.error = reply.error().message();
            done(shot);
            return;
        }

        const QVariantMap metadata = reply;
        const QString type = metadata.value(QStringLiteral("type")).toString();
        if (type != QLatin1String("raw")) {
            ::close(readFd);
            Shot shot;
            shot.error = QStringLiteral("unsupported KWin screenshot type: %1").arg(type);
            done(shot);
            return;
        }

        auto *futureWatcher = new QFutureWatcher<QImage>(context);
        QObject::connect(futureWatcher, &QFutureWatcher<QImage>::finished, context,
                         [futureWatcher, done] {
            Shot shot;
            shot.image = futureWatcher->result();
            futureWatcher->deleteLater();
            if (shot.image.isNull())
                shot.error = QStringLiteral("could not read the screenshot from KWin's pipe");
            done(shot);
        });
        futureWatcher->setFuture(QtConcurrent::run(readImageFromPipe, readFd, metadata));
    });
}

void KWinFrameSource::captureScreen(const QString &method, const QVariantList &args,
                                    const QRect &logical)
{
    call(method, args, kCallTimeoutMs, this, [this, logical](const Shot &shot) {
        if (shot.image.isNull()) {
            m_failed = true;
            m_denied = m_denied || shot.denied;
            m_cancelled = m_cancelled || shot.cancelled;
            m_lastError = shot.error;
        } else {
            m_frames.append({logical, shot.image});
        }
        screenDone();
    });
}

void KWinFrameSource::screenDone()
{
    if (--m_pending > 0)
        return;
    m_busy = false;

    if (m_failed || m_frames.isEmpty()) {
        m_frames.clear();
        const QString reason = m_denied
            ? QStringLiteral("KWin refused the screenshot: no desktop entry authorizes "
                             "org.kde.KWin.ScreenShot2 for this app")
            : (m_lastError.isEmpty() ? QStringLiteral("KWin captured no screen") : m_lastError);
        emit frameFailed(reason, m_cancelled && !m_denied);
        return;
    }
    m_denied = false;

    const QRect virtualDesktop = qtVirtualDesktop();
    const QImage stitched = stitchDesktop(m_frames, virtualDesktop);
    m_frames.clear();
    if (stitched.isNull()) {
        emit frameFailed(QStringLiteral("KWin's screenshots could not be placed on the desktop"), false);
        return;
    }
    QPixmap frame = QPixmap::fromImage(stitched);
    frame.setDevicePixelRatio(stitched.devicePixelRatio());
    emit frameReady(frame, virtualDesktop);
}

QImage KWinFrameSource::readImageFromPipe(int fd, const QVariantMap &metadata)
{
    QFile file;
    if (!file.open(fd, QFileDevice::ReadOnly, QFileDevice::AutoCloseHandle)) {
        ::close(fd);
        qWarning() << "Failed to open pipe FD for reading";
        return {};
    }

    bool ok = false;
    const int width = metadata.value("width").toInt(&ok);
    if (!ok || width <= 0) {
        qWarning() << "Bad width from KWin metadata:" << metadata.value("width");
        return {};
    }

    const int height = metadata.value("height").toInt(&ok);
    if (!ok || height <= 0) {
        qWarning() << "Bad height from KWin metadata:" << metadata.value("height");
        return {};
    }

    const uint format = metadata.value("format").toUInt(&ok);
    if (!ok || format <= QImage::Format_Invalid || format >= QImage::NImageFormats) {
        qWarning() << "Bad format from KWin metadata:" << metadata.value("format");
        return {};
    }

    QImage image(width, height, static_cast<QImage::Format>(format));

    const qreal scale = metadata.value("scale").toReal(&ok);
    if (ok && scale > 0)
        image.setDevicePixelRatio(scale);

    QDataStream stream(&file);
    stream.readRawData(reinterpret_cast<char *>(image.bits()), image.sizeInBytes());
    return image;
}

} // namespace Screen
