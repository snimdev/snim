#ifndef CORE_DESKTOPINTEGRATION_H
#define CORE_DESKTOPINTEGRATION_H

#include <QString>

/**
 * KDE desktop-entry repair helper for the fast, dialog-free capture path.
 *
 * KWin authorizes org.kde.KWin.ScreenShot2 only for callers that own an installed
 * desktop entry which both declares X-KDE-DBUS-Restricted-Interfaces=org.kde.KWin.ScreenShot2
 * and has an Exec resolving to the running binary; without it every non-interactive
 * ScreenShot2 call fails with NoAuthorized and KWinCaptureStrategy silently falls back
 * to the interactive portal path. The .deb and .rpm ship such an entry system-wide and
 * dev-build.sh installs one, but dev builds started some other way and tarball users
 * have none, so the app repairs it itself. status() judges the entry the desktop actually
 * uses: the user's own when present, else the first one on XDG_DATA_DIRS.
 *
 * Linux-only behaviour; the header compiles everywhere and reports NotApplicable
 * elsewhere. No UI lives here: App::CaptureWorkflow owns the prompt and the tray action.
 */
namespace Core::DesktopIntegration {

enum class Status {
    Installed,                // entry present, authorization key set, Exec is this executable
    ExecMismatch,             // entry present but Exec points somewhere else
    MissingAuthorizationKey,  // entry present but does not declare the ScreenShot2 interface
    NotInstalled,             // no entry in the user's or any system applications folder
    NotApplicable             // not Linux, or inside Flatpak
};

// Where the user-local entry and its icon live (under XDG_DATA_HOME, so tests can redirect them).
[[nodiscard]] QString desktopFilePath();
[[nodiscard]] QString iconFilePath();

// The entry that install() writes: deploy/dev.snim.Snim.desktop.in (a resource) with
// execPath as its Exec, or empty when the binary lacks that resource.
[[nodiscard]] QString desktopEntryContents(const QString &execPath);

[[nodiscard]] Status status();

// Writes the entry (Exec = this executable, quoted when the path has spaces) plus the
// icon, then refreshes the desktop caches best effort.
// Returns false and fills errorOut when the entry cannot be written.
bool install(QString *errorOut = nullptr);

} // namespace Core::DesktopIntegration

#endif // CORE_DESKTOPINTEGRATION_H
