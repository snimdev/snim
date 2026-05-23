#ifndef CORE_DYNAMICMODULE_H
#define CORE_DYNAMICMODULE_H

#include <QString>
#include <QStringList>
#include <QtGlobal>

/**
 * An optional backend shipped as its own shared library and dlopened at runtime, so
 * nothing the app links depends on what the module needs (GStreamer, for the Linux
 * ones). resolve() is the lookup half of a Factory Method: each module exports one
 * extern "C" function that builds its product, and a thin wrapper per module casts the
 * pointer and calls it.
 */
namespace Core {

struct DynamicModule {
    const char *baseName;      // file lib<baseName>.so
    const char *envOverride;   // variable that replaces the whole search list when set
    const char *entry;         // the exported factory function

    // Where the module is looked for, in order: beside the binary (a build tree), then the
    // lib dir of an install beside it (usr/bin + usr/lib is what /opt/snim and the
    // portable tarball lay out), then the bare name so QLibrary's own search (rpath,
    // ldconfig) still applies to a distro package. The override variable replaces the
    // whole list when set, a dev and test seam. Pure string work, so it needs no module
    // on disk.
    [[nodiscard]] QStringList candidatePaths(const QString &binDir) const;

    // Loads the module once and returns its entry point, or nullptr on ANY failure: no
    // module on disk, a dependency missing on this host, a symbol that will not resolve.
    [[nodiscard]] QFunctionPointer resolve() const;
};

} // namespace Core

#endif // CORE_DYNAMICMODULE_H
