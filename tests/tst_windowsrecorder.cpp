#include <QtTest>
#include <QGuiApplication>
#include <QScreen>
#include <QTemporaryDir>

#include "record/RecordTarget.h"
#include "record/strategies/windows/WindowsRecordingStrategy.h"
#include "Mp4Boxes.h"

#include <windows.h>
#include <mmdeviceapi.h>
#include <wrl/client.h>

using namespace Record;
using namespace TestSupport;

namespace {

// Service sessions (CI runners, scheduled tasks) have no desktop to capture.
bool interactiveDesktop()
{
    DWORD session = 0;
    if (!ProcessIdToSessionId(GetCurrentProcessId(), &session) || session == 0)
        return false;
    HDESK desktop = OpenInputDesktop(0, FALSE, DESKTOP_READOBJECTS);
    if (!desktop)
        return false;
    CloseDesktop(desktop);
    return true;
}

// Whether a default playback device exists for loopback audio.
bool hasPlaybackDevice()
{
    const HRESULT com = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    bool found = false;
    {
        Microsoft::WRL::ComPtr<IMMDeviceEnumerator> enumerator;
        Microsoft::WRL::ComPtr<IMMDevice> device;
        found = SUCCEEDED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                                           IID_PPV_ARGS(&enumerator)))
                && SUCCEEDED(enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &device));
    }
    if (SUCCEEDED(com))
        CoUninitialize();
    return found;
}

} // namespace

// Records two seconds of the real primary monitor through Graphics Capture and
// WASAPI. libx264 is forced so every machine takes the same encoder path.
class tst_WindowsRecorder : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        qputenv("SNIM_H264_ENCODER", "libx264");
    }

    void recordsThePrimaryMonitor()
    {
        if (QGuiApplication::platformName() != QLatin1String("windows"))
            QSKIP("Needs the windows platform plugin, not offscreen");
        if (!interactiveDesktop())
            QSKIP("No interactive desktop to capture");
        WindowsRecordingStrategy strategy;
        if (!strategy.isAvailable())
            QSKIP("Windows Graphics Capture is unavailable");

        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath(QStringLiteral("smoke.mp4"));

        RecordTarget target;
        target.regionVirtual = QGuiApplication::primaryScreen()->geometry();
        target.fps = 30;
        target.captureSystemAudio = hasPlaybackDevice();

        QSignalSpy started(&strategy, &RecordingStrategy::started);
        QSignalSpy finished(&strategy, &RecordingStrategy::finished);
        QSignalSpy failed(&strategy, &RecordingStrategy::failed);
        strategy.start(target, path);

        QTRY_VERIFY_WITH_TIMEOUT(!started.isEmpty() || !failed.isEmpty(), 10000);
        QVERIFY2(failed.isEmpty(), qPrintable(failed.value(0).value(0).toString()));
        QTest::qWait(2000);
        strategy.stop();

        QTRY_VERIFY_WITH_TIMEOUT(!finished.isEmpty() || !failed.isEmpty(), 20000);
        QVERIFY2(failed.isEmpty(), qPrintable(failed.value(0).value(0).toString()));
        QCOMPARE(finished.first().first().toString(), path);

        const Mp4Info info = readMp4(path);
        QCOMPARE(info.brand, QByteArray("isom"));
        const Mp4Track *video = trackOf(info, "vide");
        QVERIFY(video);
        QVERIFY2(video->durationMs >= 1800 && video->durationMs <= 3500,
                 qPrintable(QString::number(video->durationMs)));

        const Mp4Track *audio = trackOf(info, "soun");
        if (!target.captureSystemAudio) {
            QVERIFY(!audio);
            return;
        }
        QVERIFY(audio);
        QVERIFY2(qAbs(audio->durationMs - video->durationMs) <= 300,
                 qPrintable(QStringLiteral("%1 vs %2").arg(audio->durationMs).arg(video->durationMs)));
    }
};

QTEST_MAIN(tst_WindowsRecorder)
#include "tst_windowsrecorder.moc"
