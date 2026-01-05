#include "hotkeys/WinKeyMapping.h"

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
    const std::optional<quint32> vk = virtualKeyFor(chord.key());
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

} // namespace Hotkeys
