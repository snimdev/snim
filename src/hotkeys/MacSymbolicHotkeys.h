#ifndef HOTKEYS_MACSYMBOLICHOTKEYS_H
#define HOTKEYS_MACSYMBOLICHOTKEYS_H

#include <QKeySequence>
#include <QList>
#include <QMap>
#include <QString>

#include <array>
#include <optional>

namespace Hotkeys {

/**
 * The macOS screenshot shortcuts as com.apple.symbolichotkeys keeps them: entries of its
 * AppleSymbolicHotKeys dictionary, keyed by id, each {enabled, value = {parameters =
 * (character, virtual key code, modifier flags), type = standard}}. Pure and compiled
 * everywhere, like MacKeyMapping, so MacScreenshotKey only reads and writes what these
 * functions decide; it static_asserts the constants against the SDK.
 */
struct MacSymbolicHotkey {
    int id;
    std::array<int, 3> parameters;   // '3', kVK_ANSI_3, NSEvent Shift | Command
    QKeySequence sequence;           // the preset key it holds; Qt's Ctrl is Command
};

// ⇧⌘3 screen to a file (28), ⇧⌘4 area to a file (30), ⇧⌘5 screenshot options (184).
[[nodiscard]] QList<MacSymbolicHotkey> macScreenshotHotkeys();
// The entry that answers to seq; nullopt for any other key.
[[nodiscard]] std::optional<MacSymbolicHotkey> macScreenshotHotkeyOn(const QKeySequence &seq);

// An entry as the dictionary holds it; absent means the system default, which is on.
enum class MacSymbolicState { Absent, Enabled, Disabled };
using MacSymbolicStates = QMap<int, MacSymbolicState>;   // id -> state

[[nodiscard]] constexpr bool macSymbolicHotkeyIsOn(MacSymbolicState state)
{
    return state != MacSymbolicState::Disabled;
}

// The memento: "28=absent,30=1,184=0", ids ascending.
[[nodiscard]] QString encodeMacSymbolicStates(const MacSymbolicStates &states);
// The inverse; nullopt for text never encoded, or an id that is not a screenshot key.
[[nodiscard]] std::optional<MacSymbolicStates> decodeMacSymbolicStates(const QString &memento);

// One edit to an entry.
enum class MacSymbolicWrite {
    Keep,          // left as it is
    Disable,       // enabled = NO, its value kept
    Enable,        // enabled = YES, its value kept
    AddDisabled,   // the whole entry: enabled = NO and the system's default value
    Remove         // dropped, so the system default applies again
};
using MacSymbolicWrites = QMap<int, MacSymbolicWrite>;   // id -> edit

// Freeing the keys: every entry that is on goes off; an absent one is written in full.
[[nodiscard]] MacSymbolicWrites macReleaseWrites(const MacSymbolicStates &now);
// Giving them back: only an entry still off is put back as before was. A null before
// (an unreadable memento) falls back to the system default.
[[nodiscard]] MacSymbolicWrites macRestoreWrites(const std::optional<MacSymbolicStates> &before,
                                                 const MacSymbolicStates &now);
// Some edit is not Keep, so the prefs need writing.
[[nodiscard]] bool macSymbolicWritesChange(const MacSymbolicWrites &writes);

} // namespace Hotkeys

#endif // HOTKEYS_MACSYMBOLICHOTKEYS_H
