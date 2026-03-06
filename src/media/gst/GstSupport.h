#ifndef MEDIA_GST_GSTSUPPORT_H
#define MEDIA_GST_GSTSUPPORT_H

#include <gst/gst.h>

#include <QString>

#include <memory>

/**
 * GStreamer plumbing shared by the dlopened Linux modules (the recorder and the trim
 * exporter). Each module compiles its own copy: snim_lib must never link GStreamer.
 */
namespace Media::Gst {

// Initializes GStreamer once per process, after pointing it at any bundled plugins.
[[nodiscard]] bool ensureInitialized();

[[nodiscard]] bool hasFactory(const char *name);

[[nodiscard]] bool hasAnyH264Encoder();

// Whether decodebin can autoplug something that decodes H.264.
[[nodiscard]] bool hasH264Decoder();

// AAC encoders in preference order. Empty when none is installed.
[[nodiscard]] QString aacEncoderChain();

struct EncoderTuning {
    enum Mode { Live, Offline };
    Mode mode = Live;
    int keyIntMax = 0;   // 0 keeps the encoder's default
};

// Software x264 first (predictable and always present when installed), then the VA-API
// encoder, then OpenH264 as the last resort. Empty when none is installed.
[[nodiscard]] QString h264EncoderChain(const EncoderTuning &tuning);

// The message of a GError, or an empty string for none.
[[nodiscard]] QString errorText(const GError *error);

struct GstUnref {
    void operator()(gpointer object) const
    {
        if (object)
            gst_object_unref(object);
    }
};

template <typename T>
using GstPtr = std::unique_ptr<T, GstUnref>;

} // namespace Media::Gst

#endif // MEDIA_GST_GSTSUPPORT_H
