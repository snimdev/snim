#include "hotkeys/screenshotkey/MacScreenshotKey.h"

#include <utility>

namespace Hotkeys {

namespace {

// Settings asks on every keystroke; older answers are read again.
constexpr qint64 kCacheMs = 2000;

QString shortcutsPage()
{
    return ScreenshotKey::tr("System Settings > Keyboard > Keyboard Shortcuts > Screenshots");
}

} // namespace

MacScreenshotKey::MacScreenshotKey(Prefs prefs) : m_prefs(std::move(prefs))
{
}

ScreenshotKey::Support MacScreenshotKey::support() const
{
    return Support::Manual;
}

QString MacScreenshotKey::keyName() const
{
    return tr("⇧⌘3, ⇧⌘4 and ⇧⌘5");
}

QString MacScreenshotKey::ownerName() const
{
    return tr("macOS");
}

QString MacScreenshotKey::holderOf(const QKeySequence &seq) const
{
    const std::optional<MacSymbolicHotkey> hotkey = macScreenshotHotkeyOn(seq);
    if (!hotkey)
        return {};
    // Unreadable prefs most likely hold the system default.
    const MacSymbolicState state =
        states().value_or(MacSymbolicStates()).value(hotkey->id, MacSymbolicState::Absent);
    return macSymbolicHotkeyIsOn(state) ? ownerName() : QString();
}

QList<HotkeyBinding> MacScreenshotKey::preset() const
{
    return screenshotKeyPreset(HotkeyPlatform::Mac);
}

QString MacScreenshotKey::guidance() const
{
    return tr("Turn off ⇧⌘3, ⇧⌘4 and ⇧⌘5 in %1, then use them for Snim's Capture Full "
              "Screen, Capture Area and Record Area.")
        .arg(shortcutsPage());
}

QUrl MacScreenshotKey::settingsPage() const
{
    return QUrl(QStringLiteral("x-apple.systempreferences:com.apple.Keyboard-Settings.extension"));
}

ScreenshotKey::Result MacScreenshotKey::release()
{
    return {false, {}, guidance()};
}

ScreenshotKey::Result MacScreenshotKey::restore(const QString &)
{
    return {false, {}, guidance()};
}

std::optional<MacSymbolicStates> MacScreenshotKey::states() const
{
    if (!m_statesRead.isValid() || m_statesRead.hasExpired(kCacheMs)) {
        m_states = m_prefs.read ? m_prefs.read() : std::nullopt;
        m_statesRead.start();
    }
    return m_states;
}

} // namespace Hotkeys
