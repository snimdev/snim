#include "hotkeys/ScreenshotKey.h"

#include "core/Desktop.h"

namespace Hotkeys {

ScreenshotKeyKind chooseScreenshotKey(HotkeyPlatform platform, const QString &currentDesktop,
                                      bool portalAvailable)
{
    switch (platform) {
    case HotkeyPlatform::Windows: return ScreenshotKeyKind::Windows;
    case HotkeyPlatform::Mac:     return ScreenshotKeyKind::Mac;
    case HotkeyPlatform::Linux:   break;
    }

    // Without the GlobalShortcuts portal Snim has no hotkeys to put on the key.
    if (!portalAvailable)
        return ScreenshotKeyKind::Unsupported;
    if (Core::desktopIs(currentDesktop, QStringLiteral("KDE")))
        return ScreenshotKeyKind::Kde;
    if (Core::desktopIs(currentDesktop, QStringLiteral("GNOME")))
        return ScreenshotKeyKind::Gnome;
    return ScreenshotKeyKind::Unsupported;
}

QList<HotkeyBinding> screenshotKeyPreset(HotkeyPlatform platform)
{
    if (platform == HotkeyPlatform::Mac) {
        return {
            {HotkeyAction::CaptureArea, QKeySequence(QStringLiteral("Ctrl+Shift+4"))},
            {HotkeyAction::CaptureFullScreen, QKeySequence(QStringLiteral("Ctrl+Shift+3"))},
            {HotkeyAction::RecordArea, QKeySequence(QStringLiteral("Ctrl+Shift+5"))},
        };
    }
    return {{HotkeyAction::CaptureArea, QKeySequence(QStringLiteral("Print"))}};
}

} // namespace Hotkeys
