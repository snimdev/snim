#include "media/ffmpeg/FfmpegTranscoder.h"

extern "C" {
#include <libavutil/mathematics.h>
}

#include <QFile>

namespace Media::Ffmpeg {

namespace {

constexpr AVRational kMicroseconds{1, 1000000};

} // namespace

struct FfmpegTranscoder::Decoder {
    AVStream *stream = nullptr;   // owned by the input
    CodecContextPtr codec;
    FramePtr frame;
    bool done = false;
    bool video = false;
};

FfmpegTranscoder::FfmpegTranscoder() = default;
FfmpegTranscoder::~FfmpegTranscoder() = default;

bool FfmpegTranscoder::fail(const QString &message, int error)
{
    m_error = error ? QStringLiteral("%1: %2").arg(message, Ffmpeg::errorString(error)) : message;
    return false;
}

bool FfmpegTranscoder::run(const QString &input, const QString &output)
{
    m_error.clear();
    m_videoEncoderName.clear();
    m_videoFramesKept = 0;
    QFile::remove(output);
    const bool ok = transcode(input, output) && !wasCancelled();
    if (!ok) {
        if (wasCancelled())
            m_error = QStringLiteral("Cancelled");
        QFile::remove(output);
    }
    return ok;
}

bool FfmpegTranscoder::transcode(const QString &input, const QString &output)
{
    const QByteArray path = input.toUtf8();
    AVFormatContext *rawInput = nullptr;
    int result = avformat_open_input(&rawInput, path.constData(), nullptr, nullptr);
    InputFormatPtr format(rawInput);
    if (result < 0)
        return fail(QStringLiteral("Cannot open %1").arg(input), result);
    result = avformat_find_stream_info(format.get(), nullptr);
    if (result < 0)
        return fail(QStringLiteral("Cannot read %1").arg(input), result);

    Decoder video;
    Decoder audio;
    video.video = true;
    for (Decoder *decoder : {&video, &audio}) {
        const AVMediaType type = decoder->video ? AVMEDIA_TYPE_VIDEO : AVMEDIA_TYPE_AUDIO;
        const AVCodec *codec = nullptr;
        const int index = av_find_best_stream(format.get(), type, -1, -1, &codec, 0);
        if (index < 0 || !codec)
            continue;
        decoder->stream = format->streams[index];
        decoder->codec.reset(avcodec_alloc_context3(codec));
        decoder->frame = makeFrame();
        if (!decoder->codec || !decoder->frame)
            return fail(QStringLiteral("Out of memory"));
        avcodec_parameters_to_context(decoder->codec.get(), decoder->stream->codecpar);
        decoder->codec->pkt_timebase = decoder->stream->time_base;
        decoder->codec->thread_count = 0;
        result = avcodec_open2(decoder->codec.get(), codec, nullptr);
        if (result < 0) {
            if (decoder->video)
                return fail(QStringLiteral("Cannot decode the video"), result);
            decoder->stream = nullptr;   // carry on without the audio
            decoder->codec.reset();
        }
    }
    if (!video.stream)
        return fail(QStringLiteral("The recording has no video track"));
    audio.done = !audio.stream;

    m_startUs = format->start_time != AV_NOPTS_VALUE ? format->start_time : 0;

    const AVCodecParameters *source = video.stream->codecpar;
    FfmpegEncoderSettings settings;
    settings.path = output;
    settings.video.width = source->width;
    settings.video.height = source->height;
    const AVRational rate = av_guess_frame_rate(format.get(), video.stream, nullptr);
    if (rate.num > 0 && rate.den > 0)
        settings.video.frameRate = rate;
    settings.video.tuning = H264EncoderSettings::Offline;
    settings.video.colorRange = source->color_range == AVCOL_RANGE_JPEG ? AVCOL_RANGE_JPEG
                                                                          : AVCOL_RANGE_MPEG;
    settings.video.colorPrimaries = source->color_primaries;
    settings.video.colorTrc = source->color_trc;
    settings.video.colorSpace = source->color_space;
    if (audio.stream) {
        settings.sampleRate = audio.codec->sample_rate;
        settings.channels = audio.codec->ch_layout.nb_channels;
    } else {
        settings.channels = 0;
    }
    configure(settings);

    const qint64 seek = seekUs();
    if (seek > 0) {
        const qint64 target = m_startUs + seek;   // AV_TIME_BASE is microseconds too
        result = avformat_seek_file(format.get(), -1, INT64_MIN, target, target, 0);
        if (result < 0)
            return fail(QStringLiteral("Cannot seek in %1").arg(input), result);
    }

    FfmpegEncoder encoder;
    if (!encoder.open(settings))
        return fail(encoder.errorString());
    m_videoEncoderName = encoder.videoEncoderName();

    PacketPtr packet = makePacket();
    if (!packet)
        return fail(QStringLiteral("Out of memory"));
    while (!(video.done && audio.done)) {
        if (wasCancelled())
            return false;
        result = av_read_frame(format.get(), packet.get());
        if (result == AVERROR_EOF)
            break;
        if (result < 0)
            return fail(QStringLiteral("Cannot read %1").arg(input), result);
        Decoder *decoder = nullptr;
        if (packet->stream_index == video.stream->index)
            decoder = &video;
        else if (audio.stream && packet->stream_index == audio.stream->index)
            decoder = &audio;
        const bool ok = !decoder || decoder->done || decodePacket(*decoder, packet.get(), encoder);
        av_packet_unref(packet.get());
        if (!ok)
            return false;
    }

    // Flushes the decoders' held frames.
    for (Decoder *decoder : {&video, &audio}) {
        if (decoder->codec && !decoder->done && !decodePacket(*decoder, nullptr, encoder))
            return false;
    }
    if (wasCancelled())
        return false;
    if (m_videoFramesKept == 0)
        return fail(QStringLiteral("The selected range holds no video frames"));
    if (!encoder.finish())
        return fail(encoder.errorString());
    return true;
}

bool FfmpegTranscoder::decodePacket(Decoder &decoder, const AVPacket *packet,
                                    FfmpegEncoder &encoder)
{
    int result = avcodec_send_packet(decoder.codec.get(), packet);
    if (result < 0 && result != AVERROR_EOF) {
        // A damaged packet costs one frame, not the whole export.
        if (packet && result == AVERROR_INVALIDDATA)
            return true;
        return fail(QStringLiteral("Decoding failed"), result);
    }
    while (!decoder.done) {
        result = avcodec_receive_frame(decoder.codec.get(), decoder.frame.get());
        if (result == AVERROR(EAGAIN) || result == AVERROR_EOF)
            return true;
        if (result < 0)
            return fail(QStringLiteral("Decoding failed"), result);
        const bool ok = handleFrame(decoder, encoder);
        av_frame_unref(decoder.frame.get());
        if (!ok)
            return false;
    }
    return true;
}

bool FfmpegTranscoder::handleFrame(Decoder &decoder, FfmpegEncoder &encoder)
{
    const AVRational timeBase = decoder.stream->time_base;
    const qint64 pts = decoder.frame->best_effort_timestamp;
    if (pts == AV_NOPTS_VALUE)
        return true;
    qint64 timeUs = av_rescale_q(pts, timeBase, kMicroseconds) - m_startUs;

    // The hooks may swap the frame, so they get a reference of their own.
    FramePtr frame(av_frame_clone(decoder.frame.get()));
    if (!frame)
        return fail(QStringLiteral("Out of memory"));

    FrameAction action = FrameAction::Keep;
    if (decoder.video) {
        const qint64 sourceUs = timeUs;
        qint64 durationUs = frame->duration > 0
            ? av_rescale_q(frame->duration, timeBase, kMicroseconds)
            : av_rescale_q(1, av_inv_q(decoder.codec->framerate.num > 0
                                           ? decoder.codec->framerate : AVRational{30, 1}),
                           kMicroseconds);
        action = videoFrame(frame, timeUs, durationUs);
        if (action == FrameAction::Keep) {
            if (!encoder.addVideoFrame(frame.get(), timeUs, durationUs))
                return fail(encoder.errorString());
            ++m_videoFramesKept;
        }
        progress(sourceUs);
    } else {
        action = audioFrame(frame, timeUs);
        if (action == FrameAction::Keep && !encoder.addAudioFrame(frame.get(), timeUs))
            return fail(encoder.errorString());
    }
    if (action == FrameAction::Stop)
        decoder.done = true;
    return true;
}

} // namespace Media::Ffmpeg
