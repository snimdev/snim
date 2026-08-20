#include "hotkeys/backends/PortalHotkeyBackend.h"

#include "core/Portal.h"
#include "hotkeys/PortalKeyMapping.h"

#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusMetaType>
#include <QDBusServiceWatcher>
#include <QDebug>
#include <QPair>

#include <optional>
#include <utility>

// BindShortcuts takes a(sa{sv}); QtDBus marshals the pairs once registered. Global scope is
// required: Q_DECLARE_METATYPE cannot sit in a namespace.
using SnimPortalShortcut = QPair<QString, QVariantMap>;
using SnimPortalShortcutList = QList<SnimPortalShortcut>;

Q_DECLARE_METATYPE(SnimPortalShortcut)
Q_DECLARE_METATYPE(SnimPortalShortcutList)

namespace Hotkeys {

namespace {

namespace Portal = Core::Portal;

const QString kShortcuts = QStringLiteral("org.freedesktop.portal.GlobalShortcuts");

const char *const kCreateSlot = SLOT(handleCreateSessionResponse(uint,QVariantMap));
const char *const kBindSlot = SLOT(handleBindShortcutsResponse(uint,QVariantMap));

} // namespace

PortalHotkeyBackend::PortalHotkeyBackend(QObject *parent) : HotkeyBackend(parent)
{
    qDBusRegisterMetaType<SnimPortalShortcut>();
    qDBusRegisterMetaType<SnimPortalShortcutList>();

    // Connected once for the object's life: Activated carries the session handle, so one
    // slot serves every session this backend creates.
    QDBusConnection::sessionBus().connect(
        Portal::kService, Portal::kPath, kShortcuts, QStringLiteral("Activated"), this,
        SLOT(handleActivated(QDBusObjectPath,QString,qulonglong,QVariantMap)));

    // A portal restart kills the session silently, so drop it and let the next
    // registerAll() build a fresh one.
    m_serviceWatcher = new QDBusServiceWatcher(Portal::kService, QDBusConnection::sessionBus(),
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
    return Portal::hasInterface(kShortcuts);
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

    m_createRequest.stop();
    m_bindRequest.stop();

    if (!m_sessionPath.isEmpty()) {
        // Closing drops every shortcut in the session; the portal keeps the user's keys
        // under the shortcut ids, so the next session gets them back.
        Portal::closeSession(m_sessionPath);
        m_sessionPath.clear();
    }

    m_state = SessionState::NoSession;
}

void PortalHotkeyBackend::createSession()
{
    // Connected before the call: the portal may answer before the method reply arrives.
    if (!m_createRequest.listen(this, kCreateSlot)) {
        failAll(m_pendingBindings, tr("the portal reply could not be listened for"));
        m_pendingBindings.clear();
        return;
    }

    QVariantMap options;
    options.insert(QStringLiteral("handle_token"), m_createRequest.token());
    options.insert(QStringLiteral("session_handle_token"), Portal::newToken());

    m_state = SessionState::CreatingSession;
    Portal::sendRequest(kShortcuts, QStringLiteral("CreateSession"),
                        {QVariant::fromValue(options)}, this, [this](const QString &error) {
        m_createRequest.stop();
        m_state = SessionState::NoSession;
        failAll(m_pendingBindings, error);
        m_pendingBindings.clear();
    });
}

void PortalHotkeyBackend::handleCreateSessionResponse(uint response, const QVariantMap &results)
{
    m_createRequest.stop();

    if (response != 0) {
        m_state = SessionState::NoSession;
        failAll(m_pendingBindings, response == 1
                    ? tr("the shortcuts session was cancelled")
                    : tr("the desktop refused a shortcuts session"));
        m_pendingBindings.clear();
        return;
    }

    m_sessionPath = Portal::sessionHandle(results);
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
    SnimPortalShortcutList shortcuts;
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

    if (!m_bindRequest.listen(this, kBindSlot)) {
        failAll(requested, tr("the portal reply could not be listened for"));
        return;
    }
    m_boundBindings = requested;

    QVariantMap options;
    options.insert(QStringLiteral("handle_token"), m_bindRequest.token());
    // Empty parent_window: a tray app has no window to parent the desktop's dialog to.
    Portal::sendRequest(kShortcuts, QStringLiteral("BindShortcuts"),
                        {QVariant::fromValue(QDBusObjectPath(m_sessionPath)),
                         QVariant::fromValue(shortcuts), QString(), QVariant::fromValue(options)},
                        this, [this](const QString &error) {
        m_bindRequest.stop();
        failAll(std::exchange(m_boundBindings, {}), error);
    });
}

void PortalHotkeyBackend::handleBindShortcutsResponse(uint response, const QVariantMap &results)
{
    Q_UNUSED(results)

    m_bindRequest.stop();

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

} // namespace Hotkeys
