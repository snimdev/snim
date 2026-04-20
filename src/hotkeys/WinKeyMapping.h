#ifndef HOTKEYS_WINKEYMAPPING_H
#define HOTKEYS_WINKEYMAPPING_H

#include <QKeySequence>
#include <QtGlobal>

#include <optional>

namespace Hotkeys {

// The pair RegisterHotKey() takes: a virtual key code plus a modifier mask built
// from MOD_ALT 0x1 | MOD_CONTROL 0x2 | MOD_SHIFT 0x4 | MOD_WIN 0x8.
struct WinHotkey {
    quint32 virtualKey;
    quint32 modifiers;
};

/**
 * Translate a QKeySequence into that pair. Pure and compiled everywhere (no windows.h
 * here), like MacKeyMapping, so the table is testable on any host.
 *
 * MOD_NOREPEAT is NOT included: it is the backend's policy for how a held key
 * behaves, not part of the sequence, so the backend ORs it in at registration.
 * Only the first chord is considered; nullopt for an empty sequence, a chord with no
 * non-modifier key, or a key with no virtual-key code.
 */
[[nodiscard]] std::optional<WinHotkey> toWinHotkey(const QKeySequence &seq);

/**
 * Whether Windows hands bare Print Screen to Snipping Tool, from the DWORD at
 * HKCU\Control Panel\Keyboard\PrintScreenKeyForSnippingEnabled (nullopt when absent)
 * and the OS build number. Absent means the OS default: on from Windows 11 22H2
 * (build 22621), off before. RegisterHotKey still succeeds then; the key never arrives.
 */
[[nodiscard]] bool snippingToolOwnsPrintScreen(std::optional<quint32> setting,
                                               quint32 buildNumber);

} // namespace Hotkeys

#endif // HOTKEYS_WINKEYMAPPING_H
