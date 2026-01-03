#include <QtTest>
#include <QSignalSpy>

#include "editor/video/GifParams.h"
#include "editor/video/VideoExporter.h"
#include "editor/video/StubVideoExporter.h"

using namespace Editor::Video;

// Pure GIF-export math (frame planning, delay, downscale, filename) plus the
// cross-platform exporter contract for GIF. No AVFoundation / ImageIO here — the
// actual encode is verified live; this locks the logic that feeds it.
class tst_GifParams : public QObject
{
    Q_OBJECT

private slots:
    void planFrames_basic()
    {
        // 2 s span at 10 fps -> 20 frames, first at inMs, evenly spaced, last < outMs.
        const QVector<qint64> t = planGifFrames(1000, 3000, 10);
        QCOMPARE(t.size(), 20);
        QCOMPARE(t.first(), qint64(1000));
        QVERIFY(t.last() < 3000);
        QCOMPARE(t.at(1) - t.at(0), qint64(100));   // 1/10 s step
    }

    void planFrames_flooring()
    {
        // 1.25 s at 10 fps floors to 12 frames (not 13).
        QCOMPARE(planGifFrames(0, 1250, 10).size(), 12);
    }

    void planFrames_subSecondAndDegenerate()
    {
        QCOMPARE(planGifFrames(0, 150, 10).size(), 1);   // <1 frame worth -> floor to 1
        QCOMPARE(planGifFrames(500, 500, 10).size(), 1); // empty span -> single frame
        const QVector<qint64> z = planGifFrames(500, 500, 10);
        QCOMPARE(z.first(), qint64(500));                // sits at inMs, no out-1 underflow
    }

    void frameDelay()
    {
        QCOMPARE(gifFrameDelaySec(10), 0.1);
        QCOMPARE(gifFrameDelaySec(0), 1.0);              // clamped to >=1 fps
    }

    void scaledSize()
    {
        QCOMPARE(gifScaledSize(QSize(1920, 1080), 600), QSize(600, 338));  // longest edge capped
        QCOMPARE(gifScaledSize(QSize(1080, 1920), 600), QSize(338, 600));  // portrait
        QCOMPARE(gifScaledSize(QSize(400, 300), 600), QSize(400, 300));    // no upscale
        QCOMPARE(gifScaledSize(QSize(400, 300), 0), QSize(400, 300));      // 0 = passthrough
        QCOMPARE(gifScaledSize(QSize(), 600), QSize());                    // empty passthrough
    }

    void clamping()
    {
        const GifParams g = GifParams{99, -5, -1}.clamped();
        QCOMPARE(g.fps, 50);
        QCOMPARE(g.maxWidth, 0);
        QCOMPARE(g.loopCount, 0);
    }

    void fileName()
    {
        QCOMPARE(gifFileNameFor("Niceshot_recording_2026.mp4"), QStringLiteral("Niceshot_recording_2026.gif"));
        QCOMPARE(gifFileNameFor("/tmp/clip.mov"), QStringLiteral("clip.gif"));
        QCOMPARE(gifFileNameFor(""), QStringLiteral("recording.gif"));
    }

    void stubReportsGifUnsupported()
    {
        // The stub backend must resolve a GIF request via failed() (async), so the
        // editor never hangs on platforms without an encoder. Construct it directly
        // so the test is platform-independent (create() returns Mac's on macOS).
        StubVideoExporter stub;
        QVERIFY(!stub.isAvailable());
        QSignalSpy failedSpy(&stub, &VideoExporter::failed);
        stub.toGif("/in.mp4", "/out.gif", 0, 1000, GifParams{});
        QVERIFY(failedSpy.wait(1000));
        QCOMPARE(failedSpy.count(), 1);
    }
};

QTEST_MAIN(tst_GifParams)
#include "tst_gifparams.moc"
