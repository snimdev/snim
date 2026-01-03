#ifndef RECORDING_PCMMIXBUFFER_H
#define RECORDING_PCMMIXBUFFER_H

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <vector>

namespace Recording {

/**
 * Maps one live PCM source onto the shared mix timeline. Each buffer's PTS *suggests*
 * a frame position, but successive buffers are placed back-to-back (sample-accurate)
 * unless the PTS drifts more than resyncThreshold frames from the running position —
 * that is a real discontinuity (pause/resume, device hiccup) and the clock re-anchors.
 * This keeps per-buffer PTS rounding / resampler jitter from punching audible clicks
 * into the mix.
 */
class MixSourceClock
{
public:
    explicit MixSourceClock(std::int64_t resyncThresholdFrames = 0)
        : m_threshold(resyncThresholdFrames) {}

    /// Where should a buffer of `frames` frames whose PTS says `ptsFrame` land?
    [[nodiscard]] std::int64_t place(std::int64_t ptsFrame, std::int64_t frames)
    {
        if (!m_valid || std::llabs(ptsFrame - m_next) > m_threshold) {
            m_next = ptsFrame;          // first buffer or a genuine gap: trust the PTS
            m_valid = true;
        }
        const std::int64_t at = m_next;
        m_next += frames;
        return at;
    }

    void reset() { m_valid = false; }

private:
    std::int64_t m_threshold = 0;
    std::int64_t m_next = 0;
    bool m_valid = false;
};

/**
 * Sums interleaved float PCM from multiple live sources on one sample timeline
 * (frame 0 = recording start) and hands the mixed result downstream once a region is
 * "settled": holdbackFrames behind the newest audio seen, so a slightly-later buffer
 * from the other source can still land before its region is emitted. Audio arriving
 * for an already-emitted region is trimmed away; gaps stay silent (zeros); the
 * emitted mix is hard-clipped to [-1, 1].
 *
 * Single-threaded by design — the macOS recorder feeds and flushes it from one serial
 * capture queue. Pure C++ so the mixing math is unit-testable off-platform.
 */
class PcmMixBuffer
{
public:
    PcmMixBuffer(int channels, std::int64_t holdbackFrames)
        : m_channels(channels), m_holdback(holdbackFrames) {}

    /// Sum `frames` frames of interleaved PCM in at absolute frame index `startFrame`.
    void mix(std::int64_t startFrame, const float *interleaved, std::int64_t frames)
    {
        if (frames <= 0 || startFrame + frames <= m_start)
            return;                                    // entirely before the watermark
        if (startFrame < m_start) {                    // straddles it: keep the tail
            const std::int64_t skip = m_start - startFrame;
            interleaved += skip * m_channels;
            frames -= skip;
            startFrame = m_start;
        }
        const auto needed = std::size_t((startFrame + frames - m_start) * m_channels);
        if (m_buf.size() < needed)
            m_buf.resize(needed, 0.0f);                // zero fill: silence in gaps
        float *dst = m_buf.data() + (startFrame - m_start) * m_channels;
        const std::int64_t n = frames * m_channels;
        for (std::int64_t i = 0; i < n; ++i)
            dst[i] += interleaved[i];
        m_maxEnd = std::max(m_maxEnd, startFrame + frames);
    }

    /// Frames flush() would emit: everything at least holdback behind the newest audio.
    [[nodiscard]] std::int64_t flushableFrames(bool drain = false) const
    {
        return std::max<std::int64_t>(0, (drain ? m_maxEnd : m_maxEnd - m_holdback) - m_start);
    }

    /// Emit the settled region (all remaining audio when draining) as one chunk via
    /// sink(startFrame, interleavedData, frames), then advance the watermark past it.
    /// No-op while nothing is settled — deferring a flush never loses audio.
    template <typename Sink>
    void flush(Sink &&sink, bool drain = false)
    {
        const std::int64_t frames = flushableFrames(drain);
        if (frames <= 0)
            return;
        const std::int64_t n = frames * m_channels;
        for (std::int64_t i = 0; i < n; ++i)
            m_buf[i] = std::clamp(m_buf[i], -1.0f, 1.0f);
        sink(m_start, m_buf.data(), frames);
        m_buf.erase(m_buf.begin(), m_buf.begin() + n);
        m_start += frames;
    }

    [[nodiscard]] int channels() const { return m_channels; }

private:
    int m_channels;
    std::int64_t m_holdback;
    std::vector<float> m_buf;     // interleaved; covers [m_start, m_start + size/channels)
    std::int64_t m_start = 0;     // absolute frame index of m_buf[0]; the emit watermark
    std::int64_t m_maxEnd = 0;    // newest frame end any source has written
};

} // namespace Recording

#endif // RECORDING_PCMMIXBUFFER_H
