#include <QtTest>

#include "screen/DesktopStitch.h"

using namespace Screen;

// Frames arrive one per monitor (ScreenCast streams, KWin screens, screencopy outputs);
// these lock down how they become the one virtual-desktop image AreaSelector crops from.
class tst_ScreencastStitch : public QObject
{
    Q_OBJECT

    static QImage solid(int width, int height, QColor color)
    {
        QImage image(width, height, QImage::Format_RGB32);
        image.fill(color);
        return image;
    }

    // What ScreencastFrameSource does with the streams Start returned.
    static QImage stitchStreams(const QList<PlacedFrame> &streams, const QRect &fallbackDesktop,
                                QRect *covered)
    {
        const QList<PlacedFrame> frames = placeFramesWithoutGeometry(streams, fallbackDesktop);
        *covered = coveredGeometry(frames);
        return stitchDesktop(frames, *covered);
    }

private slots:
    void placesTwoMonitorsSideBySide()
    {
        QRect covered;
        const QImage desktop = stitchStreams(
            {{QRect(0, 0, 2560, 1440), solid(2560, 1440, Qt::red)},
             {QRect(2560, 0, 2560, 1440), solid(2560, 1440, Qt::blue)}},
            QRect(), &covered);
        QCOMPARE(covered, QRect(0, 0, 5120, 1440));
        QCOMPARE(desktop.size(), QSize(5120, 1440));
        QCOMPARE(desktop.devicePixelRatio(), 1.0);
        QCOMPARE(desktop.pixelColor(2559, 700), QColor(Qt::red));
        QCOMPARE(desktop.pixelColor(2560, 700), QColor(Qt::blue));
    }

    void keepsANegativeOriginAndFillsGapsBlack()
    {
        QRect covered;
        const QImage desktop = stitchStreams(
            {{QRect(-1920, 360, 1920, 1080), solid(1920, 1080, Qt::green)},
             {QRect(0, 0, 2560, 1440), solid(2560, 1440, Qt::red)}},
            QRect(), &covered);
        QCOMPARE(covered, QRect(-1920, 0, 4480, 1440));
        QCOMPARE(desktop.size(), QSize(4480, 1440));
        QCOMPARE(desktop.pixelColor(0, 0), QColor(Qt::black));
        QCOMPARE(desktop.pixelColor(0, 360), QColor(Qt::green));
        QCOMPARE(desktop.pixelColor(1920, 0), QColor(Qt::red));
    }

    void tagsTheHighestDprAndScalesTheLowerOne()
    {
        // A 2x laptop panel beside a 1x monitor, both 1280x800 logical.
        QRect covered;
        const QImage desktop = stitchStreams(
            {{QRect(0, 0, 1280, 800), solid(2560, 1600, Qt::red)},
             {QRect(1280, 0, 1280, 800), solid(1280, 800, Qt::blue)}},
            QRect(), &covered);
        QCOMPARE(covered, QRect(0, 0, 2560, 800));
        QCOMPARE(desktop.devicePixelRatio(), 2.0);
        QCOMPARE(desktop.size(), QSize(5120, 1600));
        QCOMPARE(desktop.pixelColor(5119, 1599), QColor(Qt::blue));
    }

    void aLoneStreamWithoutGeometryIsTheWholeDesktop()
    {
        QRect covered;
        const QImage desktop = stitchStreams({{QRect(), solid(3840, 2160, Qt::red)}},
                                             QRect(0, 0, 1920, 1080), &covered);
        QCOMPARE(covered, QRect(0, 0, 1920, 1080));
        QCOMPARE(desktop.devicePixelRatio(), 2.0);
        QCOMPARE(desktop.size(), QSize(3840, 2160));
    }

    void streamsWithoutGeometryLineUpAfterTheKnownOnes()
    {
        const QList<PlacedFrame> frames = placeFramesWithoutGeometry(
            {{QRect(0, 0, 100, 50), solid(100, 50, Qt::red)},
             {QRect(), solid(30, 20, Qt::blue)},
             {QRect(), solid(40, 20, Qt::green)}},
            QRect(0, 0, 999, 999));
        QCOMPARE(frames.at(1).logical, QRect(100, 0, 30, 20));
        QCOMPARE(frames.at(2).logical, QRect(130, 0, 40, 20));
    }

    void nothingUsableGivesANullImage()
    {
        QRect covered;
        QVERIFY(stitchStreams({}, QRect(), &covered).isNull());
        QVERIFY(covered.isEmpty());
        QVERIFY(stitchStreams({{QRect(0, 0, 10, 10), QImage()}}, QRect(), &covered).isNull());
        QVERIFY(covered.isEmpty());
    }

    // The shared stitcher behind every frame source.

    void sameScaleScreensAreCopiedPixelForPixel()
    {
        const QImage desktop = stitchDesktop(
            {{QRect(0, 0, 1280, 720), solid(2560, 1440, Qt::red)},
             {QRect(1280, 0, 1280, 720), solid(2560, 1440, Qt::blue)}},
            QRect(0, 0, 2560, 720));
        QCOMPARE(desktop.size(), QSize(5120, 1440));
        QCOMPARE(desktop.devicePixelRatio(), 2.0);
        QCOMPARE(desktop.pixelColor(2559, 1439), QColor(Qt::red));
        QCOMPARE(desktop.pixelColor(2560, 0), QColor(Qt::blue));
    }

    void aSparserScreenFillsItsWholeArea()
    {
        // A 2x panel beside a 1x monitor: the 1x frame is scaled up, not drawn a quarter size.
        const QImage desktop = stitchDesktop(
            {{QRect(0, 0, 1280, 800), solid(2560, 1600, Qt::red)},
             {QRect(1280, 0, 1280, 800), solid(1280, 800, Qt::blue)}},
            QRect(0, 0, 2560, 800));
        QCOMPARE(desktop.devicePixelRatio(), 2.0);
        QCOMPARE(desktop.size(), QSize(5120, 1600));
        QCOMPARE(desktop.pixelColor(2560, 0), QColor(Qt::blue));
        QCOMPARE(desktop.pixelColor(5119, 1599), QColor(Qt::blue));
        QCOMPARE(desktop.pixelColor(3900, 1300), QColor(Qt::blue));
    }

    void negativeOriginsKeepTheirPlaceAndGapsStayBlack()
    {
        // Native-origin layout: a 1.5x screen left of a 1x primary leaves a logical gap.
        const QImage desktop = stitchDesktop(
            {{QRect(-1200, 0, 800, 600), solid(1200, 900, Qt::green)},
             {QRect(0, 0, 800, 600), solid(800, 600, Qt::red)},
             {QRect(800, 0, 800, 600), solid(1200, 900, Qt::blue)}},
            QRect(-1200, 0, 2800, 600));
        QCOMPARE(desktop.devicePixelRatio(), 1.5);
        QCOMPARE(desktop.size(), QSize(4200, 900));
        QCOMPARE(desktop.pixelColor(0, 0), QColor(Qt::green));
        QCOMPARE(desktop.pixelColor(1199, 899), QColor(Qt::green));
        QCOMPARE(desktop.pixelColor(1200, 0), QColor(Qt::black));      // the gap
        QCOMPARE(desktop.pixelColor(1799, 899), QColor(Qt::black));
        QCOMPARE(desktop.pixelColor(1800, 0), QColor(Qt::red));
        QCOMPARE(desktop.pixelColor(2999, 899), QColor(Qt::red));       // 1x, scaled up
        QCOMPARE(desktop.pixelColor(3000, 0), QColor(Qt::blue));
    }

    void screensOfDifferentHeightsLeaveBlackBelow()
    {
        const QImage desktop = stitchDesktop(
            {{QRect(0, 0, 100, 50), solid(100, 50, Qt::red)},
             {QRect(0, 50, 60, 40), solid(60, 40, Qt::blue)}},
            QRect(0, 0, 100, 90));
        QCOMPARE(desktop.size(), QSize(100, 90));
        QCOMPARE(desktop.pixelColor(10, 60), QColor(Qt::blue));
        QCOMPARE(desktop.pixelColor(80, 60), QColor(Qt::black));
    }

    void fractionalScalesKeepTheDenseFrameIntact()
    {
        // 2560x1440 at 1.25 is 2048x1152 logical, beside a 1x 1920x1080 monitor.
        const QImage desktop = stitchDesktop(
            {{QRect(0, 0, 2048, 1152), solid(2560, 1440, Qt::red)},
             {QRect(2048, 0, 1920, 1080), solid(1920, 1080, Qt::blue)}},
            QRect(0, 0, 3968, 1152));
        QCOMPARE(desktop.devicePixelRatio(), 1.25);
        QCOMPARE(desktop.size(), QSize(4960, 1440));
        QCOMPARE(desktop.pixelColor(2559, 1439), QColor(Qt::red));
        QCOMPARE(desktop.pixelColor(2560, 0), QColor(Qt::blue));
        QCOMPARE(desktop.pixelColor(4959, 1349), QColor(Qt::blue));
        QCOMPARE(desktop.pixelColor(4000, 1400), QColor(Qt::black));
    }

    void aTruncatedLogicalSizeTakesTheSmallerRatio()
    {
        // 1600x1200 at 1.5 reports 1066x800 logical: the ratio is 1.5, not 1.5009.
        QCOMPARE(frameDevicePixelRatio(solid(1600, 1200, Qt::red), QRect(0, 0, 1066, 800)), 1.5);
        QCOMPARE(frameDevicePixelRatio(QImage(), QRect(0, 0, 10, 10)), 0.0);
        QCOMPARE(frameDevicePixelRatio(solid(10, 10, Qt::red), QRect()), 0.0);
    }

    void aLoneNativeFrameIsTaggedNotRepainted()
    {
        const QImage frame = solid(3840, 2160, Qt::red);
        const QImage desktop = stitchDesktop({{QRect(0, 0, 1920, 1080), frame}}, QRect(0, 0, 1920, 1080));
        QCOMPARE(desktop.size(), frame.size());
        QCOMPARE(desktop.devicePixelRatio(), 2.0);
        QCOMPARE(desktop.format(), frame.format());
        QCOMPARE(desktop.pixelColor(3839, 2159), QColor(Qt::red));
    }

    void stitchingNothingUsableIsNull()
    {
        QVERIFY(stitchDesktop({}, QRect(0, 0, 10, 10)).isNull());
        QVERIFY(stitchDesktop({{QRect(0, 0, 10, 10), QImage()}}, QRect(0, 0, 10, 10)).isNull());
        QVERIFY(stitchDesktop({{QRect(), solid(10, 10, Qt::red)}}, QRect(0, 0, 10, 10)).isNull());
        QVERIFY(stitchDesktop({{QRect(0, 0, 10, 10), solid(10, 10, Qt::red)}}, QRect()).isNull());
    }
};

QTEST_MAIN(tst_ScreencastStitch)
#include "tst_screencaststitch.moc"
