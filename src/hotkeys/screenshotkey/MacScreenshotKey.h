#ifndef HOTKEYS_MACSCREENSHOTKEY_H
#define HOTKEYS_MACSCREENSHOTKEY_H

#include "hotkeys/MacSymbolicHotkeys.h"
#include "hotkeys/ScreenshotKey.h"

#include <QElapsedTimer>

#include <functional>

namespace Hotkeys {

/**
 * macOS: ⇧⌘3, ⇧⌘4 and ⇧⌘5 are the system's screenshot shortcuts (symbolic hotkeys 28, 30
 * and 184 in com.apple.symbolichotkeys, see MacSymbolicHotkeys). The user turns them off
 * in System Settings as guidance() says. Compiled everywhere with the prefs as a seam,
 * like GnomeScreenshotKey; macSymbolicPrefs() is the real one.
 */
class MacScreenshotKey : public ScreenshotKey
{
public:
    // What the strategy does with com.apple.symbolichotkeys.
    struct Prefs {
        // The three entries as they are now; nullopt when they cannot be read.
        std::function<std::optional<MacSymbolicStates>()> read;
    };

    // Nothing is read before the first holderOf().
    explicit MacScreenshotKey(Prefs prefs);

    [[nodiscard]] Support support() const override;
    [[nodiscard]] QString keyName() const override;
    [[nodiscard]] QString ownerName() const override;
    // Any of the three while its entry is on (absent counts: that is the system default).
    [[nodiscard]] QString holderOf(const QKeySequence &seq) const override;
    [[nodiscard]] QList<HotkeyBinding> preset() const override;
    [[nodiscard]] QString guidance() const override;
    [[nodiscard]] QUrl settingsPage() const override;

    Result release() override;
    Result restore(const QString &memento) override;

private:
    // The three entries, read at most once per short interval.
    [[nodiscard]] std::optional<MacSymbolicStates> states() const;

    Prefs m_prefs;
    mutable std::optional<MacSymbolicStates> m_states;
    mutable QElapsedTimer m_statesRead;
};

// The user's real prefs, from MacSymbolicPrefs.mm (APPLE builds only). Under test mode
// they cannot be read and nothing is touched.
[[nodiscard]] MacScreenshotKey::Prefs macSymbolicPrefs();

} // namespace Hotkeys

#endif // HOTKEYS_MACSCREENSHOTKEY_H
