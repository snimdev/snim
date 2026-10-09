#ifndef HOTKEYS_MACSCREENSHOTKEY_H
#define HOTKEYS_MACSCREENSHOTKEY_H

#include "hotkeys/MacSymbolicHotkeys.h"
#include "hotkeys/ScreenshotKey.h"

#include <QElapsedTimer>

#include <functional>

namespace Hotkeys {

/**
 * macOS: ⇧⌘3, ⇧⌘4 and ⇧⌘5 are the system's screenshot shortcuts (symbolic hotkeys 28, 30
 * and 184 in com.apple.symbolichotkeys, see MacSymbolicHotkeys). Where the private
 * activateSettings tool exists, release() turns them off in the prefs and has the system
 * reload them; elsewhere the user does it as guidance() says. The memento is
 * encodeMacSymbolicStates() of the entries before. Compiled everywhere with the prefs as
 * a seam, like GnomeScreenshotKey; macSymbolicPrefs() is the real one.
 */
class MacScreenshotKey : public ScreenshotKey
{
public:
    // What the strategy does with com.apple.symbolichotkeys.
    struct Prefs {
        // The three entries as they are now; nullopt when they cannot be read.
        std::function<std::optional<MacSymbolicStates>()> read;
        // Applies the edits to AppleSymbolicHotKeys and saves them; false when that failed.
        std::function<bool(const MacSymbolicWrites &)> write;
        // The system can reload saved shortcuts, so no log out is needed.
        std::function<bool()> canActivate;
        // Has the system reload them; false when that failed.
        std::function<bool()> activate;
    };

    // Asks the system nothing until a method needs it.
    explicit MacScreenshotKey(Prefs prefs);

    // Automatic where the system can reload its shortcuts, else Manual.
    [[nodiscard]] Support support() const override;
    [[nodiscard]] QString keyName() const override;
    [[nodiscard]] QString ownerName() const override;
    // Any of the three while its entry is on (absent counts: that is the system default).
    [[nodiscard]] QString holderOf(const QKeySequence &seq) const override;
    [[nodiscard]] QList<HotkeyBinding> preset() const override;
    [[nodiscard]] QString guidance() const override;
    [[nodiscard]] QUrl settingsPage() const override;

    Result release() override;
    // Works without the reload too: the prefs are written and a log out finishes it.
    Result restore(const QString &memento) override;

    // The real reload: activateSettings -u, the private tool System Settings uses. Both
    // refuse in test mode.
    static bool canActivateSettings();
    static bool activateSettings();

private:
    // The three entries, read at most once per short interval.
    [[nodiscard]] std::optional<MacSymbolicStates> states() const;
    // All three read now, a missing one as absent.
    [[nodiscard]] std::optional<MacSymbolicStates> readAll() const;
    // Writes the edits and reloads; the result restore() or release() returns.
    [[nodiscard]] Result apply(const MacSymbolicWrites &writes, const QString &memento,
                               const QString &failure, const QString &logOut);
    void forget() const;

    Prefs m_prefs;
    mutable std::optional<MacSymbolicStates> m_states;
    mutable QElapsedTimer m_statesRead;
    mutable std::optional<bool> m_oneClick;   // asked once: only an OS update changes it
};

// The user's real prefs, from MacSymbolicPrefs.mm (APPLE builds only). Under test mode
// they cannot be read and nothing is touched.
[[nodiscard]] MacScreenshotKey::Prefs macSymbolicPrefs();

} // namespace Hotkeys

#endif // HOTKEYS_MACSCREENSHOTKEY_H
