#include "hotkeys/MacSymbolicHotkeys.h"

#include "hotkeys/HotkeyBindings.h"

#include <QStringList>

namespace Hotkeys {

namespace {

// NSEventModifierFlagShift | NSEventModifierFlagCommand.
constexpr int kShiftCommand = 0x120000;

const QString kAbsent = QStringLiteral("absent");
const QString kOn = QStringLiteral("1");
const QString kOff = QStringLiteral("0");

bool isScreenshotHotkey(int id)
{
    for (const MacSymbolicHotkey &hotkey : macScreenshotHotkeys()) {
        if (hotkey.id == id)
            return true;
    }
    return false;
}

} // namespace

QList<MacSymbolicHotkey> macScreenshotHotkeys()
{
    // The virtual key codes are kVK_ANSI_3, kVK_ANSI_4 and kVK_ANSI_5.
    return {
        {28, {'3', 20, kShiftCommand}, QKeySequence(QStringLiteral("Ctrl+Shift+3"))},
        {30, {'4', 21, kShiftCommand}, QKeySequence(QStringLiteral("Ctrl+Shift+4"))},
        {184, {'5', 23, kShiftCommand}, QKeySequence(QStringLiteral("Ctrl+Shift+5"))},
    };
}

std::optional<MacSymbolicHotkey> macScreenshotHotkeyOn(const QKeySequence &seq)
{
    const QKeySequence key = HotkeyBindings::normalized(seq);
    if (key.isEmpty())
        return std::nullopt;
    for (const MacSymbolicHotkey &hotkey : macScreenshotHotkeys()) {
        if (hotkey.sequence == key)
            return hotkey;
    }
    return std::nullopt;
}

QString encodeMacSymbolicStates(const MacSymbolicStates &states)
{
    QStringList parts;
    for (auto it = states.cbegin(); it != states.cend(); ++it) {
        QString value;
        switch (it.value()) {
        case MacSymbolicState::Absent:   value = kAbsent; break;
        case MacSymbolicState::Enabled:  value = kOn; break;
        case MacSymbolicState::Disabled: value = kOff; break;
        }
        parts << QString::number(it.key()) + QLatin1Char('=') + value;
    }
    return parts.join(QLatin1Char(','));
}

std::optional<MacSymbolicStates> decodeMacSymbolicStates(const QString &memento)
{
    if (memento.isEmpty())
        return std::nullopt;
    MacSymbolicStates states;
    for (const QString &part : memento.split(QLatin1Char(','))) {
        const qsizetype equals = part.indexOf(QLatin1Char('='));
        if (equals < 0)
            return std::nullopt;
        const QString idText = part.left(equals);
        const QString value = part.mid(equals + 1);
        bool ok = false;
        const int id = idText.toInt(&ok);
        // Only the spelling encode writes: no sign, padding or repeats.
        if (!ok || QString::number(id) != idText || !isScreenshotHotkey(id) || states.contains(id))
            return std::nullopt;
        if (value == kAbsent)
            states.insert(id, MacSymbolicState::Absent);
        else if (value == kOn)
            states.insert(id, MacSymbolicState::Enabled);
        else if (value == kOff)
            states.insert(id, MacSymbolicState::Disabled);
        else
            return std::nullopt;
    }
    return states;
}

MacSymbolicWrites macReleaseWrites(const MacSymbolicStates &now)
{
    MacSymbolicWrites writes;
    for (const MacSymbolicHotkey &hotkey : macScreenshotHotkeys()) {
        MacSymbolicWrite write = MacSymbolicWrite::Keep;
        switch (now.value(hotkey.id, MacSymbolicState::Absent)) {
        // An entry with no value would not say which key it turns off.
        case MacSymbolicState::Absent:   write = MacSymbolicWrite::AddDisabled; break;
        case MacSymbolicState::Enabled:  write = MacSymbolicWrite::Disable; break;
        case MacSymbolicState::Disabled: write = MacSymbolicWrite::Keep; break;
        }
        writes.insert(hotkey.id, write);
    }
    return writes;
}

MacSymbolicWrites macRestoreWrites(const std::optional<MacSymbolicStates> &before,
                                   const MacSymbolicStates &now)
{
    MacSymbolicWrites writes;
    for (const MacSymbolicHotkey &hotkey : macScreenshotHotkeys()) {
        MacSymbolicWrite write = MacSymbolicWrite::Keep;
        // One the user turned back on since is theirs; one off before stays off.
        if (now.value(hotkey.id, MacSymbolicState::Absent) == MacSymbolicState::Disabled) {
            const MacSymbolicState was =
                before ? before->value(hotkey.id, MacSymbolicState::Disabled)
                       : MacSymbolicState::Absent;
            if (was == MacSymbolicState::Absent)
                write = MacSymbolicWrite::Remove;
            else if (was == MacSymbolicState::Enabled)
                write = MacSymbolicWrite::Enable;
        }
        writes.insert(hotkey.id, write);
    }
    return writes;
}

bool macSymbolicWritesChange(const MacSymbolicWrites &writes)
{
    for (const MacSymbolicWrite write : writes) {
        if (write != MacSymbolicWrite::Keep)
            return true;
    }
    return false;
}

} // namespace Hotkeys
