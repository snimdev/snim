#include <QtTest>
#include <QLibrary>
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
        QVERIFY2(library.resolve(LinuxRecorderModule::kMissingEntryPoint),
                 qPrintable(library.errorString()));
    }

    void namesTheModuleWhenItCannotBeLoaded()
    {
        qputenv("SNIM_RECORDER_MODULE", "/nonexistent/libsnim-recorder-linux.so");
        const QStringList missing = LinuxRecorderModule::missingPieces();
        QCOMPARE(missing.size(), 1);
        QVERIFY(missing.first().contains(QStringLiteral("recorder module")));
    }

    void theFrameGrabberFailsCleanlyWithoutTheModule()
    {
        qputenv("SNIM_RECORDER_MODULE", "/nonexistent/libsnim-recorder-linux.so");
        QVERIFY(!Screen::PipeWireFrames::canGrab());
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

    // Last: once the real module is loaded it stays loaded, whatever the variable says.
    void missingPiecesAgreeWithAvailability()
    {
        qputenv("SNIM_RECORDER_MODULE", SNIM_RECORDER_MODULE_PATH);
        const QStringList missing = LinuxRecorderModule::missingPieces();
        qInfo() << "Missing here:" << missing;
        QCOMPARE(missing.isEmpty(), LinuxRecorderModule::isAvailable());
    }
};

QTEST_MAIN(tst_LinuxRecorderModule)
#include "tst_linuxrecordermodule.moc"
