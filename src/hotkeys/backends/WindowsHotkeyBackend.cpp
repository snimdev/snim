#include "hotkeys/backends/WindowsHotkeyBackend.h"

// Everything below is Win32; on any other host this compiles to an empty translation
// unit, so a stray build outside the WIN32 CMake block stays harmless.
#ifdef Q_OS_WIN

#include "hotkeys/WinKeyMapping.h"
#include "hotkeys/WinPrintScreen.h"

#include <QCoreApplication>
#include <QDebug>

#include <optional>

#include <windows.h>

#ifndef MOD_NOREPEAT
#define MOD_NOREPEAT 0x4000
#endif

namespace Hotkeys {

namespace {

QString snippingToolClashReason()
{
    return WindowsHotkeyBackend::tr(
        "Windows gives Print Screen to Snipping Tool; turn off \"Use the Print "
        "screen key to open screen capture\" in Settings > Accessibility > "
        "Keyboard, then restart Snim");
}

} // namespace

WindowsHotkeyBackend::WindowsHotkeyBackend(QObject *parent) : HotkeyBackend(parent)
{
    // WM_HOTKEY goes to the thread that called RegisterHotKey, which is this (GUI) one.
    if (qApp)
        qApp->installNativeEventFilter(this);
}

WindowsHotkeyBackend::~WindowsHotkeyBackend()
{
    unregisterAll();
    if (qApp)
        qApp->removeNativeEventFilter(this);
}

void WindowsHotkeyBackend::registerAll(const QList<HotkeyBinding> &bindings)
{
    unregisterAll();

    int id = 0;
    for (const HotkeyBinding &binding : bindings) {
        const std::optional<WinHotkey> hotkey = toWinHotkey(binding.sequence);
        if (!hotkey) {
            emit registrationFailed(binding.action,
                                    tr("this key combination is not supported on Windows"));
            continue;
        }

        ++id;
        const bool barePrintScreen = hotkey->virtualKey == VK_SNAPSHOT && hotkey->modifiers == 0;
        // MOD_NOREPEAT: a held-down chord fires once, not once per auto-repeat.
        if (!RegisterHotKey(nullptr, id, hotkey->modifiers | MOD_NOREPEAT, hotkey->virtualKey)) {
            const DWORD error = GetLastError();
            // Best effort: a rejected sequence must not cost the remaining ones.
            if (error == ERROR_HOTKEY_ALREADY_REGISTERED && barePrintScreen)
                emit registrationFailed(binding.action, snippingToolClashReason());
            else if (error == ERROR_HOTKEY_ALREADY_REGISTERED)
                emit registrationFailed(binding.action,
                                        tr("already in use by another application"));
            else
                emit registrationFailed(binding.action,
                                        tr("registration failed (error %1)").arg(error));
            continue;
        }

        m_registered.insert(id, binding.action);
        // Kept registered, so it works as soon as the user turns the setting off.
        if (barePrintScreen && snippingToolOwnsPrintScreen(printScreenSnippingSetting(),
                                                           windowsBuildNumber()))
            emit registrationFailed(binding.action, snippingToolClashReason());
    }
}

void WindowsHotkeyBackend::unregisterAll()
{
    for (auto it = m_registered.cbegin(); it != m_registered.cend(); ++it)
        UnregisterHotKey(nullptr, it.key());
    m_registered.clear();
}

bool WindowsHotkeyBackend::nativeEventFilter(const QByteArray &eventType, void *message,
                                             qintptr *result)
{
    Q_UNUSED(result)

    if (eventType != "windows_generic_MSG" || !message)
        return false;

    const MSG *msg = static_cast<MSG *>(message);
    if (msg->message != WM_HOTKEY)
        return false;

    const auto it = m_registered.constFind(static_cast<int>(msg->wParam));
    if (it == m_registered.cend())
        return false;

    // The filter runs on the GUI thread, so this lands where a tray click would.
    emit activated(it.value());
    return true;
}

} // namespace Hotkeys

#endif // Q_OS_WIN
