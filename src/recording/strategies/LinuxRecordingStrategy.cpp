#include "recording/strategies/LinuxRecordingStrategy.h"

#include "recording/RecordingGeometry.h"
#include "recording/strategies/ScreenCastPortalSession.h"

#include <gst/gst.h>

#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QScreen>
#include <QTimer>

#include <unistd.h>

namespace Recording {

namespace {

constexpr int kDurationIntervalMs = 250;
constexpr int kEosTimeoutMs = 5000;

bool ensureGstInitialized()
{
    static const bool ok = [] {
        GError *error = nullptr;
        const gboolean initialized = gst_init_check(nullptr, nullptr, &error);
        if (error) {
            qWarning() << "GStreamer init failed:" << error->message;
            g_clear_error(&error);
        }
        return initialized != FALSE;
    }();
    return ok;
}

bool hasFactory(const char *name)
{
    GstElementFactory *factory = gst_element_factory_find(name);
    if (!factory)
        return false;
    gst_object_unref(factory);
    return true;
}

// Software x264 first (predictable and always present when installed), then the VA-API
// encoder, then OpenH264 as the last resort.
QString encoderChain(int fps)
{
    if (hasFactory("x264enc"))
        return QStringLiteral("x264enc tune=zerolatency speed-preset=veryfast pass=qual "
                              "quantizer=22 key-int-max=%1").arg(fps * 2);
    if (hasFactory("vapostproc") && hasFactory("vah264enc"))
        return QStringLiteral("vapostproc ! vah264enc");
    if (hasFactory("openh264enc"))
        return QStringLiteral("openh264enc complexity=0");
    return {};
}

bool hasAnyEncoder()
{
    return hasFactory("x264enc") || hasFactory("vah264enc") || hasFactory("openh264enc");
}

// AAC encoders in preference order. Empty when none is installed.
QString aacEncoderChain()
{
    for (const char *name : {"fdkaacenc", "avenc_aac", "voaacenc"}) {
        if (hasFactory(name))
            return QString::fromLatin1(name);
    }
    return {};
}

// The screen holding the selection, used when the portal omits the stream position/size.
QRect screenRectFor(const QRect &regionVirtual)
{
    const QScreen *screen = QGuiApplication::screenAt(regionVirtual.center());
    if (!screen)
        screen = QGuiApplication::primaryScreen();
    return screen ? screen->geometry() : QRect();
}

// The strategy plus the pipeline it belongs to; callbacks run on GStreamer threads.
struct CallbackContext {
    LinuxRecordingStrategy *strategy = nullptr;
    quint64 generation = 0;
};

struct CropContext {
    LinuxRecordingStrategy *strategy = nullptr;
    quint64 generation = 0;
    QRect regionVirtual;
    QRect streamRect;
    bool retina = true;
    GstElement *crop = nullptr;      // owned ref
    GstElement *outcaps = nullptr;   // owned ref
};

void freeCallbackContext(gpointer data)
{
    delete static_cast<CallbackContext *>(data);
}

void freeCropContext(gpointer data)
{
    auto *ctx = static_cast<CropContext *>(data);
    if (ctx->crop)
        gst_object_unref(ctx->crop);
    if (ctx->outcaps)
        gst_object_unref(ctx->outcaps);
    delete ctx;
}

// The muxer's video sink pad, which parse-launch has already requested for us. mp4mux and
// qtmux name their request pads video_%u / audio_%u, so match on the prefix.
GstPad *videoSinkPad(GstElement *element)
{
    GstIterator *it = gst_element_iterate_sink_pads(element);
    if (!it)
        return nullptr;

    GValue value = G_VALUE_INIT;
    GstPad *fallback = nullptr;
    GstPad *video = nullptr;
    while (!video && gst_iterator_next(it, &value) == GST_ITERATOR_OK) {
        GstPad *pad = GST_PAD(g_value_dup_object(&value));
        g_value_reset(&value);
        if (!pad)
            continue;

        gchar *name = gst_pad_get_name(pad);
        const bool isVideo = name && g_str_has_prefix(name, "video");
        g_free(name);

        if (isVideo) {
            video = pad;
        } else if (!fallback) {
            fallback = pad;
        } else {
            gst_object_unref(pad);
        }
    }
    g_value_unset(&value);
    gst_iterator_free(it);

    if (video) {
        if (fallback)
            gst_object_unref(fallback);
        return video;
    }
    return fallback;
}

GstPadProbeReturn onCapsEvent(GstPad *pad, GstPadProbeInfo *info, gpointer data)
{
    Q_UNUSED(pad)

    GstEvent *event = GST_PAD_PROBE_INFO_EVENT(info);
    if (!event || GST_EVENT_TYPE(event) != GST_EVENT_CAPS)
        return GST_PAD_PROBE_OK;

    auto *ctx = static_cast<CropContext *>(data);

    GstCaps *caps = nullptr;
    gst_event_parse_caps(event, &caps);
    int width = 0;
    int height = 0;
    const GstStructure *structure = caps ? gst_caps_get_structure(caps, 0) : nullptr;
    if (!structure || !gst_structure_get_int(structure, "width", &width)
        || !gst_structure_get_int(structure, "height", &height)) {
        return GST_PAD_PROBE_OK;
    }

    // The negotiated caps carry the real pixel size, which the logical stream metadata
    // cannot give us under fractional scaling.
    const StreamCrop crop = portalStreamCrop(ctx->regionVirtual, ctx->streamRect,
                                             QSize(width, height), ctx->retina);
    if (!crop.valid) {
        LinuxRecordingStrategy *strategy = ctx->strategy;
        const quint64 generation = ctx->generation;
        QMetaObject::invokeMethod(strategy, [strategy, generation] {
            strategy->reportError(generation,
                                  LinuxRecordingStrategy::tr(
                                      "The selected area is not on the shared screen. "
                                      "Pick the screen containing your selection."));
        }, Qt::QueuedConnection);
        return GST_PAD_PROBE_REMOVE;
    }

    g_object_set(ctx->crop,
                 "left", crop.cropPx.x(),
                 "top", crop.cropPx.y(),
                 "right", width - (crop.cropPx.x() + crop.cropPx.width()),
                 "bottom", height - (crop.cropPx.y() + crop.cropPx.height()),
                 nullptr);

    GstCaps *outCaps = gst_caps_new_simple("video/x-raw",
                                           "width", G_TYPE_INT, crop.outputPx.width(),
                                           "height", G_TYPE_INT, crop.outputPx.height(),
                                           "pixel-aspect-ratio", GST_TYPE_FRACTION, 1, 1,
                                           nullptr);
    g_object_set(ctx->outcaps, "caps", outCaps, nullptr);
    gst_caps_unref(outCaps);

    return GST_PAD_PROBE_REMOVE;
}

GstPadProbeReturn onFirstBuffer(GstPad *pad, GstPadProbeInfo *info, gpointer data)
{
    Q_UNUSED(pad)
    Q_UNUSED(info)

    auto *ctx = static_cast<CallbackContext *>(data);
    LinuxRecordingStrategy *strategy = ctx->strategy;
    const quint64 generation = ctx->generation;
    QMetaObject::invokeMethod(strategy, [strategy, generation] {
        strategy->reportStarted(generation);
    }, Qt::QueuedConnection);

    return GST_PAD_PROBE_REMOVE;
}

// Sync handler rather than a watch: this app has no GLib main loop to dispatch a bus
// watch from.
GstBusSyncReply onBusMessage(GstBus *bus, GstMessage *message, gpointer data)
{
    Q_UNUSED(bus)

    auto *ctx = static_cast<CallbackContext *>(data);
    LinuxRecordingStrategy *strategy = ctx->strategy;
    const quint64 generation = ctx->generation;

    switch (GST_MESSAGE_TYPE(message)) {
        case GST_MESSAGE_ERROR: {
            GError *error = nullptr;
            gchar *debug = nullptr;
            gst_message_parse_error(message, &error, &debug);

            QString text;
            if (error && error->domain == GST_RESOURCE_ERROR
                && error->code == GST_RESOURCE_ERROR_NO_SPACE_LEFT) {
                text = LinuxRecordingStrategy::tr("Not enough disk space to continue recording.");
            } else {
                text = error ? QString::fromUtf8(error->message)
                             : LinuxRecordingStrategy::tr("The recording pipeline failed.");
            }
            qWarning() << "GStreamer recording error:" << text
                       << (debug ? QString::fromUtf8(debug) : QString());

            if (error)
                g_error_free(error);
            g_free(debug);
            gst_message_unref(message);

            QMetaObject::invokeMethod(strategy, [strategy, generation, text] {
                strategy->reportError(generation, text);
            }, Qt::QueuedConnection);
            return GST_BUS_DROP;
        }
        case GST_MESSAGE_EOS: {
            gst_message_unref(message);
            QMetaObject::invokeMethod(strategy, [strategy, generation] {
                strategy->reportEos(generation);
            }, Qt::QueuedConnection);
            return GST_BUS_DROP;
        }
        default:
            return GST_BUS_PASS;
    }
}

} // namespace

LinuxRecordingStrategy::LinuxRecordingStrategy(QObject *parent) : RecordingStrategy(parent)
{
    m_durationTimer = new QTimer(this);
    m_durationTimer->setInterval(kDurationIntervalMs);
    connect(m_durationTimer, &QTimer::timeout, this, [this] {
        gint64 position = 0;
        if (m_pipeline && gst_element_query_position(m_pipeline, GST_FORMAT_TIME, &position)
            && position >= 0) {
            emit durationChanged(position / GST_MSECOND);
        }
    });

    m_eosTimer = new QTimer(this);
    m_eosTimer->setSingleShot(true);
    m_eosTimer->setInterval(kEosTimeoutMs);
    connect(m_eosTimer, &QTimer::timeout, this, [this] {
        qWarning() << "GStreamer did not finalize the recording within" << kEosTimeoutMs << "ms";
        fail(tr("The recording could not be finalized."));
    });
}

LinuxRecordingStrategy::~LinuxRecordingStrategy()
{
    teardown();
}

bool LinuxRecordingStrategy::isAvailable() const
{
    if (m_available.has_value())
        return *m_available;

    m_available = ensureGstInitialized()
                  && ScreenCastPortalSession::isPortalAvailable()
                  && hasFactory("pipewiresrc")
                  && hasFactory("videorate")
                  && hasFactory("videocrop")
                  && hasFactory("videoscale")
                  && hasFactory("videoconvert")
                  && hasFactory("capsfilter")
                  && hasFactory("h264parse")
                  && hasFactory("mp4mux")
                  && hasAnyEncoder();
    return *m_available;
}

void LinuxRecordingStrategy::start(const RecordTarget &target, const QString &outputPath)
{
    if (isRecording()) {
        emit failed(tr("A recording is already running."));
        return;
    }
    if (!isAvailable()) {
        emit failed(tr("Screen recording is not available on this system."));
        return;
    }

    m_target = target;
    m_outputPath = outputPath;
    m_starting = true;
    m_stopping = false;

    if (!m_session) {
        m_session = new ScreenCastPortalSession(this);
        connect(m_session, &ScreenCastPortalSession::ready,
                this, &LinuxRecordingStrategy::handleSessionReady);
        connect(m_session, &ScreenCastPortalSession::failed,
                this, &LinuxRecordingStrategy::handleSessionFailed);
        connect(m_session, &ScreenCastPortalSession::sessionClosed,
                this, &LinuxRecordingStrategy::handleSessionClosed);
    }

    // Window capture goes through regionVirtual too: there are no window ids to hand the
    // portal on Linux yet.
    m_session->open(target.captureCursor);
}

void LinuxRecordingStrategy::stop()
{
    if (!isRecording() || m_stopping)
        return;

    m_stopping = true;

    if (!m_pipeline) {
        // The portal handshake never produced a stream, so there is nothing to finalize.
        teardown();
        emit failed(tr("The recording was stopped before it started."));
        return;
    }

    m_durationTimer->stop();
    gst_element_send_event(m_pipeline, gst_event_new_eos());
    m_eosTimer->start();
}

void LinuxRecordingStrategy::handleSessionReady(quint32 nodeId, const QRect &streamRectLogical,
                                                int pipewireFd)
{
    if (!m_starting) {
        ::close(pipewireFd);
        return;
    }

    m_pipewireFd = pipewireFd;
    m_streamRect = streamRectLogical.isEmpty() ? screenRectFor(m_target.regionVirtual)
                                               : streamRectLogical;

    QString error;
    if (!buildPipeline(nodeId, &error)) {
        teardown();
        emit failed(error);
        return;
    }

    if (gst_element_set_state(m_pipeline, GST_STATE_PLAYING) == GST_STATE_CHANGE_FAILURE) {
        teardown();
        emit failed(tr("The recording pipeline could not be started."));
    }
}

void LinuxRecordingStrategy::handleSessionFailed(const QString &error)
{
    if (!isRecording())
        return;
    fail(error);
}

void LinuxRecordingStrategy::handleSessionClosed()
{
    // The compositor's stop-sharing button: finalize what has been recorded so far.
    if (isRecording())
        stop();
}

bool LinuxRecordingStrategy::buildPipeline(quint32 nodeId, QString *error)
{
    const QString encoder = encoderChain(m_target.fps);
    if (encoder.isEmpty()) {
        *error = tr("No H.264 encoder is installed.");
        return false;
    }

    const QString mux = QFileInfo(m_outputPath).suffix().compare(QStringLiteral("mov"),
                                                                 Qt::CaseInsensitive) == 0
                        ? QStringLiteral("qtmux") : QStringLiteral("mp4mux");

    QString description =
        QStringLiteral("pipewiresrc name=src keepalive-time=1000 resend-last=true "
                       "! videorate drop-only=true max-rate=%1 skip-to-first=true "
                       "! videocrop name=crop ! videoscale ! videoconvert "
                       "! capsfilter name=outcaps caps=video/x-raw,pixel-aspect-ratio=1/1 "
                       "! queue ! %2 ! h264parse ! queue ! %3 name=mux ! filesink name=sink")
            .arg(QString::number(m_target.fps), encoder, mux);

    if (m_target.captureMic || m_target.captureSystemAudio) {
        const QString aac = aacEncoderChain();
        // Audio is best-effort: a missing piece costs the audio track, not the recording.
        if (!hasFactory("pulsesrc") || !hasFactory("aacparse") || aac.isEmpty()) {
            qWarning() << "Audio capture requested but pulsesrc, aacparse or an AAC encoder is "
                          "missing; recording video only";
        } else {
            // An audiomixer even for a single source, so mic, system audio and both share
            // one code path.
            description += QStringLiteral(
                " audiomixer name=amix ! audioconvert ! audioresample "
                "! audio/x-raw,rate=48000,channels=2 ! %1 ! aacparse ! queue ! mux.").arg(aac);
            if (m_target.captureMic) {
                description += QStringLiteral(
                    " pulsesrc name=micsrc ! queue ! audioconvert ! audioresample ! amix.");
            }
            if (m_target.captureSystemAudio) {
                // @DEFAULT_MONITOR@ is a pipewire-pulse alias for the default sink's monitor.
                description += QStringLiteral(
                    " pulsesrc name=syssrc device=@DEFAULT_MONITOR@ provide-clock=false "
                    "! queue ! audioconvert ! audioresample ! amix.");
            }
        }
    }

    GError *parseError = nullptr;
    m_pipeline = gst_parse_launch(description.toUtf8().constData(), &parseError);
    if (!m_pipeline) {
        *error = parseError ? tr("The recording pipeline could not be built: %1")
                                  .arg(QString::fromUtf8(parseError->message))
                            : tr("The recording pipeline could not be built.");
        g_clear_error(&parseError);
        return false;
    }
    g_clear_error(&parseError);

    ++m_generation;

    GstElement *src = gst_bin_get_by_name(GST_BIN(m_pipeline), "src");
    GstElement *crop = gst_bin_get_by_name(GST_BIN(m_pipeline), "crop");
    GstElement *outcaps = gst_bin_get_by_name(GST_BIN(m_pipeline), "outcaps");
    GstElement *muxer = gst_bin_get_by_name(GST_BIN(m_pipeline), "mux");
    GstElement *sink = gst_bin_get_by_name(GST_BIN(m_pipeline), "sink");
    const auto unrefAll = [&] {
        for (GstElement *element : {src, crop, outcaps, muxer, sink}) {
            if (element)
                gst_object_unref(element);
        }
    };

    if (!src || !crop || !outcaps || !muxer || !sink) {
        unrefAll();
        *error = tr("The recording pipeline could not be built.");
        return false;
    }

    // Set as properties rather than inside the launch string: no escaping to get wrong.
    g_object_set(src, "fd", m_pipewireFd,
                 "path", QByteArray::number(nodeId).constData(), nullptr);
    g_object_set(sink, "location", m_outputPath.toUtf8().constData(), nullptr);

    if (GstElement *micsrc = gst_bin_get_by_name(GST_BIN(m_pipeline), "micsrc")) {
        // Under Qt's pulse backend QAudioDevice::id() is the pulse source name.
        if (!m_target.micDeviceId.isEmpty())
            g_object_set(micsrc, "device", m_target.micDeviceId.constData(), nullptr);
        gst_object_unref(micsrc);
    }

    if (GstPad *srcPad = gst_element_get_static_pad(src, "src")) {
        auto *cropCtx = new CropContext;
        cropCtx->strategy = this;
        cropCtx->generation = m_generation;
        cropCtx->regionVirtual = m_target.regionVirtual;
        cropCtx->streamRect = m_streamRect;
        cropCtx->retina = m_target.retinaCapture;
        cropCtx->crop = GST_ELEMENT(gst_object_ref(crop));
        cropCtx->outcaps = GST_ELEMENT(gst_object_ref(outcaps));
        gst_pad_add_probe(srcPad, GST_PAD_PROBE_TYPE_EVENT_DOWNSTREAM, onCapsEvent, cropCtx,
                          freeCropContext);
        gst_object_unref(srcPad);
    }

    // The first video buffer reaching the muxer is the moment capture is really live.
    if (GstPad *muxPad = videoSinkPad(muxer)) {
        auto *bufferCtx = new CallbackContext{this, m_generation};
        gst_pad_add_probe(muxPad, GST_PAD_PROBE_TYPE_BUFFER, onFirstBuffer, bufferCtx,
                          freeCallbackContext);
        gst_object_unref(muxPad);
    }

    GstBus *bus = gst_element_get_bus(m_pipeline);
    auto *busCtx = new CallbackContext{this, m_generation};
    gst_bus_set_sync_handler(bus, onBusMessage, busCtx, freeCallbackContext);
    gst_object_unref(bus);

    unrefAll();
    return true;
}

void LinuxRecordingStrategy::reportStarted(quint64 generation)
{
    if (generation != m_generation || m_recording || m_stopping)
        return;

    m_starting = false;
    m_recording = true;
    m_durationTimer->start();
    emit started();
}

void LinuxRecordingStrategy::reportEos(quint64 generation)
{
    if (generation != m_generation)
        return;

    const QString path = m_outputPath;
    teardown();
    emit finished(path);
}

void LinuxRecordingStrategy::reportError(quint64 generation, const QString &error)
{
    if (generation != m_generation)
        return;
    fail(error);
}

void LinuxRecordingStrategy::fail(const QString &error)
{
    const QString path = m_outputPath;
    teardown();
    if (!path.isEmpty())
        QFile::remove(path);
    emit failed(error);
}

void LinuxRecordingStrategy::teardown()
{
    // Invalidates every callback still in flight for the pipeline being dropped.
    ++m_generation;

    m_durationTimer->stop();
    m_eosTimer->stop();

    if (m_pipeline) {
        // Blocks until the streaming threads are joined, so no probe or bus callback can
        // still be running once this returns.
        gst_element_set_state(m_pipeline, GST_STATE_NULL);
        gst_object_unref(m_pipeline);
        m_pipeline = nullptr;
    }

    if (m_pipewireFd >= 0) {
        ::close(m_pipewireFd);
        m_pipewireFd = -1;
    }

    if (m_session)
        m_session->close();

    m_starting = false;
    m_recording = false;
    m_stopping = false;
}

} // namespace Recording
