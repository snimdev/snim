#include <QtTest>
#include <QLibrary>
#include <QTemporaryDir>
#include <algorithm>
#include <memory>

#include "recording/RecordingFactory.h"
#include "recording/RecordingStrategy.h"
#include "recording/strategies/LinuxRecorderModule.h"

using namespace Recording;

// The Linux recorder lives in a dlopened module so nothing else links GStreamer. Two
// things have to hold: the module this build produced really loads against the binary
// that loads it, and a module that cannot be loaded degrades to the stub instead of
// taking the app down with it. Never calls the entry point: constructing the backend
// would init GStreamer and probe the portal.
class tst_LinuxRecorderModule : public QObject
{
    Q_OBJECT

private slots:
    void cleanup() { qunsetenv("SNIM_RECORDER_MODULE"); }

    void looksBesideTheBinaryFirst()
    {
        qunsetenv("SNIM_RECORDER_MODULE");
        const QStringList paths = LinuxRecorderModule::candidatePaths(
            QStringLiteral("/opt/snim/usr/bin"));
        QCOMPARE(paths.first(), QStringLiteral("/opt/snim/usr/bin/libsnim-recorder-linux.so"));
        // usr/bin + usr/lib is what /opt/snim and the portable tarball install into.
        QVERIFY(paths.contains(QStringLiteral("/opt/snim/usr/lib/libsnim-recorder-linux.so")));
        // The bare name last, so QLibrary's own search still finds a distro package.
        QCOMPARE(paths.last(), QStringLiteral("snim-recorder-linux"));
    }

    void theOverrideReplacesEveryOtherCandidate()
    {
        qputenv("SNIM_RECORDER_MODULE", "/tmp/custom/libsnim-recorder-linux.so");
        QCOMPARE(LinuxRecorderModule::candidatePaths(QStringLiteral("/opt/snim/usr/bin")),
                 QStringList{QStringLiteral("/tmp/custom/libsnim-recorder-linux.so")});
    }

    void theBuiltModuleResolvesAgainstItsHost()
    {
        QLibrary library(QStringLiteral(SNIM_RECORDER_MODULE_PATH));
        // Every symbol now, so an unresolved one fails here and not mid-recording.
        library.setLoadHints(QLibrary::ResolveAllSymbolsHint);
        QVERIFY2(library.load(), qPrintable(library.errorString()));
        QVERIFY2(library.resolve(LinuxRecorderModule::kEntryPoint),
                 qPrintable(library.errorString()));
        QVERIFY2(library.resolve(Screen::PipeWireFrames::kGrabEntryPoint),
                 qPrintable(library.errorString()));
        QVERIFY2(library.resolve(Screen::PipeWireFrames::kCanGrabEntryPoint),
                 qPrintable(library.errorString()));
        QVERIFY2(library.resolve(LinuxRecorderModule::kMissingEntryPoint),
                 qPrintable(library.errorString()));
        QVERIFY2(library.resolve(LinuxRecorderModule::kCheckEntryPoint),
                 qPrintable(library.errorString()));
    }

    void namesTheModuleWhenItCannotBeLoaded()
    {
        qputenv("SNIM_RECORDER_MODULE", "/nonexistent/libsnim-recorder-linux.so");
        const QStringList missing = LinuxRecorderModule::missingPieces();
        QCOMPARE(missing.size(), 1);
        QVERIFY(missing.first().contains(QStringLiteral("recorder module")));

        const LinuxRecorderModule::ElementCheck check = LinuxRecorderModule::checkElements();
        QVERIFY(check.found.isEmpty());
        QCOMPARE(check.missing, missing);
    }

    void theFrameGrabberFailsCleanlyWithoutTheModule()
    {
        qputenv("SNIM_RECORDER_MODULE", "/nonexistent/libsnim-recorder-linux.so");
        QString missing;
        QVERIFY(!Screen::PipeWireFrames::canGrab(&missing));
        QVERIFY(missing.contains(QStringLiteral("recorder module")));
        QList<QImage> frames;
        QString error;
        QVERIFY(!Screen::PipeWireFrames::grab(-1, {1}, 10, &frames, &error));
        QVERIFY(!error.isEmpty());
    }

    void fallsBackToTheStubWhenTheModuleCannotBeLoaded()
    {
        qputenv("SNIM_RECORDER_MODULE", "/nonexistent/libsnim-recorder-linux.so");
        QVERIFY(!LinuxRecorderModule::create());
        QVERIFY(!RecordingFactory::isStrategyAvailable(RecordingFactory::StrategyType::Linux));

        auto strategy = RecordingFactory::createStrategy(RecordingFactory::StrategyType::Linux);
        QVERIFY(strategy);
        QCOMPARE(strategy->name(), QStringLiteral("Unsupported"));
        QVERIFY(!strategy->isAvailable());
    }

    void autoStillResolvesWithoutTheModule()
    {
        qputenv("SNIM_RECORDER_MODULE", "/nonexistent/libsnim-recorder-linux.so");
        QVERIFY(RecordingFactory::isStrategyAvailable(RecordingFactory::StrategyType::Auto));
        QVERIFY(RecordingFactory::createStrategy());
        // Built with Linux recording support, whether or not it can run here.
        QCOMPARE(RecordingFactory::getDefaultStrategyType(), RecordingFactory::StrategyType::Linux);
    }

    // Last two: once the real module is loaded it stays loaded, and GStreamer keeps the
    // registry it started with, whatever the variables say.
    void aHostWithoutPluginsIsToldWhatToInstall()
    {
        // GStreamer itself, but none of its plugins: an empty registry and no plugin paths.
        QTemporaryDir registry;
        QVERIFY(registry.isValid());
        qputenv("GST_REGISTRY_1_0", registry.filePath(QStringLiteral("registry.bin")).toUtf8());
        qputenv("GST_PLUGIN_SYSTEM_PATH_1_0", "");
        qputenv("GST_PLUGIN_PATH_1_0", "");
        qputenv("SNIM_RECORDER_MODULE", SNIM_RECORDER_MODULE_PATH);

        const QStringList missing = LinuxRecorderModule::missingPieces();
        qInfo() << "Missing without plugins:" << missing;
        const auto names = [&missing](const QString &needle) {
            return std::any_of(missing.cbegin(), missing.cend(), [&needle](const QString &piece) {
                return piece.contains(needle);
            });
        };
        // Named with the packages that ship them, encoder included.
        QVERIFY(names(QStringLiteral("mp4mux")));
        QVERIFY(names(QStringLiteral("gstreamer1.0-plugins-good")));
        QVERIFY(names(QStringLiteral("an H.264 encoder")));

        // Every way in agrees: the module, a strategy of its own and the factory's stub.
        QCOMPARE(LinuxRecorderModule::missingPieces(), missing);
        QVERIFY(!LinuxRecorderModule::isAvailable());
        const std::unique_ptr<RecordingStrategy> strategy(LinuxRecorderModule::create());
        QVERIFY(strategy);
        QVERIFY(!strategy->isAvailable());
        QCOMPARE(RecordingFactory::createStrategy(RecordingFactory::StrategyType::Linux)->name(),
                 QStringLiteral("Unsupported"));
    }

    // Still without plugins: the self-test's check finds nothing and names what is missing.
    void theElementCheckNamesWhatIsMissing()
    {
        const LinuxRecorderModule::ElementCheck check = LinuxRecorderModule::checkElements();
        qInfo() << "Found:" << check.found << "missing:" << check.missing;
        QVERIFY(check.found.isEmpty());
        const auto names = [&check](const QString &needle) {
            return std::any_of(check.missing.cbegin(), check.missing.cend(),
                               [&needle](const QString &piece) { return piece.contains(needle); });
        };
        // Offscreen is no X11 session, so the PipeWire source and the frame grab are checked.
        QVERIFY(names(QStringLiteral("pipewiresrc")));
        QVERIFY(names(QStringLiteral("appsink")));
        QVERIFY(names(QStringLiteral("an H.264 encoder")));
    }
};

QTEST_MAIN(tst_LinuxRecorderModule)
#include "tst_linuxrecordermodule.moc"
