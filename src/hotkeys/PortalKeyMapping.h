#ifndef HOTKEYS_PORTALKEYMAPPING_H
#define HOTKEYS_PORTALKEYMAPPING_H

#include <QKeySequence>
#include <QString>

namespace Hotkeys {

/**
 * Spell a QKeySequence the way the xdg-desktop-portal GlobalShortcuts interface wants
 * a preferred_trigger: uppercase modifiers in the fixed order CTRL+SHIFT+ALT+LOGO,
 * then the key as an XKB keysym name ("CTRL+SHIFT+a").
 *
 * This is only a SUGGESTION to the desktop, which owns the final binding and may
 * hand back something else entirely, so an approximate spelling costs the user a
 * re-bind, never a broken app. Returns "" when the sequence cannot be spelled at
 * all; the backend then registers the shortcut with no preferred trigger.
 */
[[nodiscard]] QString toPortalTrigger(const QKeySequence &seq);

} // namespace Hotkeys

#endif // HOTKEYS_PORTALKEYMAPPING_H
