#include "hotkeys/ScreenshotKeyFactory.h"

#include "hotkeys/HotkeyBackendFactory.h"
#include "hotkeys/screenshotkey/UnsupportedScreenshotKey.h"

#include <QtGlobal>

#ifdef Q_OS_WIN
#include "hotkeys/screenshotkey/WindowsScreenshotKey.h"
#endif
#ifdef Q_OS_LINUX
#include "hotkeys/screenshotkey/KdeScreenshotKey.h"
#endif

namespace Hotkeys {

std::unique_ptr<ScreenshotKey> ScreenshotKeyFactory::create()
{
    // The portal answer is the one the hotkey backend was chosen by (cached for the run).
    const ScreenshotKeyKind kind =
        chooseScreenshotKey(hostHotkeyPlatform(), qEnvironmentVariable("XDG_CURRENT_DESKTOP"),
                            HotkeyBackendFactory::isAvailable());
    switch (kind) {
    case ScreenshotKeyKind::Windows:
#ifdef Q_OS_WIN
        return std::make_unique<WindowsScreenshotKey>();
#else
        break;
#endif
    case ScreenshotKeyKind::Kde:
#ifdef Q_OS_LINUX
        return std::make_unique<KdeScreenshotKey>();
#else
        break;
#endif
    case ScreenshotKeyKind::Mac:
    case ScreenshotKeyKind::Gnome:
    case ScreenshotKeyKind::Unsupported:
        break;
    }
    return std::make_unique<UnsupportedScreenshotKey>();
}

} // namespace Hotkeys
