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

bool hasAnyH264Encoder()
{
    return hasFactory("x264enc") || hasFactory("vah264enc") || hasFactory("openh264enc");
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

QString h264EncoderChain(const EncoderTuning &tuning)
{
    if (hasFactory("x264enc")) {
        QString chain = tuning.mode == EncoderTuning::Live
                            ? QStringLiteral("x264enc tune=zerolatency speed-preset=veryfast "
                                             "pass=qual quantizer=22")
                            : QStringLiteral("x264enc speed-preset=faster pass=qual quantizer=20");
        if (tuning.keyIntMax > 0)
            chain += QStringLiteral(" key-int-max=%1").arg(tuning.keyIntMax);
        return chain;
    }
    if (hasFactory("vapostproc") && hasFactory("vah264enc"))
        return QStringLiteral("vapostproc ! vah264enc");
    if (hasFactory("openh264enc"))
        return QStringLiteral("openh264enc complexity=0");
    return {};
}

QString errorText(const GError *error)
{
    return error && error->message ? QString::fromUtf8(error->message) : QString();
}

} // namespace Media::Gst
