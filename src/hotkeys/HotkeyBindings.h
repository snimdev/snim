#ifndef HOTKEYS_HOTKEYBINDINGS_H
#define HOTKEYS_HOTKEYBINDINGS_H

#include "hotkeys/HotkeyAction.h"

namespace Hotkeys {

/**
 * The persisted shape of the hotkey configuration, over Core::Settings - the role
 * UploadProfiles plays for the upload JSON. Sequences are stored as
 * QKeySequence::PortableText so the store stays platform-neutral.
 *
 * Absent and empty are NOT the same thing: an action nobody has configured resolves
 * to its factory default, while one stored as "" was explicitly unbound and stays
 * unbound. Core::Settings preserves that by only applying its default when the key
 * is missing.
 */
class HotkeyBindings
{
public:
    // What this action currently answers to: the stored sequence, or the default
    // when the key was never written. Empty means unbound.
    [[nodiscard]] static QKeySequence sequence(HotkeyAction a);
    // Stores only overrides: the default removes the key, "" unbinds a bound default.
    static void setSequence(HotkeyAction a, const QKeySequence &seq);
    // True when the action has a stored override (including "").
    [[nodiscard]] static bool isCustomized(HotkeyAction a);

    // Everything worth handing to a backend: normalized, unbound actions dropped,
    // in enum order.
    [[nodiscard]] static QList<HotkeyBinding> activeBindings();

    // Reduce a sequence to its first chord. QKeySequence holds up to four ("Ctrl+A,
    // Ctrl+B"), but no platform backend here can register a multi-chord shortcut, so
    // the extra chords are dropped rather than silently mis-registered.
    [[nodiscard]] static QKeySequence normalized(const QKeySequence &seq);

private:
    HotkeyBindings() = default;   // Static class
};

} // namespace Hotkeys

#endif // HOTKEYS_HOTKEYBINDINGS_H
