#include <QtTest>
#include <QColor>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QImageReader>
#include <QTemporaryDir>

#include "editor/video/AnimationParams.h"
#include "editor/video/GifEncoder.h"

using namespace Editor::Video;

namespace {

QImage solidFrame(int width, int height, const QColor &color)
{
    QImage frame(width, height, QImage::Format_ARGB32);
    frame.fill(color);
    return frame;
}

// A gradient, so the quantizer has more than one color to work with.
QImage gradientFrame(int width, int height, int seed)
{
    QImage frame(width, height, QImage::Format_ARGB32);
    for (int y = 0; y < height; ++y) {
        auto *line = reinterpret_cast<QRgb *>(frame.scanLine(y));
        for (int x = 0; x < width; ++x)
            line[x] = qRgb((x * 255 / width + seed * 40) & 0xff, (y * 255 / height) & 0xff,
                           (seed * 70) & 0xff);
    }
    return frame;
}

QByteArray readAll(const QString &path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

int readLe16(const QByteArray &bytes, int at)
{
    return int(quint8(bytes.at(at))) | (int(quint8(bytes.at(at + 1))) << 8);
}

struct GifBlocks {
    bool valid = false;            // header, screen, and a trailer all parsed
    QByteArray version;
    QSize screen;
    bool hasGlobalPalette = false;
    int loopCount = -1;            // -1 = no NETSCAPE2.0 block
    bool loopBeforeFirstFrame = false;
    QVector<int> delaysCs;         // one entry per graphics control extension
    QVector<QSize> frameSizes;     // one entry per image descriptor
    int localPalettes = 0;
};

// Skips a run of data sub-blocks; returns the offset after the terminator, or -1.
int skipSubBlocks(const QByteArray &bytes, int offset)
{
    while (offset < bytes.size()) {
        const int length = quint8(bytes.at(offset));
        offset += 1 + length;
        if (length == 0)
            return offset;
    }
    return -1;
}

// Walk the block structure instead of decoding it: the loop, the frame count and every
// delay are then read straight from the bytes we wrote, with no decoder in between.
GifBlocks inspectGif(const QByteArray &bytes)
{
    GifBlocks out;
    if (bytes.size() < 13)
        return out;
    out.version = bytes.left(6);
    out.screen = QSize(readLe16(bytes, 6), readLe16(bytes, 8));
    const int packed = quint8(bytes.at(10));
    out.hasGlobalPalette = packed & 0x80;
    int offset = 13;
    if (out.hasGlobalPalette)
        offset += 3 * (1 << ((packed & 0x07) + 1));

    while (offset >= 0 && offset < bytes.size()) {
        const int introducer = quint8(bytes.at(offset));
        if (introducer == 0x3B) {                       // trailer
            out.valid = true;
            return out;
        }
        if (introducer == 0x21 && offset + 1 < bytes.size()) {
            const int label = quint8(bytes.at(offset + 1));
            const int body = offset + 2;
            if (label == 0xFF && body + 12 < bytes.size() && quint8(bytes.at(body)) == 11
                && bytes.mid(body + 1, 11) == QByteArrayLiteral("NETSCAPE2.0")) {
                const int sub = body + 12;              // [3, 1, lo, hi]
                if (sub + 3 < bytes.size() && quint8(bytes.at(sub)) == 3) {
                    out.loopCount = readLe16(bytes, sub + 2);
                    out.loopBeforeFirstFrame = out.frameSizes.isEmpty();
                }
            } else if (label == 0xF9 && body + 4 < bytes.size()) {
                out.delaysCs.append(readLe16(bytes, body + 2));   // [4, flags, lo, hi, idx]
            }
            offset = skipSubBlocks(bytes, body);
            continue;
        }
        if (introducer == 0x2C && offset + 10 <= bytes.size()) {
            out.frameSizes.append(QSize(readLe16(bytes, offset + 5), readLe16(bytes, offset + 7)));
            const int flags = quint8(bytes.at(offset + 9));
            offset += 10;
            if (flags & 0x80) {
                ++out.localPalettes;
                offset += 3 * (1 << ((flags & 0x07) + 1));
            }
            offset = skipSubBlocks(bytes, offset + 1);  // past the LZW minimum code size
            continue;
        }
        return out;                                     // unknown block: not valid
    }
    return out;
}

bool qtReadsGif()
{
    return QImageReader::supportedImageFormats().contains("gif");
}

} // namespace

// The animated-GIF encoder: container bytes, centisecond timing with its carried rounding,
// and the failure paths that keep the editor from leaving a partial file behind.
class tst_GifEncoder : public QObject
{
    Q_OBJECT

private slots:
    void writesAnAnimatedGif89a()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath("clip.gif");

        GifEncoder encoder;
        QString error;
        QVERIFY2(encoder.begin(path, QSize(64, 48), AnimationParams{}, &error), qPrintable(error));
        QVERIFY2(encoder.addFrame(gradientFrame(64, 48, 0), 0, &error), qPrintable(error));
        QVERIFY2(encoder.addFrame(gradientFrame(64, 48, 1), 100, &error), qPrintable(error));
        QVERIFY2(encoder.addFrame(gradientFrame(64, 48, 2), 200, &error), qPrintable(error));
        QVERIFY2(encoder.finish(&error), qPrintable(error));

        const GifBlocks gif = inspectGif(readAll(path));
        QVERIFY(gif.valid);
        QCOMPARE(gif.version, QByteArrayLiteral("GIF89a"));
        QCOMPARE(gif.screen, QSize(64, 48));
        QVERIFY(!gif.hasGlobalPalette);          // every frame brings its own
        QCOMPARE(gif.loopCount, 0);              // params default: loop forever
        QVERIFY(gif.loopBeforeFirstFrame);
        QCOMPARE(gif.frameSizes.size(), 3);
        QCOMPARE(gif.localPalettes, 3);
        QCOMPARE(gif.delaysCs, (QVector<int>{10, 10, 10}));
        for (const QSize &size : gif.frameSizes)
            QCOMPARE(size, QSize(64, 48));
    }

    void carriesRoundingAcrossFrames()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath("clip.gif");

        // 15 fps samples 67 ms apart and ends the last one 67 ms later (201 ms in all).
        // Rounding each delta would give 7/7/7 = 210 ms; rounding each timestamp keeps
        // the total at 20 cs and spreads the error as 7/6/7.
        GifEncoder encoder;
        QString error;
        QVERIFY2(encoder.begin(path, QSize(16, 16), AnimationParams{15, 0, 0}, &error),
                 qPrintable(error));
        const QVector<qint64> times = planAnimationFrames(0, 201, 15);
        QCOMPARE(times, (QVector<qint64>{0, 67, 134}));
        for (int i = 0; i < times.size(); ++i) {
            QVERIFY2(encoder.addFrame(gradientFrame(16, 16, i), times.at(i), &error),
                     qPrintable(error));
        }
        QVERIFY2(encoder.finish(&error), qPrintable(error));

        const GifBlocks gif = inspectGif(readAll(path));
        QVERIFY(gif.valid);
        QCOMPARE(gif.delaysCs, (QVector<int>{7, 6, 7}));

        // A longer run never drifts: the delays always sum to the rounded end time.
        const QString longPath = dir.filePath("long.gif");
        QVERIFY2(encoder.begin(longPath, QSize(16, 16), AnimationParams{15, 0, 0}, &error),
                 qPrintable(error));
        const QVector<qint64> longTimes = planAnimationFrames(0, 3000, 15);
        for (int i = 0; i < longTimes.size(); ++i) {
            QVERIFY2(encoder.addFrame(gradientFrame(16, 16, i), longTimes.at(i), &error),
                     qPrintable(error));
        }
        QVERIFY2(encoder.finish(&error), qPrintable(error));
        const GifBlocks longGif = inspectGif(readAll(longPath));
        QCOMPARE(longGif.delaysCs.size(), longTimes.size());
        int total = 0;
        for (int delay : longGif.delaysCs) {
            QVERIFY(delay == 6 || delay == 7);
            total += delay;
        }
        QCOMPARE(total, int((longTimes.last() + 67 + 5) / 10));
    }

    void offsetsTheTimelineAndMergesZeroDelays()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath("clip.gif");

        GifEncoder encoder;
        QString error;
        QVERIFY2(encoder.begin(path, QSize(16, 16), AnimationParams{10, 0, 0}, &error),
                 qPrintable(error));
        // A trim a minute in, with a repeated timestamp: the repeat is nudged 1 ms, which
        // rounds to no delay at all, so the first frame is dropped and its time goes to
        // the second. The third frame is untouched.
        QVERIFY2(encoder.addFrame(solidFrame(16, 16, Qt::red), 60000, &error), qPrintable(error));
        QVERIFY2(encoder.addFrame(solidFrame(16, 16, Qt::green), 60000, &error), qPrintable(error));
        QVERIFY2(encoder.addFrame(solidFrame(16, 16, Qt::blue), 60100, &error), qPrintable(error));
        QVERIFY2(encoder.finish(&error), qPrintable(error));

        const GifBlocks gif = inspectGif(readAll(path));
        QVERIFY(gif.valid);
        QCOMPARE(gif.frameSizes.size(), 2);
        QCOMPARE(gif.delaysCs, (QVector<int>{10, 10}));

        if (!qtReadsGif())
            QSKIP("Qt has no gif image plugin here");
        QImageReader reader(path);
        const QImage first = reader.read();
        QCOMPARE(first.pixelColor(8, 8), QColor(Qt::green));
    }

    void singleFrameIsAOneFrameGif()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath("clip.gif");

        GifEncoder encoder;
        QString error;
        QVERIFY2(encoder.begin(path, QSize(16, 16), AnimationParams{}, &error), qPrintable(error));
        QVERIFY2(encoder.addFrame(solidFrame(16, 16, Qt::red), 0, &error), qPrintable(error));
        QVERIFY2(encoder.finish(&error), qPrintable(error));

        // Unlike WebP, the container stays the same for one frame: a loop block and a
        // frame that shows for one interval.
        const GifBlocks gif = inspectGif(readAll(path));
        QVERIFY(gif.valid);
        QCOMPARE(gif.frameSizes.size(), 1);
        QCOMPARE(gif.delaysCs, (QVector<int>{10}));
        QCOMPARE(gif.loopCount, 0);

        if (!qtReadsGif())
            QSKIP("Qt has no gif image plugin here");
        QImageReader reader(path);
        const QImage still = reader.read();
        QCOMPARE(still.size(), QSize(16, 16));
        QCOMPARE(still.pixelColor(8, 8), QColor(Qt::red));   // a flat color survives exactly
    }

    void scalesAFrameThatDoesNotMatchTheCanvas()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath("clip.gif");

        GifEncoder encoder;
        QString error;
        QVERIFY2(encoder.begin(path, QSize(16, 16), AnimationParams{}, &error), qPrintable(error));
        QVERIFY2(encoder.addFrame(gradientFrame(64, 64, 0), 0, &error), qPrintable(error));
        QVERIFY2(encoder.addFrame(gradientFrame(16, 16, 1), 100, &error), qPrintable(error));
        QVERIFY2(encoder.finish(&error), qPrintable(error));

        const GifBlocks gif = inspectGif(readAll(path));
        QVERIFY(gif.valid);
        QCOMPARE(gif.screen, QSize(16, 16));
        QCOMPARE(gif.frameSizes, (QVector<QSize>{QSize(16, 16), QSize(16, 16)}));
    }

    void qtReadsBackFramesDelaysAndLoop()
    {
        if (!qtReadsGif())
            QSKIP("Qt has no gif image plugin here");

        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath("clip.gif");

        GifEncoder encoder;
        QString error;
        // loopCount 3 so the loop metadata is distinguishable from the 0 = forever default.
        QVERIFY2(encoder.begin(path, QSize(32, 24), AnimationParams{15, 0, 3}, &error),
                 qPrintable(error));
        QVERIFY2(encoder.addFrame(solidFrame(32, 24, Qt::red), 0, &error), qPrintable(error));
        QVERIFY2(encoder.addFrame(solidFrame(32, 24, Qt::blue), 67, &error), qPrintable(error));
        QVERIFY2(encoder.addFrame(solidFrame(32, 24, Qt::green), 134, &error), qPrintable(error));
        QVERIFY2(encoder.finish(&error), qPrintable(error));

        QCOMPARE(inspectGif(readAll(path)).loopCount, 3);

        QImageReader reader(path);
        QVERIFY(reader.canRead());
        QVERIFY(reader.supportsAnimation());
        QCOMPARE(reader.size(), QSize(32, 24));
        QCOMPARE(reader.imageCount(), 3);
        QCOMPARE(reader.loopCount(), 3);
        const QVector<QColor> colors{Qt::red, Qt::blue, Qt::green};
        const QVector<int> delaysMs{70, 60, 70};
        for (int i = 0; i < colors.size(); ++i) {
            const QImage frame = reader.read();
            QVERIFY2(!frame.isNull(), qPrintable(reader.errorString()));
            QCOMPARE(frame.pixelColor(16, 12), colors.at(i));
            QCOMPARE(reader.nextImageDelay(), delaysMs.at(i));
        }
    }

    void failuresLeaveNothingBehind()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath("clip.gif");

        GifEncoder encoder;
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

        // An unwritable destination fails in begin(), naming the path.
        error.clear();
        const QString missing = dir.filePath("missing/clip.gif");
        QVERIFY(!encoder.begin(missing, QSize(8, 8), AnimationParams{}, &error));
        QVERIFY2(error.contains(missing), qPrintable(error));
        QVERIFY(!QFile::exists(missing));

        // begin() with no frames: no file, and not even QSaveFile's temp left over.
        error.clear();
        QVERIFY2(encoder.begin(path, QSize(8, 8), AnimationParams{}, &error), qPrintable(error));
        QVERIFY(!encoder.finish(&error));
        QVERIFY(!error.isEmpty());
        QVERIFY(!QFile::exists(path));
        QVERIFY(QDir(dir.path()).entryList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot)
                    .isEmpty());
    }

    void cancelDiscardsThePartialFileAndAllowsReuse()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath("clip.gif");

        GifEncoder encoder;
        QString error;
        QVERIFY2(encoder.begin(path, QSize(16, 16), AnimationParams{}, &error), qPrintable(error));
        QVERIFY2(encoder.addFrame(solidFrame(16, 16, Qt::red), 0, &error), qPrintable(error));
        QVERIFY2(encoder.addFrame(solidFrame(16, 16, Qt::blue), 100, &error), qPrintable(error));
        encoder.cancel();
        QVERIFY(!QFile::exists(path));
        QVERIFY(QDir(dir.path()).entryList(QDir::Files).isEmpty());

        // The same instance is reusable, which the editor relies on across exports.
        QVERIFY2(encoder.begin(path, QSize(16, 16), AnimationParams{}, &error), qPrintable(error));
        QVERIFY2(encoder.addFrame(solidFrame(16, 16, Qt::red), 0, &error), qPrintable(error));
        QVERIFY2(encoder.finish(&error), qPrintable(error));
        QVERIFY(inspectGif(readAll(path)).valid);
    }

    void destroyingMidEncodeLeavesNoFile()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath("clip.gif");
        {
            GifEncoder encoder;
            QString error;
            QVERIFY2(encoder.begin(path, QSize(16, 16), AnimationParams{}, &error),
                     qPrintable(error));
            QVERIFY2(encoder.addFrame(solidFrame(16, 16, Qt::red), 0, &error), qPrintable(error));
        }
        QVERIFY(QDir(dir.path()).entryList(QDir::Files).isEmpty());
    }
};

QTEST_MAIN(tst_GifEncoder)
#include "tst_gifencoder.moc"
