#include "PortalFrameSource.h"
#include "screen/DesktopStitch.h"

#include <QDebug>
#include <QFile>
#include <QImage>
#include <QTimer>
#include <QUrl>
#include <QtDBus/QDBusConnection>

namespace Screen {

namespace {

const QString kScreenshot = QStringLiteral("org.freedesktop.portal.Screenshot");

// Room for a first-time consent prompt or the dialog itself.
constexpr int kResponseTimeoutMs = 120000;

// Set once a portal refuses a silent request, so later requests skip straight to its dialog.
bool s_portalNeedsDialog = false;

} // namespace

bool PortalFrameSource::isPortalReachable()
{
    // The interface's version property answers only where a backend provides it.
    return Core::Portal::hasInterface(kScreenshot);
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
    m_request.stop();
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
    if (!QDBusConnection::sessionBus().isConnected()) {
        *error = QStringLiteral("no D-Bus session bus");
        return false;
    }
    if (!m_request.listen(this, SLOT(handleResponse(uint, QVariantMap)))) {
        *error = QStringLiteral("could not subscribe to the portal's response");
        return false;
    }

    QVariantMap options;
    options[QStringLiteral("handle_token")] = m_request.token();
    options[QStringLiteral("interactive")] = interactive;
    options[QStringLiteral("modal")] = false;
    m_interactive = interactive;

    const QString requestPath = m_request.path();
    Core::Portal::sendRequest(kScreenshot, QStringLiteral("Screenshot"),
                              {QString(), QVariant::fromValue(options)}, this,
                              [this, requestPath](const QString &message) {
        // A reply to a request this source already gave up on changes nothing.
        if (m_busy && m_request.path() == requestPath)
            finishFailed(QStringLiteral("the Screenshot call failed: %1").arg(message), false);
    });
    qInfo() << "Screenshot portal: requested" << (interactive ? "with its dialog" : "silently");
    m_timeout->start();
    return true;
}

void PortalFrameSource::finishFailed(const QString &reason, bool cancelled)
{
    m_timeout->stop();
    m_request.stop();
    m_busy = false;
    emit frameFailed(reason, cancelled);
}

void PortalFrameSource::handleResponse(uint status, const QVariantMap &results)
{
    if (!m_busy)
        return;
    m_timeout->stop();
    m_request.stop();

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
