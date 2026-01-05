#include "recording/strategies/ScreenCastPortalSession.h"

#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QDBusPendingCall>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusUnixFileDescriptor>
#include <QDBusVariant>
#include <QDebug>
#include <QSettings>
#include <QUuid>

#include <unistd.h>

namespace Recording {

namespace {

const QString kService = QStringLiteral("org.freedesktop.portal.Desktop");
const QString kPath = QStringLiteral("/org/freedesktop/portal/desktop");
const QString kScreenCast = QStringLiteral("org.freedesktop.portal.ScreenCast");
const QString kRequest = QStringLiteral("org.freedesktop.portal.Request");
const QString kSession = QStringLiteral("org.freedesktop.portal.Session");
const QString kProperties = QStringLiteral("org.freedesktop.DBus.Properties");
const QString kClosed = QStringLiteral("Closed");

const QString kRestoreTokenKey = QStringLiteral("recording/screencastRestoreToken");

const char *const kCreateSlot = SLOT(handleCreateSessionResponse(uint,QVariantMap));
const char *const kSelectSlot = SLOT(handleSelectSourcesResponse(uint,QVariantMap));
const char *const kStartSlot = SLOT(handleStartResponse(uint,QVariantMap));
const char *const kClosedSlot = SLOT(handleSessionClosed());
const char *const kClosedDetailsSlot = SLOT(handleSessionClosedWithDetails(QVariantMap));

// SelectSources source types and cursor modes, as the ScreenCast spec numbers them.
constexpr uint kSourceMonitor = 1;
constexpr uint kCursorHidden = 1;
constexpr uint kCursorEmbedded = 2;
constexpr uint kPersistWhileRunning = 2;

// The stream vardict carries position and size as (ii); a missing one leaves the rect
// null and the caller substitutes screen geometry.
QRect rectFromStream(const QVariantMap &properties)
{
    const QVariant position = properties.value(QStringLiteral("position"));
    const QVariant size = properties.value(QStringLiteral("size"));
    if (!position.canConvert<QDBusArgument>() || !size.canConvert<QDBusArgument>())
        return {};

    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;

    const QDBusArgument positionArg = position.value<QDBusArgument>();
    positionArg.beginStructure();
    positionArg >> x >> y;
    positionArg.endStructure();

    const QDBusArgument sizeArg = size.value<QDBusArgument>();
    sizeArg.beginStructure();
    sizeArg >> width >> height;
    sizeArg.endStructure();

    if (width <= 0 || height <= 0)
        return {};
    return {x, y, width, height};
}

} // namespace

ScreenCastPortalSession::ScreenCastPortalSession(QObject *parent) : QObject(parent)
{
}

ScreenCastPortalSession::~ScreenCastPortalSession()
{
    close();
}

bool ScreenCastPortalSession::isPortalAvailable()
{
    QDBusMessage msg = QDBusMessage::createMethodCall(kService, kPath, kProperties,
                                                      QStringLiteral("Get"));
    msg.setArguments({kScreenCast, QStringLiteral("version")});

    // Blocking, but bounded and only used to decide whether a recorder exists at all.
    const QDBusMessage reply = QDBusConnection::sessionBus().call(msg, QDBus::Block, 2000);
    return reply.type() == QDBusMessage::ReplyMessage;
}

void ScreenCastPortalSession::open(bool captureCursor)
{
    close();
    m_captureCursor = captureCursor;
    createSession();
}

void ScreenCastPortalSession::close()
{
    reset();
    m_restoreRetried = false;
}

void ScreenCastPortalSession::createSession()
{
    const QString handleToken = newToken();
    m_createRequestPath = requestPath(handleToken);

    // Connected before the call: the portal may answer before the method reply arrives.
    if (!connectResponse(m_createRequestPath, kCreateSlot)) {
        m_createRequestPath.clear();
        fail(tr("The screen sharing portal could not be listened to."));
        return;
    }

    QVariantMap options;
    options.insert(QStringLiteral("handle_token"), handleToken);
    options.insert(QStringLiteral("session_handle_token"), newToken());

    QDBusMessage msg = QDBusMessage::createMethodCall(kService, kPath, kScreenCast,
                                                      QStringLiteral("CreateSession"));
    msg.setArguments({QVariant::fromValue(options)});

    auto *watcher = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(msg), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this](QDBusPendingCallWatcher *w) {
        w->deleteLater();
        // The method reply only acknowledges the request; the answer is the Response signal.
        const QDBusPendingReply<QDBusObjectPath> reply = *w;
        if (!reply.isError())
            return;

        qWarning() << "ScreenCast CreateSession failed:" << reply.error().message();
        fail(tr("The screen sharing portal is not available: %1").arg(reply.error().message()));
    });
}

void ScreenCastPortalSession::handleCreateSessionResponse(uint response,
                                                          const QVariantMap &results)
{
    disconnectResponse(m_createRequestPath, kCreateSlot);
    m_createRequestPath.clear();

    if (response != 0) {
        fail(response == 1 ? tr("Screen sharing was cancelled.")
                           : tr("The desktop refused a screen sharing session."));
        return;
    }

    // Spec types session_handle as a string; some backends send an object path.
    const QVariant handle = results.value(QStringLiteral("session_handle"));
    m_sessionPath = handle.toString();
    if (m_sessionPath.isEmpty() && handle.canConvert<QDBusObjectPath>())
        m_sessionPath = handle.value<QDBusObjectPath>().path();

    if (m_sessionPath.isEmpty()) {
        fail(tr("The screen sharing portal returned no session handle."));
        return;
    }

    // Closed grew an a{sv} argument along the way, so listen for both spellings; the
    // handler ignores everything after the first one.
    QDBusConnection::sessionBus().connect(kService, m_sessionPath, kSession, kClosed, this,
                                          kClosedSlot);
    QDBusConnection::sessionBus().connect(kService, m_sessionPath, kSession, kClosed, this,
                                          kClosedDetailsSlot);

    selectSources();
}

void ScreenCastPortalSession::selectSources()
{
    const QString handleToken = newToken();
    m_selectRequestPath = requestPath(handleToken);

    if (!connectResponse(m_selectRequestPath, kSelectSlot)) {
        m_selectRequestPath.clear();
        fail(tr("The screen sharing portal could not be listened to."));
        return;
    }

    uint cursorMode = m_captureCursor ? kCursorEmbedded : kCursorHidden;
    const uint availableModes = readUintProperty(QStringLiteral("AvailableCursorModes"));
    if (availableModes != 0 && (availableModes & cursorMode) == 0)
        cursorMode = kCursorHidden;

    QVariantMap options;
    options.insert(QStringLiteral("handle_token"), handleToken);
    options.insert(QStringLiteral("types"), kSourceMonitor);
    options.insert(QStringLiteral("multiple"), false);
    options.insert(QStringLiteral("cursor_mode"), cursorMode);

    // Restore tokens landed in version 4; asking older portals for them is an error.
    if (readUintProperty(QStringLiteral("version")) >= 4) {
        options.insert(QStringLiteral("persist_mode"), kPersistWhileRunning);
        const QString restoreToken = QSettings().value(kRestoreTokenKey).toString();
        if (!restoreToken.isEmpty()) {
            options.insert(QStringLiteral("restore_token"), restoreToken);
            m_sentRestoreToken = restoreToken;
        }
    }

    QDBusMessage msg = QDBusMessage::createMethodCall(kService, kPath, kScreenCast,
                                                      QStringLiteral("SelectSources"));
    msg.setArguments({QVariant::fromValue(QDBusObjectPath(m_sessionPath)),
                      QVariant::fromValue(options)});

    auto *watcher = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(msg), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this](QDBusPendingCallWatcher *w) {
        w->deleteLater();
        const QDBusPendingReply<QDBusObjectPath> reply = *w;
        if (!reply.isError())
            return;

        qWarning() << "ScreenCast SelectSources failed:" << reply.error().message();
        fail(tr("The screen to record could not be selected: %1").arg(reply.error().message()));
    });
}

void ScreenCastPortalSession::handleSelectSourcesResponse(uint response,
                                                          const QVariantMap &results)
{
    Q_UNUSED(results)

    disconnectResponse(m_selectRequestPath, kSelectSlot);
    m_selectRequestPath.clear();

    if (response != 0) {
        fail(response == 1 ? tr("Screen sharing was cancelled.")
                           : tr("The desktop refused the requested screen source."));
        return;
    }

    start();
}

void ScreenCastPortalSession::start()
{
    const QString handleToken = newToken();
    m_startRequestPath = requestPath(handleToken);

    if (!connectResponse(m_startRequestPath, kStartSlot)) {
        m_startRequestPath.clear();
        fail(tr("The screen sharing portal could not be listened to."));
        return;
    }

    QVariantMap options;
    options.insert(QStringLiteral("handle_token"), handleToken);

    QDBusMessage msg = QDBusMessage::createMethodCall(kService, kPath, kScreenCast,
                                                      QStringLiteral("Start"));
    // Empty parent_window: a tray app has no window to parent the desktop's dialog to.
    msg.setArguments({QVariant::fromValue(QDBusObjectPath(m_sessionPath)),
                      QString(),
                      QVariant::fromValue(options)});

    auto *watcher = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(msg), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this](QDBusPendingCallWatcher *w) {
        w->deleteLater();
        const QDBusPendingReply<QDBusObjectPath> reply = *w;
        if (!reply.isError())
            return;

        qWarning() << "ScreenCast Start failed:" << reply.error().message();
        failStart(tr("Screen sharing could not be started: %1").arg(reply.error().message()));
    });
}

void ScreenCastPortalSession::handleStartResponse(uint response, const QVariantMap &results)
{
    disconnectResponse(m_startRequestPath, kStartSlot);
    m_startRequestPath.clear();

    if (response != 0) {
        failStart(response == 1 ? tr("Screen sharing was cancelled.")
                                : tr("The desktop refused to start screen sharing."));
        return;
    }

    const QString restoreToken = results.value(QStringLiteral("restore_token")).toString();
    if (!restoreToken.isEmpty())
        QSettings().setValue(kRestoreTokenKey, restoreToken);

    // streams is a(ua{sv}), which QtDBus cannot hand over as a typed value, so walk it.
    const QVariant streams = results.value(QStringLiteral("streams"));
    if (!streams.canConvert<QDBusArgument>()) {
        fail(tr("The screen sharing portal returned no stream."));
        return;
    }

    bool haveStream = false;
    const QDBusArgument arg = streams.value<QDBusArgument>();
    arg.beginArray();
    while (!arg.atEnd()) {
        uint nodeId = 0;
        QVariantMap properties;

        arg.beginStructure();
        arg >> nodeId;
        arg.beginMap();
        while (!arg.atEnd()) {
            QString key;
            QDBusVariant value;
            arg.beginMapEntry();
            arg >> key >> value;
            arg.endMapEntry();
            properties.insert(key, value.variant());
        }
        arg.endMap();
        arg.endStructure();

        // Only the first stream is recorded; multiple was false.
        if (!haveStream) {
            m_nodeId = nodeId;
            m_streamRect = rectFromStream(properties);
            haveStream = true;
        }
    }
    arg.endArray();

    if (!haveStream) {
        fail(tr("The screen sharing portal returned no stream."));
        return;
    }

    openPipeWireRemote();
}

void ScreenCastPortalSession::openPipeWireRemote()
{
    QDBusMessage msg = QDBusMessage::createMethodCall(kService, kPath, kScreenCast,
                                                      QStringLiteral("OpenPipeWireRemote"));
    // Not a Request: the reply itself carries the PipeWire socket.
    msg.setArguments({QVariant::fromValue(QDBusObjectPath(m_sessionPath)),
                      QVariant::fromValue(QVariantMap())});

    auto *watcher = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(msg), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this](QDBusPendingCallWatcher *w) {
        w->deleteLater();
        const QDBusPendingReply<QDBusUnixFileDescriptor> reply = *w;
        if (reply.isError()) {
            qWarning() << "ScreenCast OpenPipeWireRemote failed:" << reply.error().message();
            fail(tr("The PipeWire connection could not be opened: %1")
                     .arg(reply.error().message()));
            return;
        }

        // QDBusUnixFileDescriptor closes its copy when it dies, so hand out a dup; the
        // receiver owns that fd and must close it.
        const int fd = ::dup(reply.value().fileDescriptor());
        if (fd < 0) {
            fail(tr("The PipeWire connection could not be opened."));
            return;
        }

        emit ready(m_nodeId, m_streamRect, fd);
    });
}

void ScreenCastPortalSession::handleSessionClosed()
{
    if (m_sessionPath.isEmpty())
        return;

    // The portal already tore the session down, so drop the path before cleaning up and
    // no Close call goes to a dead object.
    QDBusConnection::sessionBus().disconnect(kService, m_sessionPath, kSession, kClosed, this,
                                             kClosedSlot);
    QDBusConnection::sessionBus().disconnect(kService, m_sessionPath, kSession, kClosed, this,
                                             kClosedDetailsSlot);
    m_sessionPath.clear();

    close();
    emit sessionClosed();
}

void ScreenCastPortalSession::handleSessionClosedWithDetails(const QVariantMap &details)
{
    Q_UNUSED(details)
    handleSessionClosed();
}

void ScreenCastPortalSession::failStart(const QString &error)
{
    // A restore_token the portal no longer knows makes Start fail outright, so drop it
    // and run the whole handshake once more without it.
    if (!m_sentRestoreToken.isEmpty() && !m_restoreRetried) {
        m_restoreRetried = true;
        QSettings().remove(kRestoreTokenKey);
        reset();
        createSession();
        return;
    }

    fail(error);
}

void ScreenCastPortalSession::fail(const QString &error)
{
    close();
    emit failed(error);
}

void ScreenCastPortalSession::reset()
{
    disconnectResponse(m_createRequestPath, kCreateSlot);
    m_createRequestPath.clear();
    disconnectResponse(m_selectRequestPath, kSelectSlot);
    m_selectRequestPath.clear();
    disconnectResponse(m_startRequestPath, kStartSlot);
    m_startRequestPath.clear();

    if (!m_sessionPath.isEmpty()) {
        QDBusConnection::sessionBus().disconnect(kService, m_sessionPath, kSession, kClosed, this,
                                                 kClosedSlot);
        QDBusConnection::sessionBus().disconnect(kService, m_sessionPath, kSession, kClosed, this,
                                                 kClosedDetailsSlot);
        QDBusConnection::sessionBus().asyncCall(
            QDBusMessage::createMethodCall(kService, m_sessionPath, kSession,
                                           QStringLiteral("Close")));
        m_sessionPath.clear();
    }

    m_sentRestoreToken.clear();
    m_nodeId = 0;
    m_streamRect = QRect();
}

bool ScreenCastPortalSession::connectResponse(const QString &path, const char *slot)
{
    const bool ok = QDBusConnection::sessionBus().connect(kService, path, kRequest,
                                                          QStringLiteral("Response"), this, slot);
    if (!ok)
        qWarning() << "Failed to connect portal Response signal on path:" << path;
    return ok;
}

void ScreenCastPortalSession::disconnectResponse(const QString &path, const char *slot)
{
    if (path.isEmpty())
        return;
    QDBusConnection::sessionBus().disconnect(kService, path, kRequest,
                                             QStringLiteral("Response"), this, slot);
}

QString ScreenCastPortalSession::newToken()
{
    // The token becomes an object-path element, which allows no braces or dashes.
    return QUuid::createUuid().toString().remove('-').remove('{').remove('}');
}

QString ScreenCastPortalSession::requestPath(const QString &token)
{
    // The portal derives this path from our unique name and the token, so it is known
    // before the call is sent.
    const QString sender =
        QDBusConnection::sessionBus().baseService().remove(':').replace('.', '_');
    return QStringLiteral("/org/freedesktop/portal/desktop/request/%1/%2").arg(sender, token);
}

uint ScreenCastPortalSession::readUintProperty(const QString &name)
{
    QDBusMessage msg = QDBusMessage::createMethodCall(kService, kPath, kProperties,
                                                      QStringLiteral("Get"));
    msg.setArguments({kScreenCast, name});

    const QDBusMessage reply = QDBusConnection::sessionBus().call(msg, QDBus::Block, 2000);
    if (reply.type() != QDBusMessage::ReplyMessage || reply.arguments().isEmpty())
        return 0;

    const QVariant value = reply.arguments().constFirst().value<QDBusVariant>().variant();
    bool ok = false;
    const uint number = value.toUInt(&ok);
    return ok ? number : 0;
}

} // namespace Recording
