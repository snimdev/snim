#include "KWinCaptureStrategy.h"
#include "../AreaSelector.h"

#include <QApplication>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusInterface>
#include <QDBusMessage>
#include <QDBusPendingCall>
#include <QDBusPendingReply>
#include <QDBusUnixFileDescriptor>
#include <QDataStream>
#include <QDebug>
#include <QFile>
#include <QFileDevice>
#include <QFuture>
#include <QFutureWatcher>
#include <QGuiApplication>
#include <QPainter>
#include <QScreen>
#include <QWindow>
#include <QtConcurrentRun>

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
{
    // Query API version
    auto msg = QDBusMessage::createMethodCall(
        kServiceName, kObjectPath,
        QStringLiteral("org.freedesktop.DBus.Properties"),
        QStringLiteral("Get"));
    msg.setArguments({kInterface, QStringLiteral("Version")});

    const QDBusMessage reply = QDBusConnection::sessionBus().call(msg);
    if (reply.type() == QDBusMessage::ReplyMessage) {
        m_apiVersion = reply.arguments().constFirst().value<QDBusVariant>().variant().toUInt();
        qDebug() << "KWin ScreenShot2 API version:" << m_apiVersion;
    }
}

bool KWinCaptureStrategy::isKWinAvailable()
{
    return QDBusConnection::sessionBus().interface()->isServiceRegistered(kServiceName);
}

bool KWinCaptureStrategy::isAvailable() const
{
    return isKWinAvailable() && m_apiVersion > 0;
}

// --- Options builder ---

QVariantMap KWinCaptureStrategy::buildOptions(bool includeCursor, bool nativeResolution)
{
    QVariantMap options;
    if (includeCursor) {
        options.insert(QStringLiteral("include-cursor"), true);
    }
    if (nativeResolution) {
        options.insert(QStringLiteral("native-resolution"), true);
    }
    options.insert(QStringLiteral("include-shadow"), false);
    return options;
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
    if (m_apiVersion >= 2) {
        QVariantList args;
        args << QVariant::fromValue(buildOptions());
        callScreenShotMethod(QStringLiteral("CaptureActiveWindow"), args, false);
    } else {
        // Fallback: interactive window pick
        fallbackToInteractive(false, 0); // kind=0 = window
    }
}

// --- Workspace capture (per-screen, then composite) ---

void KWinCaptureStrategy::captureWorkspace(bool showSelector)
{
    const auto screens = QGuiApplication::screens();
    if (screens.isEmpty()) {
        emit screenshotFailed("No screens available");
        return;
    }

    // For single screen, use CaptureActiveScreen (simpler)
    if (screens.size() == 1) {
        QVariantList args;
        args << QVariant::fromValue(buildOptions());

        if (m_apiVersion >= 2) {
            callScreenShotMethod(QStringLiteral("CaptureActiveScreen"), args, showSelector);
        } else {
            // CaptureScreen with screen name
            QVariantList screenArgs;
            screenArgs << screens.first()->name() << QVariant::fromValue(buildOptions());
            callScreenShotMethod(QStringLiteral("CaptureScreen"), screenArgs, showSelector);
        }
        return;
    }

    // Multi-screen: capture each screen individually, composite results
    auto *results = new QList<QImage>();
    auto *remaining = new int(screens.size());
    bool *failed = new bool(false);

    for (const QScreen *screen : screens) {
        int pipeFds[2]{-1, -1};
        if (pipe2(pipeFds, O_CLOEXEC) == -1) {
            qWarning() << "pipe2() failed:" << strerror(errno);
            continue;
        }

        QDBusMessage msg = QDBusMessage::createMethodCall(kServiceName, kObjectPath, kInterface,
                                                           QStringLiteral("CaptureScreen"));
        QVariantList args;
        args << screen->name() << QVariant::fromValue(buildOptions());
        args << QVariant::fromValue(QDBusUnixFileDescriptor(pipeFds[1]));
        msg.setArguments(args);

        QDBusPendingCall pending = QDBusConnection::sessionBus().asyncCall(msg, 4000);
        ::close(pipeFds[1]);

        int readFd = pipeFds[0];
        auto *watcher = new QDBusPendingCallWatcher(pending, this);

        connect(watcher, &QDBusPendingCallWatcher::finished, this,
                [this, watcher, readFd, results, remaining, failed, showSelector](void) {
            watcher->deleteLater();
            const QDBusPendingReply<QVariantMap> reply = *watcher;

            if (reply.isError()) {
                ::close(readFd);
                qDebug() << "CaptureScreen error:" << reply.error().name() << reply.error().message();

                // On permission error, fall back to interactive for the whole workspace
                if (reply.error().name().contains("NoAuthorized") ||
                    reply.error().name().contains("AccessDenied")) {
                    *failed = true;
                }
            } else {
                const QVariantMap &metadata = reply;
                QImage image = readImageFromPipe(readFd, metadata);
                if (!image.isNull()) {
                    results->append(image);
                }
            }

            (*remaining)--;
            if (*remaining == 0) {
                bool permFailed = *failed;
                delete remaining;
                delete failed;

                if (permFailed && results->isEmpty()) {
                    delete results;
                    qDebug() << "Permission denied, falling back to CaptureInteractive";
                    fallbackToInteractive(showSelector, 1);
                    return;
                }

                if (results->isEmpty()) {
                    delete results;
                    emit screenshotFailed("Failed to capture any screen");
                    return;
                }

                QImage composited = compositeScreenImages(*results);
                delete results;

                QPixmap screenshot = QPixmap::fromImage(composited);
                screenshot.setDevicePixelRatio(composited.devicePixelRatio());

                if (showSelector) {
                    QRect virtualDesktop;
                    for (QScreen *s : QGuiApplication::screens()) {
                        virtualDesktop = virtualDesktop.united(s->geometry());
                    }
                    this->showAreaSelector(screenshot, virtualDesktop);
                } else {
                    emit screenshotReady(screenshot);
                }
            }
        });
    }
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
    handleReply(watcher, pipeFds[0], showAreaSel, method);
}

// --- Async reply handler ---

void KWinCaptureStrategy::handleReply(QDBusPendingCallWatcher *watcher, int readFd,
                                       bool showAreaSel, const QString &method)
{
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this, readFd, showAreaSel, method](QDBusPendingCallWatcher *w) {
        w->deleteLater();
        const QDBusPendingReply<QVariantMap> reply = *w;

        if (reply.isError()) {
            ::close(readFd);
            const QString errorName = reply.error().name();
            qDebug() << "KWin" << method << "error:" << errorName << reply.error().message();

            // Permission denied: fall back to CaptureInteractive
            if (errorName.contains("NoAuthorized") || errorName.contains("AccessDenied")) {
                qDebug() << "Permission denied, falling back to CaptureInteractive";
                int kind = (method == "CaptureActiveWindow") ? 0 : 1;
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

        QFuture<QImage> future = QtConcurrent::run(readImageFromPipe, readFd, metadata);
        futureWatcher->setFuture(future);
    });
}

// --- Read raw pixels from pipe ---

QImage KWinCaptureStrategy::readImageFromPipe(int fd, const QVariantMap &metadata)
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

    qreal scale = metadata.value("scale").toReal(&ok);
    if (ok && scale > 0) {
        image.setDevicePixelRatio(scale);
    }

    // Store screen name for compositing position lookup
    const QString screenId = metadata.value("screen").toString();

    QDataStream stream(&file);
    stream.readRawData(reinterpret_cast<char *>(image.bits()), image.sizeInBytes());

    // Store logical position for compositing
    if (!screenId.isEmpty()) {
        for (QScreen *screen : QGuiApplication::screens()) {
            if (screen->name() == screenId) {
                QPoint pos = screen->geometry().topLeft();
                image.setText("logicalX", QString::number(pos.x()));
                image.setText("logicalY", QString::number(pos.y()));
                break;
            }
        }
    }

    return image;
}

// --- Composite multiple screen images ---

QImage KWinCaptureStrategy::compositeScreenImages(const QList<QImage> &images)
{
    if (images.isEmpty()) return {};
    if (images.size() == 1) return images.first();

    // Find virtual desktop bounds and max DPR
    QRectF virtualRect;
    qreal maxDpr = 1.0;

    for (const QImage &img : images) {
        qreal dpr = img.devicePixelRatio();
        maxDpr = qMax(maxDpr, dpr);

        qreal lx = img.text("logicalX").toDouble();
        qreal ly = img.text("logicalY").toDouble();
        QRectF logicalRect(lx, ly, img.width() / dpr, img.height() / dpr);
        virtualRect |= logicalRect;
    }

    // Create composited image
    QImage result(QSize(virtualRect.width() * maxDpr, virtualRect.height() * maxDpr),
                  QImage::Format_RGBA8888_Premultiplied);
    result.fill(Qt::black);

    QPainter painter(&result);
    for (const QImage &img : images) {
        qreal lx = img.text("logicalX").toDouble();
        qreal ly = img.text("logicalY").toDouble();
        QPointF offset((lx - virtualRect.x()) * maxDpr, (ly - virtualRect.y()) * maxDpr);
        painter.drawImage(QRectF(offset, img.size()), img);
    }
    painter.end();

    result.setDevicePixelRatio(maxDpr);
    return result;
}

// --- CaptureInteractive fallback ---

void KWinCaptureStrategy::fallbackToInteractive(bool showSelector, int kind)
{
    qDebug() << "Using CaptureInteractive (kind:" << kind << ") — no .desktop permissions needed";

    QVariantList args;
    args << quint32(kind) << QVariant::fromValue(buildOptions());
    callScreenShotMethod(QStringLiteral("CaptureInteractive"), args, showSelector, 60000);
}

// --- Area selector (reused from WaylandCaptureStrategy pattern) ---

void KWinCaptureStrategy::showAreaSelector(const QPixmap &screenshot, const QRect &virtualGeometry)
{
    qDebug() << "Showing area selector, virtual geometry:" << virtualGeometry
             << "screenshot:" << screenshot.size() << "DPR:" << screenshot.devicePixelRatio();

    QList<QScreen*> screens = QGuiApplication::screens();
    auto *selectors = new QList<AreaSelector*>();

    for (QScreen *screen : screens) {
        QRect screenGeometry = screen->geometry();
        auto *selector = new AreaSelector();
        selector->setScreenshot(screenshot);
        selector->setVirtualGeometry(virtualGeometry);
        selector->setScreenOffset(screenGeometry.topLeft());

        selector->setGeometry(screenGeometry);
        selector->setWindowState(Qt::WindowFullScreen);
        selector->windowHandle()->setScreen(screen);
        selector->showFullScreen();

        selectors->append(selector);
    }

    for (auto *selector : *selectors) {
        connect(selector, &AreaSelector::areaSelected,
                this, [this, selectors, screenshot, virtualGeometry](const QRect &area) {
            for (auto *sel : *selectors) {
                sel->blockSignals(true);
                sel->close();
                sel->deleteLater();
            }
            selectors->clear();
            delete selectors;

            if (area.isEmpty()) {
                qDebug() << "Area selection cancelled";
                return;
            }

            qreal dpr = screenshot.devicePixelRatio();
            QRect physicalArea(
                (area.x() - virtualGeometry.x()) * dpr,
                (area.y() - virtualGeometry.y()) * dpr,
                area.width() * dpr,
                area.height() * dpr
            );
            physicalArea = physicalArea.intersected(screenshot.rect());

            QPixmap finalScreenshot = screenshot.copy(physicalArea);
            finalScreenshot.setDevicePixelRatio(dpr);

            emit screenshotReady(finalScreenshot);
        });
    }
}

} // namespace Capture
