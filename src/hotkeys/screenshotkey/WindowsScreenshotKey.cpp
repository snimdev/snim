#include "hotkeys/screenshotkey/WindowsScreenshotKey.h"

// Win32 only (the registry helpers); on any other host this is an empty translation unit.
#ifdef Q_OS_WIN

#include "hotkeys/HotkeyBindings.h"
#include "hotkeys/WinKeyMapping.h"
#include "hotkeys/WinPrintScreen.h"

#include <QStandardPaths>

#include <optional>

namespace Hotkeys {

namespace {

QString testModeRefusal()
{
    return ScreenshotKey::tr("Snim leaves the Windows registry alone while testing.");
}

QString writeFailure()
{
    return ScreenshotKey::tr("Windows did not let Snim change the Print Screen setting.");
}

} // namespace

QString WindowsScreenshotKey::keyName() const
{
    return tr("Print Screen");
}

QString WindowsScreenshotKey::ownerName() const
{
    return tr("Snipping Tool");
}

QString WindowsScreenshotKey::holderOf(const QKeySequence &seq) const
{
    if (HotkeyBindings::normalized(seq) != QKeySequence(Qt::Key_Print))
        return {};
    // One registry read, cheap enough for every keystroke in Settings.
    return snippingToolOwnsPrintScreen(printScreenSnippingSetting(), windowsBuildNumber())
               ? ownerName()
               : QString();
}

QList<HotkeyBinding> WindowsScreenshotKey::preset() const
{
    return screenshotKeyPreset(HotkeyPlatform::Windows);
}

ScreenshotKey::Result WindowsScreenshotKey::release()
{
    // CI runs on a real Windows box: tests must never flip the user's setting.
    if (QStandardPaths::isTestModeEnabled())
        return {false, {}, testModeRefusal()};

    const std::optional<quint32> before = printScreenSnippingSetting();
    if (!writePrintScreenSnippingSetting(0u))
        return {false, {}, writeFailure()};
    announceKeyboardSettingChange();
    // Snim cannot tell whether Explorer picked the change up, so this is always said.
    return {true, encodeSnippingSetting(before),
            tr("If Print Screen still opens Snipping Tool, sign out of Windows and back in.")};
}

ScreenshotKey::Result WindowsScreenshotKey::restore(const QString &memento)
{
    if (QStandardPaths::isTestModeEnabled())
        return {false, {}, testModeRefusal()};

    // An unreadable memento falls back to the OS default, which is Snipping Tool's.
    const std::optional<quint32> before =
        decodeSnippingSetting(memento).value_or(std::optional<quint32>());
    if (!writePrintScreenSnippingSetting(before))
        return {false, {}, writeFailure()};
    announceKeyboardSettingChange();
    if (!snippingToolOwnsPrintScreen(before, windowsBuildNumber()))
        return {true, {}, {}};
    return {true, {},
            tr("If Print Screen does not open Snipping Tool, sign out of Windows and back in.")};
}

} // namespace Hotkeys

#endif // Q_OS_WIN
