#include "hotkeys/WinKeyMapping.h"

#include <algorithm>

namespace Hotkeys {

namespace {

// RegisterHotKey modifier flags (winuser.h), repeated so this file needs no windows.h.
constexpr quint32 kModAlt     = 0x0001;
constexpr quint32 kModControl = 0x0002;
constexpr quint32 kModShift   = 0x0004;
constexpr quint32 kModWin     = 0x0008;

// VK codes for 'A'..'Z' / '0'..'9' are their ASCII values, same as Qt::Key.
static_assert(static_cast<int>(Qt::Key_A) == 0x41);
static_assert(static_cast<int>(Qt::Key_0) == 0x30);

// Keypad keys that have their own VK; the rest (Enter, and Home etc. with Num Lock
// off) share the main-block VK, which is all RegisterHotKey can tell apart anyway.
std::optional<quint32> keypadVirtualKeyFor(Qt::Key key)
{
    // VK_NUMPAD0 0x60 .. VK_NUMPAD9 0x69.
    if (key >= Qt::Key_0 && key <= Qt::Key_9)
        return 0x60 + static_cast<quint32>(key - Qt::Key_0);

    switch (key) {
    case Qt::Key_Asterisk: return 0x6A;   // VK_MULTIPLY
    case Qt::Key_Plus:     return 0x6B;   // VK_ADD
    case Qt::Key_Minus:    return 0x6D;   // VK_SUBTRACT
    case Qt::Key_Period:   return 0x6E;   // VK_DECIMAL
    case Qt::Key_Slash:    return 0x6F;   // VK_DIVIDE
    default: return std::nullopt;
    }
}

std::optional<quint32> virtualKeyFor(Qt::Key key)
{
    if (key >= Qt::Key_A && key <= Qt::Key_Z)
        return static_cast<quint32>(key);
    if (key >= Qt::Key_0 && key <= Qt::Key_9)
        return static_cast<quint32>(key);
    // VK_F1 0x70 .. VK_F24 0x87, contiguous in both enumerations.
    if (key >= Qt::Key_F1 && key <= Qt::Key_F24)
        return 0x70 + static_cast<quint32>(key - Qt::Key_F1);

    switch (key) {
    case Qt::Key_Space:     return 0x20;   // VK_SPACE
    case Qt::Key_Return:    return 0x0D;   // VK_RETURN
    case Qt::Key_Enter:     return 0x0D;   // keypad Enter is VK_RETURN too
    case Qt::Key_Tab:       return 0x09;   // VK_TAB
    case Qt::Key_Escape:    return 0x1B;   // VK_ESCAPE
    case Qt::Key_Backspace: return 0x08;   // VK_BACK
    case Qt::Key_Delete:    return 0x2E;   // VK_DELETE
    case Qt::Key_Home:      return 0x24;   // VK_HOME
    case Qt::Key_End:       return 0x23;   // VK_END
    case Qt::Key_PageUp:    return 0x21;   // VK_PRIOR
    case Qt::Key_PageDown:  return 0x22;   // VK_NEXT
    case Qt::Key_Left:      return 0x25;   // VK_LEFT
    case Qt::Key_Up:        return 0x26;   // VK_UP
    case Qt::Key_Right:     return 0x27;   // VK_RIGHT
    case Qt::Key_Down:      return 0x28;   // VK_DOWN
    case Qt::Key_Insert:    return 0x2D;   // VK_INSERT
    case Qt::Key_Print:     return 0x2C;   // VK_SNAPSHOT
    case Qt::Key_Pause:     return 0x13;   // VK_PAUSE
    case Qt::Key_ScrollLock: return 0x91;  // VK_SCROLL

    // OEM keys. These are US-layout positions, which is the same compromise
    // RegisterHotKey itself makes.
    case Qt::Key_Semicolon:    return 0xBA;   // VK_OEM_1
    case Qt::Key_Equal:        return 0xBB;   // VK_OEM_PLUS
    case Qt::Key_Comma:        return 0xBC;   // VK_OEM_COMMA
    case Qt::Key_Minus:        return 0xBD;   // VK_OEM_MINUS
    case Qt::Key_Period:       return 0xBE;   // VK_OEM_PERIOD
    case Qt::Key_Slash:        return 0xBF;   // VK_OEM_2
    case Qt::Key_QuoteLeft:    return 0xC0;   // VK_OEM_3 (grave)
    case Qt::Key_BracketLeft:  return 0xDB;   // VK_OEM_4
    case Qt::Key_Backslash:    return 0xDC;   // VK_OEM_5
    case Qt::Key_BracketRight: return 0xDD;   // VK_OEM_6
    case Qt::Key_Apostrophe:   return 0xDE;   // VK_OEM_7

    default: return std::nullopt;
    }
}

} // namespace

std::optional<WinHotkey> toWinHotkey(const QKeySequence &seq)
{
    if (seq.isEmpty())
        return std::nullopt;

    const QKeyCombination chord = seq[0];
    std::optional<quint32> vk;
    if (chord.keyboardModifiers() & Qt::KeypadModifier)
        vk = keypadVirtualKeyFor(chord.key());
    if (!vk)
        vk = virtualKeyFor(chord.key());
    if (!vk)
        return std::nullopt;

    // No Qt/Windows surprises here, unlike the macOS mapping: Ctrl is Ctrl and Meta
    // is the Windows key.
    const Qt::KeyboardModifiers mods = chord.keyboardModifiers();
    quint32 winMods = 0;
    if (mods & Qt::ControlModifier) winMods |= kModControl;
    if (mods & Qt::AltModifier)     winMods |= kModAlt;
    if (mods & Qt::ShiftModifier)   winMods |= kModShift;
    if (mods & Qt::MetaModifier)    winMods |= kModWin;

    return WinHotkey{*vk, winMods};
}

bool snippingToolOwnsPrintScreen(std::optional<quint32> setting, quint32 buildNumber)
{
    if (setting)
        return *setting != 0;
    return buildNumber >= 22621;
}

QString encodeSnippingSetting(std::optional<quint32> setting)
{
    return setting ? QString::number(*setting) : QStringLiteral("absent");
}

std::optional<std::optional<quint32>> decodeSnippingSetting(const QString &memento)
{
    using Decoded = std::optional<std::optional<quint32>>;
    if (memento == QLatin1String("absent"))
        return Decoded(std::in_place);   // engaged, holding "absent"
    // Digits only: toUInt() alone would also take a sign or surrounding spaces.
    const auto isDigit = [](QChar c) { return c >= u'0' && c <= u'9'; };
    if (memento.isEmpty() || !std::all_of(memento.cbegin(), memento.cend(), isDigit))
        return std::nullopt;
    bool ok = false;
    const quint32 value = memento.toUInt(&ok);
    if (!ok)
        return std::nullopt;
    return Decoded(std::in_place, value);
}

} // namespace Hotkeys
