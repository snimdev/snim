#include "hotkeys/HotkeyBackendFactory.h"

#include <QtGlobal>

#include "hotkeys/backends/StubHotkeyBackend.h"
#ifdef NICESHOT_HAVE_MAC_HOTKEYS
#include "hotkeys/backends/MacHotkeyBackend.h"
#elif defined(NICESHOT_HAVE_WIN_HOTKEYS)
#include "hotkeys/backends/WindowsHotkeyBackend.h"
#elif defined(Q_OS_LINUX)
#include "hotkeys/backends/PortalHotkeyBackend.h"
#endif

namespace Hotkeys {

std::unique_ptr<HotkeyBackend> HotkeyBackendFactory::create(QObject *parent)
{
#ifdef NICESHOT_HAVE_MAC_HOTKEYS
    return std::make_unique<MacHotkeyBackend>(parent);
#elif defined(NICESHOT_HAVE_WIN_HOTKEYS)
    return std::make_unique<WindowsHotkeyBackend>(parent);
#elif defined(Q_OS_LINUX)
    // Side-effect-free D-Bus probe; no session is created.
    if (PortalHotkeyBackend::isPortalAvailable())
        return std::make_unique<PortalHotkeyBackend>(parent);
    return std::make_unique<StubHotkeyBackend>(parent);
#else
    return std::make_unique<StubHotkeyBackend>(parent);
#endif
}

bool HotkeyBackendFactory::isAvailable()
{
#ifdef NICESHOT_HAVE_MAC_HOTKEYS
    return true;
#elif defined(NICESHOT_HAVE_WIN_HOTKEYS)
    return true;
#elif defined(Q_OS_LINUX)
    return PortalHotkeyBackend::isPortalAvailable();
#else
    return false;
#endif
}

HotkeyBackend::Capabilities HotkeyBackendFactory::capabilities()
{
#if defined(NICESHOT_HAVE_MAC_HOTKEYS) || defined(NICESHOT_HAVE_WIN_HOTKEYS)
    return HotkeyBackend::Capability::UserConfiguresKeys;
#else
    // Linux included: the desktop owns the portal keys, ours are only suggestions.
    return HotkeyBackend::Capability::None;
#endif
}

} // namespace Hotkeys
