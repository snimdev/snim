#ifndef HOTKEYS_SCREENSHOTKEY_H
#define HOTKEYS_SCREENSHOTKEY_H

#include "hotkeys/HotkeyAction.h"

#include <QCoreApplication>
#include <QKeySequence>
#include <QList>
#include <QString>
#include <QUrl>

namespace Hotkeys {

/**
 * Strategy: how this platform lends its screenshot key to Snim. The OS holds the key
 * (Snipping Tool on Windows, the system on macOS, Spectacle on KDE, GNOME's screenshot
 * UI); release() frees it and returns a memento that restore() takes to give it back.
 * UI-free, so the swap and the first-run offer share it.
 */
class ScreenshotKey
{
    Q_DECLARE_TR_FUNCTIONS(ScreenshotKey)

public:
    enum class Support {
        Automatic,    // one click: release() frees the key
        Assisted,     // release() frees it, the user finishes as guidance() says
        Manual,       // only guidance() and settingsPage()
        Unsupported   // this system offers no way
    };

    struct Result {
        bool ok = false;
        QString memento;   // release(): the previous OS state, for restore()
        QString message;   // shown to the user; on success too, when there is more to do
    };

    virtual ~ScreenshotKey() = default;

    [[nodiscard]] virtual Support support() const = 0;
    // "Print Screen", or "⇧⌘3, ⇧⌘4 and ⇧⌘5".
    [[nodiscard]] virtual QString keyName() const = 0;
    // Who holds the key by default: "Snipping Tool", "macOS", "Spectacle", "GNOME".
    [[nodiscard]] virtual QString ownerName() const = 0;
    // Who else answers to seq; empty = nobody known.
    [[nodiscard]] virtual QString holderOf(const QKeySequence &seq) const = 0;
    // Snim's bindings once the key is free.
    [[nodiscard]] virtual QList<HotkeyBinding> preset() const = 0;
    [[nodiscard]] virtual QString guidance() const { return {}; }
    [[nodiscard]] virtual QUrl settingsPage() const { return {}; }

    virtual Result release() = 0;
    virtual Result restore(const QString &memento) = 0;
};

enum class ScreenshotKeyKind { Windows, Mac, Kde, Gnome, Unsupported };

// Pure: the strategy for this platform and XDG_CURRENT_DESKTOP; Linux needs the portal.
[[nodiscard]] ScreenshotKeyKind chooseScreenshotKey(HotkeyPlatform platform,
                                                    const QString &currentDesktop,
                                                    bool portalAvailable);

// Snim's bindings after the swap: Print, or the macOS system keys (Qt's Ctrl is Command).
[[nodiscard]] QList<HotkeyBinding> screenshotKeyPreset(HotkeyPlatform platform);

} // namespace Hotkeys

#endif // HOTKEYS_SCREENSHOTKEY_H
