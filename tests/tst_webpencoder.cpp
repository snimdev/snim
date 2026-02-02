#include <QtTest>
#include <QColor>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QImageReader>
#include <QTemporaryDir>

#include "editor/video/AnimationParams.h"
#include "editor/video/WebpEncoder.h"

using namespace Editor::Video;

namespace {

QImage solidFrame(int width, int height, const QColor &color)
{
    QImage frame(width, height, QImage::Format_ARGB32);
    frame.fill(color);
    return frame;
}

QByteArray readAll(const QString &path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

quint32 readLe32(const QByteArray &bytes, int at)
{
    return quint32(quint8(bytes.at(at)))
           | (quint32(quint8(bytes.at(at + 1))) << 8)
           | (quint32(quint8(bytes.at(at + 2))) << 16)
           | (quint32(quint8(bytes.at(at + 3))) << 24);
}

int readLe24(const QByteArray &bytes, int at)
{
    return int(quint8(bytes.at(at)))
           | (int(quint8(bytes.at(at + 1))) << 8)
           | (int(quint8(bytes.at(at + 2))) << 16);
}

struct WebpChunks {
    QSize canvas;
    int loopCount = -1;
    bool hasAnimationHeader = false;   // VP8X
    QVector<int> frameDurationsMs;     // one entry per ANMF
};

// Walk the RIFF chunk list instead of decoding it: canvas, loop count, frame count and
// per-frame durations are then read straight from the container we wrote, with no extra
// dependency and nothing left to the decoder's interpretation.
WebpChunks inspectWebp(const QByteArray &bytes)
{
    WebpChunks out;
    if (bytes.size() < 12 || bytes.left(4) != QByteArrayLiteral("RIFF")
        || bytes.mid(8, 4) != QByteArrayLiteral("WEBP")) {
        return out;
    }

    int offset = 12;
    while (offset + 8 <= bytes.size()) {
        const QByteArray fourcc = bytes.mid(offset, 4);
        const quint32 size = readLe32(bytes, offset + 4);
        const int payload = offset + 8;
        if (payload + int(size) > bytes.size())
            break;                                  // truncated

        if (fourcc == QByteArrayLiteral("VP8X") && size >= 10) {
            // flags(1) reserved(3) canvasWidth-1(3) canvasHeight-1(3)
            out.hasAnimationHeader = true;
            out.canvas = QSize(readLe24(bytes, payload + 4) + 1,
                               readLe24(bytes, payload + 7) + 1);
        } else if (fourcc == QByteArrayLiteral("ANIM") && size >= 6) {
            // backgroundColor(4) loopCount(2)
            out.loopCount = int(quint8(bytes.at(payload + 4)))
                            | (int(quint8(bytes.at(payload + 5))) << 8);
        } else if (fourcc == QByteArrayLiteral("ANMF") && size >= 16) {
            // x(3) y(3) width-1(3) height-1(3) duration(3) flags(1)
            out.frameDurationsMs.append(readLe24(bytes, payload + 12));
        }
        offset = payload + int(size) + (int(size) & 1);   // chunks pad to even
    }
    return out;
}

struct QtDecoded {
    QSize canvas;
    int frameCount = 0;
    QVector<QImage> frames;
};

// The frames as a viewer sees them, through Qt's own webp plugin (the same path a future
// in-app preview would take). Missing plugin support skips rather than fails.
QtDecoded readWithQt(const QString &path)
{
    QtDecoded out;
    QImageReader reader(path);
    out.canvas = reader.size();
    if (!reader.canRead() || !reader.supportsAnimation())
        return out;
    out.frameCount = reader.imageCount();
    if (out.frameCount < 1)
        return out;
    // Repeated read() walks the frames. The plugin does not implement jumpToNextImage()
    // for animated WebP, so a jump-based loop would stop after the first frame.
    for (int i = 0; i < out.frameCount; ++i) {
        const QImage frame = reader.read();
        if (frame.isNull())
            break;
        out.frames.append(frame);
    }
    return out;
}

} // namespace

// The animated-WebP encoder: container bytes, frame timing, and the failure paths that
// keep the editor from hanging or leaving a partial file behind. Video decoding is the
// grabber's job and gets its own test.
class tst_WebpEncoder : public QObject
{
    Q_OBJECT

private slots:
    void writesAnAnimatedContainer()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath("clip.webp");

        WebpEncoder encoder;
        QString error;
        QVERIFY2(encoder.begin(path, QSize(64, 64), AnimationParams{10, 0, 0, 75, true}, &error),
                 qPrintable(error));
        QVERIFY2(encoder.addFrame(solidFrame(64, 64, Qt::red), 0, &error), qPrintable(error));
        QVERIFY2(encoder.addFrame(solidFrame(64, 64, Qt::green), 100, &error), qPrintable(error));
        QVERIFY2(encoder.addFrame(solidFrame(64, 64, Qt::blue), 200, &error), qPrintable(error));
        QVERIFY2(encoder.finish(&error), qPrintable(error));

        const QByteArray bytes = readAll(path);
        QVERIFY(bytes.size() > 100);
        QCOMPARE(bytes.left(4), QByteArrayLiteral("RIFF"));
        QCOMPARE(bytes.mid(8, 4), QByteArrayLiteral("WEBP"));

        const WebpChunks chunks = inspectWebp(bytes);
        QVERIFY(chunks.hasAnimationHeader);
        QCOMPARE(chunks.canvas, QSize(64, 64));
        QCOMPARE(chunks.loopCount, 0);          // params default: loop forever
        QCOMPARE(chunks.frameDurationsMs.size(), 3);
    }

    void frameTimingMatchesTheRequestedFps()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath("clip.webp");

        WebpEncoder encoder;
        QString error;
        QVERIFY2(encoder.begin(path, QSize(16, 16), AnimationParams{10, 0, 0, 75, true}, &error),
                 qPrintable(error));
        // Distinct frames: libwebp collapses a run of identical ones, and a one-frame
        // animation is written as a still image with no ANIM/ANMF chunks at all.
        const QVector<QColor> colors{Qt::red, Qt::green, Qt::blue};
        for (int i = 0; i < colors.size(); ++i) {
            QVERIFY2(encoder.addFrame(solidFrame(16, 16, colors.at(i)), i * 100, &error),
                     qPrintable(error));
        }
        QVERIFY2(encoder.finish(&error), qPrintable(error));

        // 10 fps means 100 ms per frame, and the last frame's duration is closed by
        // finish(): three samples at 0/100/200 ms play for 300 ms, same as the GIF path.
        const QVector<int> durations = inspectWebp(readAll(path)).frameDurationsMs;
        QCOMPARE(durations.size(), 3);
        QCOMPARE(durations.at(0), 100);
        QCOMPARE(durations.at(1), 100);
        QCOMPARE(durations.at(2), 100);
    }

    void decodesBackToTheSameFramesAndLoop()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath("clip.webp");

        WebpEncoder encoder;
        QString error;
        // Lossless so the decoded pixels are the ones that went in, and loopCount 3 so the
        // loop metadata is distinguishable from the 0 = forever default.
        QVERIFY2(encoder.begin(path, QSize(32, 24), AnimationParams{10, 0, 3, 75, true}, &error),
                 qPrintable(error));
        QVERIFY2(encoder.addFrame(solidFrame(32, 24, Qt::red), 0, &error), qPrintable(error));
        QVERIFY2(encoder.addFrame(solidFrame(32, 24, Qt::blue), 100, &error), qPrintable(error));
        QVERIFY2(encoder.finish(&error), qPrintable(error));

        QCOMPARE(inspectWebp(readAll(path)).loopCount, 3);

        const QtDecoded decoded = readWithQt(path);
        if (decoded.frames.size() < 2)
            QSKIP("the Qt webp plugin cannot read animated WebP here");
        QCOMPARE(decoded.canvas, QSize(32, 24));
        QCOMPARE(decoded.frames.size(), 2);
        QCOMPARE(decoded.frames.at(0).pixelColor(16, 12), QColor(Qt::red));
        QCOMPARE(decoded.frames.at(1).pixelColor(16, 12), QColor(Qt::blue));
    }

    void offsetsTheTimelineSoTheLastFrameSurvives()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath("clip.webp");

        WebpEncoder encoder;
        QString error;
        QVERIFY2(encoder.begin(path, QSize(16, 16), AnimationParams{10, 0, 0, 75, true}, &error),
                 qPrintable(error));
        // A trim starting a minute into the recording: the source offset must not leak into
        // the animation, and the final frame needs its duration closed by finish().
        QVERIFY2(encoder.addFrame(solidFrame(16, 16, Qt::red), 60000, &error), qPrintable(error));
        QVERIFY2(encoder.addFrame(solidFrame(16, 16, Qt::blue), 60100, &error), qPrintable(error));
        QVERIFY2(encoder.finish(&error), qPrintable(error));

        const QVector<int> durations = inspectWebp(readAll(path)).frameDurationsMs;
        QCOMPARE(durations.size(), 2);
        QCOMPARE(durations.at(0), 100);
        QCOMPARE(durations.at(1), 100);
    }

    void repeatedTimestampStillEncodes()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath("clip.webp");

        WebpEncoder encoder;
        QString error;
        QVERIFY2(encoder.begin(path, QSize(16, 16), AnimationParams{10, 0, 0, 75, true}, &error),
                 qPrintable(error));
        // Two frames resolving to the same source time (a snapped seek): the second is
        // nudged 1 ms forward rather than collapsing to a zero-length frame.
        QVERIFY2(encoder.addFrame(solidFrame(16, 16, Qt::red), 500, &error), qPrintable(error));
        QVERIFY2(encoder.addFrame(solidFrame(16, 16, Qt::blue), 500, &error), qPrintable(error));
        QVERIFY2(encoder.finish(&error), qPrintable(error));

        const QVector<int> durations = inspectWebp(readAll(path)).frameDurationsMs;
        QCOMPARE(durations.size(), 2);
        QCOMPARE(durations.at(0), 1);
        QCOMPARE(durations.at(1), 100);
    }

    void scalesAFrameThatDoesNotMatchTheCanvas()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath("clip.webp");

        WebpEncoder encoder;
        QString error;
        QVERIFY2(encoder.begin(path, QSize(16, 16), AnimationParams{10, 0, 0, 75, true}, &error),
                 qPrintable(error));
        // The first frame is twice the canvas, the second matches it: the canvas is what
        // begin() was given, and neither frame may fail the encode.
        QVERIFY2(encoder.addFrame(solidFrame(64, 64, Qt::red), 0, &error), qPrintable(error));
        QVERIFY2(encoder.addFrame(solidFrame(16, 16, Qt::blue), 100, &error), qPrintable(error));
        QVERIFY2(encoder.finish(&error), qPrintable(error));

        const WebpChunks chunks = inspectWebp(readAll(path));
        QVERIFY(chunks.hasAnimationHeader);
        QCOMPARE(chunks.canvas, QSize(16, 16));
        QCOMPARE(chunks.frameDurationsMs.size(), 2);
    }

    void singleFrameLandsAsAStillImage()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath("clip.webp");

        WebpEncoder encoder;
        QString error;
        // Lossy on purpose: Qt's image plugin refuses a lossless *still* WebP (it reads
        // lossless animations fine, and every libwebp tool and browser is happy with
        // both), and this test wants a reader to confirm the frame actually landed.
        QVERIFY2(encoder.begin(path, QSize(16, 16), AnimationParams{10, 0, 0, 75, false}, &error),
                 qPrintable(error));
        QVERIFY2(encoder.addFrame(solidFrame(16, 16, Qt::red), 0, &error), qPrintable(error));
        QVERIFY2(encoder.finish(&error), qPrintable(error));

        // libwebp omits the animation chunks for a single frame, so a trim shorter than
        // one frame interval exports as a still image, not a one-frame loop. Locked here
        // so a future libwebp change to that shows up as a test failure, not a UI surprise.
        QVERIFY(!inspectWebp(readAll(path)).hasAnimationHeader);
        QImageReader reader(path);
        QVERIFY(reader.canRead());
        const QImage still = reader.read();
        QCOMPARE(still.size(), QSize(16, 16));
        // Lossy, so the pixel is near-red rather than exactly red.
        const QColor center = still.pixelColor(8, 8);
        QVERIFY(center.red() > 200 && center.green() < 60 && center.blue() < 60);
    }

    void failuresLeaveNothingBehind()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath("clip.webp");

        WebpEncoder encoder;
        QString error;
        // Before begin().
        QVERIFY(!encoder.addFrame(solidFrame(8, 8, Qt::red), 0, &error));
        QVERIFY(!error.isEmpty());
        QVERIFY(!encoder.finish(&error));
        QVERIFY(!error.isEmpty());

        // An empty canvas is refused before anything is created.
        error.clear();
        QVERIFY(!encoder.begin(path, QSize(), AnimationParams{}, &error));
        QVERIFY(!error.isEmpty());
        QVERIFY(!QFile::exists(path));

        // begin() with no frames: no file, and not even QSaveFile's temp left over.
        error.clear();
        QVERIFY2(encoder.begin(path, QSize(8, 8), AnimationParams{}, &error), qPrintable(error));
        QVERIFY(!encoder.finish(&error));
        QVERIFY(!error.isEmpty());
        QVERIFY(!QFile::exists(path));
        QVERIFY(QDir(dir.path()).entryList(QDir::Files).isEmpty());
    }

    void cancelDiscardsThePartialFileAndAllowsReuse()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath("clip.webp");

        WebpEncoder encoder;
        QString error;
        QVERIFY2(encoder.begin(path, QSize(16, 16), AnimationParams{}, &error), qPrintable(error));
        QVERIFY2(encoder.addFrame(solidFrame(16, 16, Qt::red), 0, &error), qPrintable(error));
        encoder.cancel();
        QVERIFY(!QFile::exists(path));
        QVERIFY(QDir(dir.path()).entryList(QDir::Files).isEmpty());

        // The same instance is reusable, which the editor relies on across exports.
        QVERIFY2(encoder.begin(path, QSize(16, 16), AnimationParams{}, &error), qPrintable(error));
        QVERIFY2(encoder.addFrame(solidFrame(16, 16, Qt::red), 0, &error), qPrintable(error));
        QVERIFY2(encoder.finish(&error), qPrintable(error));
        QVERIFY(QFile::exists(path));
    }
};

QTEST_MAIN(tst_WebpEncoder)
#include "tst_webpencoder.moc"
