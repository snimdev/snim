#ifndef MEDIA_FFMPEG_FFMPEGTRANSCODER_H
#define MEDIA_FFMPEG_FFMPEGTRANSCODER_H

#include "media/ffmpeg/FfmpegEncoder.h"
#include "media/ffmpeg/FfmpegSupport.h"

#include <QString>
#include <QtGlobal>

#include <atomic>

namespace Media::Ffmpeg {

/**
 * Re-encodes one media file into another (Template Method). run() fixes the skeleton:
 * demux, decode, hand every frame to the hooks, encode through FfmpegEncoder, mux.
 * Subclasses override the hooks to decide which frames survive and when they land;
 * trimming is the first, the render pipeline (edit lists, crop, blur, speed) the next.
 * run() blocks; cancel() may be called from any thread.
 */
class FfmpegTranscoder
{
public:
    enum class FrameAction {
        Keep,   // encode it (at the time the hook left it)
        Drop,   // skip it, keep going
        Stop    // skip it and every later frame of this stream
    };

    FfmpegTranscoder();
    virtual ~FfmpegTranscoder();

    FfmpegTranscoder(const FfmpegTranscoder &) = delete;
    FfmpegTranscoder &operator=(const FfmpegTranscoder &) = delete;

    // Writes output (MP4, or MOV by suffix). On failure or cancel nothing is left behind.
    [[nodiscard]] bool run(const QString &input, const QString &output);

    void cancel() { m_cancelled.store(true); }
    [[nodiscard]] bool wasCancelled() const { return m_cancelled.load(); }

    [[nodiscard]] QString errorString() const { return m_error; }
    [[nodiscard]] QString videoEncoderName() const { return m_videoEncoderName; }

protected:
    // Source time to seek to before decoding (it lands on the keyframe at or before it).
    [[nodiscard]] virtual qint64 seekUs() const { return 0; }

    // Last chance to change the output, once the source's streams are known.
    virtual void configure(FfmpegEncoderSettings &settings) { Q_UNUSED(settings) }

    // Times are microseconds from the start of the source; a hook may rewrite them to
    // place the frame on the output's timeline, or replace the frame itself.
    virtual FrameAction videoFrame(FramePtr &frame, qint64 &timeUs, qint64 &durationUs)
    {
        Q_UNUSED(frame) Q_UNUSED(timeUs) Q_UNUSED(durationUs)
        return FrameAction::Keep;
    }
    virtual FrameAction audioFrame(FramePtr &frame, qint64 &timeUs)
    {
        Q_UNUSED(frame) Q_UNUSED(timeUs)
        return FrameAction::Keep;
    }

    // After each video frame, with its source time. Runs on run()'s thread.
    virtual void progress(qint64 sourceUs) { Q_UNUSED(sourceUs) }

private:
    struct Decoder;

    bool transcode(const QString &input, const QString &output);
    bool decodePacket(Decoder &decoder, const AVPacket *packet, FfmpegEncoder &encoder);
    bool handleFrame(Decoder &decoder, FfmpegEncoder &encoder);
    bool fail(const QString &message, int error = 0);

    std::atomic_bool m_cancelled{false};
    QString m_error;
    QString m_videoEncoderName;
    qint64 m_startUs = 0;
    int m_videoFramesKept = 0;
};

} // namespace Media::Ffmpeg

#endif // MEDIA_FFMPEG_FFMPEGTRANSCODER_H
