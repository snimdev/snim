#include "capture/FrozenFrameGrabber.h"

#include <QGuiApplication>
#include <QList>
#include <QPainter>
#include <QScreen>
#include <algorithm>

#ifdef Q_OS_LINUX
#include <QFile>
#include <QImage>
#include <QUrl>
#include <QUuid>
#include <QtDBus/QDBusConnection>
#include <QtDBus/QDBusInterface>
#include <QtDBus/QDBusObjectPath>
#include <QtDBus/QDBusReply>
#endif

namespace Capture {

FrozenFrameGrabber::FrozenFrameGrabber(QObject *parent)
    : QObject(parent)
{
}

QPixmap FrozenFrameGrabber::grabAllScreens(QRect &virtualGeometryOut)
{
    const QList<QScreen*> screens = QGuiApplication::screens();
    if (screens.isEmpty())
        return {};

    QRect virtualDesktop;
    for (QScreen *s : screens)
        virtualDesktop = virtualDesktop.united(s->geometry());
    virtualGeometryOut = virtualDesktop;

    qreal dpr = 1.0;
    for (QScreen *s : screens)
        dpr = std::max(dpr, s->devicePixelRatio());

    QPixmap full(virtualDesktop.size() * dpr);
    full.setDevicePixelRatio(dpr);
    full.fill(Qt::black);
    QPainter painter(&full);
    for (QScreen *s : screens) {
        const QRect geo = s->geometry();
        const QPixmap shot = s->grabWindow(0);
        const QRect destLogical(geo.topLeft() - virtualDesktop.topLeft(), geo.size());
        painter.drawPixmap(destLogical, shot, shot.rect());
    }
    painter.end();
    return full;
}

void FrozenFrameGrabber::grab(Done done)
{
    if (!done)
        return;
    if (m_done)
        return;   // a portal request is still in flight: ignore the re-entry

#ifdef Q_OS_LINUX
    if (QGuiApplication::platformName() == QLatin1String("wayland")) {
        m_done = std::move(done);
        if (!requestPortalFrame()) {
            const Done failed = std::move(m_done);
            m_done = nullptr;
            failed({}, {});
        }
        return;
    }
#endif

    // Synchronous on every non-Wayland platform (macOS, X11, offscreen).
    QRect virtualGeometry;
    QPixmap frozen = grabAllScreens(virtualGeometry);
    done(frozen, virtualGeometry);
}

#ifdef Q_OS_LINUX
bool FrozenFrameGrabber::requestPortalFrame()
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected())
        return false;

    QDBusInterface portal("org.freedesktop.portal.Desktop",
                          "/org/freedesktop/portal/desktop",
                          "org.freedesktop.portal.Screenshot",
                          bus);
    if (!portal.isValid())
        return false;

    const QString token = QUuid::createUuid().toString()
                              .remove('-').remove('{').remove('}');
    // The portal derives the Request path from our unique name plus the token, so we
    // can subscribe BEFORE the call and never race the response.
    m_requestPath = "/org/freedesktop/portal/desktop/request/"
                    + bus.baseService().remove(':').replace('.', '_')
                    + "/" + token;

    if (!bus.connect("org.freedesktop.portal.Desktop",
                     m_requestPath,
                     "org.freedesktop.portal.Request",
                     "Response",
                     this,
                     SLOT(handlePortalResponse(uint, QVariantMap)))) {
        m_requestPath.clear();
        return false;
    }

    QVariantMap options;
    options["handle_token"] = token;
    options["interactive"] = false;   // freeze the desktop as it is, no portal picker
    options["modal"] = false;

    const QDBusReply<QDBusObjectPath> reply =
        portal.call("Screenshot", QString(""), QVariant::fromValue(options));
    if (!reply.isValid()) {
        disconnectPortalResponse();
        return false;
    }
    return true;
}

void FrozenFrameGrabber::disconnectPortalResponse()
{
    if (m_requestPath.isEmpty())
        return;
    QDBusConnection::sessionBus().disconnect("org.freedesktop.portal.Desktop",
                                             m_requestPath,
                                             "org.freedesktop.portal.Request",
                                             "Response",
                                             this,
                                             SLOT(handlePortalResponse(uint, QVariantMap)));
    m_requestPath.clear();
}

void FrozenFrameGrabber::handlePortalResponse(uint status, QVariantMap results)
{
    if (!m_done)
        return;
    disconnectPortalResponse();
    const Done done = std::move(m_done);
    m_done = nullptr;

    if (status != 0) {   // 1 = cancelled by the user, 2 = other failure
        done({}, {});
        return;
    }

    const QString filePath = QUrl(results.value("uri").toString()).toLocalFile();
    const QImage image(filePath);
    QFile::remove(filePath);   // the portal hands us a throwaway file to own
    if (image.isNull()) {
        done({}, {});
        return;
    }

    QRect virtualDesktop;
    for (QScreen *s : QGuiApplication::screens())
        virtualDesktop = virtualDesktop.united(s->geometry());
    if (virtualDesktop.isEmpty()) {
        done({}, {});
        return;
    }

    // The portal returns physical pixels: recover the ratio against the logical desktop.
    const qreal dpr = qMax(image.width() * 1.0 / virtualDesktop.width(),
                           image.height() * 1.0 / virtualDesktop.height());

    QPixmap frozen = QPixmap::fromImage(image);
    frozen.setDevicePixelRatio(dpr);
    done(frozen, virtualDesktop);
}
#endif // Q_OS_LINUX

} // namespace Capture
