#ifndef MEDIA_GST_GSTSUPPORT_H
#define MEDIA_GST_GSTSUPPORT_H

#include <gst/gst.h>

#include <QList>
#include <QString>
#include <QStringList>

#include <functional>
#include <memory>

/**
 * GStreamer plumbing shared by the dlopened Linux modules (the recorder and the trim
 * exporter). Each module compiles its own copy: snim_lib must never link GStreamer.
 */
namespace Media::Gst {

// Initializes GStreamer once per process, on the host's or the runtime's plugin path.
[[nodiscard]] bool ensureInitialized();

[[nodiscard]] bool hasFactory(const char *name);

// Which H.264 pieces are installed. va means both vapostproc and vah264enc.
struct H264Plugins {
    bool x264 = false;
    bool va = false;
    bool openh264 = false;
    bool parser = false;   // h264parse
};

enum class H264Encoder { None, X264, Va, OpenH264 };

[[nodiscard]] H264Plugins probeH264Plugins();

// Software x264 first (predictable and always present when installed), then the VA-API
// encoder, then OpenH264 as the last resort. VA and OpenH264 emit only byte-stream, which
// mp4mux rejects, so they need h264parse; x264 can hand mp4mux avc on its own.
[[nodiscard]] H264Encoder chooseH264Encoder(const H264Plugins &plugins);

// The encoder's own element, as GStreamer names it; nullptr for None.
[[nodiscard]] const char *encoderElement(H264Encoder encoder);

[[nodiscard]] bool hasUsableH264Encoder();

// Whether decodebin can autoplug something that decodes H.264.
[[nodiscard]] bool hasH264Decoder();

// AAC encoders in preference order. Empty when none is installed.
[[nodiscard]] QString aacEncoderChain();

struct EncoderTuning {
    enum Mode { Live, Offline };
    Mode mode = Live;
    int keyIntMax = 0;   // 0 keeps the encoder's default
};

// The encoder plus h264parse, or a caps filter asking for avc where there is no parser: a
// chain that links straight into mp4mux or qtmux. Empty for None.
[[nodiscard]] QString h264EncoderChain(H264Encoder encoder, bool parser,
                                       const EncoderTuning &tuning);

// The chain for the best usable encoder installed here. Empty when there is none.
[[nodiscard]] QString h264EncoderChain(const EncoderTuning &tuning);

// What stops H.264 into MP4 here, for a user to install: the `required` elements `has`
// lacks, grouped by the package that ships them, then the encoder or the parser it needs.
// Empty when nothing is missing.
[[nodiscard]] QStringList missingPieces(const QList<const char *> &required,
                                        const std::function<bool(const char *)> &has,
                                        const H264Plugins &h264);

// How often a recording's MP4 closes a fragment: at most this much is lost to a crash.
inline constexpr int kFragmentIntervalMs = 1000;

// Sets up a recording's qtmux or mp4mux and filesink so a killed recording stays readable.
void configureRecordingOutput(GstElement *mux, GstElement *filesink);

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
