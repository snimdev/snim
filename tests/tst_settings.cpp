#include <QtTest>
#include <QStandardPaths>
#include <QSettings>
#include <QColor>

#include "core/Settings.h"
#include "editor/video/AnimationParams.h"

using namespace Core;

// Validates both the Core::Settings wrapper and the test harness itself
// (offscreen QApplication via QTEST_MAIN, test-mode QSettings isolation).
class tst_Settings : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        QCoreApplication::setOrganizationName("SnimTest");
        QCoreApplication::setApplicationName("tst_settings");
        QStandardPaths::setTestModeEnabled(true);   // redirect QSettings to a throwaway store
        QSettings().clear();                          // start from a clean store
    }

    void imageFormat_default_then_roundtrip()
    {
        QCOMPARE(Settings::imageFormat(), QStringLiteral("png"));  // default
        Settings::setImageFormat("jpg");
        QCOMPARE(Settings::imageFormat(), QStringLiteral("jpg"));
    }

    void screenshotFolder_roundtrip()
    {
        Settings::setScreenshotFolder("/tmp/snim-shots");
        QCOMPARE(Settings::screenshotFolder(), QStringLiteral("/tmp/snim-shots"));
    }

    void editorColors_default_then_roundtrip()
    {
        QCOMPARE(Settings::editorForeground(), QColor(Qt::red));          // default
        QCOMPARE(Settings::editorBackground(), QColor(Qt::transparent));  // default
        Settings::setEditorForeground(QColor("#112233"));
        Settings::setEditorBackground(QColor("#44556677"));
        QCOMPARE(Settings::editorForeground(), QColor("#112233"));
        QCOMPARE(Settings::editorBackground(), QColor("#44556677"));
    }

    void backdrop_roundtrip()
    {
        const QString json = QStringLiteral("[{\"name\":\"X\",\"config\":{}}]");
        Settings::setBackdropPresetsJson(json);
        QCOMPARE(Settings::backdropPresetsJson(), json);

        Settings::setBackdropDefaultName("Indigo");
        QCOMPARE(Settings::backdropDefaultName(), QStringLiteral("Indigo"));
    }

    void recording_defaults_then_roundtrip()
    {
        QCOMPARE(Settings::recordingFormat(), QStringLiteral("mp4"));   // default
        QCOMPARE(Settings::recordingFps(), 30);                        // default
        QVERIFY(Settings::recordingCaptureCursor());                   // default on
        QVERIFY(Settings::recordingFolder().endsWith("Snim"));     // default <Movies>/Snim

        Settings::setRecordingFps(60);
        QCOMPARE(Settings::recordingFps(), 60);
    }

    void recordingInputs_defaults_then_roundtrip()
    {
        QVERIFY(!Settings::cameraEnabled());            // default off
        QVERIFY(!Settings::micEnabled());               // default off
        QVERIFY(Settings::systemAudioEnabled());        // default on
        QVERIFY(Settings::recordingFrameEnabled());     // default on
        QVERIFY(Settings::recordingRetina());           // default native scale
        QVERIFY(Settings::cameraDeviceId().isEmpty());
        QVERIFY(Settings::micDeviceId().isEmpty());

        Settings::setCameraEnabled(true);
        Settings::setCameraDeviceId(QByteArray("cam-1"));
        Settings::setMicEnabled(true);
        Settings::setMicDeviceId(QByteArray("mic-2"));
        Settings::setSystemAudioEnabled(false);
        Settings::setRecordingFrameEnabled(false);
        Settings::setRecordingRetina(false);
        QVERIFY(Settings::cameraEnabled());
        QCOMPARE(Settings::cameraDeviceId(), QByteArray("cam-1"));
        QVERIFY(Settings::micEnabled());
        QCOMPARE(Settings::micDeviceId(), QByteArray("mic-2"));
        QVERIFY(!Settings::systemAudioEnabled());
        QVERIFY(!Settings::recordingFrameEnabled());
        QVERIFY(!Settings::recordingRetina());
    }

    void animation_defaults_then_roundtrip()
    {
        const Editor::Video::AnimationParams def;
        for (const QString format : {QStringLiteral("gif"), QStringLiteral("webp")}) {
            QCOMPARE(Settings::animationFps(format), def.fps);
            QCOMPARE(Settings::animationMaxWidth(format), def.maxWidth);
            QCOMPARE(Settings::animationQuality(format), def.quality);
            QCOMPARE(Settings::animationLossless(format), def.lossless);
            QCOMPARE(Settings::animationLoopCount(format), def.loopCount);
        }
        QVERIFY(!Settings::animationOptionsSkip());

        Settings::setAnimationFps("webp", 24);
        Settings::setAnimationMaxWidth("webp", 0);
        Settings::setAnimationQuality("webp", 90);
        Settings::setAnimationLossless("webp", true);
        Settings::setAnimationLoopCount("webp", 3);
        Settings::setAnimationOptionsSkip(true);
        QCOMPARE(Settings::animationFps("webp"), 24);
        QCOMPARE(Settings::animationMaxWidth("webp"), 0);
        QCOMPARE(Settings::animationQuality("webp"), 90);
        QVERIFY(Settings::animationLossless("webp"));
        QCOMPARE(Settings::animationLoopCount("webp"), 3);
        QVERIFY(Settings::animationOptionsSkip());

        // Each format keeps its own options.
        QCOMPARE(Settings::animationFps("gif"), def.fps);
        QVERIFY(!Settings::animationLossless("gif"));
    }

    void upload_defaults_then_roundtrip()
    {
        QVERIFY(!Settings::uploadEnabled());   // default off
        Settings::setUploadEnabled(true);
        QVERIFY(Settings::uploadEnabled());

        // The secret key must NEVER be a persisted QSettings key.
        const QStringList keys = QSettings().allKeys();
        for (const QString &k : keys)
            QVERIFY(!k.contains("Secret", Qt::CaseInsensitive));
    }

    void hotkey_set_has_remove()
    {
        QVERIFY(!Settings::hasHotkey("CaptureArea"));
        QCOMPARE(Settings::hotkey("CaptureArea", "Ctrl+Print"), QStringLiteral("Ctrl+Print"));

        Settings::setHotkey("CaptureArea", "Meta+F9");
        QVERIFY(Settings::hasHotkey("CaptureArea"));
        QCOMPARE(Settings::hotkey("CaptureArea", "Ctrl+Print"), QStringLiteral("Meta+F9"));

        // A stored "" is still a stored key (explicitly unbound), not an absent one.
        Settings::setHotkey("CaptureArea", "");
        QVERIFY(Settings::hasHotkey("CaptureArea"));
        QVERIFY(Settings::hotkey("CaptureArea", "Ctrl+Print").isEmpty());

        Settings::removeHotkey("CaptureArea");
        QVERIFY(!Settings::hasHotkey("CaptureArea"));
        QCOMPARE(Settings::hotkey("CaptureArea", "Ctrl+Print"), QStringLiteral("Ctrl+Print"));
        Settings::removeHotkey("CaptureArea");   // removing an absent key is harmless
        QVERIFY(!Settings::hasHotkey("CaptureArea"));
    }

    void portableFileIn_needsSnimIni()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QVERIFY(Settings::portableFileIn(dir.path()).isEmpty());

        QFile ini(dir.filePath("snim.ini"));
        QVERIFY(ini.open(QIODevice::WriteOnly));
        ini.close();
        QCOMPARE(Settings::portableFileIn(dir.path()), dir.filePath("snim.ini"));
    }

    void portableFile_keepsThePlatformStoreUntouched()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString ini = dir.filePath("snim.ini");
        Settings::setImageFormat("png");

        Settings::setPortableFile(ini);
        Settings::setImageFormat("webp");
        QCOMPARE(Settings::imageFormat(), QStringLiteral("webp"));
        QCOMPARE(QSettings(ini, QSettings::IniFormat).value("General/ImageFormat").toString(),
                 QStringLiteral("webp"));

        Settings::setPortableFile(QString());
        QCOMPARE(Settings::imageFormat(), QStringLiteral("png"));
    }
};

QTEST_MAIN(tst_Settings)
#include "tst_settings.moc"
