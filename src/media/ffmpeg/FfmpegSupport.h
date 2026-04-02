#ifndef MEDIA_FFMPEG_FFMPEGSUPPORT_H
#define MEDIA_FFMPEG_FFMPEGSUPPORT_H

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/frame.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>
}

#include <QString>

#include <memory>

/**
 * Owners for the libav objects the FFmpeg layer holds, so every early return frees
 * them. The layer links FFmpeg statically; nothing here may reach Qt Multimedia's copy.
 */
namespace Media::Ffmpeg {

struct InputFormatDeleter {
    void operator()(AVFormatContext *context) const { avformat_close_input(&context); }
};

// Closes the file too, when the muxer opened one.
struct OutputFormatDeleter {
    void operator()(AVFormatContext *context) const
    {
        if (!context)
            return;
        if (context->pb && context->oformat && !(context->oformat->flags & AVFMT_NOFILE))
            avio_closep(&context->pb);
        avformat_free_context(context);
    }
};

struct CodecContextDeleter {
    void operator()(AVCodecContext *context) const { avcodec_free_context(&context); }
};

struct FrameDeleter {
    void operator()(AVFrame *frame) const { av_frame_free(&frame); }
};

struct PacketDeleter {
    void operator()(AVPacket *packet) const { av_packet_free(&packet); }
};

struct SwsContextDeleter {
    void operator()(SwsContext *context) const { sws_freeContext(context); }
};

struct SwrContextDeleter {
    void operator()(SwrContext *context) const { swr_free(&context); }
};

using InputFormatPtr = std::unique_ptr<AVFormatContext, InputFormatDeleter>;
using OutputFormatPtr = std::unique_ptr<AVFormatContext, OutputFormatDeleter>;
using CodecContextPtr = std::unique_ptr<AVCodecContext, CodecContextDeleter>;
using FramePtr = std::unique_ptr<AVFrame, FrameDeleter>;
using PacketPtr = std::unique_ptr<AVPacket, PacketDeleter>;
using SwsContextPtr = std::unique_ptr<SwsContext, SwsContextDeleter>;
using SwrContextPtr = std::unique_ptr<SwrContext, SwrContextDeleter>;

[[nodiscard]] inline FramePtr makeFrame() { return FramePtr(av_frame_alloc()); }
[[nodiscard]] inline PacketPtr makePacket() { return PacketPtr(av_packet_alloc()); }

// av_strerror's text for an AVERROR code.
[[nodiscard]] QString errorString(int error);

} // namespace Media::Ffmpeg

#endif // MEDIA_FFMPEG_FFMPEGSUPPORT_H
