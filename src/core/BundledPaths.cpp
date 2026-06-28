#include "core/BundledPaths.h"

#include <QDir>

namespace Core::BundledPaths {

namespace {

// A distro install has nothing to relocate.
bool systemInstall(const QString &binDir)
{
    const QString cleaned = QDir::cleanPath(binDir);
    return cleaned == QLatin1String("/usr/bin") || cleaned == QLatin1String("/bin");
}

} // namespace

Paths forBinaryDir(const QString &binDir)
{
    if (systemInstall(binDir))
        return {};
    return Paths{QDir::cleanPath(binDir + QLatin1String("/../share/tessdata"))};
}

Paths forWindowsBinaryDir(const QString &binDir)
{
    return Paths{QDir::cleanPath(binDir + QLatin1String("/tessdata"))};
}

Paths forThisPlatform(const QString &binDir)
{
#ifdef Q_OS_WIN
    return forWindowsBinaryDir(binDir);
#else
    return forBinaryDir(binDir);
#endif
}

} // namespace Core::BundledPaths
