#include "PortalFrameSource.h"
#include "screen/DesktopStitch.h"

#include <QDebug>
#include <QFile>
#include <QImage>
#include <QTimer>
#include <QUrl>
#include <QUuid>
#include <QtDBus/QDBusConnection>
#include <QtDBus/QDBusMessage>
#include <QtDBus/QDBusObjectPath>
#include <QtDBus/QDBusPendingCallWatcher>
#include <QtDBus/QDBusPendingReply>

namespace Screen {

namespace {

const QString kService = QStringLiteral("org.freedesktop.portal.Desktop");
const QString kPath = QStringLiteral("/org/freedesktop/portal/desktop");
const QString kRequest = QStringLiteral("org.freedesktop.portal.Request");
const QString kResponse = QStringLiteral("Response");
const QString kScreenshot = QStringLiteral("org.freedesktop.portal.Screenshot");

// Probes run on the GUI thread, so a stuck portal may only hold it this long.
constexpr int kProbeTimeoutMs = 2000;

// Room for a first-time consent prompt or the dialog itself.
constexpr int kResponseTimeoutMs = 120000;

// Set once a portal refuses a silent request, so later requests skip straight to its dialog.
bool s_portalNeedsDialog = false;
// A yes is kept for the run; a no is asked again, the portal may still be starting.
bool s_portalReachable = false;

} // namespace

bool PortalFrameSource::isPortalReachable()
{
    if (s_portalReachable)
        return true;
    // The interface's version property answers only where a backend provides it.
    QDBusMessage msg = QDBusMessage::createMethodCall(
        kService, kPath, QStringLiteral("org.freedesktop.DBus.Properties"), QStringLiteral("Get"));
    msg.setArguments({kScreenshot, QStringLiteral("version")});
    const QDBusMessage reply = QDBusConnection::sessionBus().call(msg, QDBus::Block, kProbeTimeoutMs);
    s_portalReachable = reply.type() == QDBusMessage::ReplyMessage;
    return s_portalReachable;
}

PortalFrameSource::PortalFrameSource(QObject *parent)
    : DesktopFrameSource(parent)
    , m_timeout(new QTimer(this))
{
    m_timeout->setSingleShot(true);
    m_timeout->setInterval(kResponseTimeoutMs);
    connect(m_timeout, &QTimer::timeout, this, [this] {
        finishFailed(QStringLiteral("the Screenshot portal did not answer"), false);
    });
}

PortalFrameSource::~PortalFrameSource()
{
    disconnectResponse();
}

void PortalFrameSource::grab()
{
    if (m_busy) {
        qDebug() << "Screenshot portal request already in flight";
        return;
    }
    m_busy = true;
    QString error;
    if (!request(s_portalNeedsDialog, &error))
        finishFailed(error, false);
}

bool PortalFrameSource::request(bool interactive, QString *error)
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected()) {
        *error = QStringLiteral("no D-Bus session bus");
        return false;
    }
    const QString token = QUuid::createUuid().toString(QUuid::Id128);
    // The portal derives the Request path from our unique name plus the token, so we
    // can subscribe BEFORE the call and never race the response.
    m_requestPath = kPath + QStringLiteral("/request/")
                    + bus.baseService().remove(':').replace('.', '_') + '/' + token;
    if (!bus.connect(kService, m_requestPath, kRequest, kResponse, this,
                     SLOT(handleResponse(uint, QVariantMap)))) {
        m_requestPath.clear();
        *error = QStringLiteral("could not subscribe to the portal's response");
        return false;
    }

    QVariantMap options;
    options[QStringLiteral("handle_token")] = token;
    options[QStringLiteral("interactive")] = interactive;
    options[QStringLiteral("modal")] = false;
    m_interactive = interactive;

    QDBusMessage msg = QDBusMessage::createMethodCall(kService, kPath, kScreenshot,
                                                      QStringLiteral("Screenshot"));
    msg.setArguments({QString(), QVariant::fromValue(options)});
    // Asynchronous: the reply only acknowledges the request, the Response signal answers it.
    auto *watcher = new QDBusPendingCallWatcher(bus.asyncCall(msg), this);
    const QString requestPath = m_requestPath;
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this, requestPath](QDBusPendingCallWatcher *w) {
        w->deleteLater();
        const QDBusPendingReply<QDBusObjectPath> reply = *w;
        if (reply.isError() && m_busy && m_requestPath == requestPath)
            finishFailed(QStringLiteral("the Screenshot call failed: %1").arg(reply.error().message()),
                         false);
    });
    qInfo() << "Screenshot portal: requested" << (interactive ? "with its dialog" : "silently");
    m_timeout->start();
    return true;
}

void PortalFrameSource::disconnectResponse()
{
    if (m_requestPath.isEmpty())
        return;
    QDBusConnection::sessionBus().disconnect(kService, m_requestPath, kRequest, kResponse, this,
                                             SLOT(handleResponse(uint, QVariantMap)));
    m_requestPath.clear();
}

void PortalFrameSource::finishFailed(const QString &reason, bool cancelled)
{
    m_timeout->stop();
    disconnectResponse();
    m_busy = false;
    emit frameFailed(reason, cancelled);
}

void PortalFrameSource::handleResponse(uint status, const QVariantMap &results)
{
    if (!m_busy)
        return;
    m_timeout->stop();
    disconnectResponse();

    if (status != 0 && !m_interactive) {
        qInfo() << "Screenshot portal: silent request refused, status" << status
                << "- asking again with its dialog";
        s_portalNeedsDialog = true;
        QString error;
        if (!request(true, &error))
            finishFailed(error, false);
        return;
    }
    if (status != 0) {   // 1 = cancelled by the user, 2 = other failure
        finishFailed(status == 1 ? QStringLiteral("the screenshot dialog was cancelled")
                                 : QStringLiteral("the Screenshot portal failed (status %1)").arg(status),
                     status == 1);
        return;
    }

    const QString filePath = QUrl(results.value(QStringLiteral("uri")).toString()).toLocalFile();
    const QImage image(filePath);
    QFile::remove(filePath);   // the portal hands us a throwaway file to own
    if (image.isNull()) {
        finishFailed(QStringLiteral("the portal's screenshot could not be loaded"), false);
        return;
    }

    const QRect virtualDesktop = qtVirtualDesktop();
    if (virtualDesktop.isEmpty()) {
        finishFailed(QStringLiteral("no screens to map the screenshot onto"), false);
        return;
    }

    // The portal returns physical pixels: recover the ratio against the logical desktop.
    qreal dpr = frameDevicePixelRatio(image, virtualDesktop);
    if (!frameCoversGeometry(image.size(), dpr, virtualDesktop)) {
        // A region or one monitor picked in the portal's dialog, whose own scale is unknown.
        qInfo() << "Screenshot portal: the pick covers only part of the desktop";
        dpr = qGuiApp->devicePixelRatio();
    }
    qDebug() << "Screenshot portal:" << image.size() << "over" << virtualDesktop << "DPR" << dpr;

    QPixmap frame = QPixmap::fromImage(image);
    frame.setDevicePixelRatio(dpr);
    m_busy = false;
    emit frameReady(frame, virtualDesktop);
}

} // namespace Screen
