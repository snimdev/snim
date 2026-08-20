#include "hotkeys/HotkeyBackendFactory.h"

#include <QtGlobal>

#include "hotkeys/backends/StubHotkeyBackend.h"
#ifdef SNIM_HAVE_MAC_HOTKEYS
#include "hotkeys/backends/MacHotkeyBackend.h"
#elif defined(SNIM_HAVE_WIN_HOTKEYS)
#include "hotkeys/backends/WindowsHotkeyBackend.h"
#elif defined(Q_OS_LINUX)
#include "hotkeys/backends/PortalHotkeyBackend.h"
#endif

namespace Hotkeys {

std::unique_ptr<HotkeyBackend> HotkeyBackendFactory::create(QObject *parent)
{
#ifdef SNIM_HAVE_MAC_HOTKEYS
    return std::make_unique<MacHotkeyBackend>(parent);
#elif defined(SNIM_HAVE_WIN_HOTKEYS)
    return std::make_unique<WindowsHotkeyBackend>(parent);
#elif defined(Q_OS_LINUX)
    if (isAvailable())
        return std::make_unique<PortalHotkeyBackend>(parent);
    return std::make_unique<StubHotkeyBackend>(parent);
#else
    return std::make_unique<StubHotkeyBackend>(parent);
#endif
}

bool HotkeyBackendFactory::isAvailable()
{
#ifdef SNIM_HAVE_MAC_HOTKEYS
    return true;
#elif defined(SNIM_HAVE_WIN_HOTKEYS)
    return true;
#elif defined(Q_OS_LINUX)
    // Side-effect-free D-Bus probe; no session is created. Asked once: the app picks its
    // backend at startup and keeps it, even if the portal only comes up later.
    static const bool portal = PortalHotkeyBackend::isPortalAvailable();
    return portal;
#else
    return false;
#endif
}

bool HotkeyBackendFactory::userConfiguresKeys()
{
#if defined(SNIM_HAVE_MAC_HOTKEYS) || defined(SNIM_HAVE_WIN_HOTKEYS)
    return true;
#else
    return false;
#endif
}

} // namespace Hotkeys
