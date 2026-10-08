#include "hotkeys/ScreenshotKeyFactory.h"

#include "hotkeys/HotkeyBackendFactory.h"
#include "hotkeys/screenshotkey/UnsupportedScreenshotKey.h"

#include <QtGlobal>

namespace Hotkeys {

std::unique_ptr<ScreenshotKey> ScreenshotKeyFactory::create()
{
    // The portal answer is the one the hotkey backend was chosen by (cached for the run).
    const ScreenshotKeyKind kind =
        chooseScreenshotKey(hostHotkeyPlatform(), qEnvironmentVariable("XDG_CURRENT_DESKTOP"),
                            HotkeyBackendFactory::isAvailable());
    switch (kind) {
    case ScreenshotKeyKind::Windows:
    case ScreenshotKeyKind::Mac:
    case ScreenshotKeyKind::Kde:
    case ScreenshotKeyKind::Gnome:
    case ScreenshotKeyKind::Unsupported:
        break;
    }
    return std::make_unique<UnsupportedScreenshotKey>();
}

} // namespace Hotkeys
