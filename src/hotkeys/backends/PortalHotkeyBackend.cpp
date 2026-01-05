#include "hotkeys/backends/PortalHotkeyBackend.h"

#include "hotkeys/PortalKeyMapping.h"

#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusMetaType>
#include <QDBusPendingCall>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusServiceWatcher>
#include <QDebug>
#include <QPair>
#include <QUuid>

#include <optional>

// BindShortcuts takes a(sa{sv}), which QtDBus cannot marshal on its own. Global scope is
// required: Q_DECLARE_METATYPE cannot sit in a namespace.
using NiceshotPortalShortcut = QPair<QString, QVariantMap>;
using NiceshotPortalShortcutList = QList<NiceshotPortalShortcut>;

Q_DECLARE_METATYPE(NiceshotPortalShortcut)
Q_DECLARE_METATYPE(NiceshotPortalShortcutList)

QDBusArgument &operator<<(QDBusArgument &arg, const NiceshotPortalShortcut &shortcut)
{
    arg.beginStructure();
    arg << shortcut.first << shortcut.second;
    arg.endStructure();
    return arg;
}

const QDBusArgument &operator>>(const QDBusArgument &arg, NiceshotPortalShortcut &shortcut)
{
    arg.beginStructure();
    arg >> shortcut.first >> shortcut.second;
    arg.endStructure();
    return arg;
}

namespace Hotkeys {

namespace {

const QString kService = QStringLiteral("org.freedesktop.portal.Desktop");
const QString kPath = QStringLiteral("/org/freedesktop/portal/desktop");
const QString kShortcuts = QStringLiteral("org.freedesktop.portal.GlobalShortcuts");
const QString kRequest = QStringLiteral("org.freedesktop.portal.Request");
const QString kSession = QStringLiteral("org.freedesktop.portal.Session");
const QString kProperties = QStringLiteral("org.freedesktop.DBus.Properties");

const char *const kCreateSlot = SLOT(handleCreateSessionResponse(uint,QVariantMap));
const char *const kBindSlot = SLOT(handleBindShortcutsResponse(uint,QVariantMap));

} // namespace

PortalHotkeyBackend::PortalHotkeyBackend(QObject *parent) : HotkeyBackend(parent)
{
    qDBusRegisterMetaType<NiceshotPortalShortcut>();
    qDBusRegisterMetaType<NiceshotPortalShortcutList>();

    // Connected once for the object's life: Activated carries the session handle, so one
    // slot serves every session this backend creates.
    QDBusConnection::sessionBus().connect(
        kService, kPath, kShortcuts, QStringLiteral("Activated"), this,
        SLOT(handleActivated(QDBusObjectPath,QString,qulonglong,QVariantMap)));

    // A portal restart kills the session silently, so drop it and let the next
    // registerAll() build a fresh one.
    m_serviceWatcher = new QDBusServiceWatcher(kService, QDBusConnection::sessionBus(),
                                               QDBusServiceWatcher::WatchForUnregistration, this);
    connect(m_serviceWatcher, &QDBusServiceWatcher::serviceUnregistered, this,
            [this](const QString &) {
                m_state = SessionState::NoSession;
                m_sessionPath.clear();
            });
}

PortalHotkeyBackend::~PortalHotkeyBackend()
{
    unregisterAll();
}

bool PortalHotkeyBackend::isPortalAvailable()
{
    QDBusMessage msg = QDBusMessage::createMethodCall(kService, kPath, kProperties,
                                                      QStringLiteral("Get"));
    msg.setArguments({kShortcuts, QStringLiteral("version")});

    // Blocking, but bounded and called once at startup, before a backend is chosen.
    const QDBusMessage reply = QDBusConnection::sessionBus().call(msg, QDBus::Block, 2000);
    return reply.type() == QDBusMessage::ReplyMessage;
}

bool PortalHotkeyBackend::isAvailable() const
{
    return isPortalAvailable();
}

QString PortalHotkeyBackend::name() const
{
    return QStringLiteral("GlobalShortcuts portal");
}

HotkeyBackend::Capabilities PortalHotkeyBackend::capabilities() const
{
    // No UserConfiguresKeys: the desktop owns the final bindings and our sequences are
    // only preferred_trigger hints, so the settings UI must show them as suggestions.
    return Capability::None;
}

void PortalHotkeyBackend::registerAll(const QList<HotkeyBinding> &bindings)
{
    m_pendingBindings = bindings;

    switch (m_state) {
    case SessionState::NoSession:
        createSession();
        break;
    case SessionState::CreatingSession:
        // The in-flight CreateSession binds whatever is queued when it lands.
        break;
    case SessionState::Ready: {
        const QList<HotkeyBinding> queued = m_pendingBindings;
        m_pendingBindings.clear();
        bindShortcuts(queued);
        break;
    }
    }
}

void PortalHotkeyBackend::unregisterAll()
{
    m_pendingBindings.clear();
    m_boundBindings.clear();

    if (!m_createRequestPath.isEmpty()) {
        disconnectResponse(m_createRequestPath, kCreateSlot);
        m_createRequestPath.clear();
    }
    if (!m_bindRequestPath.isEmpty()) {
        disconnectResponse(m_bindRequestPath, kBindSlot);
        m_bindRequestPath.clear();
    }

    if (!m_sessionPath.isEmpty()) {
        // Closing drops every shortcut in the session; the portal keeps the user's keys
        // under the shortcut ids, so the next session gets them back.
        QDBusConnection::sessionBus().asyncCall(
            QDBusMessage::createMethodCall(kService, m_sessionPath, kSession,
                                           QStringLiteral("Close")));
        m_sessionPath.clear();
    }

    m_state = SessionState::NoSession;
}

void PortalHotkeyBackend::createSession()
{
    const QString handleToken = newToken();
    m_createRequestPath = requestPath(handleToken);

    // Connected before the call: the portal may answer before the method reply arrives.
    if (!connectResponse(m_createRequestPath, kCreateSlot)) {
        m_createRequestPath.clear();
        failAll(m_pendingBindings, tr("the portal reply could not be listened for"));
        m_pendingBindings.clear();
        return;
    }

    QVariantMap options;
    options.insert(QStringLiteral("handle_token"), handleToken);
    options.insert(QStringLiteral("session_handle_token"), newToken());

    QDBusMessage msg = QDBusMessage::createMethodCall(kService, kPath, kShortcuts,
                                                      QStringLiteral("CreateSession"));
    msg.setArguments({QVariant::fromValue(options)});

    m_state = SessionState::CreatingSession;

    auto *watcher = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(msg), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this](QDBusPendingCallWatcher *w) {
        w->deleteLater();
        // The method reply only acknowledges the request; the answer is the Response signal.
        const QDBusPendingReply<QDBusObjectPath> reply = *w;
        if (!reply.isError())
            return;

        qWarning() << "GlobalShortcuts CreateSession failed:" << reply.error().message();
        disconnectResponse(m_createRequestPath, kCreateSlot);
        m_createRequestPath.clear();
        m_state = SessionState::NoSession;
        failAll(m_pendingBindings, reply.error().message());
        m_pendingBindings.clear();
    });
}

void PortalHotkeyBackend::handleCreateSessionResponse(uint response, const QVariantMap &results)
{
    disconnectResponse(m_createRequestPath, kCreateSlot);
    m_createRequestPath.clear();

    if (response != 0) {
        m_state = SessionState::NoSession;
        failAll(m_pendingBindings, response == 1
                    ? tr("the shortcuts session was cancelled")
                    : tr("the desktop refused a shortcuts session"));
        m_pendingBindings.clear();
        return;
    }

    // Spec types session_handle as a string; some backends send an object path.
    const QVariant handle = results.value(QStringLiteral("session_handle"));
    m_sessionPath = handle.toString();
    if (m_sessionPath.isEmpty() && handle.canConvert<QDBusObjectPath>())
        m_sessionPath = handle.value<QDBusObjectPath>().path();

    if (m_sessionPath.isEmpty()) {
        m_state = SessionState::NoSession;
        failAll(m_pendingBindings, tr("the portal returned no session handle"));
        m_pendingBindings.clear();
        return;
    }

    m_state = SessionState::Ready;

    const QList<HotkeyBinding> queued = m_pendingBindings;
    m_pendingBindings.clear();
    if (!queued.isEmpty())
        bindShortcuts(queued);
}

void PortalHotkeyBackend::bindShortcuts(const QList<HotkeyBinding> &bindings)
{
    NiceshotPortalShortcutList shortcuts;
    QList<HotkeyBinding> requested;

    for (const HotkeyBinding &binding : bindings) {
        const QString trigger = toPortalTrigger(binding.sequence);
        if (trigger.isEmpty()) {
            emit registrationFailed(binding.action,
                                    tr("this key combination has no portal spelling"));
            continue;
        }

        QVariantMap metadata;
        metadata.insert(QStringLiteral("description"), hotkeyActionDescription(binding.action));
        metadata.insert(QStringLiteral("preferred_trigger"), trigger);
        shortcuts.append({hotkeyActionId(binding.action), metadata});
        requested.append(binding);
    }

    if (shortcuts.isEmpty())
        return;

    const QString handleToken = newToken();
    m_bindRequestPath = requestPath(handleToken);
    if (!connectResponse(m_bindRequestPath, kBindSlot)) {
        m_bindRequestPath.clear();
        failAll(requested, tr("the portal reply could not be listened for"));
        return;
    }
    m_boundBindings = requested;

    QVariantMap options;
    options.insert(QStringLiteral("handle_token"), handleToken);

    QDBusMessage msg = QDBusMessage::createMethodCall(kService, kPath, kShortcuts,
                                                      QStringLiteral("BindShortcuts"));
    // Empty parent_window: a tray app has no window to parent the desktop's dialog to.
    msg.setArguments({QVariant::fromValue(QDBusObjectPath(m_sessionPath)),
                      QVariant::fromValue(shortcuts),
                      QString(),
                      QVariant::fromValue(options)});

    auto *watcher = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(msg), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this](QDBusPendingCallWatcher *w) {
        w->deleteLater();
        const QDBusPendingReply<QDBusObjectPath> reply = *w;
        if (!reply.isError())
            return;

        qWarning() << "GlobalShortcuts BindShortcuts failed:" << reply.error().message();
        disconnectResponse(m_bindRequestPath, kBindSlot);
        m_bindRequestPath.clear();
        const QList<HotkeyBinding> requestedNow = m_boundBindings;
        m_boundBindings.clear();
        failAll(requestedNow, reply.error().message());
    });
}

void PortalHotkeyBackend::handleBindShortcutsResponse(uint response, const QVariantMap &results)
{
    Q_UNUSED(results)

    disconnectResponse(m_bindRequestPath, kBindSlot);
    m_bindRequestPath.clear();

    const QList<HotkeyBinding> requested = m_boundBindings;
    m_boundBindings.clear();

    if (response != 0) {
        failAll(requested, response == 1
                    ? tr("the desktop's shortcuts dialog was cancelled")
                    : tr("the desktop refused the shortcuts"));
        return;
    }

    // results["shortcuts"] holds what the desktop actually bound; not read back, since
    // it may differ from our hints by design.
    qDebug() << "GlobalShortcuts bound" << requested.size() << "shortcut(s)";
}

void PortalHotkeyBackend::handleActivated(const QDBusObjectPath &sessionHandle,
                                          const QString &shortcutId, qulonglong timestamp,
                                          const QVariantMap &options)
{
    Q_UNUSED(timestamp)
    Q_UNUSED(options)

    // The connection is interface-wide, so other sessions' activations land here too.
    if (sessionHandle.path() != m_sessionPath)
        return;

    const std::optional<HotkeyAction> action = hotkeyActionFromId(shortcutId);
    if (!action) {
        qDebug() << "GlobalShortcuts Activated for unknown shortcut id" << shortcutId;
        return;
    }

    emit activated(*action);
}

void PortalHotkeyBackend::failAll(const QList<HotkeyBinding> &bindings, const QString &reason)
{
    for (const HotkeyBinding &binding : bindings)
        emit registrationFailed(binding.action, reason);
}

bool PortalHotkeyBackend::connectResponse(const QString &path, const char *slot)
{
    const bool ok = QDBusConnection::sessionBus().connect(kService, path, kRequest,
                                                          QStringLiteral("Response"), this, slot);
    if (!ok)
        qWarning() << "Failed to connect portal Response signal on path:" << path;
    return ok;
}

void PortalHotkeyBackend::disconnectResponse(const QString &path, const char *slot)
{
    if (path.isEmpty())
        return;
    QDBusConnection::sessionBus().disconnect(kService, path, kRequest,
                                             QStringLiteral("Response"), this, slot);
}

QString PortalHotkeyBackend::newToken()
{
    // The token becomes an object-path element, which allows no braces or dashes.
    return QUuid::createUuid().toString().remove('-').remove('{').remove('}');
}

QString PortalHotkeyBackend::requestPath(const QString &token)
{
    // The portal derives this path from our unique name and the token, so it is known
    // before the call is sent.
    const QString sender =
        QDBusConnection::sessionBus().baseService().remove(':').replace('.', '_');
    return QStringLiteral("/org/freedesktop/portal/desktop/request/%1/%2").arg(sender, token);
}

} // namespace Hotkeys
