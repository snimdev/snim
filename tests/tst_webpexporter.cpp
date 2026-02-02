#include <QtTest>
#include <QColor>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QImageReader>
#include <QElapsedTimer>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTimer>
#include <algorithm>

#include "editor/video/AnimationParams.h"
#include "editor/video/VideoFrameGrabber.h"
#include "editor/video/WebpExporter.h"

using namespace Editor::Video;

namespace {

// One distinct frame per index. Not a flat fill: a solid color compresses to nothing,
// so the encode would not cost what a real frame costs, which the responsiveness test
// depends on measuring.
QImage patternFrame(int index, const QSize &size = QSize(32, 24))
{
    QImage frame(size, QImage::Format_ARGB32);
    for (int y = 0; y < frame.height(); ++y) {
        auto *line = reinterpret_cast<QRgb *>(frame.scanLine(y));
        for (int x = 0; x < frame.width(); ++x)
            line[x] = qRgb((x * 7 + index * 29) & 0xff, (y * 13 + index * 11) & 0xff,
                           (x * 3 + y * 5 + index * 53) & 0xff);
    }
    return frame;
}

QByteArray readAll(const QString &path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

// The cheapest libwebp settings (effort 0, no size-minimising passes). These tests are
// about the exporter's plumbing, not the compression, and minimizeSize off also keeps
// libwebp from merging similar frames, which the frame-count assertions depend on.
const AnimationParams kCheap{10, 600, 0, 75, false, 0, false};

// Stands in for the real decoder, so the exporter's own logic (canvas from the first
// frame, per-frame progress, the terminal paths) is testable with no media at all.
class FakeGrabber : public VideoFrameGrabber
{
public:
    enum class Mode { Frames, Fail, FinishWithoutFrames, HangAfterFirstFrame };

    explicit FakeGrabber(Mode mode = Mode::Frames, QObject *parent = nullptr)
        : VideoFrameGrabber(parent), m_mode(mode) {}

    void start(const QString &input, qint64 inMs, qint64 outMs, int fps, int maxWidth) override
    {
        startedInput = input;
        startedIn = inMs;
        startedOut = outMs;
        startedFps = fps;
        startedMaxWidth = maxWidth;
        ++startCount;

        const QVector<qint64> plan = planAnimationFrames(inMs, outMs, fps);
        QMetaObject::invokeMethod(this, [this, plan] {
            if (m_mode == Mode::Fail) {
                emit failed(QStringLiteral("the fake decoder exploded"));
                return;
            }
            if (m_mode == Mode::FinishWithoutFrames) {
                emit finished();
                return;
            }
            for (int i = 0; i < plan.size(); ++i) {
                if (i > 0 && m_mode == Mode::HangAfterFirstFrame)
                    return;
                // Distinct frames, so libwebp keeps every one instead of merging them.
                emit frameReady(patternFrame(i, frameSize), plan.at(i));
            }
            if (m_mode != Mode::HangAfterFirstFrame)
                emit finished();
        }, Qt::QueuedConnection);
    }

    void cancel() override { ++cancelCount; }
    void pause() override { ++pauseCount; }
    void resume() override { ++resumeCount; }

    QSize frameSize{32, 24};   // raised by the tests that want a costly encode
    QString startedInput;
    qint64 startedIn = -1;
    qint64 startedOut = -1;
    int startedFps = 0;
    int startedMaxWidth = -1;
    int startCount = 0;
    int cancelCount = 0;
    int pauseCount = 0;
    int resumeCount = 0;

private:
    Mode m_mode;
};

} // namespace

// The shared animated-WebP export path: grabber into encoder, the progress it reports,
// and the failure paths the editor relies on to never be left busy. The real decoder is
// covered by tst_videoframegrabber; here the frames are synthetic.
class tst_WebpExporter : public QObject
{
    Q_OBJECT

private slots:
    void writesAnAnimationFromGrabbedFrames()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString output = dir.filePath("clip.webp");

        WebpExporter exporter(std::make_unique<FakeGrabber>());
        QSignalSpy finished(&exporter, &WebpExporter::finished);
        QSignalSpy failed(&exporter, &WebpExporter::failed);
        QSignalSpy progress(&exporter, &WebpExporter::progress);

        exporter.start(QStringLiteral("/tmp/recording.mp4"), output, 0, 300, kCheap);

        QVERIFY2(finished.wait(5000), "the export never finished");
        QCOMPARE(failed.count(), 0);
        QCOMPARE(finished.first().at(0).toString(), output);

        // Three samples at 10 fps over 0..300 ms, one progress tick each.
        QCOMPARE(progress.count(), 3);
        QCOMPARE(progress.at(2).at(0).toInt(), 3);
        QCOMPARE(progress.at(2).at(1).toInt(), 3);

        const QByteArray bytes = readAll(output);
        QVERIFY(bytes.size() > 100);
        QCOMPARE(bytes.left(4), QByteArrayLiteral("RIFF"));
        QCOMPARE(bytes.mid(8, 4), QByteArrayLiteral("WEBP"));

        // The canvas comes from the first grabbed frame.
        QImageReader reader(output);
        QVERIFY(reader.canRead());
        QCOMPARE(reader.imageCount(), 3);
        QCOMPARE(reader.size(), QSize(32, 24));
    }

    void passesTheTrimAndScalingToTheGrabber()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        auto fake = std::make_unique<FakeGrabber>();
        auto *raw = fake.get();
        WebpExporter exporter(std::move(fake));
        QSignalSpy finished(&exporter, &WebpExporter::finished);

        exporter.start(QStringLiteral("/tmp/recording.mp4"), dir.filePath("clip.webp"),
                       1500, 4000, AnimationParams{5, 320, 0, 75, false, 0, false});
        QVERIFY(finished.wait(5000));

        QCOMPARE(raw->startedInput, QStringLiteral("/tmp/recording.mp4"));
        QCOMPARE(raw->startedIn, qint64(1500));
        QCOMPARE(raw->startedOut, qint64(4000));
        QCOMPARE(raw->startedFps, 5);
        QCOMPARE(raw->startedMaxWidth, 320);   // the grabber owns the downscale
    }

    void reusesOneGrabberAcrossExports()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        auto fake = std::make_unique<FakeGrabber>();
        auto *raw = fake.get();
        WebpExporter exporter(std::move(fake));
        QSignalSpy finished(&exporter, &WebpExporter::finished);
        QSignalSpy failed(&exporter, &WebpExporter::failed);

        exporter.start(QStringLiteral("/tmp/recording.mp4"), dir.filePath("first.webp"),
                       0, 300, kCheap);
        QVERIFY2(finished.wait(5000), "the first export never finished");
        exporter.start(QStringLiteral("/tmp/recording.mp4"), dir.filePath("second.webp"),
                       0, 300, kCheap);
        QVERIFY2(finished.wait(5000), "the second export never finished");

        QCOMPARE(failed.count(), 0);
        QCOMPARE(raw->startCount, 2);
        QVERIFY(QFile::exists(dir.filePath("first.webp")));
        QVERIFY(QFile::exists(dir.filePath("second.webp")));
    }

    void grabberFailureFailsTheExport()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString output = dir.filePath("clip.webp");

        WebpExporter exporter(std::make_unique<FakeGrabber>(FakeGrabber::Mode::Fail));
        QSignalSpy finished(&exporter, &WebpExporter::finished);
        QSignalSpy failed(&exporter, &WebpExporter::failed);

        exporter.start(QStringLiteral("/tmp/recording.mp4"), output, 0, 300, kCheap);

        QVERIFY(failed.wait(5000));
        QCOMPARE(finished.count(), 0);
        QCOMPARE(failed.first().at(0).toString(), QStringLiteral("the fake decoder exploded"));
        QVERIFY(!QFile::exists(output));
        QVERIFY(QDir(dir.path()).entryList(QDir::Files).isEmpty());
    }

    void noFramesFailsTheExport()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString output = dir.filePath("clip.webp");

        WebpExporter exporter(std::make_unique<FakeGrabber>(FakeGrabber::Mode::FinishWithoutFrames));
        QSignalSpy finished(&exporter, &WebpExporter::finished);
        QSignalSpy failed(&exporter, &WebpExporter::failed);

        exporter.start(QStringLiteral("/tmp/recording.mp4"), output, 0, 300, kCheap);

        QVERIFY(failed.wait(5000));
        QCOMPARE(finished.count(), 0);
        QVERIFY(!failed.first().at(0).toString().isEmpty());
        QVERIFY(!QFile::exists(output));
    }

    void unwritableOutputFailsTheExport()
    {
        // The encoder's own failure path: the grabber is perfectly healthy, but the
        // destination directory does not exist, so begin() fails on the first frame.
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString output = dir.filePath("missing/clip.webp");

        WebpExporter exporter(std::make_unique<FakeGrabber>());
        QSignalSpy finished(&exporter, &WebpExporter::finished);
        QSignalSpy failed(&exporter, &WebpExporter::failed);

        exporter.start(QStringLiteral("/tmp/recording.mp4"), output, 0, 300, kCheap);

        QVERIFY(failed.wait(5000));
        QTest::qWait(100);   // the fake's remaining frames must not add a second result
        QCOMPARE(failed.count(), 1);
        QCOMPARE(finished.count(), 0);
        // The reported reason is the one the user can act on (the path), not a later
        // "addFrame() before begin()" from an encoder that was let run on regardless.
        QVERIFY2(failed.first().at(0).toString().contains(output),
                 qPrintable(failed.first().at(0).toString()));
        QVERIFY(!QFile::exists(output));
        QVERIFY(QDir(dir.path()).entryList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot).isEmpty());
    }

    void cancelStopsQuietlyAndLeavesNoFile()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString output = dir.filePath("clip.webp");

        auto fake = std::make_unique<FakeGrabber>(FakeGrabber::Mode::HangAfterFirstFrame);
        auto *raw = fake.get();
        WebpExporter exporter(std::move(fake));
        QSignalSpy finished(&exporter, &WebpExporter::finished);
        QSignalSpy failed(&exporter, &WebpExporter::failed);
        QSignalSpy progress(&exporter, &WebpExporter::progress);

        exporter.start(QStringLiteral("/tmp/recording.mp4"), output, 0, 3000, kCheap);
        QTRY_COMPARE(progress.count(), 1);

        exporter.cancel();
        QTest::qWait(100);
        QCOMPARE(finished.count(), 0);   // a cancel is not a result
        QCOMPARE(failed.count(), 0);
        QVERIFY(raw->cancelCount > 0);
        QVERIFY(!QFile::exists(output));
        QVERIFY(QDir(dir.path()).entryList(QDir::Files).isEmpty());
    }

    void cancelMidEncodeLeavesNoFileAndNoLateSignal()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString output = dir.filePath("clip.webp");

        // Cancelling while the worker is genuinely mid-frame: the thread has to be joined
        // before cancel() returns, or a late signal or a leftover QSaveFile temp escapes.
        auto fake = std::make_unique<FakeGrabber>();
        fake->frameSize = QSize(640, 480);
        WebpExporter exporter(std::move(fake));
        QSignalSpy finished(&exporter, &WebpExporter::finished);
        QSignalSpy failed(&exporter, &WebpExporter::failed);
        QSignalSpy progress(&exporter, &WebpExporter::progress);

        exporter.start(QStringLiteral("/tmp/recording.mp4"), output, 0, 1500, kCheap);
        QTRY_VERIFY(progress.count() >= 2);
        QVERIFY(progress.count() < 15);   // still mid-run, or this proves nothing

        exporter.cancel();
        const int encoded = progress.count();
        QTest::qWait(150);
        QCOMPARE(progress.count(), encoded);   // the worker stopped, it did not drain
        QCOMPARE(finished.count(), 0);
        QCOMPARE(failed.count(), 0);
        QVERIFY(!QFile::exists(output));
        // Not just the output: the encoder writes through a QSaveFile temp in the same
        // directory, and an abandoned thread would leave it behind.
        QVERIFY(QDir(dir.path()).entryList(QDir::Files).isEmpty());
    }

    void throttlesTheGrabberWhileTheEncoderLags()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString output = dir.filePath("clip.webp");

        // The fake delivers its whole plan in one burst, which is exactly the case the
        // backpressure exists for: without it those frames would all sit in the worker's
        // queue at full size.
        auto fake = std::make_unique<FakeGrabber>();
        fake->frameSize = QSize(320, 240);
        auto *raw = fake.get();
        WebpExporter exporter(std::move(fake));
        QSignalSpy finished(&exporter, &WebpExporter::finished);
        QSignalSpy failed(&exporter, &WebpExporter::failed);
        QSignalSpy progress(&exporter, &WebpExporter::progress);

        exporter.start(QStringLiteral("/tmp/recording.mp4"), output, 0, 4000, kCheap);
        QVERIFY2(finished.wait(120000),
                 qPrintable(failed.isEmpty() ? QStringLiteral("timed out")
                                             : failed.first().at(0).toString()));

        QVERIFY2(raw->pauseCount > 0, "the grabber was never held back");
        QVERIFY(raw->resumeCount > 0);          // and it was let go again
        QCOMPARE(progress.count(), 40);         // 4 s at 10 fps, every frame encoded
        QCOMPARE(progress.last().at(0).toInt(), 40);
        QVERIFY(QFile::exists(output));
    }

    void keepsTheGuiThreadResponsiveWhileEncoding()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString output = dir.filePath("clip.webp");

        // The point of the worker thread: the encode is the expensive part, and it must
        // not starve the event loop the editor's UI runs on. 640x480 noise frames cost
        // real time to encode, so a synchronous encoder stalls the timer below for
        // seconds instead of the few ms it is allowed here.
        auto fake = std::make_unique<FakeGrabber>();
        fake->frameSize = QSize(640, 480);
        WebpExporter exporter(std::move(fake));
        QSignalSpy finished(&exporter, &WebpExporter::finished);
        QSignalSpy failed(&exporter, &WebpExporter::failed);

        QTimer ticker;
        ticker.setTimerType(Qt::PreciseTimer);
        ticker.setInterval(20);
        QElapsedTimer sinceTick;
        qint64 worstGapMs = 0;
        int ticks = 0;
        connect(&ticker, &QTimer::timeout, [&] {
            worstGapMs = std::max(worstGapMs, sinceTick.restart());
            ++ticks;
        });
        sinceTick.start();
        ticker.start();

        QElapsedTimer run;
        run.start();
        exporter.start(QStringLiteral("/tmp/recording.mp4"), output, 0, 1400, AnimationParams{});
        QVERIFY2(finished.wait(120000),
                 qPrintable(failed.isEmpty() ? QStringLiteral("timed out")
                                             : failed.first().at(0).toString()));
        const qint64 elapsed = run.elapsed();
        ticker.stop();
        // The tail counts too: a thread blocked to the very end leaves a gap no timeout
        // ever closed, which would otherwise read as a perfect score of zero.
        worstGapMs = std::max(worstGapMs, sinceTick.elapsed());

        qInfo("export %lld ms, %d ticks, worst gap %lld ms", elapsed, ticks, worstGapMs);
        QVERIFY2(elapsed > 200, "the encode was too cheap to prove anything");
        // Loose on purpose: a loaded box schedules badly, but a blocked GUI thread misses
        // every tick and stalls for whole seconds, not for a tenth of one.
        QVERIFY2(ticks > int(elapsed / 20 / 4),
                 qPrintable(QStringLiteral("only %1 ticks in %2 ms").arg(ticks).arg(elapsed)));
        QVERIFY2(worstGapMs < 500,
                 qPrintable(QStringLiteral("the GUI thread stalled for %1 ms").arg(worstGapMs)));
    }

    void exportsARealClipEndToEnd()
    {
        // The only test that joins the real decoder to the real encoder through the real
        // exporter. Everything else here fakes one end, so a canvas or timing mismatch
        // between the two halves would otherwise only show up in a user's hands.
        const QString clip = QFINDTESTDATA("data/clip.mp4");
        QVERIFY2(!clip.isEmpty(), "the clip.mp4 fixture is missing");
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString output = dir.filePath("clip.webp");

        WebpExporter exporter;   // the default ctor: the real grabber, created lazily
        QSignalSpy finished(&exporter, &WebpExporter::finished);
        QSignalSpy failed(&exporter, &WebpExporter::failed);
        QSignalSpy progress(&exporter, &WebpExporter::progress);

        exporter.start(clip, output, 500, 2000, AnimationParams{10, 0, 0, 75, false, 0, false});

        QVERIFY2(finished.wait(30000),
                 qPrintable(failed.isEmpty() ? QStringLiteral("timed out")
                                             : failed.first().at(0).toString()));
        QCOMPARE(failed.count(), 0);
        QCOMPARE(progress.count(), 15);   // 1500 ms at 10 fps

        QImageReader reader(output);
        QVERIFY(reader.canRead());
        QCOMPARE(reader.size(), QSize(128, 96));   // the clip's own size, no downscale
        QCOMPARE(reader.imageCount(), 15);
    }
};

QTEST_MAIN(tst_WebpExporter)
#include "tst_webpexporter.moc"
