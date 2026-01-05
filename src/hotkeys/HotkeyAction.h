#ifndef HOTKEYS_HOTKEYACTION_H
#define HOTKEYS_HOTKEYACTION_H

#include <QKeySequence>
#include <QList>
#include <QMetaType>
#include <QString>

#include <optional>

namespace Hotkeys {

// The app actions a global hotkey can fire; id, label and default all hang off it.
enum class HotkeyAction {
    CaptureArea,
    CaptureWindow,
    CaptureFullScreen,
    OcrTextSnip,
    RecordArea,
    RecordWindow
};

inline constexpr int kHotkeyActionCount = 6;

// Every action, in enum order (the order the settings UI lists them in).
[[nodiscard]] QList<HotkeyAction> allHotkeyActions();

// Stable machine id, also the portal shortcut id; renaming one orphans stored bindings.
[[nodiscard]] QString hotkeyActionId(HotkeyAction a);
[[nodiscard]] std::optional<HotkeyAction> hotkeyActionFromId(const QString &id);

// User-facing label (settings row, portal description, failure messages).
[[nodiscard]] QString hotkeyActionDescription(HotkeyAction a);

// Factory default; empty = deliberately unbound (the free combos collide with common bindings).
[[nodiscard]] QKeySequence hotkeyActionDefault(HotkeyAction a);

struct HotkeyBinding {
    HotkeyAction action;
    QKeySequence sequence;
};

} // namespace Hotkeys

// Metatype so queued activated() emissions from platform callbacks can marshal it.
Q_DECLARE_METATYPE(Hotkeys::HotkeyAction)

#endif // HOTKEYS_HOTKEYACTION_H
