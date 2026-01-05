#include "hotkeys/HotkeyBindings.h"

#include "core/Settings.h"

namespace Hotkeys {

namespace {

QString storedText(HotkeyAction a)
{
    switch (a) {
    case HotkeyAction::CaptureArea:       return Core::Settings::hotkeyCaptureArea();
    case HotkeyAction::CaptureWindow:     return Core::Settings::hotkeyCaptureWindow();
    case HotkeyAction::CaptureFullScreen: return Core::Settings::hotkeyCaptureFullScreen();
    case HotkeyAction::OcrTextSnip:       return Core::Settings::hotkeyOcrTextSnip();
    case HotkeyAction::RecordArea:        return Core::Settings::hotkeyRecordArea();
    case HotkeyAction::RecordWindow:      return Core::Settings::hotkeyRecordWindow();
    }
    return {};
}

void storeText(HotkeyAction a, const QString &text)
{
    switch (a) {
    case HotkeyAction::CaptureArea:       Core::Settings::setHotkeyCaptureArea(text); return;
    case HotkeyAction::CaptureWindow:     Core::Settings::setHotkeyCaptureWindow(text); return;
    case HotkeyAction::CaptureFullScreen: Core::Settings::setHotkeyCaptureFullScreen(text); return;
    case HotkeyAction::OcrTextSnip:       Core::Settings::setHotkeyOcrTextSnip(text); return;
    case HotkeyAction::RecordArea:        Core::Settings::setHotkeyRecordArea(text); return;
    case HotkeyAction::RecordWindow:      Core::Settings::setHotkeyRecordWindow(text); return;
    }
}

} // namespace

QKeySequence HotkeyBindings::sequence(HotkeyAction a)
{
    return QKeySequence::fromString(storedText(a), QKeySequence::PortableText);
}

void HotkeyBindings::setSequence(HotkeyAction a, const QKeySequence &seq)
{
    // Empty writes "" (explicit unbound); removing the key would resurrect the default.
    storeText(a, seq.toString(QKeySequence::PortableText));
}

QKeySequence HotkeyBindings::defaultSequence(HotkeyAction a)
{
    return hotkeyActionDefault(a);
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
