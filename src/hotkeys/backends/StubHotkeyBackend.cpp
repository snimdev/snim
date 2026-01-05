#include "hotkeys/backends/StubHotkeyBackend.h"

namespace Hotkeys {

void StubHotkeyBackend::registerAll(const QList<HotkeyBinding> &)
{
    // Silent by design: an unsupported platform is a property of the build, already
    // surfaced once by isAvailable(), not a per-binding failure worth a message each.
}

} // namespace Hotkeys
