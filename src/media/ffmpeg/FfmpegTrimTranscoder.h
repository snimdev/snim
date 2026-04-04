#ifndef MEDIA_FFMPEG_FFMPEGTRIMTRANSCODER_H
#define MEDIA_FFMPEG_FFMPEGTRIMTRANSCODER_H

#include "media/ffmpeg/FfmpegTranscoder.h"

#include <functional>

namespace Media::Ffmpeg {

/**
 * Cuts [inUs, outUs) out of a recording, frame- and sample-accurately: it re-encodes,
 * so the cut never snaps to a keyframe. The frame on screen at inUs starts the output
 * and audio is sliced to the sample.
 */
class FfmpegTrimTranscoder : public FfmpegTranscoder
{
public:
    FfmpegTrimTranscoder(qint64 inUs, qint64 outUs);

    // Called from run()'s thread with microseconds done out of the range.
    void setProgressCallback(std::function<void(qint64 doneUs, qint64 totalUs)> callback);

protected:
    [[nodiscard]] qint64 seekUs() const override { return m_inUs; }
    FrameAction videoFrame(FramePtr &frame, qint64 &timeUs, qint64 &durationUs) override;
    FrameAction audioFrame(FramePtr &frame, qint64 &timeUs) override;
    void progress(qint64 sourceUs) override;

private:
    qint64 m_inUs;
    qint64 m_outUs;
    std::function<void(qint64, qint64)> m_progress;
};

} // namespace Media::Ffmpeg

#endif // MEDIA_FFMPEG_FFMPEGTRIMTRANSCODER_H
