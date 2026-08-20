#ifndef HOTKEYS_HOTKEYBACKENDFACTORY_H
#define HOTKEYS_HOTKEYBACKENDFACTORY_H

#include "hotkeys/HotkeyBackend.h"

#include <memory>

namespace Hotkeys {

/**
 * Creates the global-hotkey backend for the current system. The ladder is compile-time
 * per OS (plus the portal probe on Linux), with StubHotkeyBackend as the universal fallback.
 *
 * isAvailable() and userConfiguresKeys() answer for what create() returns without
 * constructing anything - no Carbon handler installed, no portal session opened -
 * so the settings dialog can describe the platform.
 */
class HotkeyBackendFactory
{
public:
    [[nodiscard]] static std::unique_ptr<HotkeyBackend> create(QObject *parent = nullptr);
    // Whether create() yields a real backend. The first answer holds for the run, so the
    // settings describe the backend the app actually got.
    [[nodiscard]] static bool isAvailable();
    // Configured sequences ARE the real bindings; false on Linux, where the desktop owns
    // the portal keys and ours are only preferred triggers.
    [[nodiscard]] static bool userConfiguresKeys();

private:
    HotkeyBackendFactory() = default;   // Static class
};

} // namespace Hotkeys

#endif // HOTKEYS_HOTKEYBACKENDFACTORY_H
