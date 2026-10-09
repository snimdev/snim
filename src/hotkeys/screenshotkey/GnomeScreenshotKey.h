#ifndef HOTKEYS_GNOMESCREENSHOTKEY_H
#define HOTKEYS_GNOMESCREENSHOTKEY_H

#include "core/Sandbox.h"
#include "hotkeys/ScreenshotKey.h"

#include <QElapsedTimer>
#include <QString>
#include <QStringList>

#include <functional>
#include <optional>

namespace Hotkeys {

// gsettings' GVariant text for an `as` value: "['Print']", "['<Shift>Print', 'Print']",
// "@as []". nullopt for anything else.
[[nodiscard]] std::optional<QStringList> parseGSettingsList(const QString &text);
// The inverse; an empty list is "@as []", since gsettings set needs the type then.
[[nodiscard]] QString formatGSettingsList(const QStringList &list);

/**
 * GNOME 48+: Print opens the screenshot UI (org.gnome.shell.keybindings
 * show-screenshot-ui). release() takes Print out of that list with gsettings; GNOME
 * then lets the user give it to Snim in its own shortcut dialog, as guidance() says.
 * Inside Flatpak GNOME's settings are out of reach, so there is only guidance. The
 * memento is the previous list as GVariant text.
 */
class GnomeScreenshotKey : public ScreenshotKey
{
public:
    // Runs `gsettings <args>` and returns its standard output; nullopt when it failed.
    using GSettings = std::function<std::optional<QString>(const QStringList &args)>;

    explicit GnomeScreenshotKey(GSettings gsettings = &GnomeScreenshotKey::runGSettings,
                                bool flatpak = Core::Sandbox::isFlatpak());

    [[nodiscard]] Support support() const override;
    [[nodiscard]] QString keyName() const override;
    [[nodiscard]] QString ownerName() const override;
    [[nodiscard]] QString holderOf(const QKeySequence &seq) const override;
    [[nodiscard]] QList<HotkeyBinding> preset() const override;
    [[nodiscard]] QString guidance() const override;

    Result release() override;
    Result restore(const QString &memento) override;

    // The real runner: gsettings from PATH with a short timeout; refuses in test mode.
    static std::optional<QString> runGSettings(const QStringList &args);

private:
    // The screenshot UI's keys, read at most once per short interval.
    [[nodiscard]] std::optional<QStringList> screenshotKeys() const;
    void forget() const;

    GSettings m_gsettings;
    bool m_flatpak;
    mutable std::optional<QStringList> m_keys;
    mutable QElapsedTimer m_keysRead;
};

} // namespace Hotkeys

#endif // HOTKEYS_GNOMESCREENSHOTKEY_H
