#include <QtTest>
#include <QDir>
#include <QFileInfo>
#include <QTemporaryDir>

#include <gst/gst.h>

#include "media/gst/GstSupport.h"
#include "record/strategies/LinuxPipeline.h"

using namespace Media::Gst;

Q_DECLARE_METATYPE(Media::Gst::H264Plugins)

namespace {

constexpr int kFrames = 30;

H264Plugins plugins(bool x264, bool va, bool openh264, bool parser)
{
    H264Plugins p;
    p.x264 = x264;
    p.va = va;
    p.openh264 = openh264;
    p.parser = parser;
    return p;
}

// Encodes kFrames BGRx frames (what PipeWire hands over) through `chain` into an MP4,
// then counts what qtdemux reads and the H.264 profile it reports.
int framesThrough(const QString &chain, const QString &path, QByteArray *profile)
{
    const QString encode = QStringLiteral(
        "videotestsrc num-buffers=%1 "
        "! video/x-raw,format=BGRx,width=320,height=240,framerate=30/1 "
        "! videoconvert ! queue ! %2 ! queue ! mp4mux name=mux ! filesink name=sink")
        .arg(kFrames).arg(chain);
    GError *error = nullptr;
    GstPtr<GstElement> pipeline(gst_parse_launch(encode.toUtf8().constData(), &error));
    g_clear_error(&error);
    if (!pipeline)
        return -1;
    GstPtr<GstElement> mux(gst_bin_get_by_name(GST_BIN(pipeline.get()), "mux"));
    GstPtr<GstElement> sink(gst_bin_get_by_name(GST_BIN(pipeline.get()), "sink"));
    g_object_set(sink.get(), "location", path.toUtf8().constData(), nullptr);
    configureRecordingOutput(mux.get(), sink.get());

    const auto runToEos = [](GstElement *element) {
        gst_element_set_state(element, GST_STATE_PLAYING);
        GstPtr<GstBus> bus(gst_element_get_bus(element));
        GstMessage *message = gst_bus_timed_pop_filtered(
            bus.get(), 20 * GST_SECOND, GstMessageType(GST_MESSAGE_EOS | GST_MESSAGE_ERROR));
        const bool eos = message && GST_MESSAGE_TYPE(message) == GST_MESSAGE_EOS;
        if (message)
            gst_message_unref(message);
        gst_element_set_state(element, GST_STATE_NULL);
        return eos;
    };
    if (!runToEos(pipeline.get()))
        return -1;

    GstPtr<GstElement> reader(gst_parse_launch(
        "filesrc name=src ! qtdemux ! video/x-h264 ! fakesink name=out sync=false", &error));
    g_clear_error(&error);
    if (!reader)
        return -1;
    GstPtr<GstElement> src(gst_bin_get_by_name(GST_BIN(reader.get()), "src"));
    GstPtr<GstElement> out(gst_bin_get_by_name(GST_BIN(reader.get()), "out"));
    g_object_set(src.get(), "location", path.toUtf8().constData(), nullptr);
    int frames = 0;
    GstPtr<GstPad> pad(gst_element_get_static_pad(out.get(), "sink"));
    gst_pad_add_probe(pad.get(), GST_PAD_PROBE_TYPE_BUFFER,
                      [](GstPad *, GstPadProbeInfo *, gpointer data) {
                          ++*static_cast<int *>(data);
                          return GST_PAD_PROBE_OK;
                      }, &frames, nullptr);
    gst_pad_add_probe(pad.get(), GST_PAD_PROBE_TYPE_EVENT_DOWNSTREAM,
                      [](GstPad *, GstPadProbeInfo *info, gpointer data) {
                          GstEvent *event = GST_PAD_PROBE_INFO_EVENT(info);
                          if (GST_EVENT_TYPE(event) == GST_EVENT_CAPS) {
                              GstCaps *caps = nullptr;
                              gst_event_parse_caps(event, &caps);
                              *static_cast<QByteArray *>(data) = gst_structure_get_string(
                                  gst_caps_get_structure(caps, 0), "profile");
                          }
                          return GST_PAD_PROBE_OK;
                      }, profile, nullptr);
    return runToEos(reader.get()) ? frames : -1;
}

} // namespace

class tst_GstSupport : public QObject
{
    Q_OBJECT

private slots:
    // First, so it owns the process's one gst_init.
    void initLeavesThePluginPathAlone()
    {
        // A lib/gstreamer-1.0 beside the binary once replaced the host's or runtime's path.
        const QString appDir = QCoreApplication::applicationDirPath();
        const QString libDir = QDir::cleanPath(appDir + QStringLiteral("/../lib"));
        const QString pluginDir = libDir + QStringLiteral("/gstreamer-1.0");
        const bool libExisted = QFileInfo::exists(libDir);
        const bool created = !QFileInfo::exists(pluginDir) && QDir().mkpath(pluginDir);
        const char *const variables[] = {"GST_PLUGIN_SYSTEM_PATH_1_0", "GST_PLUGIN_PATH_1_0",
                                         "GST_REGISTRY_REUSE_PLUGIN_SCANNER"};
        QList<QByteArray> before;
        for (const char *name : variables)
            before << qgetenv(name);

        const bool initialized = ensureInitialized();
        if (created)
            QDir(libExisted ? pluginDir : libDir).removeRecursively();

        QVERIFY(initialized);
        for (qsizetype i = 0; i < before.size(); ++i)
            QCOMPARE(qgetenv(variables[i]), before[i]);
    }

    void choosesAnEncoderTheMuxerCanTake_data()
    {
        QTest::addColumn<H264Plugins>("installed");
        QTest::addColumn<int>("expected");

        const auto none = int(H264Encoder::None);
        const auto x264 = int(H264Encoder::X264);
        const auto va = int(H264Encoder::Va);
        const auto openh264 = int(H264Encoder::OpenH264);
        QTest::newRow("nothing") << plugins(false, false, false, false) << none;
        QTest::newRow("parser only") << plugins(false, false, false, true) << none;
        QTest::newRow("x264 alone") << plugins(true, false, false, false) << x264;
        QTest::newRow("x264 and parser") << plugins(true, false, false, true) << x264;
        QTest::newRow("x264 over va") << plugins(true, true, true, true) << x264;
        QTest::newRow("va with parser") << plugins(false, true, true, true) << va;
        QTest::newRow("va without parser") << plugins(false, true, false, false) << none;
        QTest::newRow("openh264 with parser") << plugins(false, false, true, true) << openh264;
        QTest::newRow("openh264 without parser") << plugins(false, false, true, false) << none;
        QTest::newRow("va and openh264 without parser") << plugins(false, true, true, false)
                                                        << none;
    }

    void choosesAnEncoderTheMuxerCanTake()
    {
        QFETCH(H264Plugins, installed);
        QFETCH(int, expected);
        QCOMPARE(int(chooseH264Encoder(installed)), expected);
    }

    void namesEachEncoderElement()
    {
        QCOMPARE(QByteArray(encoderElement(H264Encoder::X264)), QByteArray("x264enc"));
        QCOMPARE(QByteArray(encoderElement(H264Encoder::Va)), QByteArray("vah264enc"));
        QCOMPARE(QByteArray(encoderElement(H264Encoder::OpenH264)), QByteArray("openh264enc"));
        QVERIFY(!encoderElement(H264Encoder::None));
    }

    void chainEndsInWhatTheMuxerTakes()
    {
        const EncoderTuning live{EncoderTuning::Live, 30};
        QCOMPARE(h264EncoderChain(H264Encoder::X264, true, live),
                 QStringLiteral("capsfilter caps=video/x-raw,format=I420 ! x264enc "
                                "tune=zerolatency speed-preset=veryfast pass=qual "
                                "quantizer=22 vbv-buf-capacity=0 key-int-max=30 ! h264parse"));
        QCOMPARE(h264EncoderChain(H264Encoder::X264, false, {EncoderTuning::Offline, 0}),
                 QStringLiteral("capsfilter caps=video/x-raw,format=I420 ! x264enc "
                                "speed-preset=faster pass=qual quantizer=20 "
                                "! capsfilter caps=video/x-h264,stream-format=avc,alignment=au"));
        QCOMPARE(h264EncoderChain(H264Encoder::Va, true, live),
                 QStringLiteral("vapostproc ! vah264enc ! h264parse"));
        QCOMPARE(h264EncoderChain(H264Encoder::OpenH264, true, live),
                 QStringLiteral("capsfilter caps=video/x-raw,format=I420 ! "
                                "openh264enc complexity=0 ! h264parse"));
        QVERIFY(h264EncoderChain(H264Encoder::None, true, live).isEmpty());
    }

    void missingPiecesNameThePackages()
    {
        const QList<const char *> required{"pipewiresrc", "mp4mux", "valve", "videocrop"};
        const auto allBut = [](QList<QByteArray> absent) {
            return [absent](const char *name) { return !absent.contains(QByteArray(name)); };
        };

        QVERIFY(missingPieces(required, allBut({}), plugins(true, false, false, true)).isEmpty());
        // x264 needs no parser, so nothing is missing.
        QVERIFY(missingPieces(required, allBut({}), plugins(true, false, false, false)).isEmpty());

        QCOMPARE(missingPieces(required, allBut({"pipewiresrc", "mp4mux", "videocrop"}),
                               plugins(false, false, false, false)),
                 QStringList({
                     QStringLiteral("pipewiresrc (gstreamer1.0-pipewire; "
                                    "Fedora: pipewire-gstreamer)"),
                     QStringLiteral("mp4mux, videocrop (gstreamer1.0-plugins-good; "
                                    "Fedora: gstreamer1-plugins-good)"),
                     QStringLiteral("an H.264 encoder (gstreamer1.0-plugins-ugly or "
                                    "gstreamer1.0-plugins-bad; Fedora: "
                                    "gstreamer1-plugin-openh264 and "
                                    "gstreamer1-plugins-bad-free)"),
                 }));

        // The X11 recorder's source comes from plugins-good, not the PipeWire package.
        QCOMPARE(missingPieces({"ximagesrc", "videocrop"}, allBut({"ximagesrc"}),
                               plugins(true, false, false, true)),
                 QStringList({QStringLiteral("ximagesrc (gstreamer1.0-plugins-good; "
                                             "Fedora: gstreamer1-plugins-good)")}));

        // The frame grab's sink ships with plugins-base, not GStreamer's core.
        QCOMPARE(missingPieces({"pipewiresrc", "appsink"}, allBut({"appsink"}),
                               plugins(true, false, false, true)),
                 QStringList({QStringLiteral("appsink (gstreamer1.0-plugins-base; "
                                             "Fedora: gstreamer1-plugins-base)")}));

        // OpenH264 without h264parse: the parser is all that is missing.
        QCOMPARE(missingPieces(required, allBut({"valve"}), plugins(false, false, true, false)),
                 QStringList({
                     QStringLiteral("valve (libgstreamer1.0-0; Fedora: gstreamer1)"),
                     QStringLiteral("h264parse (gstreamer1.0-plugins-bad; "
                                    "Fedora: gstreamer1-plugins-bad-free)"),
                 }));
    }

    void encodesToMp4WithAndWithoutTheParser_data()
    {
        QTest::addColumn<int>("encoder");
        QTest::addColumn<bool>("parser");
        QTest::newRow("x264, parser") << int(H264Encoder::X264) << true;
        QTest::newRow("x264, no parser") << int(H264Encoder::X264) << false;
        QTest::newRow("openh264, parser") << int(H264Encoder::OpenH264) << true;
    }

    void encodesToMp4WithAndWithoutTheParser()
    {
        QFETCH(int, encoder);
        QFETCH(bool, parser);
        QVERIFY(ensureInitialized());
        const char *factory = encoder == int(H264Encoder::X264) ? "x264enc" : "openh264enc";
        for (const char *name : {factory, "videotestsrc", "videoconvert", "mp4mux", "qtdemux",
                                 "filesink", "fakesink"}) {
            if (!hasFactory(name))
                QSKIP(qPrintable(QStringLiteral("Missing %1").arg(QLatin1String(name))));
        }
        if (parser && !hasFactory("h264parse"))
            QSKIP("Missing h264parse");

        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString chain = h264EncoderChain(H264Encoder(encoder), parser,
                                               {EncoderTuning::Live, 30});
        // The trim exporter wraps the chain in a bin of its own.
        GError *error = nullptr;
        GstPtr<GstElement> bin(gst_parse_bin_from_description(chain.toUtf8().constData(), TRUE,
                                                              &error));
        QVERIFY2(bin, qPrintable(errorText(error)));
        g_clear_error(&error);
        QByteArray profile;
        QCOMPARE(framesThrough(chain, dir.filePath(QStringLiteral("out.mp4")), &profile),
                 kFrames);
        // 4:2:0 profiles only; 4:4:4 does not play in browsers.
        QVERIFY2(profile == "high" || profile == "main" || profile == "constrained-baseline"
                     || profile == "baseline",
                 profile.constData());
    }

    void parseFailsOnAnyError()
    {
        QVERIFY(ensureInitialized());
        QString error;
        // gst_parse_launch alone recovers from this by dropping the element.
        GstPtr<GstElement> bad(parseLaunch(
            QStringLiteral("fakesrc name=src no-such-property=1 ! fakesink"), &error));
        QVERIFY(!bad);
        QVERIFY2(error.contains(QStringLiteral("no-such-property")), qPrintable(error));

        GstPtr<GstElement> good(parseLaunch(QStringLiteral("fakesrc ! fakesink"), &error));
        QVERIFY2(good, qPrintable(error));
        QVERIFY(error.isEmpty());
    }

    // Parsing reaches no PipeWire or pulse server. ximagesrc reads the X display to link,
    // so it gets none.
    void parsesTheRecorderPipelines()
    {
        using namespace Record::LinuxPipeline;
        qunsetenv("DISPLAY");
        QVERIFY(ensureInitialized());
        const QString encoder = h264EncoderChain({EncoderTuning::Live, 30});
        const QString aac = aacEncoderChain();
        if (encoder.isEmpty() || aac.isEmpty())
            QSKIP("No H.264 or AAC encoder");
        for (const char *name : {"pulsesrc", "audiomixer", "aacparse", "valve", "mp4mux"}) {
            if (!hasFactory(name))
                QSKIP(qPrintable(QStringLiteral("Missing %1").arg(QLatin1String(name))));
        }

        int parsed = 0;
        for (const QString &source : {portalSource(), x11Source(QRect(0, 0, 64, 64), true, 30),
                                      x11WindowSource(1, true, 30)}) {
            if (!hasFactory(source.section(QLatin1Char(' '), 0, 0).toLatin1().constData()))
                continue;
            const QString description = videoChain(source, 30, encoder, QStringLiteral("mp4mux"))
                                        + audioChain(aac, true, true);
            QString error;
            GstPtr<GstElement> pipeline(parseLaunch(description, &error));
            QVERIFY2(pipeline, qPrintable(error));
            ++parsed;
        }
        if (parsed == 0)
            QSKIP("Neither pipewiresrc nor ximagesrc");
    }

    // Frames 6, 12 or 18 ms apart, as a 165 Hz screen cast sends them, leave the chain
    // exactly 1/fps apart.
    void theRecorderChainKeepsAConstantFrameRate()
    {
        using namespace Record::LinuxPipeline;
        QVERIFY(ensureInitialized());
        for (const char *name : {"videotestsrc", "valve", "videorate", "videocrop", "videoscale",
                                 "videoconvert", "identity", "filesink"}) {
            if (!hasFactory(name))
                QSKIP(qPrintable(QStringLiteral("Missing %1").arg(QLatin1String(name))));
        }

        constexpr int kFps = 60;
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QString error;
        GstPtr<GstElement> pipeline(parseLaunch(
            videoChain(QStringLiteral("videotestsrc name=src num-buffers=330 "
                                      "! video/x-raw,width=64,height=48,framerate=165/1"),
                       kFps, QStringLiteral("identity name=enc"), QStringLiteral("identity")),
            &error));
        QVERIFY2(pipeline, qPrintable(error));
        GstPtr<GstElement> sink(gst_bin_get_by_name(GST_BIN(pipeline.get()), "sink"));
        g_object_set(sink.get(), "location",
                     dir.filePath(QStringLiteral("raw")).toUtf8().constData(), nullptr);

        // Keeps refreshes 0, 1, 3 and 6 of every 7: gaps of 1, 2, 3 and 1 refreshes.
        GstPtr<GstElement> src(gst_bin_get_by_name(GST_BIN(pipeline.get()), "src"));
        GstPtr<GstPad> srcPad(gst_element_get_static_pad(src.get(), "src"));
        gst_pad_add_probe(srcPad.get(), GST_PAD_PROBE_TYPE_BUFFER,
                          [](GstPad *, GstPadProbeInfo *info, gpointer) {
                              const guint64 refresh =
                                  GST_BUFFER_OFFSET(GST_PAD_PROBE_INFO_BUFFER(info)) % 7;
                              return refresh == 0 || refresh == 1 || refresh == 3
                                             || refresh == 6
                                         ? GST_PAD_PROBE_OK
                                         : GST_PAD_PROBE_DROP;
                          }, nullptr, nullptr);
        QList<GstClockTime> stamps;
        GstPtr<GstElement> enc(gst_bin_get_by_name(GST_BIN(pipeline.get()), "enc"));
        GstPtr<GstPad> encPad(gst_element_get_static_pad(enc.get(), "sink"));
        gst_pad_add_probe(encPad.get(), GST_PAD_PROBE_TYPE_BUFFER,
                          [](GstPad *, GstPadProbeInfo *info, gpointer data) {
                              static_cast<QList<GstClockTime> *>(data)->append(
                                  GST_BUFFER_PTS(GST_PAD_PROBE_INFO_BUFFER(info)));
                              return GST_PAD_PROBE_OK;
                          }, &stamps, nullptr);

        gst_element_set_state(pipeline.get(), GST_STATE_PLAYING);
        GstPtr<GstBus> bus(gst_element_get_bus(pipeline.get()));
        GstMessage *message = gst_bus_timed_pop_filtered(
            bus.get(), 20 * GST_SECOND, GstMessageType(GST_MESSAGE_EOS | GST_MESSAGE_ERROR));
        const bool eos = message && GST_MESSAGE_TYPE(message) == GST_MESSAGE_EOS;
        if (message)
            gst_message_unref(message);
        gst_element_set_state(pipeline.get(), GST_STATE_NULL);
        QVERIFY(eos);

        // Two seconds of input.
        QVERIFY2(stamps.size() >= 2 * kFps && stamps.size() <= 2 * kFps + 2,
                 qPrintable(QString::number(stamps.size())));
        const GstClockTime period = GST_SECOND / kFps;
        for (qsizetype i = 1; i < stamps.size(); ++i) {
            const GstClockTime step = stamps[i] - stamps[i - 1];
            QVERIFY2(step + 1 >= period && step <= period + 1,
                     qPrintable(QStringLiteral("frame %1: %2 ns").arg(i).arg(step)));
        }
    }
};

QTEST_GUILESS_MAIN(tst_GstSupport)
#include "tst_gstsupport.moc"
