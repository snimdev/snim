#include "screen/ScreenCastPortalSession.h"

#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusUnixFileDescriptor>
#include <QDBusVariant>
#include <QDebug>
#include <QSettings>

#include <unistd.h>

namespace Screen {

namespace {

namespace Portal = Core::Portal;

const QString kScreenCast = QStringLiteral("org.freedesktop.portal.ScreenCast");
const QString kSession = QStringLiteral("org.freedesktop.portal.Session");
const QString kClosed = QStringLiteral("Closed");

const char *const kCreateSlot = SLOT(handleCreateSessionResponse(uint,QVariantMap));
const char *const kSelectSlot = SLOT(handleSelectSourcesResponse(uint,QVariantMap));
const char *const kStartSlot = SLOT(handleStartResponse(uint,QVariantMap));
const char *const kClosedSlot = SLOT(handleSessionClosed());
const char *const kClosedDetailsSlot = SLOT(handleSessionClosedWithDetails(QVariantMap));

// SelectSources source types and cursor modes, as the ScreenCast spec numbers them.
constexpr uint kSourceMonitor = 1;
constexpr uint kSourceWindow = 2;
constexpr uint kCursorHidden = 1;
constexpr uint kCursorEmbedded = 2;
constexpr uint kPersistUntilRevoked = 2;

// One ScreenCast property; 0 when the portal does not say.
uint screenCastProperty(const char *name)
{
    return Portal::property(kScreenCast, QLatin1String(name)).toUInt();
}

uint sourceType(ScreenCastPortalSession::Source source)
{
    return source == ScreenCastPortalSession::Source::Window ? kSourceWindow : kSourceMonitor;
}

// The stream vardict carries position and size as (ii); a missing one leaves the rect
// null and the caller infers the geometry from the stream's pixel size.
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

uint ScreenCastPortalSession::portalVersion()
{
    return screenCastProperty("version");
}

QVariantMap ScreenCastPortalSession::sourceSelection(const Options &options,
                                                     const Capabilities &portal,
                                                     const QString &restoreToken)
{
    uint cursorMode = options.captureCursor ? kCursorEmbedded : kCursorHidden;
    if (portal.cursorModes != 0 && (portal.cursorModes & cursorMode) == 0)
        cursorMode = kCursorHidden;

    QVariantMap selection;
    selection.insert(QStringLiteral("types"), sourceType(options.source));
    selection.insert(QStringLiteral("multiple"), options.multiple);
    selection.insert(QStringLiteral("cursor_mode"), cursorMode);
    if (!options.restoreTokenKey.isEmpty() && portal.version >= kFirstPersistingVersion) {
        selection.insert(QStringLiteral("persist_mode"), kPersistUntilRevoked);
        if (!restoreToken.isEmpty())
            selection.insert(QStringLiteral("restore_token"), restoreToken);
    }
    return selection;
}

bool ScreenCastPortalSession::offers(Source source, const Capabilities &portal)
{
    return portal.sourceTypes == 0 || (portal.sourceTypes & sourceType(source)) != 0;
}

QString ScreenCastPortalSession::restoreToken(const QString &key)
{
    return key.isEmpty() ? QString() : QSettings().value(key).toString();
}

void ScreenCastPortalSession::storeRestoreToken(const QString &key, const QVariantMap &startResults)
{
    if (key.isEmpty())
        return;
    const QString token = startResults.value(QStringLiteral("restore_token")).toString();
    if (token.isEmpty())
        QSettings().remove(key);
    else
        QSettings().setValue(key, token);
}

void ScreenCastPortalSession::open(const Options &options)
{
    close();

    m_options = options;
    m_cancelled = false;
    createSession();
}

void ScreenCastPortalSession::close()
{
    reset();
}

void ScreenCastPortalSession::createSession()
{
    // Connected before the call: the portal may answer before the method reply arrives.
    if (!m_createRequest.listen(this, kCreateSlot)) {
        fail(tr("The screen sharing portal could not be listened to."));
        return;
    }

    QVariantMap options;
    options.insert(QStringLiteral("handle_token"), m_createRequest.token());
    options.insert(QStringLiteral("session_handle_token"), Portal::newToken());
    Portal::sendRequest(kScreenCast, QStringLiteral("CreateSession"),
                        {QVariant::fromValue(options)}, this, [this](const QString &error) {
        fail(tr("The screen sharing portal is not available: %1").arg(error));
    });
}

void ScreenCastPortalSession::handleCreateSessionResponse(uint response,
                                                          const QVariantMap &results)
{
    m_createRequest.stop();

    if (response != 0) {
        failResponse("CreateSession", response);
        return;
    }

    m_sessionPath = Portal::sessionHandle(results);
    if (m_sessionPath.isEmpty()) {
        fail(tr("The screen sharing portal returned no session handle."));
        return;
    }

    watchClosed(true);

    selectSources();
}

void ScreenCastPortalSession::selectSources()
{
    // Each property is a blocking read, so only those that decide something.
    Capabilities portal;
    portal.cursorModes = screenCastProperty("AvailableCursorModes");
    if (!m_options.restoreTokenKey.isEmpty())
        portal.version = portalVersion();
    if (m_options.source != Source::Monitor)
        portal.sourceTypes = screenCastProperty("AvailableSourceTypes");
    if (!offers(m_options.source, portal)) {
        fail(tr("This desktop's screen sharing cannot share a single window. "
                "Record an area instead."));
        return;
    }

    if (!m_selectRequest.listen(this, kSelectSlot)) {
        fail(tr("The screen sharing portal could not be listened to."));
        return;
    }

    QVariantMap options = sourceSelection(m_options, portal,
                                          restoreToken(m_options.restoreTokenKey));
    options.insert(QStringLiteral("handle_token"), m_selectRequest.token());
    Portal::sendRequest(kScreenCast, QStringLiteral("SelectSources"),
                        {QVariant::fromValue(QDBusObjectPath(m_sessionPath)),
                         QVariant::fromValue(options)},
                        this, [this](const QString &error) {
        fail(tr("The screen to record could not be selected: %1").arg(error));
    });
}

void ScreenCastPortalSession::handleSelectSourcesResponse(uint response,
                                                          const QVariantMap &results)
{
    Q_UNUSED(results)

    m_selectRequest.stop();

    if (response != 0) {
        failResponse("SelectSources", response);
        return;
    }

    start();
}

void ScreenCastPortalSession::start()
{
    if (!m_startRequest.listen(this, kStartSlot)) {
        fail(tr("The screen sharing portal could not be listened to."));
        return;
    }

    QVariantMap options;
    options.insert(QStringLiteral("handle_token"), m_startRequest.token());
    // Empty parent_window: a tray app has no window to parent the desktop's dialog to.
    Portal::sendRequest(kScreenCast, QStringLiteral("Start"),
                        {QVariant::fromValue(QDBusObjectPath(m_sessionPath)), QString(),
                         QVariant::fromValue(options)},
                        this, [this](const QString &error) {
        fail(tr("Screen sharing could not be started: %1").arg(error));
    });
}

void ScreenCastPortalSession::handleStartResponse(uint response, const QVariantMap &results)
{
    m_startRequest.stop();

    if (response != 0) {
        failResponse("Start", response);
        return;
    }

    // streams is a(ua{sv}), which QtDBus cannot hand over as a typed value, so walk it.
    const QVariant streams = results.value(QStringLiteral("streams"));
    if (!streams.canConvert<QDBusArgument>()) {
        fail(tr("The screen sharing portal returned no stream."));
        return;
    }

    storeRestoreToken(m_options.restoreTokenKey, results);

    m_streams.clear();
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

        m_streams.append(Stream{nodeId, rectFromStream(properties)});
    }
    arg.endArray();

    if (m_streams.isEmpty()) {
        fail(tr("The screen sharing portal returned no stream."));
        return;
    }

    openPipeWireRemote();
}

void ScreenCastPortalSession::openPipeWireRemote()
{
    QDBusMessage msg = QDBusMessage::createMethodCall(Portal::kService, Portal::kPath, kScreenCast,
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

        // A close() while the reply was in flight leaves nothing to hand out.
        if (m_streams.isEmpty()) {
            ::close(fd);
            return;
        }
        const Stream first = m_streams.constFirst();
        emit ready(first.nodeId, first.rectLogical, fd);
    });
}

void ScreenCastPortalSession::handleSessionClosed()
{
    if (m_sessionPath.isEmpty())
        return;

    // The portal already tore the session down, so drop the path before cleaning up and
    // no Close call goes to a dead object.
    watchClosed(false);
    m_sessionPath.clear();

    close();
    emit sessionClosed();
}

void ScreenCastPortalSession::handleSessionClosedWithDetails(const QVariantMap &details)
{
    Q_UNUSED(details)
    handleSessionClosed();
}

void ScreenCastPortalSession::watchClosed(bool watch)
{
    // Closed grew an a{sv} argument along the way, so both spellings are followed; the
    // handler ignores everything after the first one.
    QDBusConnection bus = QDBusConnection::sessionBus();
    for (const char *slot : {kClosedSlot, kClosedDetailsSlot}) {
        if (watch)
            bus.connect(Portal::kService, m_sessionPath, kSession, kClosed, this, slot);
        else
            bus.disconnect(Portal::kService, m_sessionPath, kSession, kClosed, this, slot);
    }
}

void ScreenCastPortalSession::fail(const QString &error)
{
    close();
    emit failed(error);
}

void ScreenCastPortalSession::failResponse(const char *step, uint response)
{
    qWarning() << "ScreenCast" << step << "answered" << response;
    // 1 is the user's cancel; 2 is everything else, a broken backend included.
    m_cancelled = response == 1;
    fail(response == 1 ? tr("Screen sharing was cancelled.")
                       : tr("Screen sharing failed. Check that the desktop portal "
                            "(xdg-desktop-portal and its backend for this desktop) is working."));
}

void ScreenCastPortalSession::reset()
{
    m_createRequest.stop();
    m_selectRequest.stop();
    m_startRequest.stop();

    if (!m_sessionPath.isEmpty()) {
        watchClosed(false);
        Portal::closeSession(m_sessionPath);
        m_sessionPath.clear();
    }

    m_streams.clear();
}

} // namespace Screen
