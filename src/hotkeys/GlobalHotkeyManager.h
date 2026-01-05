#ifndef HOTKEYS_GLOBALHOTKEYMANAGER_H
#define HOTKEYS_GLOBALHOTKEYMANAGER_H

#include "hotkeys/HotkeyAction.h"
#include "hotkeys/HotkeyBackend.h"

#include <QObject>
#include <QString>

#include <memory>

namespace Hotkeys {

/**
 * Cross-platform orchestration for global hotkeys, mirroring RecordingController:
 * owns the platform HotkeyBackend (picked by HotkeyBackendFactory), pushes the
 * persisted bindings into it, and turns backend events into two app-level signals.
 *
 * Deliberately UI-free and platform-free so it is unit-testable with a fake backend.
 * The app layer connects actionTriggered() to the same slots the tray menu uses, and
 * shows registrationFailed() wherever it shows its other non-fatal warnings.
 */
class GlobalHotkeyManager : public QObject
{
    Q_OBJECT

public:
    explicit GlobalHotkeyManager(QObject *parent = nullptr);
    // Test seam: inject a backend (e.g. a fake) instead of the platform default.
    explicit GlobalHotkeyManager(std::unique_ptr<HotkeyBackend> backend,
                                 QObject *parent = nullptr);

    // Make the persisted bindings the live set. Safe to call again after a settings
    // change: the old registrations are dropped first.
    void applyBindings();

    [[nodiscard]] bool isAvailable() const;
    [[nodiscard]] HotkeyBackend::Capabilities capabilities() const;

signals:
    void actionTriggered(Hotkeys::HotkeyAction action);
    void registrationFailed(const QString &message);   // ready to show, one per binding

private:
    void wireBackend();

    std::unique_ptr<HotkeyBackend> m_backend;
};

} // namespace Hotkeys

#endif // HOTKEYS_GLOBALHOTKEYMANAGER_H
