#include <QtTest>
#include <QElapsedTimer>
#include <QFile>
#include <QImage>
#include <QScopeGuard>
#include <QSignalSpy>
#include <QTemporaryDir>

#include "editor/video/AnimationParams.h"
#include "editor/video/VideoFrameGrabber.h"

#include "VideoBackendProbe.h"

using namespace Editor::Video;

namespace {

// A checked-in 3 s, 30 fps, 128x96 H.264 clip of GStreamer's moving-ball test pattern.
// It has to be a real file: the Qt backend only produces frames during playback, so the
// grabber cannot be exercised against a stub source. Regenerate with:
//   gst-launch-1.0 videotestsrc pattern=ball num-buffers=90 \
//     ! video/x-raw,framerate=30/1,width=128,height=96 ! videoconvert \
//     ! video/x-raw,format=I420 \
//     ! x264enc speed-preset=ultrafast tune=zerolatency key-int-max=15 bitrate=60 \
//     ! h264parse ! mp4mux ! filesink location=tests/data/clip.mp4
QString clipPath()
{
    return QFINDTESTDATA("data/clip.mp4");
}

QString firstFailure(const QSignalSpy &failed)
{
    return failed.isEmpty() ? QString() : failed.first().at(0).toString();
}

// What the grabber delivered over one run.
struct Collected {
    QVector<QImage> frames;
    QVector<qint64> times;
    qint64 firstFrameMs = -1;   // wall time from start() to the first delivery
};

Collected collect(VideoFrameGrabber *grabber, QSignalSpy *failed, QSignalSpy *finished,
                  const QString &path, qint64 inMs, qint64 outMs, int fps, int maxWidth)
{
    Collected got;
    QElapsedTimer elapsed;
    QObject::connect(grabber, &VideoFrameGrabber::frameReady,
                     [&got, &elapsed](const QImage &frame, qint64 at) {
                         if (got.firstFrameMs < 0)
                             got.firstFrameMs = elapsed.elapsed();
                         got.frames.append(frame);
                         got.times.append(at);
                     });
    elapsed.start();
    grabber->start(path, inMs, outMs, fps, maxWidth);
    finished->wait(20000);
    return got;
}

} // namespace

// The frame grabber against a real recording. Slower than the rest of the suite (it
// decodes video), and the only place the Qt Multimedia path is exercised at all.
class tst_VideoFrameGrabber : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        if (!TestSupport::videoFramesAvailable())
            QSKIP("this platform delivers no video frames, so the grabber cannot be exercised");
    }

    void deliversOneFramePerSample()
    {
        const QString clip = clipPath();
        QVERIFY2(!clip.isEmpty(), "the clip.mp4 fixture is missing");

        auto grabber = VideoFrameGrabber::create();
        QSignalSpy failed(grabber.get(), &VideoFrameGrabber::failed);
        QSignalSpy finished(grabber.get(), &VideoFrameGrabber::finished);

        const Collected got = collect(grabber.get(), &failed, &finished, clip, 500, 2000, 10, 0);

        QVERIFY2(finished.count() == 1, qPrintable(firstFailure(failed)));
        QCOMPARE(failed.count(), 0);
        // 500..2000 ms at 10 fps: 15 samples, evenly spaced, each at its planned time.
        QCOMPARE(got.times.size(), 15);
        QCOMPARE(got.times.first(), qint64(500));
        QCOMPARE(got.times.last(), qint64(1900));
        QCOMPARE(got.frames.first().size(), QSize(128, 96));
        // The frames must actually differ: re-delivering a single decoded frame would
        // pass every assertion above, and the moving ball rules that out.
        QVERIFY(got.frames.first() != got.frames.last());
    }

    void scalesToMaxWidth()
    {
        const QString clip = clipPath();
        QVERIFY2(!clip.isEmpty(), "the clip.mp4 fixture is missing");

        auto grabber = VideoFrameGrabber::create();
        QSignalSpy failed(grabber.get(), &VideoFrameGrabber::failed);
        QSignalSpy finished(grabber.get(), &VideoFrameGrabber::finished);

        const Collected got = collect(grabber.get(), &failed, &finished, clip, 0, 1000, 10, 64);

        QVERIFY2(finished.count() == 1, qPrintable(firstFailure(failed)));
        QVERIFY(!got.frames.isEmpty());
        QCOMPARE(got.frames.first().size(), QSize(64, 48));   // 128x96 capped at 64
    }

    void cancelStopsDelivery()
    {
        const QString clip = clipPath();
        QVERIFY2(!clip.isEmpty(), "the clip.mp4 fixture is missing");

        auto grabber = VideoFrameGrabber::create();
        Collected got;
        QObject::connect(grabber.get(), &VideoFrameGrabber::frameReady,
                         [&got](const QImage &frame, qint64 at) {
                             got.frames.append(frame);
                             got.times.append(at);
                         });
        QSignalSpy failed(grabber.get(), &VideoFrameGrabber::failed);
        QSignalSpy finished(grabber.get(), &VideoFrameGrabber::finished);

        grabber->start(clip, 0, 3000, 10, 0);
        QTRY_VERIFY_WITH_TIMEOUT(got.frames.size() >= 2, 15000);

        grabber->cancel();
        const int seen = got.frames.size();
        QTest::qWait(150);
        QCOMPARE(got.frames.size(), seen);   // nothing arrives after cancel()
        QCOMPARE(finished.count(), 0);       // and a cancel is not a result
        QCOMPARE(failed.count(), 0);
    }

    void pauseHoldsDeliveryAndResumeFinishesTheRun()
    {
        const QString clip = clipPath();
        QVERIFY2(!clip.isEmpty(), "the clip.mp4 fixture is missing");

        // The exporter throttles the grabber while the encoder lags, so a paused run has
        // to stop delivering and still complete once it is let go.
        auto grabber = VideoFrameGrabber::create();
        Collected got;
        QObject::connect(grabber.get(), &VideoFrameGrabber::frameReady,
                         [&got](const QImage &frame, qint64 at) {
                             got.frames.append(frame);
                             got.times.append(at);
                         });
        QSignalSpy failed(grabber.get(), &VideoFrameGrabber::failed);
        QSignalSpy finished(grabber.get(), &VideoFrameGrabber::finished);

        grabber->start(clip, 0, 1500, 10, 0);
        QTRY_VERIFY_WITH_TIMEOUT(got.frames.size() >= 2, 15000);

        grabber->pause();
        grabber->pause();          // redundant, and must stay a no-op
        QTest::qWait(100);         // let whatever the decoder had in flight land
        const int seen = got.frames.size();
        QVERIFY(seen < 15);        // still mid-run, or this proves nothing
        QTest::qWait(250);
        QCOMPARE(got.frames.size(), seen);   // nothing arrives while paused
        QCOMPARE(finished.count(), 0);

        grabber->resume();
        grabber->resume();
        QVERIFY2(finished.wait(20000), qPrintable(firstFailure(failed)));
        QCOMPARE(failed.count(), 0);
        QCOMPARE(got.times.size(), 15);      // 1.5 s at 10 fps, the whole plan
    }

    void reusesTheSameGrabberAfterCancel()
    {
        const QString clip = clipPath();
        QVERIFY2(!clip.isEmpty(), "the clip.mp4 fixture is missing");

        // The exporter keeps one grabber and reuses it across exports, so a second run
        // after a cancel has to work rather than leaving the player in a stopped state.
        auto grabber = VideoFrameGrabber::create();
        QSignalSpy failed(grabber.get(), &VideoFrameGrabber::failed);
        QSignalSpy finished(grabber.get(), &VideoFrameGrabber::finished);

        const Collected first = collect(grabber.get(), &failed, &finished, clip, 0, 500, 10, 0);
        QVERIFY2(finished.count() == 1, qPrintable(firstFailure(failed)));
        QCOMPARE(first.frames.size(), 5);

        finished.clear();
        const Collected second = collect(grabber.get(), &failed, &finished, clip, 1000, 1500, 10, 0);
        QVERIFY2(finished.count() == 1, qPrintable(firstFailure(failed)));
        QCOMPARE(second.times.first(), qint64(1000));
        QCOMPARE(second.frames.size(), 5);
    }

    void seeksToALateTrimStart()
    {
        const QString clip = clipPath();
        QVERIFY2(!clip.isEmpty(), "the clip.mp4 fixture is missing");

        // A short watchdog stands in for a long recording: seeking before the media has
        // loaded is a no-op, so the grabber would play from 0 and deliver nothing until
        // it crawled to 2500 ms, which the watchdog is not meant to survive.
        qputenv("SNIM_GRABBER_IDLE_TIMEOUT_MS", "400");
        const auto restore = qScopeGuard([] { qunsetenv("SNIM_GRABBER_IDLE_TIMEOUT_MS"); });

        auto grabber = VideoFrameGrabber::create();
        QSignalSpy failed(grabber.get(), &VideoFrameGrabber::failed);
        QSignalSpy finished(grabber.get(), &VideoFrameGrabber::finished);

        const Collected got = collect(grabber.get(), &failed, &finished, clip, 2500, 3000, 10, 0);

        QVERIFY2(finished.count() == 1, qPrintable(firstFailure(failed)));
        QCOMPARE(failed.count(), 0);
        QCOMPARE(got.times.first(), qint64(2500));
        // The real assertion: a crawl from 0 cannot beat 2500/4 = 625 ms, and a seek
        // lands in single digits, so this separates them by a wide margin either way.
        QVERIFY2(got.firstFrameMs >= 0 && got.firstFrameMs < 300,
                 qPrintable(QStringLiteral("first frame after %1 ms, so no seek happened")
                                .arg(got.firstFrameMs)));
    }

    void seeksToALateTrimStartOnAReusedGrabber()
    {
        const QString clip = clipPath();
        QVERIFY2(!clip.isEmpty(), "the clip.mp4 fixture is missing");

        // The exporter keeps one grabber for its life, so a second run re-seeks a file
        // that is already loaded and takes the synchronous path out of start(). Drop that
        // path and this run never seeks or plays at all.
        qputenv("SNIM_GRABBER_IDLE_TIMEOUT_MS", "400");
        const auto restore = qScopeGuard([] { qunsetenv("SNIM_GRABBER_IDLE_TIMEOUT_MS"); });

        auto grabber = VideoFrameGrabber::create();
        QSignalSpy failed(grabber.get(), &VideoFrameGrabber::failed);
        QSignalSpy finished(grabber.get(), &VideoFrameGrabber::finished);

        const Collected first = collect(grabber.get(), &failed, &finished, clip, 0, 500, 10, 0);
        QVERIFY2(finished.count() == 1, qPrintable(firstFailure(failed)));

        finished.clear();
        const Collected second = collect(grabber.get(), &failed, &finished, clip, 2500, 3000, 10, 0);
        QVERIFY2(finished.count() == 1, qPrintable(firstFailure(failed)));
        QCOMPARE(failed.count(), 0);
        QCOMPARE(second.times.first(), qint64(2500));
        QVERIFY(second.firstFrameMs >= 0 && second.firstFrameMs < 300);
    }

    void missingFileFails()
    {
        auto grabber = VideoFrameGrabber::create();
        QSignalSpy failed(grabber.get(), &VideoFrameGrabber::failed);
        QSignalSpy finished(grabber.get(), &VideoFrameGrabber::finished);

        grabber->start(QStringLiteral("/nonexistent/clip.mp4"), 0, 1000, 10, 0);

        QTRY_COMPARE(failed.count(), 1);
        QVERIFY(!firstFailure(failed).isEmpty());
        QCOMPARE(finished.count(), 0);
    }

    void unreadableFileFails()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath("broken.mp4");
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(QByteArray(4096, 'x'));
        file.close();

        auto grabber = VideoFrameGrabber::create();
        QSignalSpy failed(grabber.get(), &VideoFrameGrabber::failed);
        QSignalSpy finished(grabber.get(), &VideoFrameGrabber::finished);

        grabber->start(path, 0, 1000, 10, 0);

        // Either the backend reports the bad file or the idle watchdog gives up on it.
        // What must never happen is a silent finish or an endless wait.
        QTRY_VERIFY_WITH_TIMEOUT(failed.count() + finished.count() == 1, 15000);
        QCOMPARE(failed.count(), 1);
        QVERIFY(!firstFailure(failed).isEmpty());
    }
};

QTEST_MAIN(tst_VideoFrameGrabber)
#include "tst_videoframegrabber.moc"
