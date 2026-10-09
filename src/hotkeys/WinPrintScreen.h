#ifndef HOTKEYS_WINPRINTSCREEN_H
#define HOTKEYS_WINPRINTSCREEN_H

#include <QtGlobal>

#include <optional>

/**
 * The Windows setting that hands Print Screen to Snipping Tool: the DWORD at
 * HKCU\Control Panel\Keyboard\PrintScreenKeyForSnippingEnabled. Shared by the hotkey
 * backend and the screenshot key strategy. No Windows types here; the .cpp body is
 * Q_OS_WIN-gated and only listed in the WIN32 build.
 */
namespace Hotkeys {

// The value, or nullopt when it is absent (the OS default applies) or unreadable.
[[nodiscard]] std::optional<quint32> printScreenSnippingSetting();

// Writes the value as a DWORD; nullopt deletes it, so the OS default applies again.
bool writePrintScreenSnippingSetting(std::optional<quint32> value);

// Broadcasts WM_SETTINGCHANGE for the keyboard settings, bounded by a short timeout.
void announceKeyboardSettingChange();

// The real OS build number (22621 is Windows 11 22H2).
[[nodiscard]] quint32 windowsBuildNumber();

} // namespace Hotkeys

#endif // HOTKEYS_WINPRINTSCREEN_H
