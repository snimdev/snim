#include "media/ffmpeg/FfmpegEncoder.h"

extern "C" {
#include <libavutil/audio_fifo.h>
#include <libavutil/channel_layout.h>
#include <libavutil/mathematics.h>
#include <libavutil/pixdesc.h>
#include <libavutil/samplefmt.h>
}

#include <QFileInfo>

#include <algorithm>

namespace Media::Ffmpeg {

namespace {

constexpr AVRational kMicroseconds{1, 1000000};

int swsColorspace(AVColorSpace space)
{
    switch (space) {
        case AVCOL_SPC_BT709: return SWS_CS_ITU709;
        case AVCOL_SPC_BT2020_NCL:
        case AVCOL_SPC_BT2020_CL: return SWS_CS_BT2020;
        case AVCOL_SPC_SMPTE240M: return SWS_CS_SMPTE240M;
        case AVCOL_SPC_FCC: return SWS_CS_FCC;
        default: return SWS_CS_DEFAULT;
    }
}

bool isRgb(int format)
{
    const AVPixFmtDescriptor *descriptor = av_pix_fmt_desc_get(AVPixelFormat(format));
    return descriptor && (descriptor->flags & AV_PIX_FMT_FLAG_RGB);
}

// Crops a source one pixel larger than the (even) encoder size instead of scaling it.
int sourceExtent(int source, int target)
{
    return source == target + 1 ? target : source;
}

} // namespace

FfmpegEncoder::FfmpegEncoder() = default;

FfmpegEncoder::~FfmpegEncoder()
{
    if (m_fifo)
        av_audio_fifo_free(m_fifo);
}

bool FfmpegEncoder::fail(const QString &context, int error)
{
    m_error = error ? QStringLiteral("%1: %2").arg(context, Ffmpeg::errorString(error)) : context;
    m_open = false;
    return false;
}

bool FfmpegEncoder::open(const FfmpegEncoderSettings &settings)
{
    m_settings = settings;
    m_sourceWidth = settings.video.width;
    m_sourceHeight = settings.video.height;
    m_settings.video.width &= ~1;
    m_settings.video.height &= ~1;
    m_settings.video.globalHeader = true;
    if (m_settings.video.width <= 0 || m_settings.video.height <= 0)
        return fail(QStringLiteral("The video is too small to encode"));

    const bool mov = QFileInfo(settings.path).suffix().compare(QStringLiteral("mov"),
                                                               Qt::CaseInsensitive) == 0;
    const QByteArray path = settings.path.toUtf8();
    AVFormatContext *format = nullptr;
    int result = avformat_alloc_output_context2(&format, nullptr, mov ? "mov" : "mp4",
                                                path.constData());
    m_format.reset(format);
    if (result < 0 || !m_format)
        return fail(QStringLiteral("Cannot set up the muxer"), result);

    m_packet = makePacket();
    m_converted = makeFrame();
    m_resampled = makeFrame();
    if (!m_packet || !m_converted || !m_resampled)
        return fail(QStringLiteral("Out of memory"));

    if (!openVideo())
        return false;
    if (m_settings.channels > 0 && !openAudio())
        return false;

    result = avio_open(&m_format->pb, path.constData(), AVIO_FLAG_WRITE);
    if (result < 0)
        return fail(QStringLiteral("Cannot write %1").arg(settings.path), result);

    AVDictionary *options = nullptr;
    if (m_settings.fragmented) {
        av_dict_set(&options, "movflags", "+frag_keyframe+empty_moov+default_base_moof", 0);
        av_dict_set(&options, "frag_duration", "1000000", 0);
        // Each finished fragment goes straight to the file, not into the I/O buffer.
        m_format->flush_packets = 1;
    } else {
        av_dict_set(&options, "movflags", "+faststart", 0);
    }
    result = avformat_write_header(m_format.get(), &options);
    av_dict_free(&options);
    if (result < 0)
        return fail(QStringLiteral("Cannot write the file header"), result);

    m_open = true;
    return true;
}

bool FfmpegEncoder::openVideo()
{
    const H264EncoderChain chain = m_settings.videoEncoders.isEmpty()
        ? H264EncoderChain::fromEnvironment() : H264EncoderChain(m_settings.videoEncoders);
    OpenedH264Encoder opened = chain.open(m_settings.video);
    if (!opened)
        return fail(QStringLiteral("No H.264 encoder could be opened"));
    m_video.codec = std::move(opened.context);
    m_videoEncoderName = opened.name;

    m_video.stream = avformat_new_stream(m_format.get(), nullptr);
    if (!m_video.stream)
        return fail(QStringLiteral("Cannot add the video stream"));
    m_video.stream->time_base = m_video.codec->time_base;
    m_video.stream->avg_frame_rate = m_video.codec->framerate;
    const int result = avcodec_parameters_from_context(m_video.stream->codecpar,
                                                       m_video.codec.get());
    if (result < 0)
        return fail(QStringLiteral("Cannot set up the video stream"), result);

    m_converted->format = m_video.codec->pix_fmt;
    m_converted->width = m_video.codec->width;
    m_converted->height = m_video.codec->height;
    if (av_frame_get_buffer(m_converted.get(), 0) < 0)
        return fail(QStringLiteral("Out of memory"));
    return true;
}

bool FfmpegEncoder::openAudio()
{
    const AVCodec *codec = avcodec_find_encoder(AV_CODEC_ID_AAC);
    if (!codec)
        return fail(QStringLiteral("No AAC encoder is built in"));
    m_audio.codec.reset(avcodec_alloc_context3(codec));
    if (!m_audio.codec)
        return fail(QStringLiteral("Out of memory"));
    AVCodecContext *context = m_audio.codec.get();
    context->sample_fmt = AV_SAMPLE_FMT_FLTP;
    context->sample_rate = m_settings.sampleRate;
    av_channel_layout_default(&context->ch_layout, m_settings.channels);
    context->bit_rate = 64000 * std::min(m_settings.channels, 6);
    context->time_base = AVRational{1, m_settings.sampleRate};
    context->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    int result = avcodec_open2(context, codec, nullptr);
    if (result < 0)
        return fail(QStringLiteral("Cannot open the AAC encoder"), result);

    m_audio.stream = avformat_new_stream(m_format.get(), nullptr);
    if (!m_audio.stream)
        return fail(QStringLiteral("Cannot add the audio stream"));
    m_audio.stream->time_base = context->time_base;
    result = avcodec_parameters_from_context(m_audio.stream->codecpar, context);
    if (result < 0)
        return fail(QStringLiteral("Cannot set up the audio stream"), result);

    m_fifo = av_audio_fifo_alloc(context->sample_fmt, context->ch_layout.nb_channels,
                                 std::max(context->frame_size, 1024) * 4);
    if (!m_fifo)
        return fail(QStringLiteral("Out of memory"));
    return true;
}

bool FfmpegEncoder::addVideoFrame(const uchar *bgra, int stride, qint64 timestampUs,
                                  qint64 durationUs)
{
    FramePtr frame = makeFrame();
    if (!frame)
        return fail(QStringLiteral("Out of memory"));
    // Borrows the caller's pixels: only swscale reads them, before this returns.
    frame->format = AV_PIX_FMT_BGRA;
    frame->width = m_sourceWidth;
    frame->height = m_sourceHeight;
    frame->data[0] = const_cast<uchar *>(bgra);
    frame->linesize[0] = stride;
    frame->color_range = AVCOL_RANGE_JPEG;
    return addVideoFrame(frame.get(), timestampUs, durationUs);
}

bool FfmpegEncoder::addVideoFrame(const AVFrame *frame, qint64 timestampUs, qint64 durationUs)
{
    if (!m_open)
        return fail(m_error.isEmpty() ? QStringLiteral("The encoder is not open") : m_error);

    AVCodecContext *codec = m_video.codec.get();
    qint64 pts = av_rescale_q(timestampUs, kMicroseconds, codec->time_base);
    if (m_lastVideoPts != AV_NOPTS_VALUE && pts <= m_lastVideoPts)
        pts = m_lastVideoPts + 1;
    m_lastVideoPts = pts;
    const qint64 duration = durationUs > 0
        ? av_rescale_q(durationUs, kMicroseconds, codec->time_base)
        : av_rescale_q(1, av_inv_q(codec->framerate), codec->time_base);

    FramePtr out;
    if (frame->format == codec->pix_fmt && frame->width == codec->width
        && frame->height == codec->height && frame->buf[0]) {
        out.reset(av_frame_clone(frame));
        if (!out)
            return fail(QStringLiteral("Out of memory"));
    } else {
        const int width = sourceExtent(frame->width, codec->width);
        const int height = sourceExtent(frame->height, codec->height);
        m_sws.reset(sws_getCachedContext(m_sws.release(), width, height,
                                         AVPixelFormat(frame->format), codec->width,
                                         codec->height, codec->pix_fmt, SWS_BILINEAR,
                                         nullptr, nullptr, nullptr));
        if (!m_sws)
            return fail(QStringLiteral("Cannot convert %1 frames")
                            .arg(QLatin1String(av_get_pix_fmt_name(AVPixelFormat(frame->format)))));
        const int sourceSpace = isRgb(frame->format) ? SWS_CS_ITU709
                                                     : swsColorspace(frame->colorspace);
        const int sourceFull = isRgb(frame->format) || frame->color_range == AVCOL_RANGE_JPEG;
        sws_setColorspaceDetails(m_sws.get(), sws_getCoefficients(sourceSpace), sourceFull,
                                 sws_getCoefficients(swsColorspace(codec->colorspace)),
                                 codec->color_range == AVCOL_RANGE_JPEG, 0, 1 << 16, 1 << 16);

        // The encoder may still hold the previous frame.
        if (av_frame_make_writable(m_converted.get()) < 0)
            return fail(QStringLiteral("Out of memory"));
        sws_scale(m_sws.get(), frame->data, frame->linesize, 0, height, m_converted->data,
                  m_converted->linesize);
        out.reset(av_frame_clone(m_converted.get()));
        if (!out)
            return fail(QStringLiteral("Out of memory"));
    }

    // A decoded keyframe would otherwise force one in the output.
    out->pict_type = AV_PICTURE_TYPE_NONE;
    out->pts = pts;
    out->duration = std::max<qint64>(1, duration);
    out->color_range = codec->color_range;
    out->colorspace = codec->colorspace;
    out->color_primaries = codec->color_primaries;
    out->color_trc = codec->color_trc;
    return encode(m_video, out.get());
}

bool FfmpegEncoder::addAudio(const float *interleaved, int frames, qint64 timestampUs)
{
    if (!m_audio.codec)
        return true;
    FramePtr frame = makeFrame();
    if (!frame)
        return fail(QStringLiteral("Out of memory"));
    // Borrowed like the BGRA pixels: swresample copies them out before this returns.
    frame->format = AV_SAMPLE_FMT_FLT;
    frame->sample_rate = m_settings.sampleRate;
    av_channel_layout_default(&frame->ch_layout, m_settings.channels);
    frame->nb_samples = frames;
    frame->data[0] = reinterpret_cast<uint8_t *>(const_cast<float *>(interleaved));
    frame->extended_data = frame->data;
    frame->linesize[0] = frames * m_settings.channels * int(sizeof(float));
    return addAudioFrame(frame.get(), timestampUs);
}

bool FfmpegEncoder::addAudioFrame(const AVFrame *frame, qint64 timestampUs)
{
    if (!m_audio.codec)
        return true;
    if (!m_open)
        return fail(m_error.isEmpty() ? QStringLiteral("The encoder is not open") : m_error);

    const int rate = m_audio.codec->sample_rate;
    const qint64 at = av_rescale_q(timestampUs, kMicroseconds, AVRational{1, rate});
    if (m_audioStart == AV_NOPTS_VALUE) {
        m_audioStart = at;
        m_audioNextPts = at;
    } else if (at - (m_audioStart + m_audioQueued) > rate / 10) {
        // A capture gap: fill it so audio stays in step with the video.
        if (!writeSilence(at - (m_audioStart + m_audioQueued)))
            return false;
    }
    return queueAudio(frame) && encodeQueuedAudio(false);
}

bool FfmpegEncoder::queueAudio(const AVFrame *frame)
{
    AVCodecContext *codec = m_audio.codec.get();
    if (!m_swr) {
        SwrContext *swr = nullptr;
        const int result = swr_alloc_set_opts2(&swr, &codec->ch_layout, codec->sample_fmt,
                                               codec->sample_rate,
                                               frame ? &frame->ch_layout : &codec->ch_layout,
                                               frame ? AVSampleFormat(frame->format)
                                                     : codec->sample_fmt,
                                               frame ? frame->sample_rate : codec->sample_rate,
                                               0, nullptr);
        m_swr.reset(swr);
        if (result < 0 || swr_init(m_swr.get()) < 0)
            return fail(QStringLiteral("Cannot convert the audio"), result);
    }

    av_frame_unref(m_resampled.get());
    m_resampled->format = codec->sample_fmt;
    m_resampled->sample_rate = codec->sample_rate;
    av_channel_layout_copy(&m_resampled->ch_layout, &codec->ch_layout);
    int result = swr_convert_frame(m_swr.get(), m_resampled.get(), frame);
    if (result == AVERROR_INPUT_CHANGED && frame) {
        swr_close(m_swr.get());
        result = swr_config_frame(m_swr.get(), m_resampled.get(), frame);
        if (result >= 0)
            result = swr_init(m_swr.get());
        if (result >= 0)
            result = swr_convert_frame(m_swr.get(), m_resampled.get(), frame);
    }
    if (result < 0)
        return fail(QStringLiteral("Cannot convert the audio"), result);
    if (m_resampled->nb_samples <= 0)
        return true;
    if (av_audio_fifo_write(m_fifo, reinterpret_cast<void **>(m_resampled->extended_data),
                            m_resampled->nb_samples) < m_resampled->nb_samples) {
        return fail(QStringLiteral("Out of memory"));
    }
    m_audioQueued += m_resampled->nb_samples;
    return true;
}

bool FfmpegEncoder::writeSilence(qint64 samples)
{
    AVCodecContext *codec = m_audio.codec.get();
    FramePtr silence = makeFrame();
    if (!silence)
        return fail(QStringLiteral("Out of memory"));
    silence->format = codec->sample_fmt;
    silence->nb_samples = int(std::min<qint64>(samples, codec->sample_rate * 10));
    av_channel_layout_copy(&silence->ch_layout, &codec->ch_layout);
    if (av_frame_get_buffer(silence.get(), 0) < 0)
        return fail(QStringLiteral("Out of memory"));
    av_samples_set_silence(silence->extended_data, 0, silence->nb_samples,
                           codec->ch_layout.nb_channels, codec->sample_fmt);
    if (av_audio_fifo_write(m_fifo, reinterpret_cast<void **>(silence->extended_data),
                            silence->nb_samples) < silence->nb_samples) {
        return fail(QStringLiteral("Out of memory"));
    }
    m_audioQueued += samples;   // counts the whole gap, even past the ten second cap
    return encodeQueuedAudio(false);
}

bool FfmpegEncoder::encodeQueuedAudio(bool flush)
{
    AVCodecContext *codec = m_audio.codec.get();
    const int frameSize = codec->frame_size > 0 ? codec->frame_size : 1024;
    while (av_audio_fifo_size(m_fifo) >= frameSize || (flush && av_audio_fifo_size(m_fifo) > 0)) {
        FramePtr frame = makeFrame();
        if (!frame)
            return fail(QStringLiteral("Out of memory"));
        frame->format = codec->sample_fmt;
        frame->sample_rate = codec->sample_rate;
        frame->nb_samples = std::min(frameSize, av_audio_fifo_size(m_fifo));
        av_channel_layout_copy(&frame->ch_layout, &codec->ch_layout);
        if (av_frame_get_buffer(frame.get(), 0) < 0)
            return fail(QStringLiteral("Out of memory"));
        if (av_audio_fifo_read(m_fifo, reinterpret_cast<void **>(frame->extended_data),
                               frame->nb_samples) < frame->nb_samples) {
            return fail(QStringLiteral("Cannot read queued audio"));
        }
        frame->pts = m_audioNextPts;
        m_audioNextPts += frame->nb_samples;
        if (!encode(m_audio, frame.get()))
            return false;
    }
    return true;
}

bool FfmpegEncoder::encode(Stream &stream, AVFrame *frame)
{
    for (;;) {
        const int result = avcodec_send_frame(stream.codec.get(), frame);
        if (result == AVERROR(EAGAIN)) {
            // Hardware encoders can refuse input until their output is collected.
            if (!drain(stream))
                return false;
            continue;
        }
        if (result < 0 && !(result == AVERROR_EOF && !frame))
            return fail(QStringLiteral("Encoding failed"), result);
        return drain(stream);
    }
}

bool FfmpegEncoder::drain(Stream &stream)
{
    for (;;) {
        int result = avcodec_receive_packet(stream.codec.get(), m_packet.get());
        if (result == AVERROR(EAGAIN) || result == AVERROR_EOF)
            return true;
        if (result < 0)
            return fail(QStringLiteral("Encoding failed"), result);
        av_packet_rescale_ts(m_packet.get(), stream.codec->time_base, stream.stream->time_base);
        m_packet->stream_index = stream.stream->index;
        result = av_interleaved_write_frame(m_format.get(), m_packet.get());
        if (result < 0)
            return fail(QStringLiteral("Cannot write %1").arg(m_settings.path), result);
    }
}

bool FfmpegEncoder::finish()
{
    if (!m_open)
        return fail(m_error.isEmpty() ? QStringLiteral("The encoder is not open") : m_error);

    if (m_audio.codec) {
        if (m_swr && !queueAudio(nullptr))
            return false;
        if (!encodeQueuedAudio(true) || !encode(m_audio, nullptr))
            return false;
    }
    if (!encode(m_video, nullptr))
        return false;

    const int result = av_write_trailer(m_format.get());
    m_open = false;
    m_format.reset();   // closes the file
    if (result < 0)
        return fail(QStringLiteral("Cannot finish %1").arg(m_settings.path), result);
    return true;
}

} // namespace Media::Ffmpeg
