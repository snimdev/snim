#ifndef CORE_BUNDLEDPATHS_H
#define CORE_BUNDLEDPATHS_H

#include <QString>
#include <QStringList>

/**
 * Self-location for a relocatable Linux install (the packaged /opt/snim tree, the
 * portable tarball, any prefix the user moved). Nothing but the binary's own location is known at
 * runtime, so every bundled resource is addressed relative to the directory holding it,
 * exactly where CMake's install() and the bundled tree put them:
 *
 *   ../lib/gstreamer-1.0                                  GStreamer plugins
 *   ../lib/gstreamer1.0/gstreamer-1.0/gst-plugin-scanner  its out-of-process scanner
 *   ../lib/gstreamer1.0/gstreamer-1.0/gst-ptp-helper      its PTP helper
 *   ../share/tessdata                                     Tesseract language packs
 *
 * There is no launcher script (KWin authorizes ScreenShot2 only for the real binary a
 * desktop entry's Exec names), so the binary has to configure its own environment. Rules,
 * in order: an install into /usr gets nothing at all (a distro package is already on every
 * default search path, and on a multilib system /usr/lib/gstreamer-1.0 holds the 32-bit
 * plugins), a variable the environment already sets always wins (a packager, the user), a
 * path that does not exist is skipped, and only what is left gets exported. Harmless off
 * Linux, where none of those paths exist.
 *
 * Windows ships a flat folder instead: no GStreamer, and the language packs in
 * <exe dir>/tessdata. forWindowsBinaryDir() describes it, forThisPlatform() picks one.
 */
namespace Core::BundledPaths {

struct Paths {
    QString gstPluginDir;
    QString gstPluginScanner;
    QString gstPtpHelper;
    QString tessdataDir;
};

// Pure string work, so it is testable without a bundle: existence is not checked here.
// Every field is empty for a /usr install, which means "there is nothing to point at".
[[nodiscard]] Paths forBinaryDir(const QString &binDir);

// The flat Windows layout: only tessdataDir, beside the exe. Also pure string work.
[[nodiscard]] Paths forWindowsBinaryDir(const QString &binDir);

// forWindowsBinaryDir() on Windows, forBinaryDir() everywhere else.
[[nodiscard]] Paths forThisPlatform(const QString &binDir);

// Exports the paths that exist onto their variables, skipping every variable that is
// already set. Returns the names it exported, for logging and tests.
QStringList applyToEnvironment(const Paths &paths);

// Same for the running executable's directory. Needs a live QCoreApplication.
QStringList applyForThisExecutable();

} // namespace Core::BundledPaths

#endif // CORE_BUNDLEDPATHS_H
