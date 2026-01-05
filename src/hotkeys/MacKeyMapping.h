#ifndef HOTKEYS_MACKEYMAPPING_H
#define HOTKEYS_MACKEYMAPPING_H

#include <QKeySequence>
#include <QtGlobal>

#include <optional>

namespace Hotkeys {

// The pair RegisterEventHotKey() takes: a kVK_ virtual key code plus a Carbon
// modifier mask (cmdKey | shiftKey | optionKey | controlKey).
struct CarbonHotkey {
    quint32 keyCode;
    quint32 modifiers;
};

/**
 * Translate a QKeySequence into that pair. Deliberately pure and compiled on every
 * platform - no Carbon header here - so the whole mapping stays unit-testable off
 * macOS; MacHotkeyBackend static_asserts the local constants against the SDK, which
 * turns any drift into a compile error there rather than a dead hotkey here.
 *
 * Only the first chord is considered (callers normalize beforehand). nullopt for an
 * empty sequence, a chord with no non-modifier key, or a key with no kVK_ code:
 * all three mean "cannot be registered", which the backend reports as such.
 */
[[nodiscard]] std::optional<CarbonHotkey> toCarbonHotkey(const QKeySequence &seq);

} // namespace Hotkeys

#endif // HOTKEYS_MACKEYMAPPING_H
