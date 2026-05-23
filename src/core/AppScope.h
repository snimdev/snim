#ifndef CORE_APPSCOPE_H
#define CORE_APPSCOPE_H

#include <QString>
#include <QStringView>

/**
 * Portal app identity on Linux.
 *
 * xdg-desktop-portal derives the caller's app id server-side from the systemd unit that
 * owns the caller's cgroup, so a build started from a terminal or the portable tarball sits in
 * a unit with no app id at all and the GlobalShortcuts portal refuses the session with
 * "An app id is required" - every global hotkey then fails. Moving ourselves into a
 * transient app-dev.snim.Snim-<pid>.scope, before any portal session exists, gives the
 * portal a unit name it can read the app id out of.
 *
 * Linux-only behaviour; the header compiles everywhere and reports "already scoped"
 * elsewhere, so callers need no #ifdef around the check itself.
 */
namespace Core::AppScope {

// True when the leaf unit of one /proc/self/cgroup line is an app-scoped systemd unit.
// Pure string work, so it is testable without /proc.
[[nodiscard]] bool unitLineIndicatesAppScope(QStringView cgroupLine);

// True when this process already runs under such a unit (always true off Linux).
[[nodiscard]] bool inAppScope();

// Asks systemd (session bus) to start a transient scope holding this process, then waits
// the few ms until the kernel reports the move (the reply only means "job queued", and the
// portal reads the cgroup right after). Needs a live QCoreApplication for the session bus.
// Returns false and fills errorOut with the D-Bus error on failure; never throws.
bool adoptAppScope(QString *errorOut = nullptr);

} // namespace Core::AppScope

#endif // CORE_APPSCOPE_H
