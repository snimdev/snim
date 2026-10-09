#include "hotkeys/screenshotkey/MacScreenshotKey.h"

#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>

#include <utility>

namespace Hotkeys {

namespace {

// Settings asks on every keystroke; older answers are read again.
constexpr qint64 kCacheMs = 2000;
// Reloads the saved keyboard shortcuts, as System Settings does after a change.
const QString kActivateSettings = QStringLiteral(
    "/System/Library/PrivateFrameworks/SystemAdministration.framework/Resources/activateSettings");
// It runs on the GUI thread, so a stuck run may only hold it this long.
constexpr int kActivateTimeoutMs = 5000;

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
    if (!m_oneClick)
        m_oneClick = m_prefs.canActivate && m_prefs.canActivate();
    return *m_oneClick ? Support::Automatic : Support::Manual;
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
    if (support() != Support::Automatic)
        return {false, {}, guidance()};
    forget();

    const std::optional<MacSymbolicStates> before = readAll();
    if (!before)
        return {false, {}, tr("Snim could not read the macOS screenshot shortcuts.")};
    return apply(macReleaseWrites(*before), encodeMacSymbolicStates(*before),
                 tr("macOS did not let Snim turn off its screenshot shortcuts."),
                 tr("Log out of macOS and back in to free %1 for Snim.").arg(keyName()));
}

ScreenshotKey::Result MacScreenshotKey::restore(const QString &memento)
{
    forget();
    const std::optional<MacSymbolicStates> now = readAll();
    if (!now)
        return {false, {}, tr("Snim could not read the macOS screenshot shortcuts.")};
    // An unreadable memento falls back to the system default, which is on.
    return apply(macRestoreWrites(decodeMacSymbolicStates(memento), *now), {},
                 tr("macOS did not let Snim turn its screenshot shortcuts back on."),
                 tr("Log out of macOS and back in to give %1 back to macOS.").arg(keyName()));
}

bool MacScreenshotKey::canActivateSettings()
{
    // Tests run on the developer's own Mac: never a one-click swap there.
    if (QStandardPaths::isTestModeEnabled())
        return false;
    const QFileInfo tool(kActivateSettings);
    return tool.isFile() && tool.isExecutable();
}

bool MacScreenshotKey::activateSettings()
{
    if (QStandardPaths::isTestModeEnabled())
        return false;
    QProcess process;
    process.start(kActivateSettings, {QStringLiteral("-u")});
    if (!process.waitForFinished(kActivateTimeoutMs)) {
        process.kill();
        process.waitForFinished();
        return false;
    }
    return process.exitStatus() == QProcess::NormalExit && process.exitCode() == 0;
}

std::optional<MacSymbolicStates> MacScreenshotKey::readAll() const
{
    const std::optional<MacSymbolicStates> read = m_prefs.read ? m_prefs.read() : std::nullopt;
    if (!read)
        return std::nullopt;
    MacSymbolicStates all;
    for (const MacSymbolicHotkey &hotkey : macScreenshotHotkeys())
        all.insert(hotkey.id, read->value(hotkey.id, MacSymbolicState::Absent));
    return all;
}

ScreenshotKey::Result MacScreenshotKey::apply(const MacSymbolicWrites &writes,
                                              const QString &memento, const QString &failure,
                                              const QString &logOut)
{
    // Already as wanted: nothing to write or reload.
    if (!macSymbolicWritesChange(writes))
        return {true, memento, {}};
    if (!m_prefs.write || !m_prefs.write(writes))
        return {false, {}, failure};
    // Saved either way; without the reload they apply at the next log in.
    if (!m_prefs.activate || !m_prefs.activate())
        return {true, memento, logOut};
    return {true, memento, {}};
}

std::optional<MacSymbolicStates> MacScreenshotKey::states() const
{
    if (!m_statesRead.isValid() || m_statesRead.hasExpired(kCacheMs)) {
        m_states = m_prefs.read ? m_prefs.read() : std::nullopt;
        m_statesRead.start();
    }
    return m_states;
}

void MacScreenshotKey::forget() const
{
    m_statesRead.invalidate();
}

} // namespace Hotkeys
