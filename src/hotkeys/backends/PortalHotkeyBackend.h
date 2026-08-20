#ifndef HOTKEYS_PORTALHOTKEYBACKEND_H
#define HOTKEYS_PORTALHOTKEYBACKEND_H

#include "core/Portal.h"
#include "hotkeys/HotkeyBackend.h"

#include <QDBusObjectPath>
#include <QList>
#include <QMap>
#include <QString>
#include <QVariant>

class QDBusServiceWatcher;

namespace Hotkeys {

/**
 * Linux backend over org.freedesktop.portal.GlobalShortcuts. A portal session owns the
 * shortcuts, so our sequence is only a preferred_trigger and the DESKTOP picks the final
 * keys. Fully async: bindings are queued while the session is still being created.
 */
class PortalHotkeyBackend : public HotkeyBackend
{
    Q_OBJECT

public:
    explicit PortalHotkeyBackend(QObject *parent = nullptr);
    ~PortalHotkeyBackend() override;

    void registerAll(const QList<HotkeyBinding> &bindings) override;
    void unregisterAll() override;

    // Factory probe: reads the interface version, creating no session. A yes is cached.
    [[nodiscard]] static bool isPortalAvailable();

private slots:
    // org.freedesktop.portal.Request Response handlers, connected old-style on the
    // precomputed request path before each call goes out.
    void handleCreateSessionResponse(uint response, const QVariantMap &results);
    void handleBindShortcutsResponse(uint response, const QVariantMap &results);
    void handleActivated(const QDBusObjectPath &sessionHandle, const QString &shortcutId,
                         qulonglong timestamp, const QVariantMap &options);

private:
    enum class SessionState { NoSession, CreatingSession, Ready };

    void createSession();
    void bindShortcuts(const QList<HotkeyBinding> &bindings);
    void failAll(const QList<HotkeyBinding> &bindings, const QString &reason);

    SessionState m_state = SessionState::NoSession;
    QString m_sessionPath;
    Core::Portal::Request m_createRequest;
    Core::Portal::Request m_bindRequest;
    QList<HotkeyBinding> m_pendingBindings;   // queued while the session is being created
    QList<HotkeyBinding> m_boundBindings;     // what the in-flight BindShortcuts asked for
    QDBusServiceWatcher *m_serviceWatcher = nullptr;
};

} // namespace Hotkeys

#endif // HOTKEYS_PORTALHOTKEYBACKEND_H
