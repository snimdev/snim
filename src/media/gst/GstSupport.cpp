#include "media/gst/GstSupport.h"

#include "core/BundledPaths.h"

#include <QDebug>

namespace Media::Gst {

bool ensureInitialized()
{
    static const bool ok = [] {
        // Must precede gst_init: it reads the plugin path once, when it builds its registry.
        Core::BundledPaths::applyForThisExecutable();
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
    QString chain;
    switch (encoder) {
    case H264Encoder::X264:
        chain = tuning.mode == EncoderTuning::Live
                    ? QStringLiteral("x264enc tune=zerolatency speed-preset=veryfast "
                                     "pass=qual quantizer=22")
                    : QStringLiteral("x264enc speed-preset=faster pass=qual quantizer=20");
        if (tuning.keyIntMax > 0)
            chain += QStringLiteral(" key-int-max=%1").arg(tuning.keyIntMax);
        break;
    case H264Encoder::Va:
        chain = QStringLiteral("vapostproc ! vah264enc");
        break;
    case H264Encoder::OpenH264:
        chain = QStringLiteral("openh264enc complexity=0");
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
