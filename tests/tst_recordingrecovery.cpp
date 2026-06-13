#include <QtTest>
#include <QElapsedTimer>
#include <QFile>
#include <QProcess>
#include <QTemporaryDir>

#include <gst/gst.h>

#include <cstdio>
#include <memory>

#include "editor/video/LinuxVideoModule.h"
#include "editor/video/VideoExporter.h"
#include "media/gst/GstSupport.h"
#include "Mp4Boxes.h"

using namespace Editor::Video;
using namespace TestSupport;
using Media::Gst::GstPtr;

namespace {

// Set in the child: "fragmented" or "plain", with the output path in kChildOutput.
constexpr char kChildMode[] = "SNIM_RECOVERY_CHILD";
constexpr char kChildOutput[] = "SNIM_RECOVERY_OUTPUT";
// Set in the child to record x264 straight into the muxer, as on a host without h264parse.
constexpr char kChildNoParser[] = "SNIM_RECOVERY_NO_PARSER";
constexpr int kFps = 30;
constexpr int kKillAfterMs = 4000;
constexpr qint64 kAllowedLossMs = 2500;
constexpr int kExportTimeoutMs = 30000;

// Records like LinuxRecordingStrategy (same encoder tuning and muxer setup) until killed.
int runChild()
{
    const bool fragmented = qgetenv(kChildMode) == "fragmented";
    const QByteArray output = qgetenv(kChildOutput);
    if (!Media::Gst::ensureInitialized())
        return 2;

    const Media::Gst::EncoderTuning tuning{Media::Gst::EncoderTuning::Live, kFps};
    const QString encoder = qEnvironmentVariableIsSet(kChildNoParser)
                            ? Media::Gst::h264EncoderChain(Media::Gst::H264Encoder::X264,
                                                           false, tuning)
                            : Media::Gst::h264EncoderChain(tuning);
    const QString description = QStringLiteral(
        "videotestsrc is-live=true ! video/x-raw,width=320,height=240,framerate=%1/1 "
        "! videoconvert ! queue ! %2 ! identity name=encoded ! queue ! mp4mux name=mux "
        "! filesink name=sink").arg(kFps).arg(encoder);
    GError *error = nullptr;
    GstPtr<GstElement> pipeline(gst_parse_launch(description.toUtf8().constData(), &error));
    g_clear_error(&error);
    if (!pipeline)
        return 2;

    GstPtr<GstElement> mux(gst_bin_get_by_name(GST_BIN(pipeline.get()), "mux"));
    GstPtr<GstElement> sink(gst_bin_get_by_name(GST_BIN(pipeline.get()), "sink"));
    GstPtr<GstElement> encoded(gst_bin_get_by_name(GST_BIN(pipeline.get()), "encoded"));
    g_object_set(sink.get(), "location", output.constData(), nullptr);
    if (fragmented)
        Media::Gst::configureRecordingOutput(mux.get(), sink.get());

    // The parent starts its clock when the first encoded frame heads into the muxer.
    GstPtr<GstPad> encodedPad(gst_element_get_static_pad(encoded.get(), "src"));
    gst_pad_add_probe(encodedPad.get(), GST_PAD_PROBE_TYPE_BUFFER,
                      [](GstPad *, GstPadProbeInfo *, gpointer) {
                          std::fputs("started\n", stdout);
                          std::fflush(stdout);
                          return GST_PAD_PROBE_REMOVE;
                      }, nullptr, nullptr);

    if (gst_element_set_state(pipeline.get(), GST_STATE_PLAYING) == GST_STATE_CHANGE_FAILURE)
        return 2;
    GstPtr<GstBus> bus(gst_element_get_bus(pipeline.get()));
    // Only a kill should end this; the timeout keeps an orphan from running forever.
    GstMessage *message = gst_bus_timed_pop_filtered(bus.get(), 60 * GST_SECOND,
                                                     GST_MESSAGE_ERROR);
    if (message)
        gst_message_unref(message);
    gst_element_set_state(pipeline.get(), GST_STATE_NULL);
    return 3;
}

struct Span {
    GstClockTime first = GST_CLOCK_TIME_NONE;
    GstClockTime end = 0;
};

// Seconds of video GStreamer can demux and parse out of the file; 0 when it cannot.
double recoveredSeconds(const QString &path)
{
    GError *error = nullptr;
    GstPtr<GstElement> pipeline(gst_parse_launch(
        "filesrc name=src ! qtdemux ! h264parse ! fakesink name=sink sync=false", &error));
    g_clear_error(&error);
    if (!pipeline)
        return 0.0;
    GstPtr<GstElement> src(gst_bin_get_by_name(GST_BIN(pipeline.get()), "src"));
    GstPtr<GstElement> sink(gst_bin_get_by_name(GST_BIN(pipeline.get()), "sink"));
    g_object_set(src.get(), "location", path.toUtf8().constData(), nullptr);

    Span span;
    GstPtr<GstPad> pad(gst_element_get_static_pad(sink.get(), "sink"));
    gst_pad_add_probe(pad.get(), GST_PAD_PROBE_TYPE_BUFFER,
                      [](GstPad *, GstPadProbeInfo *info, gpointer data) {
                          auto *span = static_cast<Span *>(data);
                          GstBuffer *buffer = GST_PAD_PROBE_INFO_BUFFER(info);
                          const GstClockTime pts = GST_BUFFER_PTS(buffer);
                          if (!GST_CLOCK_TIME_IS_VALID(pts))
                              return GST_PAD_PROBE_OK;
                          const GstClockTime duration = GST_BUFFER_DURATION(buffer);
                          if (!GST_CLOCK_TIME_IS_VALID(span->first) || pts < span->first)
                              span->first = pts;
                          span->end = qMax(span->end, pts + (GST_CLOCK_TIME_IS_VALID(duration)
                                                                 ? duration : 0));
                          return GST_PAD_PROBE_OK;
                      }, &span, nullptr);

    gst_element_set_state(pipeline.get(), GST_STATE_PLAYING);
    GstPtr<GstBus> bus(gst_element_get_bus(pipeline.get()));
    GstMessage *message = gst_bus_timed_pop_filtered(
        bus.get(), 20 * GST_SECOND, GstMessageType(GST_MESSAGE_EOS | GST_MESSAGE_ERROR));
    if (message)
        gst_message_unref(message);
    // Joins the streaming threads, so span is final.
    gst_element_set_state(pipeline.get(), GST_STATE_NULL);

    if (!GST_CLOCK_TIME_IS_VALID(span.first) || span.end <= span.first)
        return 0.0;
    return double(span.end - span.first) / GST_SECOND;
}

} // namespace

// Scenario A1: the app dies mid-recording. A child process records with the production
// muxer setup and is SIGKILLed; the parent measures what a demuxer still reads back.
class tst_RecordingRecovery : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        if (!qEnvironmentVariableIsSet("SNIM_VIDEO_MODULE"))
            qputenv("SNIM_VIDEO_MODULE", SNIM_VIDEO_MODULE_PATH);
        QVERIFY(m_dir.isValid());
        QVERIFY(Media::Gst::ensureInitialized());
        for (const char *name : {"videotestsrc", "videoconvert", "identity", "h264parse",
                                 "mp4mux", "qtdemux", "filesink", "fakesink"}) {
            if (!Media::Gst::hasFactory(name))
                QSKIP(qPrintable(QStringLiteral("Missing %1").arg(QLatin1String(name))));
        }
        if (!Media::Gst::hasUsableH264Encoder())
            QSKIP("No H.264 encoder");
    }

    void keepsFootageAfterAKill()
    {
        double killedAt = 0.0;
        m_fragmented = m_dir.filePath(QStringLiteral("fragmented.mp4"));
        QVERIFY(recordThenKill(QStringLiteral("fragmented"), m_fragmented, &killedAt));
        const double fragmented = recoveredSeconds(m_fragmented);

        double plainKilledAt = 0.0;
        const QString plainPath = m_dir.filePath(QStringLiteral("plain.mp4"));
        QVERIFY(recordThenKill(QStringLiteral("plain"), plainPath, &plainKilledAt));
        const double plain = recoveredSeconds(plainPath);

        qInfo("A1 recovered: fragmented=%.1fs plain=%.1fs of %.1fs", fragmented, plain,
              killedAt);
        QVERIFY2(fragmented >= killedAt - kAllowedLossMs / 1000.0,
                 qPrintable(QString::number(fragmented)));
        QCOMPARE(plain, 0.0);
    }

    void keepsFootageWithoutAParser()
    {
        if (!Media::Gst::hasFactory("x264enc"))
            QSKIP("Only x264enc can feed the muxer without h264parse");
        double killedAt = 0.0;
        const QString path = m_dir.filePath(QStringLiteral("noparser.mp4"));
        QVERIFY(recordThenKill(QStringLiteral("fragmented"), path, &killedAt, true));
        const double recovered = recoveredSeconds(path);
        qInfo("A1 recovered without h264parse: %.1fs of %.1fs", recovered, killedAt);
        QVERIFY2(recovered >= killedAt - kAllowedLossMs / 1000.0,
                 qPrintable(QString::number(recovered)));
    }

    void trimsARecoveredRecording()
    {
        if (m_fragmented.isEmpty() || !QFile::exists(m_fragmented))
            QSKIP("No recovered recording");
        std::unique_ptr<VideoExporter> exporter(LinuxVideoModule::create());
        QVERIFY2(exporter, "The video module did not load");
        if (!exporter->isAvailable())
            QSKIP("GStreamer lacks the plugins trimming needs");

        const QString output = m_dir.filePath(QStringLiteral("trimmed.mp4"));
        QSignalSpy finished(exporter.get(), &VideoExporter::finished);
        QSignalSpy failed(exporter.get(), &VideoExporter::failed);
        exporter->trim(m_fragmented, output, 500, 1500);
        QVERIFY(QTest::qWaitFor([&] { return !finished.isEmpty() || !failed.isEmpty(); },
                                kExportTimeoutMs));
        QVERIFY2(failed.isEmpty(), qPrintable(failed.isEmpty()
                                                  ? QString()
                                                  : failed.first().first().toString()));

        const Mp4Track *video = trackOf(readMp4(output), "vide");
        QVERIFY(video);
        // One frame either way.
        QVERIFY2(qAbs(video->durationMs - 1000) <= 34, qPrintable(QString::number(video->durationMs)));
    }

private:
    // Runs this binary as the recording child and SIGKILLs it kKillAfterMs after it started.
    static bool recordThenKill(const QString &mode, const QString &path, double *killedAt,
                               bool noParser = false)
    {
        QProcess child;
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        env.insert(QLatin1String(kChildMode), mode);
        env.insert(QLatin1String(kChildOutput), path);
        if (noParser)
            env.insert(QLatin1String(kChildNoParser), QStringLiteral("1"));
        child.setProcessEnvironment(env);
        child.setProcessChannelMode(QProcess::ForwardedErrorChannel);
        child.start(QCoreApplication::applicationFilePath(), {});
        if (!child.waitForStarted(10000))
            return false;

        QByteArray out;
        while (!out.contains("started")) {
            if (!child.waitForReadyRead(20000)) {
                qWarning() << "The recording child never started" << mode;
                child.kill();
                child.waitForFinished();
                return false;
            }
            out += child.readAll();
        }
        QElapsedTimer sinceStart;
        sinceStart.start();
        QTest::qWait(kKillAfterMs);
        child.kill();
        *killedAt = sinceStart.elapsed() / 1000.0;
        child.waitForFinished(10000);
        return child.exitStatus() == QProcess::CrashExit && QFile::exists(path);
    }

    QTemporaryDir m_dir;
    QString m_fragmented;
};

int main(int argc, char **argv)
{
    // First in the child too: the bundled plugin lookup resolves against the binary's dir.
    QCoreApplication app(argc, argv);
    if (qEnvironmentVariableIsSet(kChildMode))
        return runChild();
    tst_RecordingRecovery test;
    QTEST_SET_MAIN_SOURCE_PATH
    return QTest::qExec(&test, argc, argv);
}

#include "tst_recordingrecovery.moc"
