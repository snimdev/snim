#include "hotkeys/WinPrintScreen.h"

// Win32 only; on any other host this compiles to an empty translation unit.
#ifdef Q_OS_WIN

#include <QOperatingSystemVersion>

#include <algorithm>

#include <windows.h>

namespace Hotkeys {

namespace {

const wchar_t *const kKeyboardKey = L"Control Panel\\Keyboard";
const wchar_t *const kSnippingValue = L"PrintScreenKeyForSnippingEnabled";

} // namespace

std::optional<quint32> printScreenSnippingSetting()
{
    DWORD value = 0;
    DWORD size = sizeof(value);
    if (RegGetValueW(HKEY_CURRENT_USER, kKeyboardKey, kSnippingValue, RRF_RT_REG_DWORD, nullptr,
                     &value, &size) != ERROR_SUCCESS)
        return std::nullopt;
    return quint32(value);
}

bool writePrintScreenSnippingSetting(std::optional<quint32> value)
{
    if (!value) {
        const LSTATUS status = RegDeleteKeyValueW(HKEY_CURRENT_USER, kKeyboardKey, kSnippingValue);
        // Already absent is the state that was asked for.
        return status == ERROR_SUCCESS || status == ERROR_FILE_NOT_FOUND;
    }
    const DWORD data = DWORD(*value);
    return RegSetKeyValueW(HKEY_CURRENT_USER, kKeyboardKey, kSnippingValue, REG_DWORD, &data,
                           DWORD(sizeof(data)))
           == ERROR_SUCCESS;
}

void announceKeyboardSettingChange()
{
    // SMTO_ABORTIFHUNG: a hung window costs nothing, a slow one at most the timeout.
    SendMessageTimeoutW(HWND_BROADCAST, WM_SETTINGCHANGE, 0, reinterpret_cast<LPARAM>(kKeyboardKey),
                        SMTO_ABORTIFHUNG, 1000, nullptr);
}

// QOperatingSystemVersion reads the real build via RtlGetVersion, unaffected by manifests.
quint32 windowsBuildNumber()
{
    return quint32(std::max(0, QOperatingSystemVersion::current().microVersion()));
}

} // namespace Hotkeys

#endif // Q_OS_WIN
