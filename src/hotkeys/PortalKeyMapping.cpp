#include "hotkeys/PortalKeyMapping.h"

namespace Hotkeys {

namespace {

// XKB keysym names (keysymdef.h). The portal parses the trigger with libxkbcommon,
// so these spellings are literal, case included.
QString keysymFor(Qt::Key key)
{
    if (key >= Qt::Key_A && key <= Qt::Key_Z)
        return QString(QChar('a' + (key - Qt::Key_A)));
    if (key >= Qt::Key_0 && key <= Qt::Key_9)
        return QString(QChar('0' + (key - Qt::Key_0)));
    if (key >= Qt::Key_F1 && key <= Qt::Key_F20)
        return QStringLiteral("F%1").arg(key - Qt::Key_F1 + 1);

    switch (key) {
    case Qt::Key_Return:    return QStringLiteral("Return");
    case Qt::Key_Tab:       return QStringLiteral("Tab");
    case Qt::Key_Escape:    return QStringLiteral("Escape");
    case Qt::Key_Backspace: return QStringLiteral("BackSpace");
    case Qt::Key_Delete:    return QStringLiteral("Delete");
    case Qt::Key_Home:      return QStringLiteral("Home");
    case Qt::Key_End:       return QStringLiteral("End");
    case Qt::Key_PageUp:    return QStringLiteral("Prior");
    case Qt::Key_PageDown:  return QStringLiteral("Next");
    case Qt::Key_Left:      return QStringLiteral("Left");
    case Qt::Key_Right:     return QStringLiteral("Right");
    case Qt::Key_Up:        return QStringLiteral("Up");
    case Qt::Key_Down:      return QStringLiteral("Down");
    // Punctuation keysyms are lowercase words, not the characters themselves.
    case Qt::Key_Space:        return QStringLiteral("space");
    case Qt::Key_Comma:        return QStringLiteral("comma");
    case Qt::Key_Period:       return QStringLiteral("period");
    case Qt::Key_Slash:        return QStringLiteral("slash");
    case Qt::Key_Semicolon:    return QStringLiteral("semicolon");
    case Qt::Key_Apostrophe:   return QStringLiteral("apostrophe");
    case Qt::Key_QuoteLeft:    return QStringLiteral("grave");
    case Qt::Key_Minus:        return QStringLiteral("minus");
    case Qt::Key_Equal:        return QStringLiteral("equal");
    case Qt::Key_BracketLeft:  return QStringLiteral("bracketleft");
    case Qt::Key_BracketRight: return QStringLiteral("bracketright");
    case Qt::Key_Backslash:    return QStringLiteral("backslash");
    default: return {};
    }
}

} // namespace

QString toPortalTrigger(const QKeySequence &seq)
{
    if (seq.isEmpty())
        return {};

    const QKeyCombination chord = seq[0];
    const QString keysym = keysymFor(chord.key());
    if (keysym.isEmpty())
        return {};

    // Fixed modifier order: the portal compares triggers as strings.
    const Qt::KeyboardModifiers mods = chord.keyboardModifiers();
    QString trigger;
    if (mods & Qt::ControlModifier) trigger += QStringLiteral("CTRL+");
    if (mods & Qt::ShiftModifier)   trigger += QStringLiteral("SHIFT+");
    if (mods & Qt::AltModifier)     trigger += QStringLiteral("ALT+");
    if (mods & Qt::MetaModifier)    trigger += QStringLiteral("LOGO+");
    return trigger + keysym;
}

} // namespace Hotkeys
