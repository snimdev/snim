#include "hotkeys/HotkeyBackend.h"

namespace Hotkeys {

HotkeyBackend::HotkeyBackend(QObject *parent) : QObject(parent)
{
    // Backends fire activated() from platform callbacks (a Carbon event handler, a
    // D-Bus signal), which can reach receivers through a queued connection. Marshalling
    // the argument then needs the enum registered as a metatype; the call is idempotent.
    qRegisterMetaType<HotkeyAction>("Hotkeys::HotkeyAction");
}

HotkeyBackend::~HotkeyBackend() = default;

} // namespace Hotkeys
