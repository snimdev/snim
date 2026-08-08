#ifndef CORE_BUNDLEDPATHS_H
#define CORE_BUNDLEDPATHS_H

#include <QString>

/**
 * Self-location for a relocatable Linux install (the packaged /opt/snim tree, the
 * portable tarball, the Flatpak's /app, any prefix the user moved). Nothing but the
 * binary's own location is known at runtime, so the bundled Tesseract language packs are
 * addressed relative to the directory holding it, in ../share/tessdata, exactly where
 * CMake's install() and the bundled tree put them. An install into /usr gets nothing: a
 * distro package already finds its data on Tesseract's default path.
 *
 * GStreamer is never bundled: every Linux package uses the host's or the runtime's, whose
 * own plugin path must stay untouched.
 *
 * Windows ships a flat folder instead, with the language packs in <exe dir>/tessdata.
 * forWindowsBinaryDir() describes it, forThisPlatform() picks one.
 */
namespace Core::BundledPaths {

struct Paths {
    QString tessdataDir;
};

// Pure string work, so it is testable without a bundle: existence is not checked here.
// Every field is empty for a /usr install, which means "there is nothing to point at".
[[nodiscard]] Paths forBinaryDir(const QString &binDir);

// The flat Windows layout: tessdata beside the exe. Also pure string work.
[[nodiscard]] Paths forWindowsBinaryDir(const QString &binDir);

// forWindowsBinaryDir() on Windows, forBinaryDir() everywhere else.
[[nodiscard]] Paths forThisPlatform(const QString &binDir);

// The running app's language pack dir (Contents/Resources/tessdata in a macOS bundle).
[[nodiscard]] QString tessdataDirForThisApp();

} // namespace Core::BundledPaths

#endif // CORE_BUNDLEDPATHS_H
