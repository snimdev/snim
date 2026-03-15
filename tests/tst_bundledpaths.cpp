#include <QtTest>

#include "core/BundledPaths.h"

#include <QDir>
#include <QTemporaryDir>

using namespace Core;

// Self-location for a relocated install. No GStreamer and no bundle needed: the path
// computation is pure string work, and the environment half is exercised against a
// QTemporaryDir shaped like an install prefix.
class tst_BundledPaths : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void computesPathsRelativeToTheBinary();
    void claimsNothingForASystemInstall();
    void exportsWhatTheBundleActuallyHas();
    void skipsPathsThatDoNotExist();
    void neverOverridesTheEnvironment();

    void computesTheWindowsLayout();
    void exportsOnlyTessdataForTheWindowsLayout();
    void picksTheLayoutForThisPlatform();
};

namespace {

const char *const kVariables[] = {
    "GST_PLUGIN_SYSTEM_PATH_1_0", "GST_PLUGIN_PATH_1_0", "GST_PLUGIN_SCANNER_1_0",
    "GST_PTP_HELPER_1_0", "TESSDATA_PREFIX", "GST_REGISTRY_REUSE_PLUGIN_SCANNER",
};

void clearVariables()
{
    for (const char *name : kVariables)
        qunsetenv(name);
}

} // namespace

// The process environment is global state, so every test starts and ends from empty.
void tst_BundledPaths::init()
{
    clearVariables();
}

void tst_BundledPaths::cleanup()
{
    clearVariables();
}

void tst_BundledPaths::computesPathsRelativeToTheBinary()
{
    const BundledPaths::Paths paths = BundledPaths::forBinaryDir(QStringLiteral("/opt/snim/usr/bin"));
    QCOMPARE(paths.gstPluginDir, QStringLiteral("/opt/snim/usr/lib/gstreamer-1.0"));
    QCOMPARE(paths.gstPluginScanner,
             QStringLiteral("/opt/snim/usr/lib/gstreamer1.0/gstreamer-1.0/gst-plugin-scanner"));
    QCOMPARE(paths.gstPtpHelper,
             QStringLiteral("/opt/snim/usr/lib/gstreamer1.0/gstreamer-1.0/gst-ptp-helper"));
    QCOMPARE(paths.tessdataDir, QStringLiteral("/opt/snim/usr/share/tessdata"));
}

void tst_BundledPaths::claimsNothingForASystemInstall()
{
    // /usr/lib/gstreamer-1.0 is the multilib 32-bit directory on Fedora and friends, so
    // a distro install must keep GStreamer's own defaults.
    for (const QString &binDir : {QStringLiteral("/usr/bin"), QStringLiteral("/bin"),
                                  QStringLiteral("/usr/bin/")}) {
        const BundledPaths::Paths paths = BundledPaths::forBinaryDir(binDir);
        QVERIFY2(paths.gstPluginDir.isEmpty(), qPrintable(binDir));
        QVERIFY2(paths.gstPluginScanner.isEmpty(), qPrintable(binDir));
        QVERIFY2(paths.gstPtpHelper.isEmpty(), qPrintable(binDir));
        QVERIFY2(paths.tessdataDir.isEmpty(), qPrintable(binDir));
        QVERIFY(BundledPaths::applyToEnvironment(paths).isEmpty());
    }
}

void tst_BundledPaths::exportsWhatTheBundleActuallyHas()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const QString prefix = root.path() + QStringLiteral("/usr");
    QVERIFY(QDir().mkpath(prefix + QStringLiteral("/lib/gstreamer-1.0")));
    QVERIFY(QDir().mkpath(prefix + QStringLiteral("/lib/gstreamer1.0/gstreamer-1.0")));
    QVERIFY(QDir().mkpath(prefix + QStringLiteral("/share/tessdata")));
    QFile scanner(prefix + QStringLiteral("/lib/gstreamer1.0/gstreamer-1.0/gst-plugin-scanner"));
    QVERIFY(scanner.open(QIODevice::WriteOnly));
    scanner.close();

    const BundledPaths::Paths paths = BundledPaths::forBinaryDir(prefix + QStringLiteral("/bin"));
    const QStringList exported = BundledPaths::applyToEnvironment(paths);

    QCOMPARE(exported, QStringList({QStringLiteral("GST_PLUGIN_SYSTEM_PATH_1_0"),
                                    QStringLiteral("GST_PLUGIN_PATH_1_0"),
                                    QStringLiteral("GST_PLUGIN_SCANNER_1_0"),
                                    QStringLiteral("TESSDATA_PREFIX"),
                                    QStringLiteral("GST_REGISTRY_REUSE_PLUGIN_SCANNER")}));
    QCOMPARE(qEnvironmentVariable("GST_PLUGIN_SYSTEM_PATH_1_0"), paths.gstPluginDir);
    QCOMPARE(qEnvironmentVariable("GST_PLUGIN_PATH_1_0"), paths.gstPluginDir);
    QCOMPARE(qEnvironmentVariable("GST_PLUGIN_SCANNER_1_0"), paths.gstPluginScanner);
    QCOMPARE(qEnvironmentVariable("TESSDATA_PREFIX"), paths.tessdataDir);
    QCOMPARE(qEnvironmentVariable("GST_REGISTRY_REUSE_PLUGIN_SCANNER"), QStringLiteral("no"));
    // The PTP helper was never staged, so nothing points at a file that is not there.
    QVERIFY(!qEnvironmentVariableIsSet("GST_PTP_HELPER_1_0"));
}

void tst_BundledPaths::skipsPathsThatDoNotExist()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    // A system install: the binary is there, none of the bundled resources are.
    const QString prefix = root.path() + QStringLiteral("/usr");
    QVERIFY(QDir().mkpath(prefix + QStringLiteral("/bin")));

    const QStringList exported = BundledPaths::applyToEnvironment(
        BundledPaths::forBinaryDir(prefix + QStringLiteral("/bin")));

    QVERIFY(exported.isEmpty());
    for (const char *name : kVariables)
        QVERIFY2(!qEnvironmentVariableIsSet(name), name);
}

void tst_BundledPaths::neverOverridesTheEnvironment()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const QString prefix = root.path() + QStringLiteral("/usr");
    QVERIFY(QDir().mkpath(prefix + QStringLiteral("/lib/gstreamer-1.0")));
    QVERIFY(QDir().mkpath(prefix + QStringLiteral("/share/tessdata")));
    qputenv("TESSDATA_PREFIX", "/somewhere/the/user/chose");
    qputenv("GST_REGISTRY_REUSE_PLUGIN_SCANNER", "yes");

    const QStringList exported = BundledPaths::applyToEnvironment(
        BundledPaths::forBinaryDir(prefix + QStringLiteral("/bin")));

    QVERIFY(!exported.contains(QStringLiteral("TESSDATA_PREFIX")));
    QVERIFY(!exported.contains(QStringLiteral("GST_REGISTRY_REUSE_PLUGIN_SCANNER")));
    QCOMPARE(qEnvironmentVariable("TESSDATA_PREFIX"), QStringLiteral("/somewhere/the/user/chose"));
    QCOMPARE(qEnvironmentVariable("GST_REGISTRY_REUSE_PLUGIN_SCANNER"), QStringLiteral("yes"));
    // The variables nobody set still get the bundled plugin directory.
    QVERIFY(exported.contains(QStringLiteral("GST_PLUGIN_SYSTEM_PATH_1_0")));
    QCOMPARE(qEnvironmentVariable("GST_PLUGIN_PATH_1_0"),
             QDir::cleanPath(prefix + QStringLiteral("/lib/gstreamer-1.0")));
}

void tst_BundledPaths::computesTheWindowsLayout()
{
    const BundledPaths::Paths paths =
        BundledPaths::forWindowsBinaryDir(QStringLiteral("C:/Program Files/Snim"));
    QCOMPARE(paths.tessdataDir, QStringLiteral("C:/Program Files/Snim/tessdata"));
    // No GStreamer ships on Windows, so nothing may point GStreamer anywhere.
    QVERIFY(paths.gstPluginDir.isEmpty());
    QVERIFY(paths.gstPluginScanner.isEmpty());
    QVERIFY(paths.gstPtpHelper.isEmpty());
}

void tst_BundledPaths::exportsOnlyTessdataForTheWindowsLayout()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    QVERIFY(QDir().mkpath(root.path() + QStringLiteral("/tessdata")));

    const BundledPaths::Paths paths = BundledPaths::forWindowsBinaryDir(root.path());
    const QStringList exported = BundledPaths::applyToEnvironment(paths);

    QCOMPARE(exported, QStringList({QStringLiteral("TESSDATA_PREFIX")}));
    QCOMPARE(qEnvironmentVariable("TESSDATA_PREFIX"), paths.tessdataDir);
}

void tst_BundledPaths::picksTheLayoutForThisPlatform()
{
    const QString binDir = QStringLiteral("/opt/snim/usr/bin");
    const BundledPaths::Paths paths = BundledPaths::forThisPlatform(binDir);
#ifdef Q_OS_WIN
    QCOMPARE(paths.tessdataDir, BundledPaths::forWindowsBinaryDir(binDir).tessdataDir);
    QVERIFY(paths.gstPluginDir.isEmpty());
#else
    QCOMPARE(paths.tessdataDir, BundledPaths::forBinaryDir(binDir).tessdataDir);
    QCOMPARE(paths.gstPluginDir, BundledPaths::forBinaryDir(binDir).gstPluginDir);
#endif
}

QTEST_MAIN(tst_BundledPaths)
#include "tst_bundledpaths.moc"
