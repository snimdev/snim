#include "hotkeys/MacKeyMapping.h"

namespace Hotkeys {

namespace {

// Carbon modifier masks (HIToolbox Events.h). ABI-frozen since Carbon, and repeated
// here so this file needs no Apple headers.
constexpr quint32 kCarbonCmd     = 0x0100;
constexpr quint32 kCarbonShift   = 0x0200;
constexpr quint32 kCarbonOption  = 0x0800;
constexpr quint32 kCarbonControl = 0x1000;

// kVK_ virtual key codes (HIToolbox Events.h). These are positional codes for the
// ANSI layout, not characters, so they are layout-independent by design.
std::optional<quint32> virtualKeyFor(Qt::Key key)
{
    switch (key) {
    case Qt::Key_A: return 0x00;
    case Qt::Key_B: return 0x0B;
    case Qt::Key_C: return 0x08;
    case Qt::Key_D: return 0x02;
    case Qt::Key_E: return 0x0E;
    case Qt::Key_F: return 0x03;
    case Qt::Key_G: return 0x05;
    case Qt::Key_H: return 0x04;
    case Qt::Key_I: return 0x22;
    case Qt::Key_J: return 0x26;
    case Qt::Key_K: return 0x28;
    case Qt::Key_L: return 0x25;
    case Qt::Key_M: return 0x2E;
    case Qt::Key_N: return 0x2D;
    case Qt::Key_O: return 0x1F;
    case Qt::Key_P: return 0x23;
    case Qt::Key_Q: return 0x0C;
    case Qt::Key_R: return 0x0F;
    case Qt::Key_S: return 0x01;
    case Qt::Key_T: return 0x11;
    case Qt::Key_U: return 0x20;
    case Qt::Key_V: return 0x09;
    case Qt::Key_W: return 0x0D;
    case Qt::Key_X: return 0x07;
    case Qt::Key_Y: return 0x10;
    case Qt::Key_Z: return 0x06;

    case Qt::Key_0: return 0x1D;
    case Qt::Key_1: return 0x12;
    case Qt::Key_2: return 0x13;
    case Qt::Key_3: return 0x14;
    case Qt::Key_4: return 0x15;
    case Qt::Key_5: return 0x17;
    case Qt::Key_6: return 0x16;
    case Qt::Key_7: return 0x1A;
    case Qt::Key_8: return 0x1C;
    case Qt::Key_9: return 0x19;

    case Qt::Key_Equal:        return 0x18;
    case Qt::Key_Minus:        return 0x1B;
    case Qt::Key_BracketRight: return 0x1E;
    case Qt::Key_BracketLeft:  return 0x21;
    case Qt::Key_Apostrophe:   return 0x27;
    case Qt::Key_Semicolon:    return 0x29;
    case Qt::Key_Backslash:    return 0x2A;
    case Qt::Key_Comma:        return 0x2B;
    case Qt::Key_Slash:        return 0x2C;
    case Qt::Key_Period:       return 0x2F;
    case Qt::Key_QuoteLeft:    return 0x32;   // grave/backtick

    case Qt::Key_Return:    return 0x24;
    case Qt::Key_Tab:       return 0x30;
    case Qt::Key_Space:     return 0x31;
    case Qt::Key_Backspace: return 0x33;      // kVK_Delete, the backspace key
    case Qt::Key_Escape:    return 0x35;
    case Qt::Key_Delete:    return 0x75;      // kVK_ForwardDelete
    case Qt::Key_Home:      return 0x73;
    case Qt::Key_End:       return 0x77;
    case Qt::Key_PageUp:    return 0x74;
    case Qt::Key_PageDown:  return 0x79;
    case Qt::Key_Left:      return 0x7B;
    case Qt::Key_Right:     return 0x7C;
    case Qt::Key_Down:      return 0x7D;
    case Qt::Key_Up:        return 0x7E;

    case Qt::Key_F1:  return 0x7A;
    case Qt::Key_F2:  return 0x78;
    case Qt::Key_F3:  return 0x63;
    case Qt::Key_F4:  return 0x76;
    case Qt::Key_F5:  return 0x60;
    case Qt::Key_F6:  return 0x61;
    case Qt::Key_F7:  return 0x62;
    case Qt::Key_F8:  return 0x64;
    case Qt::Key_F9:  return 0x65;
    case Qt::Key_F10: return 0x6D;
    case Qt::Key_F11: return 0x67;
    case Qt::Key_F12: return 0x6F;
    case Qt::Key_F13: return 0x69;
    case Qt::Key_F14: return 0x6B;
    case Qt::Key_F15: return 0x71;
    case Qt::Key_F16: return 0x6A;
    case Qt::Key_F17: return 0x40;
    case Qt::Key_F18: return 0x4F;
    case Qt::Key_F19: return 0x50;
    case Qt::Key_F20: return 0x5A;

    default: return std::nullopt;
    }
}

} // namespace

std::optional<CarbonHotkey> toCarbonHotkey(const QKeySequence &seq)
{
    if (seq.isEmpty())
        return std::nullopt;

    const QKeyCombination chord = seq[0];
    const std::optional<quint32> vk = virtualKeyFor(chord.key());
    if (!vk)
        return std::nullopt;

    // The Qt/macOS swap, and the reason this table is not a straight copy of the
    // Windows one: Qt maps Qt::ControlModifier to Command and Qt::MetaModifier to the
    // physical Control key on macOS, so "Ctrl+Shift+A" is Cmd+Shift+A to the user.
    const Qt::KeyboardModifiers mods = chord.keyboardModifiers();
    quint32 carbonMods = 0;
    if (mods & Qt::ControlModifier) carbonMods |= kCarbonCmd;
    if (mods & Qt::ShiftModifier)   carbonMods |= kCarbonShift;
    if (mods & Qt::AltModifier)     carbonMods |= kCarbonOption;
    if (mods & Qt::MetaModifier)    carbonMods |= kCarbonControl;

    return CarbonHotkey{*vk, carbonMods};
}

} // namespace Hotkeys
