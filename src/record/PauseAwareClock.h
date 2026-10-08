#ifndef RECORDING_PAUSEAWARECLOCK_H
#define RECORDING_PAUSEAWARECLOCK_H

#include <QtGlobal>

#include <chrono>
#include <vector>

namespace Record {

/**
 * Maps capture timestamps onto a recording's output timeline, which starts at zero
 * and skips every paused span (no frozen segment). Timestamps are microseconds on one
 * monotonic clock shared by all sources; nowUs() reads it (QueryPerformanceCounter on
 * Windows, the clock Graphics Capture and WASAPI stamp their buffers with). Mapping
 * goes by capture time, so a buffer delivered late still lands on the right side of
 * a pause. Pure and not thread-safe: the owner serialises access.
 */
class PauseAwareClock
{
public:
    [[nodiscard]] static qint64 nowUs()
    {
        using namespace std::chrono;
        return duration_cast<microseconds>(steady_clock::now().time_since_epoch()).count();
    }

    void start(qint64 originUs)
    {
        m_origin = originUs;
        m_started = true;
        m_paused = false;
        m_spans.clear();
    }

    void reset() { m_started = false; m_paused = false; m_spans.clear(); }

    // Both are no-ops when they would not change the state.
    void pause(qint64 atUs)
    {
        if (!m_started || m_paused)
            return;
        m_paused = true;
        m_pauseStart = qMax(atUs, m_origin);
    }

    void resume(qint64 atUs)
    {
        if (!m_started || !m_paused)
            return;
        m_paused = false;
        if (atUs > m_pauseStart)
            m_spans.push_back({m_pauseStart, atUs});
    }

    [[nodiscard]] bool isStarted() const { return m_started; }

    // Output time for a capture timestamp, or -1 before the start or inside a pause.
    [[nodiscard]] qint64 toOutput(qint64 captureUs) const
    {
        if (!m_started || captureUs < m_origin)
            return -1;
        qint64 skipped = 0;
        for (const Span &span : m_spans) {
            if (captureUs >= span.end)
                skipped += span.end - span.begin;
            else if (captureUs >= span.begin)
                return -1;
        }
        if (m_paused && captureUs >= m_pauseStart)
            return -1;
        return captureUs - m_origin - skipped;
    }

    // Recorded time at nowUs; it holds still while paused.
    [[nodiscard]] qint64 elapsed(qint64 nowUs) const
    {
        if (!m_started)
            return 0;
        const qint64 at = m_paused ? m_pauseStart : nowUs;
        qint64 skipped = 0;
        for (const Span &span : m_spans) {
            if (at >= span.end)
                skipped += span.end - span.begin;
        }
        return qMax<qint64>(0, at - m_origin - skipped);
    }

private:
    struct Span {
        qint64 begin = 0;
        qint64 end = 0;
    };

    std::vector<Span> m_spans;   // closed pauses, oldest first
    qint64 m_origin = 0;
    qint64 m_pauseStart = 0;
    bool m_started = false;
    bool m_paused = false;
};

} // namespace Record

#endif // RECORDING_PAUSEAWARECLOCK_H
