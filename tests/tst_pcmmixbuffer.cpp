#include <QtTest>

#include "record/PcmMixBuffer.h"

#include <vector>

using namespace Record;

// The manual-mix math behind the macOS recorder's mic + system-audio track:
// MixSourceClock places live buffers on the shared timeline, PcmMixBuffer sums
// them and emits settled regions. Pure C++, so it is tested off-platform here.
class tst_PcmMixBuffer : public QObject
{
    Q_OBJECT

    struct Chunk {
        std::int64_t start = 0;
        std::vector<float> data;
    };

    // Sink that records every emitted chunk.
    static auto collectInto(std::vector<Chunk> &out)
    {
        return [&out](std::int64_t start, const float *data, std::int64_t frames) {
            Chunk c;
            c.start = start;
            c.data.assign(data, data + frames * 2);   // tests use stereo unless noted
            out.push_back(std::move(c));
        };
    }

private slots:
    void clockAnchorsJitterAndResync()
    {
        MixSourceClock clock(50);
        QCOMPARE(clock.place(1000, 100), qint64(1000));   // first buffer: trust the PTS
        QCOMPARE(clock.place(1130, 100), qint64(1100));   // 30 frames jitter: contiguous
        QCOMPARE(clock.place(1500, 100), qint64(1500));   // 300 frames: re-anchor
        clock.reset();
        QCOMPARE(clock.place(90, 10), qint64(90));        // reset: trust the PTS again
        QCOMPARE(clock.place(100, 10), qint64(100));      // exactly contiguous
    }

    void overlappingSourcesSumAndClip()
    {
        PcmMixBuffer mix(2, 0);
        const float a[] = { 0.5f, -0.5f, 0.8f, -0.8f };   // 2 stereo frames
        const float b[] = { 0.3f, -0.3f, 0.4f, -0.4f };
        mix.mix(0, a, 2);
        mix.mix(0, b, 2);

        std::vector<Chunk> chunks;
        mix.flush(collectInto(chunks));
        QCOMPARE(chunks.size(), size_t(1));
        QCOMPARE(chunks[0].start, qint64(0));
        const std::vector<float> expected = { 0.8f, -0.8f, 1.0f, -1.0f };   // 1.2 clips
        QCOMPARE(chunks[0].data, expected);
    }

    void holdbackSettlesThenDrains()
    {
        PcmMixBuffer mix(2, 100);
        const std::vector<float> pcm(150 * 2, 0.1f);
        mix.mix(0, pcm.data(), 150);
        QCOMPARE(mix.flushableFrames(), qint64(50));      // only 100 behind the newest

        std::vector<Chunk> chunks;
        mix.flush(collectInto(chunks));
        QCOMPARE(chunks.size(), size_t(1));
        QCOMPARE(chunks[0].start, qint64(0));
        QCOMPARE(chunks[0].data.size(), size_t(50 * 2));
        QCOMPARE(mix.flushableFrames(), qint64(0));       // the rest is still held back

        mix.mix(150, pcm.data(), 30);                     // newest moves to 180
        QCOMPARE(mix.flushableFrames(), qint64(30));

        mix.flush(collectInto(chunks), /*drain=*/true);   // stop: emit everything left
        QCOMPARE(chunks.size(), size_t(2));
        QCOMPARE(chunks[1].start, qint64(50));
        QCOMPARE(chunks[1].data.size(), size_t(130 * 2));
        QCOMPARE(mix.flushableFrames(true), qint64(0));
    }

    void lateAudioForEmittedRegionIsTrimmed()
    {
        PcmMixBuffer mix(2, 0);
        const std::vector<float> early(10 * 2, 0.25f);
        mix.mix(0, early.data(), 10);
        std::vector<Chunk> chunks;
        mix.flush(collectInto(chunks));                   // watermark now at frame 10

        const std::vector<float> late(10 * 2, 0.5f);
        mix.mix(5, late.data(), 10);                      // 5 late frames trimmed away
        mix.flush(collectInto(chunks));
        QCOMPARE(chunks.size(), size_t(2));
        QCOMPARE(chunks[1].start, qint64(10));
        QCOMPARE(chunks[1].data, std::vector<float>(5 * 2, 0.5f));

        mix.mix(2, late.data(), 3);                       // entirely late: dropped
        QCOMPARE(mix.flushableFrames(true), qint64(0));
    }

    void gapsBetweenBuffersStaySilent()
    {
        PcmMixBuffer mix(1, 0);                           // mono keeps the math obvious
        const float pcm[] = { 0.5f, 0.5f };
        mix.mix(0, pcm, 2);
        mix.mix(4, pcm, 2);                               // frames 2..3 never written

        std::vector<float> out;
        std::int64_t start = -1;
        mix.flush([&](std::int64_t s, const float *data, std::int64_t frames) {
            start = s;
            out.assign(data, data + frames);
        });
        QCOMPARE(start, qint64(0));
        const std::vector<float> expected = { 0.5f, 0.5f, 0.0f, 0.0f, 0.5f, 0.5f };
        QCOMPARE(out, expected);
    }

    void deferredFlushLosesNothing()
    {
        // The recorder skips a flush while the writer input is busy; the audio must
        // still be there, in one piece, on the next flush.
        PcmMixBuffer mix(2, 0);
        const std::vector<float> pcm(20 * 2, 0.2f);
        mix.mix(0, pcm.data(), 20);
        // (no flush here - "input busy")
        mix.mix(20, pcm.data(), 20);

        std::vector<Chunk> chunks;
        mix.flush(collectInto(chunks));
        QCOMPARE(chunks.size(), size_t(1));
        QCOMPARE(chunks[0].start, qint64(0));
        QCOMPARE(chunks[0].data, std::vector<float>(40 * 2, 0.2f));
    }
};

QTEST_MAIN(tst_PcmMixBuffer)
#include "tst_pcmmixbuffer.moc"
