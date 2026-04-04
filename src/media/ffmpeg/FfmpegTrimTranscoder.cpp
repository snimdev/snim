#include "media/ffmpeg/FfmpegTrimTranscoder.h"

extern "C" {
#include <libavutil/mathematics.h>
#include <libavutil/samplefmt.h>
}

#include <algorithm>
#include <utility>

namespace Media::Ffmpeg {

FfmpegTrimTranscoder::FfmpegTrimTranscoder(qint64 inUs, qint64 outUs)
    : m_inUs(inUs), m_outUs(outUs)
{
}

void FfmpegTrimTranscoder::setProgressCallback(std::function<void(qint64, qint64)> callback)
{
    m_progress = std::move(callback);
}

FfmpegTranscoder::FrameAction FfmpegTrimTranscoder::videoFrame(FramePtr &frame, qint64 &timeUs,
                                                               qint64 &durationUs)
{
    Q_UNUSED(frame)
    if (timeUs >= m_outUs)
        return FrameAction::Stop;
    const qint64 endUs = timeUs + durationUs;
    if (endUs <= m_inUs)
        return FrameAction::Drop;
    // Clipped to the range, so the output lasts exactly outUs - inUs.
    const qint64 startUs = std::max(timeUs, m_inUs);
    durationUs = std::min(endUs, m_outUs) - startUs;
    timeUs = startUs - m_inUs;
    return FrameAction::Keep;
}

FfmpegTranscoder::FrameAction FfmpegTrimTranscoder::audioFrame(FramePtr &frame, qint64 &timeUs)
{
    if (timeUs >= m_outUs)
        return FrameAction::Stop;
    const int rate = frame->sample_rate;
    if (rate <= 0)
        return FrameAction::Drop;
    const AVRational samples{1, rate};
    const qint64 endUs = timeUs + av_rescale_q(frame->nb_samples, samples, AVRational{1, 1000000});
    if (endUs <= m_inUs)
        return FrameAction::Drop;

    const int skip = timeUs < m_inUs
        ? int(av_rescale_q(m_inUs - timeUs, AVRational{1, 1000000}, samples)) : 0;
    const int end = endUs > m_outUs
        ? int(av_rescale_q(m_outUs - timeUs, AVRational{1, 1000000}, samples))
        : frame->nb_samples;
    const int keep = std::clamp(end, 0, frame->nb_samples) - std::min(skip, frame->nb_samples);
    if (keep <= 0)
        return FrameAction::Drop;

    if (skip > 0 || keep < frame->nb_samples) {
        FramePtr slice = makeFrame();
        if (!slice)
            return FrameAction::Drop;
        slice->format = frame->format;
        slice->sample_rate = rate;
        slice->nb_samples = keep;
        av_channel_layout_copy(&slice->ch_layout, &frame->ch_layout);
        if (av_frame_get_buffer(slice.get(), 0) < 0)
            return FrameAction::Drop;
        av_samples_copy(slice->extended_data, frame->extended_data, 0, skip, keep,
                        frame->ch_layout.nb_channels, AVSampleFormat(frame->format));
        frame = std::move(slice);
    }
    timeUs = std::max(timeUs, m_inUs) - m_inUs;
    return FrameAction::Keep;
}

void FfmpegTrimTranscoder::progress(qint64 sourceUs)
{
    if (!m_progress)
        return;
    const qint64 totalUs = m_outUs - m_inUs;
    m_progress(std::clamp(sourceUs - m_inUs, qint64(0), totalUs), totalUs);
}

} // namespace Media::Ffmpeg
