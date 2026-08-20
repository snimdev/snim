#ifndef HOTKEYS_STUBHOTKEYBACKEND_H
#define HOTKEYS_STUBHOTKEYBACKEND_H

#include "hotkeys/HotkeyBackend.h"

namespace Hotkeys {

/**
 * Universal fallback for systems with no global-hotkey backend (and for a Linux
 * desktop whose portal has no GlobalShortcuts interface). Registration is a no-op, so
 * the app builds everywhere and the hotkey settings simply report that this platform
 * is unsupported (HotkeyBackendFactory::isAvailable()).
 */
class StubHotkeyBackend : public HotkeyBackend
{
    Q_OBJECT

public:
    explicit StubHotkeyBackend(QObject *parent = nullptr) : HotkeyBackend(parent) {}

    // Silent: an unsupported platform is a property of the build, not a per-binding failure.
    void registerAll(const QList<HotkeyBinding> &) override {}
    void unregisterAll() override {}
};

} // namespace Hotkeys

#endif // HOTKEYS_STUBHOTKEYBACKEND_H
