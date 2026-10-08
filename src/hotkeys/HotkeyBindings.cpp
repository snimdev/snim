#include "hotkeys/HotkeyBindings.h"

#include "core/Settings.h"

namespace Hotkeys {

namespace {

// The settings key name is the action id with a capital first letter (Hotkeys/CaptureArea).
QString settingsName(HotkeyAction a)
{
    QString name = hotkeyActionId(a);
    name[0] = name[0].toUpper();
    return name;
}

} // namespace

QKeySequence HotkeyBindings::sequence(HotkeyAction a)
{
    const QString text = Core::Settings::hotkey(
        settingsName(a), hotkeyActionDefault(a).toString(QKeySequence::PortableText));
    return QKeySequence::fromString(text, QKeySequence::PortableText);
}

void HotkeyBindings::setSequence(HotkeyAction a, const QKeySequence &seq)
{
    if (normalized(seq) == hotkeyActionDefault(a)) {
        Core::Settings::removeHotkey(settingsName(a));
        return;
    }
    // Empty writes "" (explicit unbound); removing the key would resurrect the default.
    Core::Settings::setHotkey(settingsName(a), seq.toString(QKeySequence::PortableText));
}

bool HotkeyBindings::isCustomized(HotkeyAction a)
{
    return Core::Settings::hasHotkey(settingsName(a));
}

void HotkeyBindings::migrateDefaults()
{
    if (Core::Settings::hotkeyDefaultsVersion() >= 1)
        return;

    // The rc.1 defaults, "" included: Apply wrote the empty default of unbound actions too.
    const QList<QPair<HotkeyAction, QString>> rc1Defaults = {
        {HotkeyAction::CaptureArea, QStringLiteral("Ctrl+Shift+A")},
        {HotkeyAction::CaptureWindow, QStringLiteral("Ctrl+Shift+W")},
        {HotkeyAction::CaptureFullScreen, QString()},
        {HotkeyAction::OcrTextSnip, QStringLiteral("Ctrl+Shift+T")},
        {HotkeyAction::RecordArea, QStringLiteral("Ctrl+Shift+R")},
        {HotkeyAction::RecordWindow, QString()},
    };
    for (const auto &[action, oldText] : rc1Defaults) {
        const QString name = settingsName(action);
        if (!Core::Settings::hasHotkey(name))
            continue;
        const QKeySequence stored =
            QKeySequence::fromString(Core::Settings::hotkey(name, {}), QKeySequence::PortableText);
        if (normalized(stored) == QKeySequence::fromString(oldText, QKeySequence::PortableText))
            Core::Settings::removeHotkey(name);
    }
    Core::Settings::setHotkeyDefaultsVersion(1);
}

QList<HotkeyBinding> HotkeyBindings::activeBindings()
{
    QList<HotkeyBinding> out;
    for (const HotkeyAction a : allHotkeyActions()) {
        const QKeySequence seq = normalized(sequence(a));
        if (!seq.isEmpty())
            out.append(HotkeyBinding{a, seq});
    }
    return out;
}

QKeySequence HotkeyBindings::normalized(const QKeySequence &seq)
{
    if (seq.isEmpty())
        return {};
    return QKeySequence(seq[0]);
}

} // namespace Hotkeys
