#ifndef HOTKEYS_HOTKEYBACKENDFACTORY_H
#define HOTKEYS_HOTKEYBACKENDFACTORY_H

#include "hotkeys/HotkeyBackend.h"

#include <memory>

namespace Hotkeys {

/**
 * Creates the global-hotkey backend for the current system. The ladder is compile-time
 * per OS (plus the portal probe on Linux), with StubHotkeyBackend as the universal fallback.
 *
 * isAvailable() and capabilities() answer for what create() WOULD return without
 * constructing anything - no Carbon handler installed, no portal session opened -
 * so the settings dialog can describe the platform before any hotkey exists.
 */
class HotkeyBackendFactory
{
public:
    [[nodiscard]] static std::unique_ptr<HotkeyBackend> create(QObject *parent = nullptr);
    [[nodiscard]] static bool isAvailable();
    [[nodiscard]] static HotkeyBackend::Capabilities capabilities();

private:
    HotkeyBackendFactory() = default;   // Static class
};

} // namespace Hotkeys

#endif // HOTKEYS_HOTKEYBACKENDFACTORY_H
