#include "hotkeys/HotkeyAction.h"

namespace Hotkeys {

QList<HotkeyAction> allHotkeyActions()
{
    return {
        HotkeyAction::CaptureArea,
        HotkeyAction::CaptureWindow,
        HotkeyAction::CaptureFullScreen,
        HotkeyAction::OcrTextSnip,
        HotkeyAction::RecordArea,
        HotkeyAction::RecordWindow
    };
}

QString hotkeyActionId(HotkeyAction a)
{
    // Frozen: persisted, and on Linux held by the desktop's portal shortcut store.
    switch (a) {
    case HotkeyAction::CaptureArea:       return QStringLiteral("captureArea");
    case HotkeyAction::CaptureWindow:     return QStringLiteral("captureWindow");
    case HotkeyAction::CaptureFullScreen: return QStringLiteral("captureFullScreen");
    case HotkeyAction::OcrTextSnip:       return QStringLiteral("ocrTextSnip");
    case HotkeyAction::RecordArea:        return QStringLiteral("recordArea");
    case HotkeyAction::RecordWindow:      return QStringLiteral("recordWindow");
    }
    return {};
}

std::optional<HotkeyAction> hotkeyActionFromId(const QString &id)
{
    for (const HotkeyAction a : allHotkeyActions()) {
        if (hotkeyActionId(a) == id)
            return a;
    }
    return std::nullopt;
}

QString hotkeyActionDescription(HotkeyAction a)
{
    switch (a) {
    case HotkeyAction::CaptureArea:       return QStringLiteral("Capture Area");
    case HotkeyAction::CaptureWindow:     return QStringLiteral("Capture Window");
    case HotkeyAction::CaptureFullScreen: return QStringLiteral("Capture Full Screen");
    case HotkeyAction::OcrTextSnip:       return QStringLiteral("Extract Text (OCR)");
    case HotkeyAction::RecordArea:        return QStringLiteral("Record Area");
    case HotkeyAction::RecordWindow:      return QStringLiteral("Record Window");
    }
    return {};
}

QKeySequence hotkeyActionDefault(HotkeyAction a)
{
    // The bound defaults match the sequences the tray menu has always displayed.
    switch (a) {
    case HotkeyAction::CaptureArea:       return QKeySequence(QStringLiteral("Ctrl+Shift+A"));
    case HotkeyAction::CaptureWindow:     return QKeySequence(QStringLiteral("Ctrl+Shift+W"));
    case HotkeyAction::CaptureFullScreen: return {};
    case HotkeyAction::OcrTextSnip:       return QKeySequence(QStringLiteral("Ctrl+Shift+T"));
    case HotkeyAction::RecordArea:        return QKeySequence(QStringLiteral("Ctrl+Shift+R"));
    case HotkeyAction::RecordWindow:      return {};
    }
    return {};
}

} // namespace Hotkeys
