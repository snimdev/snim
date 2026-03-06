#include "editor/video/GstVideoExporter.h"

#include "editor/video/LinuxVideoModule.h"
#include "media/gst/GstSupport.h"

#include <gst/gst.h>

#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QMutex>
#include <QTimer>

#include <atomic>
#include <initializer_list>
#include <vector>

namespace Editor::Video {

namespace {

constexpr int kProgressIntervalMs = 250;
constexpr int kWatchdogMs = 10000;
// Covers the encoder's lookahead, so the audio queued ahead of the first H.264 frame
// never stalls the muxer.
constexpr guint64 kTailQueueNs = 5 * GST_SECOND;

} // namespace

// One decodebin stream feeding the muxer.
struct Branch {
    GstVideoExporter::Session *session = nullptr;
    GstElement *tail = nullptr;   // owned by the pipeline
    bool video = false;
    bool ready = false;           // touched only by this stream's probe
    std::atomic_bool seeked{false};
};

struct GstVideoExporter::Session {
    GstVideoExporter *exporter = nullptr;
    quint64 generation = 0;
    QString output;
    QString videoEncoder;
    QString audioEncoder;         // empty: audio goes to a fakesink

    GstElement *pipeline = nullptr;   // owned
    GstElement *decodebin = nullptr;  // owned by the pipeline
    GstElement *mux = nullptr;        // owned by the pipeline

    QMutex mutex;                     // guards everything below
    std::vector<std::unique_ptr<Branch>> branches;
    GstPad *seekPad = nullptr;        // owned ref, the video stream's decodebin pad
    bool padsComplete = false;

    std::atomic<qint64> videoPtsMs{-1};

    ~Session()
    {
        if (seekPad)
            gst_object_unref(seekPad);
        if (pipeline)
            gst_object_unref(pipeline);
    }
};

namespace {

using Session = GstVideoExporter::Session;

void postError(Session *session, const QString &error)
{
    GstVideoExporter *exporter = session->exporter;
    const quint64 generation = session->generation;
    QMetaObject::invokeMethod(exporter, [exporter, generation, error] {
        exporter->reportError(generation, error);
    }, Qt::QueuedConnection);
}

QString errorMessage(const GError *error, const QString &output)
{
    if (error && error->domain == GST_RESOURCE_ERROR) {
        if (error->code == GST_RESOURCE_ERROR_NO_SPACE_LEFT)
            return GstVideoExporter::tr("Not enough disk space to save the trimmed recording.");
        if (error->code == GST_RESOURCE_ERROR_OPEN_WRITE)
            return GstVideoExporter::tr("Cannot write %1").arg(output);
    }
    if (error && ((error->domain == GST_CORE_ERROR && error->code == GST_CORE_ERROR_MISSING_PLUGIN)
                  || (error->domain == GST_STREAM_ERROR
                      && error->code == GST_STREAM_ERROR_CODEC_NOT_FOUND))) {
        return GstVideoExporter::tr(
            "Missing an H.264 decoder (install gstreamer1.0-libav or openh264)");
    }
    const QString text = Media::Gst::errorText(error);
    return text.isEmpty() ? GstVideoExporter::tr("The trim export failed.") : text;
}

GstElement *makeElement(const char *factory)
{
    GstElement *element = gst_element_factory_make(factory, nullptr);
    if (!element)
        qWarning() << "GStreamer element missing:" << factory;
    return element;
}

GstElement *makeBin(const QString &description)
{
    GError *error = nullptr;
    GstElement *bin = gst_parse_bin_from_description(description.toUtf8().constData(), TRUE,
                                                     &error);
    if (!bin)
        qWarning() << "GStreamer bin failed:" << description << Media::Gst::errorText(error);
    g_clear_error(&error);
    return bin;
}

// Adds, links and starts a chain of freshly made elements. Unrefs them all on failure.
bool addChain(GstElement *pipeline, std::initializer_list<GstElement *> chain)
{
    for (GstElement *element : chain) {
        if (!element) {
            for (GstElement *other : chain) {
                if (other)
                    gst_object_unref(gst_object_ref_sink(other));
            }
            return false;
        }
    }
    GstElement *previous = nullptr;
    for (GstElement *element : chain) {
        gst_bin_add(GST_BIN(pipeline), element);
        if (previous && !gst_element_link(previous, element))
            return false;
        previous = element;
    }
    for (GstElement *element : chain)
        gst_element_sync_state_with_parent(element);
    return true;
}

void setTailLimits(GstElement *queue)
{
    g_object_set(queue, "max-size-buffers", 0u, "max-size-bytes", 0u,
                 "max-size-time", kTailQueueNs, nullptr);
}

void markReady(Branch *branch)
{
    if (branch->ready)
        return;
    branch->ready = true;
    GstVideoExporter *exporter = branch->session->exporter;
    const quint64 generation = branch->session->generation;
    QMetaObject::invokeMethod(exporter, [exporter, generation] {
        exporter->reportBranchReady(generation);
    }, Qt::QueuedConnection);
}

// Holds the stream's first buffer until the seek flushes it, so nothing from before the
// range ever reaches the muxer.
GstPadProbeReturn holdUntilSeek(GstPad *pad, GstPadProbeInfo *info, gpointer data)
{
    Q_UNUSED(pad)

    auto *branch = static_cast<Branch *>(data);
    if (GST_PAD_PROBE_INFO_TYPE(info) & GST_PAD_PROBE_TYPE_EVENT_DOWNSTREAM) {
        if (GST_EVENT_TYPE(GST_PAD_PROBE_INFO_EVENT(info)) == GST_EVENT_EOS)
            markReady(branch);
        return GST_PAD_PROBE_PASS;
    }
    if (branch->seeked.load())
        return GST_PAD_PROBE_REMOVE;

    markReady(branch);
    return GST_PAD_PROBE_OK;
}

// Blocking probes never see flush events, so a second probe watches for the seek's.
GstPadProbeReturn watchForFlush(GstPad *pad, GstPadProbeInfo *info, gpointer data)
{
    Q_UNUSED(pad)

    if (GST_EVENT_TYPE(GST_PAD_PROBE_INFO_EVENT(info)) != GST_EVENT_FLUSH_STOP)
        return GST_PAD_PROBE_OK;
    static_cast<Branch *>(data)->seeked.store(true);
    return GST_PAD_PROBE_REMOVE;
}

GstPadProbeReturn recordVideoPts(GstPad *pad, GstPadProbeInfo *info, gpointer data)
{
    Q_UNUSED(pad)

    GstBuffer *buffer = GST_PAD_PROBE_INFO_BUFFER(info);
    if (buffer && GST_BUFFER_PTS_IS_VALID(buffer)) {
        static_cast<Session *>(data)->videoPtsMs.store(
            qint64(GST_BUFFER_PTS(buffer) / GST_MSECOND));
    }
    return GST_PAD_PROBE_OK;
}

void addFakesink(Session *session, GstPad *pad)
{
    GstElement *sink = makeElement("fakesink");
    if (!sink)
        return;
    g_object_set(sink, "async", FALSE, nullptr);
    gst_bin_add(GST_BIN(session->pipeline), sink);
    gst_element_sync_state_with_parent(sink);
    GstPad *sinkPad = gst_element_get_static_pad(sink, "sink");
    gst_pad_link(pad, sinkPad);
    gst_object_unref(sinkPad);
}

void onPadAdded(GstElement *decodebin, GstPad *pad, gpointer data)
{
    Q_UNUSED(decodebin)

    auto *session = static_cast<Session *>(data);

    GstCaps *caps = gst_pad_get_current_caps(pad);
    if (!caps)
        caps = gst_pad_query_caps(pad, nullptr);
    const GstStructure *structure = caps && !gst_caps_is_empty(caps)
                                    ? gst_caps_get_structure(caps, 0) : nullptr;
    const QByteArray media = structure ? QByteArray(gst_structure_get_name(structure))
                                       : QByteArray();
    if (caps)
        gst_caps_unref(caps);

    const QMutexLocker locker(&session->mutex);

    bool haveVideo = false;
    bool haveAudio = false;
    for (const auto &branch : session->branches)
        (branch->video ? haveVideo : haveAudio) = true;

    const bool video = media == "video/x-raw" && !haveVideo;
    const bool audio = media == "audio/x-raw" && !haveAudio;
    if (session->padsComplete || (!video && !audio)) {
        qWarning() << "Trim: dropping a stream it does not carry over:" << media;
        addFakesink(session, pad);
        return;
    }
    if (audio && session->audioEncoder.isEmpty()) {
        qWarning() << "Trim: no AAC encoder or aacparse installed; dropping the audio track";
        addFakesink(session, pad);
        return;
    }

    GstElement *head = makeElement("queue");
    GstElement *tail = makeElement("queue");
    bool built = false;
    GstElement *progressAt = nullptr;
    if (video) {
        GstElement *convert = makeElement("videoconvert");
        progressAt = convert;
        built = addChain(session->pipeline, {head, convert, makeBin(session->videoEncoder),
                                             makeElement("h264parse"), tail});
    } else {
        built = addChain(session->pipeline, {head, makeElement("audioconvert"),
                                             makeElement("audioresample"),
                                             makeBin(session->audioEncoder),
                                             makeElement("aacparse"), tail});
    }
    if (!built) {
        postError(session, GstVideoExporter::tr("The trim pipeline could not be built."));
        return;
    }
    setTailLimits(tail);

    // Owned by the session from here on, since the probe below keeps a pointer to it.
    auto *branch = session->branches.emplace_back(std::make_unique<Branch>()).get();
    branch->session = session;
    branch->tail = tail;
    branch->video = video;

    // Before the link: decodebin releases its pads only once pad-added returns.
    gst_pad_add_probe(pad,
                      GstPadProbeType(GST_PAD_PROBE_TYPE_BLOCK | GST_PAD_PROBE_TYPE_BUFFER
                                      | GST_PAD_PROBE_TYPE_BUFFER_LIST
                                      | GST_PAD_PROBE_TYPE_EVENT_DOWNSTREAM),
                      holdUntilSeek, branch, nullptr);
    gst_pad_add_probe(pad, GST_PAD_PROBE_TYPE_EVENT_FLUSH, watchForFlush, branch, nullptr);

    if (video) {
        session->seekPad = GST_PAD(gst_object_ref(pad));
        if (GstPad *src = gst_element_get_static_pad(progressAt, "src")) {
            gst_pad_add_probe(src, GST_PAD_PROBE_TYPE_BUFFER, recordVideoPts, session, nullptr);
            gst_object_unref(src);
        }
    }

    GstPad *headSink = gst_element_get_static_pad(head, "sink");
    const GstPadLinkReturn linked = gst_pad_link(pad, headSink);
    gst_object_unref(headSink);
    if (linked != GST_PAD_LINK_OK)
        postError(session, GstVideoExporter::tr("The trim pipeline could not be built."));
}

GstPad *requestMuxPad(GstElement *mux, const char *name)
{
#if GST_CHECK_VERSION(1, 20, 0)
    return gst_element_request_pad_simple(mux, name);
#else
    return gst_element_get_request_pad(mux, name);
#endif
}

// Only now does the muxer learn which tracks it gets, so it never waits on an audio pad
// that is not coming.
void onNoMorePads(GstElement *decodebin, gpointer data)
{
    Q_UNUSED(decodebin)

    auto *session = static_cast<Session *>(data);
    int branches = 0;
    bool hasVideo = false;
    bool linked = true;
    {
        const QMutexLocker locker(&session->mutex);
        session->padsComplete = true;
        for (const auto &branch : session->branches) {
            GstPad *muxPad = requestMuxPad(session->mux, branch->video ? "video_%u" : "audio_%u");
            GstPad *src = gst_element_get_static_pad(branch->tail, "src");
            linked = muxPad && src && gst_pad_link(src, muxPad) == GST_PAD_LINK_OK;
            if (muxPad)
                gst_object_unref(muxPad);
            if (src)
                gst_object_unref(src);
            if (!linked)
                break;
            ++branches;
            hasVideo = hasVideo || branch->video;
        }
    }

    if (!linked) {
        postError(session, GstVideoExporter::tr("The trim pipeline could not be built."));
        return;
    }
    GstVideoExporter *exporter = session->exporter;
    const quint64 generation = session->generation;
    QMetaObject::invokeMethod(exporter, [exporter, generation, branches, hasVideo] {
        exporter->reportPadsComplete(generation, branches, hasVideo);
    }, Qt::QueuedConnection);
}

// Sync handler rather than a watch: this app has no GLib main loop to dispatch a bus
// watch from. Nothing pops the bus either, so every other message is dropped here.
GstBusSyncReply onBusMessage(GstBus *bus, GstMessage *message, gpointer data)
{
    Q_UNUSED(bus)

    auto *session = static_cast<Session *>(data);
    GstVideoExporter *exporter = session->exporter;
    const quint64 generation = session->generation;

    switch (GST_MESSAGE_TYPE(message)) {
        case GST_MESSAGE_ERROR: {
            GError *error = nullptr;
            gchar *debug = nullptr;
            gst_message_parse_error(message, &error, &debug);
            const QString text = errorMessage(error, session->output);
            qWarning() << "GStreamer trim error:" << Media::Gst::errorText(error)
                       << (debug ? QString::fromUtf8(debug) : QString());
            g_clear_error(&error);
            g_free(debug);
            QMetaObject::invokeMethod(exporter, [exporter, generation, text] {
                exporter->reportError(generation, text);
            }, Qt::QueuedConnection);
            break;
        }
        case GST_MESSAGE_EOS:
            QMetaObject::invokeMethod(exporter, [exporter, generation] {
                exporter->reportEos(generation);
            }, Qt::QueuedConnection);
            break;
        default:
            break;
    }
    gst_message_unref(message);
    return GST_BUS_DROP;
}

} // namespace

GstVideoExporter::GstVideoExporter(QObject *parent) : VideoExporter(parent)
{
    m_progressTimer = new QTimer(this);
    m_progressTimer->setInterval(kProgressIntervalMs);
    connect(m_progressTimer, &QTimer::timeout, this, &GstVideoExporter::updateProgress);

    m_watchdog = new QTimer(this);
    m_watchdog->setSingleShot(true);
    m_watchdog->setInterval(kWatchdogMs);
    connect(m_watchdog, &QTimer::timeout, this, [this] {
        qWarning() << "GStreamer trim made no progress within" << kWatchdogMs << "ms";
        fail(tr("The trim export stalled."));
    });
}

GstVideoExporter::~GstVideoExporter()
{
    teardown();
}

bool GstVideoExporter::isAvailable() const
{
    if (m_available.has_value())
        return *m_available;

    using Media::Gst::hasFactory;
    m_available = Media::Gst::ensureInitialized()
                  && hasFactory("filesrc")
                  && hasFactory("decodebin")
                  && hasFactory("queue")
                  && hasFactory("videoconvert")
                  && hasFactory("audioconvert")
                  && hasFactory("audioresample")
                  && hasFactory("h264parse")
                  && hasFactory("mp4mux")
                  && hasFactory("qtmux")
                  && hasFactory("filesink")
                  && Media::Gst::hasAnyH264Encoder()
                  && Media::Gst::hasH264Decoder();
    return *m_available;
}

void GstVideoExporter::trim(const QString &input, const QString &output,
                            qint64 inMs, qint64 outMs)
{
    if (m_session) {
        const QString error = tr("An export is already running.");
        QMetaObject::invokeMethod(this, [this, error] { emit failed(error); },
                                  Qt::QueuedConnection);
        return;
    }

    ++m_generation;
    m_outputPath = output;
    m_inMs = inMs;
    m_outMs = outMs;
    m_branches = -1;
    m_readyBranches = 0;
    m_seeked = false;
    m_lastPtsMs = -1;

    if (!isAvailable()) {
        failLater(tr("Trimming is not available (missing GStreamer plugins)."));
        return;
    }
    if (outMs <= inMs) {
        failLater(tr("The trim range is empty."));
        return;
    }

    QFile::remove(output);

    auto session = std::make_unique<Session>();
    session->exporter = this;
    session->generation = m_generation;
    session->output = output;
    session->videoEncoder = Media::Gst::h264EncoderChain({Media::Gst::EncoderTuning::Offline, 0});
    if (Media::Gst::hasFactory("aacparse"))
        session->audioEncoder = Media::Gst::aacEncoderChain();

    const bool mov = QFileInfo(output).suffix().compare(QStringLiteral("mov"),
                                                        Qt::CaseInsensitive) == 0;
    session->pipeline = gst_pipeline_new("snim-trim");
    GstElement *src = makeElement("filesrc");
    session->decodebin = makeElement("decodebin");
    session->mux = makeElement(mov ? "qtmux" : "mp4mux");
    GstElement *sink = makeElement("filesink");
    m_session = std::move(session);
    Session *s = m_session.get();

    if (!s->pipeline || !src || !s->decodebin || !s->mux || !sink) {
        for (GstElement *element : {src, s->decodebin, s->mux, sink}) {
            if (element)
                gst_object_unref(gst_object_ref_sink(element));
        }
        s->decodebin = nullptr;
        s->mux = nullptr;
        failLater(tr("The trim pipeline could not be built."));
        return;
    }

    gst_bin_add_many(GST_BIN(s->pipeline), src, s->decodebin, s->mux, sink, nullptr);
    if (!gst_element_link(src, s->decodebin) || !gst_element_link(s->mux, sink)) {
        failLater(tr("The trim pipeline could not be built."));
        return;
    }

    // Set as properties rather than inside a launch string: no escaping to get wrong.
    g_object_set(src, "location", input.toUtf8().constData(), nullptr);
    g_object_set(sink, "location", output.toUtf8().constData(), nullptr);

    g_signal_connect(s->decodebin, "pad-added", G_CALLBACK(onPadAdded), s);
    g_signal_connect(s->decodebin, "no-more-pads", G_CALLBACK(onNoMorePads), s);

    GstBus *bus = gst_element_get_bus(s->pipeline);
    gst_bus_set_sync_handler(bus, onBusMessage, s, nullptr);
    gst_object_unref(bus);

    m_watchdog->start();
    // PAUSED only: the probes hold every stream until the seek has been made.
    if (gst_element_set_state(s->pipeline, GST_STATE_PAUSED) == GST_STATE_CHANGE_FAILURE)
        failLater(tr("The trim pipeline could not be started."));
}

void GstVideoExporter::toGif(const QString &input, const QString &output,
                             qint64 inMs, qint64 outMs, const AnimationParams &params)
{
    Q_UNUSED(input); Q_UNUSED(output); Q_UNUSED(inMs); Q_UNUSED(outMs); Q_UNUSED(params);
    QMetaObject::invokeMethod(this, [this] {
        emit failed(tr("Exporting to GIF is not supported on this platform."));
    }, Qt::QueuedConnection);
}

void GstVideoExporter::cancel()
{
    if (!m_session)
        return;
    teardown();
    QFile::remove(m_outputPath);
}

// Deferred, so callers connect before the signal fires; a bus error queued first wins.
void GstVideoExporter::failLater(const QString &error)
{
    const quint64 generation = m_generation;
    QMetaObject::invokeMethod(this, [this, generation, error] {
        reportError(generation, error);
    }, Qt::QueuedConnection);
}

void GstVideoExporter::reportPadsComplete(quint64 generation, int branches, bool hasVideo)
{
    if (generation != m_generation || !m_session)
        return;
    if (!hasVideo) {
        fail(tr("The recording has no video track to trim."));
        return;
    }
    m_branches = branches;
    if (m_readyBranches >= m_branches)
        seekAndPlay();
}

void GstVideoExporter::reportBranchReady(quint64 generation)
{
    if (generation != m_generation || !m_session)
        return;
    ++m_readyBranches;
    if (m_branches > 0 && m_readyBranches >= m_branches)
        seekAndPlay();
}

void GstVideoExporter::seekAndPlay()
{
    if (m_seeked)
        return;
    m_seeked = true;

    // Through one decodebin pad rather than the pipeline: the demuxer then sees the
    // seek exactly once, and its flush releases every held buffer.
    GstPad *pad = nullptr;
    {
        const QMutexLocker locker(&m_session->mutex);
        if (m_session->seekPad)
            pad = GST_PAD(gst_object_ref(m_session->seekPad));
    }
    const bool seeked = pad && gst_pad_send_event(
        pad, gst_event_new_seek(1.0, GST_FORMAT_TIME,
                                GstSeekFlags(GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_ACCURATE),
                                GST_SEEK_TYPE_SET, GstClockTime(m_inMs) * GST_MSECOND,
                                GST_SEEK_TYPE_SET, GstClockTime(m_outMs) * GST_MSECOND));
    if (pad)
        gst_object_unref(pad);
    if (!seeked) {
        fail(tr("The recording could not be cut at the selected range."));
        return;
    }

    if (gst_element_set_state(m_session->pipeline, GST_STATE_PLAYING)
        == GST_STATE_CHANGE_FAILURE) {
        fail(tr("The trim pipeline could not be started."));
        return;
    }
    m_watchdog->start();
    m_progressTimer->start();
}

// Position queries do not work here: the muxer hands filesink a BYTES segment.
void GstVideoExporter::updateProgress()
{
    if (!m_session)
        return;
    const qint64 ptsMs = m_session->videoPtsMs.load();
    if (ptsMs < 0 || ptsMs == m_lastPtsMs)
        return;
    m_lastPtsMs = ptsMs;
    m_watchdog->start();
    const qint64 total = m_outMs - m_inMs;
    emit progress(int(qBound<qint64>(0, ptsMs - m_inMs, total)), int(total));
}

void GstVideoExporter::reportEos(quint64 generation)
{
    if (generation != m_generation || !m_session)
        return;
    const QString path = m_outputPath;
    teardown();
    emit finished(path);
}

void GstVideoExporter::reportError(quint64 generation, const QString &error)
{
    if (generation != m_generation)
        return;
    fail(error);
}

void GstVideoExporter::fail(const QString &error)
{
    teardown();
    if (!m_outputPath.isEmpty())
        QFile::remove(m_outputPath);
    emit failed(error);
}

void GstVideoExporter::teardown()
{
    // Invalidates every callback still in flight for the pipeline being dropped.
    ++m_generation;

    m_progressTimer->stop();
    m_watchdog->stop();

    if (m_session && m_session->pipeline) {
        // Blocks until the streaming threads are joined, so no probe, signal or bus
        // callback can still be running once this returns.
        gst_element_set_state(m_session->pipeline, GST_STATE_NULL);
        if (m_session->decodebin)
            g_signal_handlers_disconnect_by_data(m_session->decodebin, m_session.get());
        GstBus *bus = gst_element_get_bus(m_session->pipeline);
        gst_bus_set_sync_handler(bus, nullptr, nullptr, nullptr);
        gst_object_unref(bus);
    }
    m_session.reset();

    m_branches = -1;
    m_readyBranches = 0;
    m_seeked = false;
}

} // namespace Editor::Video

// The one symbol LinuxVideoModule resolves out of this module.
extern "C" Editor::Video::VideoExporter *snimCreateLinuxVideoExporter(QObject *parent)
{
    return new Editor::Video::GstVideoExporter(parent);
}
