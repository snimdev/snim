#ifndef HOTKEYS_STUBHOTKEYBACKEND_H
#define HOTKEYS_STUBHOTKEYBACKEND_H

#include "hotkeys/HotkeyBackend.h"

namespace Hotkeys {

/**
 * Universal fallback for systems with no global-hotkey backend (and for a Linux
 * desktop whose portal has no GlobalShortcuts interface). isAvailable() is false and
 * registration is a no-op, so the app builds everywhere and the hotkey settings
 * simply report that this platform is unsupported.
 */
class StubHotkeyBackend : public HotkeyBackend
{
    Q_OBJECT

public:
    explicit StubHotkeyBackend(QObject *parent = nullptr) : HotkeyBackend(parent) {}

    void registerAll(const QList<HotkeyBinding> &bindings) override;
    void unregisterAll() override {}
    [[nodiscard]] bool isAvailable() const override { return false; }
    [[nodiscard]] QString name() const override { return QStringLiteral("Unsupported"); }
    [[nodiscard]] Capabilities capabilities() const override { return Capability::None; }
};

} // namespace Hotkeys

#endif // HOTKEYS_STUBHOTKEYBACKEND_H
