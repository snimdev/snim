#include <QtTest>
#include <QFile>
#include <QTemporaryDir>

#include <functional>
#include <memory>

#include "editor/video/VideoExporter.h"
#include "Mp4Boxes.h"

#ifdef SNIM_HAVE_FFMPEG_VIDEO_EXPORTER
#include "editor/video/FfmpegVideoExporter.h"
#include "media/ffmpeg/FfmpegEncoder.h"

#include <cmath>
#include <vector>
#endif
#ifdef SNIM_VIDEO_MODULE_PATH
#include "editor/video/LinuxVideoModule.h"
#endif

using namespace Editor::Video;
using namespace TestSupport;

namespace {

constexpr int kExportTimeoutMs = 30000;
// One frame of the 30 fps fixture either way.
constexpr qint64 kFrameToleranceMs = 34;

struct Backend {
    const char *name;
    std::function<std::unique_ptr<VideoExporter>()> make;   // empty: skipped
    const char *skipReason = nullptr;
};

// Every trim backend this build has; the same contract runs against each of them.
QList<Backend> backends()
{
    QList<Backend> list;
#ifdef SNIM_HAVE_FFMPEG_VIDEO_EXPORTER
    list.append({"ffmpeg", [] { return std::unique_ptr<VideoExporter>(new FfmpegVideoExporter); }});
#endif
#ifdef SNIM_VIDEO_MODULE_PATH
    list.append({"gstreamer", [] {
        return std::unique_ptr<VideoExporter>(LinuxVideoModule::create());
    }});
#endif
#ifdef Q_OS_MACOS
    list.append({"avfoundation", {},
                 "The passthrough remux snaps cuts to samples and reports cancel as a failure"});
#endif
    return list;
}

#ifdef SNIM_HAVE_FFMPEG_VIDEO_EXPORTER
// A 3 s clip with a tone: the checked-in fixture has no audio track.
QString makeClipWithAudio(const QString &path, QString *skipReason)
{
    using namespace Media::Ffmpeg;
    FfmpegEncoderSettings settings;
    settings.path = path;
    settings.video.width = 128;
    settings.video.height = 96;
    FfmpegEncoder encoder;
    if (!encoder.open(settings)) {
        *skipReason = encoder.errorString();
        return {};
    }
    std::vector<uchar> pixels(size_t(128) * 96 * 4, 0x80);
    std::vector<float> pcm(size_t(480) * 2);
    for (int frame = 0; frame < 90; ++frame) {
        pixels[size_t(frame) * 4] = 0xff;
        if (!encoder.addVideoFrame(pixels.data(), 128 * 4, qint64(frame) * 1000000 / 30))
            break;
    }
    for (int chunk = 0; chunk < 300; ++chunk) {
        for (int i = 0; i < 480; ++i)
            pcm[size_t(i) * 2] = pcm[size_t(i) * 2 + 1] = 0.2f * std::sin(float(chunk * 480 + i) * 0.05f);
        if (!encoder.addAudio(pcm.data(), 480, qint64(chunk) * 10000))
            break;
    }
    if (!encoder.finish()) {
        *skipReason = encoder.errorString();
        return {};
    }
    return path;
}
#else
QString makeClipWithAudio(const QString &path, QString *skipReason)
{
    Q_UNUSED(path)
    *skipReason = QStringLiteral("No fixture encoder in this build");
    return {};
}
#endif

} // namespace

// The VideoExporter contract, run against every backend built here. Durations come out of
// the written MP4 boxes, so the checks need no decoder of their own.
class tst_VideoExporterContract : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
#ifdef SNIM_HAVE_FFMPEG_VIDEO_EXPORTER
        // CI runners have no GPU encoder: every machine takes the libx264 path.
        qputenv("SNIM_H264_ENCODER", "libx264");
#endif
#ifdef SNIM_VIDEO_MODULE_PATH
        if (!qEnvironmentVariableIsSet("SNIM_VIDEO_MODULE"))
            qputenv("SNIM_VIDEO_MODULE", SNIM_VIDEO_MODULE_PATH);
#endif
        QVERIFY(m_dir.isValid());
        m_clip = QFINDTESTDATA("data/clip.mp4");
        QVERIFY(!m_clip.isEmpty());
    }

    void trimsToTheExactFrame_data() { backendRows(); }
    void trimsToTheExactFrame()
    {
        std::unique_ptr<VideoExporter> exporter = make();
        if (!exporter)
            return;
        const QString output = path("trim.mp4");
        QVERIFY(runTrim(exporter.get(), m_clip, output, 700, 1900));

        const Mp4Info info = readMp4(output);
        QCOMPARE(info.tracks.size(), 1);
        const Mp4Track *video = trackOf(info, "vide");
        QVERIFY(video);
        QVERIFY2(qAbs(video->durationMs - 1200) <= kFrameToleranceMs,
                 qPrintable(QString::number(video->durationMs)));
        QVERIFY2(qAbs(info.durationMs - 1200) <= kFrameToleranceMs,
                 qPrintable(QString::number(info.durationMs)));
    }

    void writesQuickTimeForMov_data() { backendRows(); }
    void writesQuickTimeForMov()
    {
        std::unique_ptr<VideoExporter> exporter = make();
        if (!exporter)
            return;
        const QString output = path("trim.mov");
        QVERIFY(runTrim(exporter.get(), m_clip, output, 500, 2500));

        const Mp4Info info = readMp4(output);
        QCOMPARE(info.brand, QByteArray("qt  "));
        const Mp4Track *video = trackOf(info, "vide");
        QVERIFY(video);
        QVERIFY2(qAbs(video->durationMs - 2000) <= kFrameToleranceMs,
                 qPrintable(QString::number(video->durationMs)));
    }

    void carriesAudioAcross_data() { backendRows(); }
    void carriesAudioAcross()
    {
        std::unique_ptr<VideoExporter> exporter = make();
        if (!exporter)
            return;
        QString skipReason;
        const QString clip = makeClipWithAudio(path("av.mp4"), &skipReason);
        if (clip.isEmpty())
            QSKIP(qPrintable(skipReason));

        const QString output = path("av-trim.mp4");
        QVERIFY(runTrim(exporter.get(), clip, output, 700, 1900));

        const Mp4Info info = readMp4(output);
        QCOMPARE(info.tracks.size(), 2);
        const Mp4Track *video = trackOf(info, "vide");
        const Mp4Track *audio = trackOf(info, "soun");
        QVERIFY(video);
        QVERIFY(audio);
        QVERIFY2(qAbs(video->durationMs - 1200) <= kFrameToleranceMs,
                 qPrintable(QString::number(video->durationMs)));
        // AAC frames are 21.3 ms at 48 kHz, so audio may end a frame either side.
        QVERIFY2(qAbs(audio->durationMs - 1200) <= 50,
                 qPrintable(QString::number(audio->durationMs)));
    }

    void signalsOnlyLater_data() { backendRows(); }
    void signalsOnlyLater()
    {
        std::unique_ptr<VideoExporter> exporter = make();
        if (!exporter)
            return;
        QSignalSpy finished(exporter.get(), &VideoExporter::finished);
        QSignalSpy failed(exporter.get(), &VideoExporter::failed);
        exporter->trim(m_clip, path("later.mp4"), 1000, 1000);
        QCOMPARE(finished.count() + failed.count(), 0);
        QVERIFY(failed.wait(kExportTimeoutMs));
        QCOMPARE(finished.count(), 0);
    }

    void cancelLeavesNothingBehind_data() { backendRows(); }
    void cancelLeavesNothingBehind()
    {
        std::unique_ptr<VideoExporter> exporter = make();
        if (!exporter)
            return;
        const QString output = path("cancelled.mp4");
        QSignalSpy finished(exporter.get(), &VideoExporter::finished);
        QSignalSpy failed(exporter.get(), &VideoExporter::failed);

        exporter->trim(m_clip, output, 700, 1900);
        exporter->cancel();
        QTest::qWait(1000);

        QCOMPARE(finished.count(), 0);
        QCOMPARE(failed.count(), 0);
        QVERIFY(!QFile::exists(output));

        // Still usable afterwards.
        QVERIFY(runTrim(exporter.get(), m_clip, output, 700, 1900));
    }

    void failsOnMissingInput_data() { backendRows(); }
    void failsOnMissingInput()
    {
        std::unique_ptr<VideoExporter> exporter = make();
        if (!exporter)
            return;
        const QString output = path("missing.mp4");
        QVERIFY(!runTrim(exporter.get(), path("nope.mp4"), output, 0, 1000));
        QVERIFY(!QFile::exists(output));
    }

    void failsOnGarbageInput_data() { backendRows(); }
    void failsOnGarbageInput()
    {
        std::unique_ptr<VideoExporter> exporter = make();
        if (!exporter)
            return;
        const QString garbage = path("garbage.mp4");
        QFile file(garbage);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(QByteArray(64 * 1024, '\x5a'));
        file.close();

        const QString output = path("garbage-trim.mp4");
        QVERIFY(!runTrim(exporter.get(), garbage, output, 0, 1000));
        QVERIFY(!QFile::exists(output));
    }

private:
    void backendRows()
    {
        QTest::addColumn<QString>("backend");
        const QList<Backend> list = backends();
        for (const Backend &backend : list)
            QTest::newRow(backend.name) << QString::fromLatin1(backend.name);
        if (list.isEmpty())
            QTest::newRow("none") << QString();
    }

    // The row's exporter, or nullptr after a QSKIP when it cannot run here.
    std::unique_ptr<VideoExporter> make()
    {
        QFETCH(QString, backend);
        for (const Backend &candidate : backends()) {
            if (backend != QLatin1String(candidate.name))
                continue;
            if (!candidate.make) {
                QTest::qSkip(candidate.skipReason, __FILE__, __LINE__);
                return nullptr;
            }
            std::unique_ptr<VideoExporter> exporter = candidate.make();
            if (!exporter) {
                QTest::qFail("The backend could not be created", __FILE__, __LINE__);
                return nullptr;
            }
            if (!exporter->isAvailable()) {
                QTest::qSkip("The backend is unavailable on this host", __FILE__, __LINE__);
                return nullptr;
            }
            return exporter;
        }
        QTest::qSkip("No trim backend in this build", __FILE__, __LINE__);
        return nullptr;
    }

    QString path(const char *name) const
    {
        return m_dir.filePath(QString::fromLatin1(QTest::currentDataTag()) + QLatin1Char('-')
                              + QLatin1String(name));
    }

    // True on finished(), false on failed(); exactly one of them must fire.
    bool runTrim(VideoExporter *exporter, const QString &input, const QString &output,
                 qint64 inMs, qint64 outMs)
    {
        QSignalSpy finished(exporter, &VideoExporter::finished);
        QSignalSpy failed(exporter, &VideoExporter::failed);
        exporter->trim(input, output, inMs, outMs);
        if (!QTest::qWaitFor([&] { return !finished.isEmpty() || !failed.isEmpty(); },
                             kExportTimeoutMs)) {
            qWarning() << "The trim neither finished nor failed";
            return false;
        }
        QTest::qWait(50);
        if (finished.count() + failed.count() != 1) {
            qWarning() << "Expected exactly one terminal signal";
            return false;
        }
        if (!failed.isEmpty()) {
            qInfo() << "Trim failed:" << failed.first().first().toString();
            return false;
        }
        return finished.first().first().toString() == output && QFile::exists(output);
    }

    QTemporaryDir m_dir;
    QString m_clip;
};

QTEST_MAIN(tst_VideoExporterContract)
#include "tst_videoexportercontract.moc"
