#include <QtTest>
#include <QFile>
#include <QLibrary>
#include <QTemporaryDir>

#include <gst/gst.h>

#include <memory>

#include "editor/video/LinuxVideoModule.h"
#include "editor/video/VideoExporter.h"
#include "Mp4Boxes.h"

using namespace Editor::Video;
using namespace TestSupport;

namespace {

constexpr int kExportTimeoutMs = 30000;
// One frame of the 30 fps fixture either way.
constexpr qint64 kFrameToleranceMs = 34;

bool hasFactory(const char *name)
{
    GstElementFactory *factory = gst_element_factory_find(name);
    if (factory)
        gst_object_unref(factory);
    return factory != nullptr;
}

// A 3 s clip with a tone, made on the spot: the checked-in fixture has no audio track.
QString makeClipWithAudio(const QString &path, QString *skipReason)
{
    gst_init(nullptr, nullptr);
    const char *aac = nullptr;
    for (const char *name : {"fdkaacenc", "avenc_aac", "voaacenc"}) {
        if (hasFactory(name)) {
            aac = name;
            break;
        }
    }
    for (const char *name : {"videotestsrc", "audiotestsrc", "x264enc", "h264parse",
                             "aacparse", "mp4mux", "filesink"}) {
        if (!hasFactory(name)) {
            *skipReason = QStringLiteral("Missing %1").arg(QLatin1String(name));
            return {};
        }
    }
    if (!aac) {
        *skipReason = QStringLiteral("No AAC encoder");
        return {};
    }

    const QString description = QStringLiteral(
        "videotestsrc num-buffers=90 ! video/x-raw,width=128,height=96,framerate=30/1 "
        "! x264enc tune=zerolatency key-int-max=15 ! h264parse ! queue ! mp4mux name=mux "
        "! filesink name=sink "
        "audiotestsrc num-buffers=141 ! audio/x-raw,rate=48000,channels=2 ! audioconvert "
        "! %1 ! aacparse ! queue ! mux.").arg(QLatin1String(aac));
    GError *error = nullptr;
    GstElement *pipeline = gst_parse_launch(description.toUtf8().constData(), &error);
    if (!pipeline) {
        *skipReason = QStringLiteral("Fixture pipeline failed: %1")
                          .arg(error ? QString::fromUtf8(error->message) : QString());
        g_clear_error(&error);
        return {};
    }
    g_clear_error(&error);

    GstElement *sink = gst_bin_get_by_name(GST_BIN(pipeline), "sink");
    g_object_set(sink, "location", path.toUtf8().constData(), nullptr);
    gst_object_unref(sink);

    gst_element_set_state(pipeline, GST_STATE_PLAYING);
    GstBus *bus = gst_element_get_bus(pipeline);
    GstMessage *message = gst_bus_timed_pop_filtered(
        bus, 20 * GST_SECOND, GstMessageType(GST_MESSAGE_EOS | GST_MESSAGE_ERROR));
    const bool ok = message && GST_MESSAGE_TYPE(message) == GST_MESSAGE_EOS;
    if (message)
        gst_message_unref(message);
    gst_object_unref(bus);
    gst_element_set_state(pipeline, GST_STATE_NULL);
    gst_object_unref(pipeline);

    if (!ok) {
        *skipReason = QStringLiteral("Fixture pipeline did not finish");
        return {};
    }
    return path;
}

} // namespace

// The GStreamer trim exporter, loaded from the module this build produced. The shared
// contract runs in tst_videoexportercontract; this adds the GStreamer-made audio fixture.
class tst_GstVideoExporter : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        if (!qEnvironmentVariableIsSet("SNIM_VIDEO_MODULE"))
            qputenv("SNIM_VIDEO_MODULE", SNIM_VIDEO_MODULE_PATH);
        QVERIFY(m_dir.isValid());
    }

    void theBuiltModuleResolvesAgainstItsHost()
    {
        QLibrary library(QStringLiteral(SNIM_VIDEO_MODULE_PATH));
        // Every symbol now, so an unresolved one fails here and not mid-trim.
        library.setLoadHints(QLibrary::ResolveAllSymbolsHint);
        QVERIFY2(library.load(), qPrintable(library.errorString()));
        QVERIFY2(library.resolve(LinuxVideoModule::kEntryPoint),
                 qPrintable(library.errorString()));
    }

    void carriesAudioAcross()
    {
        std::unique_ptr<VideoExporter> exporter(LinuxVideoModule::create());
        QVERIFY2(exporter, "The video module did not load");
        if (!exporter->isAvailable())
            QSKIP("GStreamer lacks the plugins trimming needs");
        QString skipReason;
        const QString clip = makeClipWithAudio(m_dir.filePath(QStringLiteral("av.mp4")),
                                               &skipReason);
        if (clip.isEmpty())
            QSKIP(qPrintable(skipReason));

        const QString output = m_dir.filePath(QStringLiteral("av-trim.mp4"));
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

private:
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
};

QTEST_MAIN(tst_GstVideoExporter)
#include "tst_gstvideoexporter.moc"
