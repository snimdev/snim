#ifndef HOTKEYS_SCREENSHOTKEYFACTORY_H
#define HOTKEYS_SCREENSHOTKEYFACTORY_H

#include "hotkeys/ScreenshotKey.h"

#include <memory>

namespace Hotkeys {

/**
 * Creates the ScreenshotKey strategy for the running system: chooseScreenshotKey() fed
 * with the host platform, XDG_CURRENT_DESKTOP and the portal probe. Never null: a
 * system with no way to lend its key gets UnsupportedScreenshotKey.
 */
class ScreenshotKeyFactory
{
public:
    [[nodiscard]] static std::unique_ptr<ScreenshotKey> create();

private:
    ScreenshotKeyFactory() = default;   // Static class
};

} // namespace Hotkeys

#endif // HOTKEYS_SCREENSHOTKEYFACTORY_H
