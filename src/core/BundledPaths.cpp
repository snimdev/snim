#include "core/BundledPaths.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QtEnvironmentVariables>

namespace Core::BundledPaths {

namespace {

QString besideBinary(const QString &binDir, const char *suffix)
{
    return QDir::cleanPath(binDir + QLatin1String("/../") + QLatin1String(suffix));
}

// A distro install has nothing to relocate, and on a multilib system /usr/lib/gstreamer-1.0
// is the 32-bit directory, so claiming it would hide every plugin the app can actually load.
bool systemInstall(const QString &binDir)
{
    const QString cleaned = QDir::cleanPath(binDir);
    return cleaned == QLatin1String("/usr/bin") || cleaned == QLatin1String("/bin");
}

bool bundled(const QString &path)
{
    return !path.isEmpty() && QFileInfo::exists(path);
}

void exportIfBundled(const char *name, const QString &path, QStringList *exported)
{
    if (!bundled(path) || qEnvironmentVariableIsSet(name))
        return;
    qputenv(name, QFile::encodeName(path));
    exported->append(QString::fromLatin1(name));
}

} // namespace

Paths forBinaryDir(const QString &binDir)
{
    if (systemInstall(binDir))
        return {};
    return Paths{
        besideBinary(binDir, "lib/gstreamer-1.0"),
        besideBinary(binDir, "lib/gstreamer1.0/gstreamer-1.0/gst-plugin-scanner"),
        besideBinary(binDir, "lib/gstreamer1.0/gstreamer-1.0/gst-ptp-helper"),
        besideBinary(binDir, "share/tessdata"),
    };
}

Paths forWindowsBinaryDir(const QString &binDir)
{
    Paths paths;
    paths.tessdataDir = QDir::cleanPath(binDir + QLatin1String("/tessdata"));
    return paths;
}

Paths forThisPlatform(const QString &binDir)
{
#ifdef Q_OS_WIN
    return forWindowsBinaryDir(binDir);
#else
    return forBinaryDir(binDir);
#endif
}

QStringList applyToEnvironment(const Paths &paths)
{
    QStringList exported;
    exportIfBundled("GST_PLUGIN_SYSTEM_PATH_1_0", paths.gstPluginDir, &exported);
    exportIfBundled("GST_PLUGIN_PATH_1_0", paths.gstPluginDir, &exported);
    exportIfBundled("GST_PLUGIN_SCANNER_1_0", paths.gstPluginScanner, &exported);
    exportIfBundled("GST_PTP_HELPER_1_0", paths.gstPtpHelper, &exported);
    exportIfBundled("TESSDATA_PREFIX", paths.tessdataDir, &exported);

    // Only with bundled plugins: a reused scanner is the host's, from a path that is not ours.
    if (bundled(paths.gstPluginDir) && !qEnvironmentVariableIsSet("GST_REGISTRY_REUSE_PLUGIN_SCANNER")) {
        qputenv("GST_REGISTRY_REUSE_PLUGIN_SCANNER", "no");
        exported.append(QStringLiteral("GST_REGISTRY_REUSE_PLUGIN_SCANNER"));
    }
    return exported;
}

QStringList applyForThisExecutable()
{
    return applyToEnvironment(forThisPlatform(QCoreApplication::applicationDirPath()));
}

} // namespace Core::BundledPaths
