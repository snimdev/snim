#include "media/gst/GstSupport.h"

#include <QByteArrayView>
#include <QDebug>

namespace Media::Gst {

namespace {

struct Package {
    const char *debian;
    const char *fedora;
};

Package packageOf(QByteArrayView element)
{
    if (element == "pipewiresrc")
        return {"gstreamer1.0-pipewire", "pipewire-gstreamer"};
    if (element == "h264parse")
        return {"gstreamer1.0-plugins-bad", "gstreamer1-plugins-bad-free"};
    for (const char *good : {"ximagesrc", "videocrop", "mp4mux", "qtmux", "qtdemux"}) {
        if (element == good)
            return {"gstreamer1.0-plugins-good", "gstreamer1-plugins-good"};
    }
    for (const char *base : {"videorate", "videoscale", "videoconvert", "audioconvert",
                             "audioresample", "decodebin", "appsink"}) {
        if (element == base)
            return {"gstreamer1.0-plugins-base", "gstreamer1-plugins-base"};
    }
    // capsfilter, valve, queue, filesrc and filesink live in GStreamer's core.
    return {"libgstreamer1.0-0", "gstreamer1"};
}

} // namespace

bool ensureInitialized()
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

H264Plugins probeH264Plugins()
{
    H264Plugins plugins;
    plugins.x264 = hasFactory("x264enc");
    plugins.va = hasFactory("vapostproc") && hasFactory("vah264enc");
    plugins.openh264 = hasFactory("openh264enc");
    plugins.parser = hasFactory("h264parse");
    return plugins;
}

H264Encoder chooseH264Encoder(const H264Plugins &plugins)
{
    if (plugins.x264)
        return H264Encoder::X264;
    if (plugins.va && plugins.parser)
        return H264Encoder::Va;
    if (plugins.openh264 && plugins.parser)
        return H264Encoder::OpenH264;
    return H264Encoder::None;
}

const char *encoderElement(H264Encoder encoder)
{
    switch (encoder) {
    case H264Encoder::X264:
        return "x264enc";
    case H264Encoder::Va:
        return "vah264enc";
    case H264Encoder::OpenH264:
        return "openh264enc";
    case H264Encoder::None:
        break;
    }
    return nullptr;
}

bool hasUsableH264Encoder()
{
    return chooseH264Encoder(probeH264Plugins()) != H264Encoder::None;
}

bool hasH264Decoder()
{
    GList *decoders = gst_element_factory_list_get_elements(
        GST_ELEMENT_FACTORY_TYPE_DECODER | GST_ELEMENT_FACTORY_TYPE_MEDIA_VIDEO,
        GST_RANK_MARGINAL);
    GstCaps *caps = gst_caps_from_string("video/x-h264");
    GList *h264 = gst_element_factory_list_filter(decoders, caps, GST_PAD_SINK, FALSE);
    const bool found = h264 != nullptr;
    gst_plugin_feature_list_free(h264);
    gst_plugin_feature_list_free(decoders);
    gst_caps_unref(caps);
    return found;
}

QString aacEncoderChain()
{
    for (const char *name : {"fdkaacenc", "avenc_aac", "voaacenc"}) {
        if (hasFactory(name))
            return QString::fromLatin1(name);
    }
    return {};
}

QString h264EncoderChain(H264Encoder encoder, bool parser, const EncoderTuning &tuning)
{
    // 4:2:0 for the software encoders: from an RGB source x264 would pick 4:4:4, which
    // browsers and most hardware decoders cannot play.
    const QString i420 = QStringLiteral("capsfilter caps=video/x-raw,format=I420 ! ");
    QString chain;
    switch (encoder) {
    case H264Encoder::X264:
        chain = i420 + (tuning.mode == EncoderTuning::Live
                            ? QStringLiteral("x264enc tune=zerolatency speed-preset=veryfast "
                                             "pass=qual quantizer=22")
                            : QStringLiteral("x264enc speed-preset=faster pass=qual "
                                             "quantizer=20"));
        if (tuning.keyIntMax > 0)
            chain += QStringLiteral(" key-int-max=%1").arg(tuning.keyIntMax);
        break;
    case H264Encoder::Va:
        // vah264enc only encodes 4:2:0 and vapostproc converts into what it takes.
        chain = QStringLiteral("vapostproc ! vah264enc");
        break;
    case H264Encoder::OpenH264:
        chain = i420 + QStringLiteral("openh264enc complexity=0");
        break;
    case H264Encoder::None:
        return {};
    }
    // Kept whenever installed: it also mends timestamps and headers for the muxer.
    if (parser)
        return chain + QStringLiteral(" ! h264parse");
    // An element, not bare caps: a bin description cannot end in a caps link.
    return chain
           + QStringLiteral(" ! capsfilter caps=video/x-h264,stream-format=avc,alignment=au");
}

QString h264EncoderChain(const EncoderTuning &tuning)
{
    const H264Plugins plugins = probeH264Plugins();
    return h264EncoderChain(chooseH264Encoder(plugins), plugins.parser, tuning);
}

QStringList missingPieces(const QList<const char *> &required,
                          const std::function<bool(const char *)> &has, const H264Plugins &h264)
{
    QList<const char *> missing;
    for (const char *element : required) {
        if (!has(element))
            missing << element;
    }
    const bool encoderMissing = chooseH264Encoder(h264) == H264Encoder::None;
    // An encoder that only lacks the parser is one package away.
    if (encoderMissing && (h264.va || h264.openh264))
        missing << "h264parse";

    QList<Package> packages;
    QList<QStringList> elements;
    for (const char *element : std::as_const(missing)) {
        const Package package = packageOf(element);
        qsizetype index = 0;
        while (index < packages.size() && qstrcmp(packages[index].debian, package.debian) != 0)
            ++index;
        if (index == packages.size()) {
            packages << package;
            elements << QStringList();
        }
        elements[index] << QString::fromLatin1(element);
    }

    QStringList pieces;
    for (qsizetype i = 0; i < packages.size(); ++i) {
        pieces << QStringLiteral("%1 (%2; Fedora: %3)")
                      .arg(elements[i].join(QStringLiteral(", ")),
                           QLatin1String(packages[i].debian), QLatin1String(packages[i].fedora));
    }
    if (encoderMissing && !h264.va && !h264.openh264) {
        pieces << QStringLiteral("an H.264 encoder (gstreamer1.0-plugins-ugly or "
                                 "gstreamer1.0-plugins-bad; Fedora: gstreamer1-plugin-openh264 "
                                 "and gstreamer1-plugins-bad-free)");
    }
    return pieces;
}

void configureRecordingOutput(GstElement *mux, GstElement *filesink)
{
    // Each fragment reaches the disk whole, instead of partly waiting in a 64 KiB buffer.
    gst_util_set_object_arg(G_OBJECT(filesink), "buffer-mode", "unbuffered");

    GObjectClass *klass = G_OBJECT_GET_CLASS(mux);
    if (!g_object_class_find_property(klass, "fragment-duration"))
        return;
    g_object_set(mux, "fragment-duration", guint(kFragmentIntervalMs), nullptr);
    // Not first-moov-then-finalise: with audio, a killed file keeps only its first fragment.
    if (g_object_class_find_property(klass, "fragment-mode"))
        gst_util_set_object_arg(G_OBJECT(mux), "fragment-mode", "dash-or-mss");
}

QString errorText(const GError *error)
{
    return error && error->message ? QString::fromUtf8(error->message) : QString();
}

} // namespace Media::Gst
