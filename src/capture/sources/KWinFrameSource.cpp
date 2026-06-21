#include "KWinFrameSource.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusUnixFileDescriptor>
#include <QDBusVariant>
#include <QDataStream>
#include <QDebug>
#include <QFile>
#include <QFutureWatcher>
#include <QPainter>
#include <QtConcurrentRun>

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

namespace Capture {

namespace {

const QString kServiceName = QStringLiteral("org.kde.KWin.ScreenShot2");
const QString kObjectPath = QStringLiteral("/org/kde/KWin/ScreenShot2");
const QString kInterface = QStringLiteral("org.kde.KWin.ScreenShot2");
constexpr int kCallTimeoutMs = 4000;

} // namespace

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
    m_denied = false;
    m_cancelled = false;
    if (screens.isEmpty()) {
        emit frameFailed(QStringLiteral("no screens available"), false);
        return;
    }
    m_busy = true;
    m_images.clear();
    m_lastError.clear();
    m_pending = static_cast<int>(screens.size());

    if (screens.size() == 1) {
        if (m_apiVersion >= 2)
            captureScreen(QStringLiteral("CaptureActiveScreen"), {QVariant::fromValue(buildOptions())});
        else
            captureScreen(QStringLiteral("CaptureScreen"),
                          {screens.first()->name(), QVariant::fromValue(buildOptions())});
        return;
    }
    for (const QScreen *screen : screens)
        captureScreen(QStringLiteral("CaptureScreen"),
                      {screen->name(), QVariant::fromValue(buildOptions())});
}

void KWinFrameSource::captureScreen(const QString &method, const QVariantList &args)
{
    int pipeFds[2]{-1, -1};
    if (pipe2(pipeFds, O_CLOEXEC) == -1) {
        m_lastError = QStringLiteral("pipe2() failed: %1").arg(QString::fromLocal8Bit(strerror(errno)));
        qWarning() << m_lastError;
        screenDone();
        return;
    }

    QDBusMessage msg = QDBusMessage::createMethodCall(kServiceName, kObjectPath, kInterface, method);
    QVariantList fullArgs = args;
    fullArgs.append(QVariant::fromValue(QDBusUnixFileDescriptor(pipeFds[1])));
    msg.setArguments(fullArgs);

    const QDBusPendingCall pending = QDBusConnection::sessionBus().asyncCall(msg, kCallTimeoutMs);
    ::close(pipeFds[1]);

    const int readFd = pipeFds[0];
    auto *watcher = new QDBusPendingCallWatcher(pending, this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this, readFd, method](QDBusPendingCallWatcher *w) {
        w->deleteLater();
        const QDBusPendingReply<QVariantMap> reply = *w;
        if (reply.isError()) {
            ::close(readFd);
            const QString errorName = reply.error().name();
            qDebug() << "KWin" << method << "error:" << errorName << reply.error().message();
            if (errorName.contains(QLatin1String("NoAuthorized"))
                || errorName.contains(QLatin1String("AccessDenied")))
                m_denied = true;
            else if (errorName.contains(QLatin1String("Cancelled")))
                m_cancelled = true;
            m_lastError = reply.error().message();
            screenDone();
            return;
        }

        const QVariantMap metadata = reply;
        const QString type = metadata.value(QStringLiteral("type")).toString();
        if (type != QLatin1String("raw")) {
            ::close(readFd);
            m_lastError = QStringLiteral("unsupported KWin screenshot type: %1").arg(type);
            screenDone();
            return;
        }

        auto *futureWatcher = new QFutureWatcher<QImage>(this);
        connect(futureWatcher, &QFutureWatcher<QImage>::finished, this, [this, futureWatcher] {
            const QImage image = futureWatcher->result();
            futureWatcher->deleteLater();
            if (image.isNull())
                m_lastError = QStringLiteral("could not read the screenshot from KWin's pipe");
            else
                m_images.append(image);
            screenDone();
        });
        futureWatcher->setFuture(QtConcurrent::run(readImageFromPipe, readFd, metadata));
    });
}

void KWinFrameSource::screenDone()
{
    if (--m_pending > 0)
        return;
    m_busy = false;

    if (m_images.isEmpty()) {
        const QString reason = m_denied
            ? QStringLiteral("KWin refused the screenshot: no desktop entry authorizes "
                             "org.kde.KWin.ScreenShot2 for this app")
            : (m_lastError.isEmpty() ? QStringLiteral("KWin captured no screen") : m_lastError);
        emit frameFailed(reason, m_cancelled && !m_denied);
        return;
    }
    m_denied = false;

    const QImage composited = compositeScreenImages(m_images);
    m_images.clear();
    QPixmap frame = QPixmap::fromImage(composited);
    frame.setDevicePixelRatio(composited.devicePixelRatio());
    emit frameReady(frame, qtVirtualDesktop());
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

    // The screen's logical position, for compositing.
    const QString screenId = metadata.value("screen").toString();
    if (!screenId.isEmpty()) {
        for (QScreen *screen : QGuiApplication::screens()) {
            if (screen->name() == screenId) {
                const QPoint pos = screen->geometry().topLeft();
                image.setText("logicalX", QString::number(pos.x()));
                image.setText("logicalY", QString::number(pos.y()));
                break;
            }
        }
    }

    return image;
}

QImage KWinFrameSource::compositeScreenImages(const QList<QImage> &images)
{
    if (images.isEmpty())
        return {};
    if (images.size() == 1)
        return images.first();

    QRectF virtualRect;
    qreal maxDpr = 1.0;
    for (const QImage &img : images) {
        const qreal dpr = img.devicePixelRatio();
        maxDpr = qMax(maxDpr, dpr);
        const qreal lx = img.text("logicalX").toDouble();
        const qreal ly = img.text("logicalY").toDouble();
        virtualRect |= QRectF(lx, ly, img.width() / dpr, img.height() / dpr);
    }

    QImage result(QSize(virtualRect.width() * maxDpr, virtualRect.height() * maxDpr),
                  QImage::Format_RGBA8888_Premultiplied);
    result.fill(Qt::black);

    QPainter painter(&result);
    for (const QImage &img : images) {
        const qreal lx = img.text("logicalX").toDouble();
        const qreal ly = img.text("logicalY").toDouble();
        const QPointF offset((lx - virtualRect.x()) * maxDpr, (ly - virtualRect.y()) * maxDpr);
        painter.drawImage(QRectF(offset, img.size()), img);
    }
    painter.end();

    result.setDevicePixelRatio(maxDpr);
    return result;
}

} // namespace Capture
