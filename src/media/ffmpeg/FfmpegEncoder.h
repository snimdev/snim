#ifndef MEDIA_FFMPEG_FFMPEGENCODER_H
#define MEDIA_FFMPEG_FFMPEGENCODER_H

#include "media/ffmpeg/FfmpegSupport.h"
#include "media/ffmpeg/H264EncoderChain.h"

#include <QString>
#include <QStringList>
#include <QtGlobal>

struct AVAudioFifo;

namespace Media::Ffmpeg {

struct FfmpegEncoderSettings {
    QString path;                 // .mov writes QuickTime, anything else MP4
    H264EncoderSettings video;    // odd sizes lose their last column or row
    QStringList videoEncoders;    // chain order; empty uses H264EncoderChain::fromEnvironment()
    int sampleRate = 48000;
    int channels = 2;             // 0 writes no audio track
};

/**
 * Encodes frames into an H.264 + AAC MP4 or MOV with the index up front (+faststart).
 * Takes BGRA pixels and interleaved float PCM (a recorder's output) or any decoded
 * AVFrame (a transcode's). Timestamps are microseconds on the caller's clock; video
 * PTS are kept strictly increasing and audio runs on from its first timestamp by
 * sample count. Not thread-safe: one thread drives an instance from open() to finish().
 */
class FfmpegEncoder
{
public:
    FfmpegEncoder();
    ~FfmpegEncoder();

    FfmpegEncoder(const FfmpegEncoder &) = delete;
    FfmpegEncoder &operator=(const FfmpegEncoder &) = delete;

    [[nodiscard]] bool open(const FfmpegEncoderSettings &settings);

    // BGRA at the settings' size. 0 duration uses the nominal frame rate.
    [[nodiscard]] bool addVideoFrame(const uchar *bgra, int stride, qint64 timestampUs,
                                     qint64 durationUs = 0);
    [[nodiscard]] bool addVideoFrame(const AVFrame *frame, qint64 timestampUs,
                                     qint64 durationUs = 0);

    // frames counts samples per channel.
    [[nodiscard]] bool addAudio(const float *interleaved, int frames, qint64 timestampUs);
    [[nodiscard]] bool addAudioFrame(const AVFrame *frame, qint64 timestampUs);

    // Drains the encoders and writes the index. The file is complete only if this succeeds.
    [[nodiscard]] bool finish();

    [[nodiscard]] QString videoEncoderName() const { return m_videoEncoderName; }
    [[nodiscard]] bool hasAudio() const { return m_audio.codec != nullptr; }
    [[nodiscard]] QString errorString() const { return m_error; }

private:
    struct Stream {
        CodecContextPtr codec;
        AVStream *stream = nullptr;   // owned by the format context
    };

    bool openVideo();
    bool openAudio();
    bool encode(Stream &stream, AVFrame *frame);
    bool drain(Stream &stream);
    bool queueAudio(const AVFrame *frame);
    bool writeSilence(qint64 samples);
    bool encodeQueuedAudio(bool flush);
    bool fail(const QString &context, int error = 0);

    FfmpegEncoderSettings m_settings;   // video size rounded down to even
    int m_sourceWidth = 0;
    int m_sourceHeight = 0;
    OutputFormatPtr m_format;
    Stream m_video;
    Stream m_audio;
    QString m_videoEncoderName;
    QString m_error;
    bool m_open = false;

    SwsContextPtr m_sws;
    FramePtr m_converted;
    qint64 m_lastVideoPts = AV_NOPTS_VALUE;

    SwrContextPtr m_swr;
    FramePtr m_resampled;
    AVAudioFifo *m_fifo = nullptr;
    PacketPtr m_packet;
    qint64 m_audioStart = AV_NOPTS_VALUE;   // in samples
    qint64 m_audioQueued = 0;               // samples queued since m_audioStart
    qint64 m_audioNextPts = 0;
};

} // namespace Media::Ffmpeg

#endif // MEDIA_FFMPEG_FFMPEGENCODER_H
