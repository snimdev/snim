#ifndef MEDIA_FFMPEG_H264ENCODERCHAIN_H
#define MEDIA_FFMPEG_H264ENCODERCHAIN_H

#include "media/ffmpeg/FfmpegSupport.h"

#include <QString>
#include <QStringList>

#include <memory>

namespace Media::Ffmpeg {

struct H264EncoderSettings {
    enum Tuning { Live, Offline };

    int width = 0;                    // even
    int height = 0;                   // even
    AVRational timeBase{1, 90000};
    AVRational frameRate{30, 1};      // nominal, for rate control and the GOP
    Tuning tuning = Live;

    // Tagged into the stream; a transcode copies its source's.
    AVColorRange colorRange = AVCOL_RANGE_MPEG;
    AVColorPrimaries colorPrimaries = AVCOL_PRI_BT709;
    AVColorTransferCharacteristic colorTrc = AVCOL_TRC_BT709;
    AVColorSpace colorSpace = AVCOL_SPC_BT709;
};

// An encoder the chain opened, ready for avcodec_send_frame.
struct OpenedH264Encoder {
    CodecContextPtr context;
    QString name;

    explicit operator bool() const { return context != nullptr; }
};

/**
 * Picks the H.264 encoder (Chain of Responsibility): each link names one FFmpeg
 * encoder and handles the request only if that encoder actually opens with the
 * caller's settings, otherwise it passes it on. Hardware first (NVENC, Quick Sync,
 * AMF, Media Foundation), libx264 last because it always opens. SNIM_H264_ENCODER
 * names a single encoder to use instead of the whole chain.
 */
class H264EncoderChain
{
public:
    static constexpr char kOverrideVariable[] = "SNIM_H264_ENCODER";

    // FFmpeg encoder names, most preferred first.
    [[nodiscard]] static QStringList defaultOrder();

    // The default order, or only the encoder SNIM_H264_ENCODER names when it is one of them.
    [[nodiscard]] static H264EncoderChain fromEnvironment();

    explicit H264EncoderChain(const QStringList &order);
    H264EncoderChain(H264EncoderChain &&) noexcept;
    H264EncoderChain &operator=(H264EncoderChain &&) noexcept;
    ~H264EncoderChain();

    [[nodiscard]] QStringList names() const;

    // The first link whose encoder opens, or an empty result when none does.
    [[nodiscard]] OpenedH264Encoder open(const H264EncoderSettings &settings) const;

private:
    class Link;
    std::unique_ptr<Link> m_head;
};

} // namespace Media::Ffmpeg

#endif // MEDIA_FFMPEG_H264ENCODERCHAIN_H
