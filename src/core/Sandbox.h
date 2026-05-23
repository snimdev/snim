#ifndef CORE_SANDBOX_H
#define CORE_SANDBOX_H

#include <QString>

/**
 * Detects an app sandbox that takes over parts of desktop integration.
 *
 * Inside Flatpak the runtime provides the app id, the exported desktop entry and the
 * updates, so the features Snim otherwise handles itself step aside there.
 */
namespace Core::Sandbox {

// True when FLATPAK_ID is set or /.flatpak-info exists.
[[nodiscard]] bool isFlatpak();

// Same check against another info file, a test seam.
[[nodiscard]] bool isFlatpak(const QString &infoFilePath);

} // namespace Core::Sandbox

#endif // CORE_SANDBOX_H
