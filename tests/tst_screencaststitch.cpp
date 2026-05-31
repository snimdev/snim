#include <QtTest>

#include "capture/ScreencastStitch.h"

using namespace Capture;

// ScreenCast streams arrive one per monitor; these lock down how they become the one
// virtual-desktop image AreaSelector crops from.
class tst_ScreencastStitch : public QObject
{
    Q_OBJECT

    static QImage solid(int width, int height, QColor color)
    {
        QImage image(width, height, QImage::Format_RGB32);
        image.fill(color);
        return image;
    }

private slots:
    void placesTwoMonitorsSideBySide()
    {
        const StitchedDesktop desktop = stitchStreams(
            {{QRect(0, 0, 2560, 1440), solid(2560, 1440, Qt::red)},
             {QRect(2560, 0, 2560, 1440), solid(2560, 1440, Qt::blue)}},
            QRect());
        QCOMPARE(desktop.virtualGeometry, QRect(0, 0, 5120, 1440));
        QCOMPARE(desktop.image.size(), QSize(5120, 1440));
        QCOMPARE(desktop.image.devicePixelRatio(), 1.0);
        QCOMPARE(desktop.image.pixelColor(2559, 700), QColor(Qt::red));
        QCOMPARE(desktop.image.pixelColor(2560, 700), QColor(Qt::blue));
    }

    void keepsANegativeOriginAndFillsGapsBlack()
    {
        const StitchedDesktop desktop = stitchStreams(
            {{QRect(-1920, 360, 1920, 1080), solid(1920, 1080, Qt::green)},
             {QRect(0, 0, 2560, 1440), solid(2560, 1440, Qt::red)}},
            QRect());
        QCOMPARE(desktop.virtualGeometry, QRect(-1920, 0, 4480, 1440));
        QCOMPARE(desktop.image.size(), QSize(4480, 1440));
        QCOMPARE(desktop.image.pixelColor(0, 0), QColor(Qt::black));
        QCOMPARE(desktop.image.pixelColor(0, 360), QColor(Qt::green));
        QCOMPARE(desktop.image.pixelColor(1920, 0), QColor(Qt::red));
    }

    void tagsTheHighestDprAndScalesTheLowerOne()
    {
        // A 2x laptop panel beside a 1x monitor, both 1280x800 logical.
        const StitchedDesktop desktop = stitchStreams(
            {{QRect(0, 0, 1280, 800), solid(2560, 1600, Qt::red)},
             {QRect(1280, 0, 1280, 800), solid(1280, 800, Qt::blue)}},
            QRect());
        QCOMPARE(desktop.virtualGeometry, QRect(0, 0, 2560, 800));
        QCOMPARE(desktop.image.devicePixelRatio(), 2.0);
        QCOMPARE(desktop.image.size(), QSize(5120, 1600));
        QCOMPARE(desktop.image.pixelColor(5119, 1599), QColor(Qt::blue));
    }

    void aLoneStreamWithoutGeometryIsTheWholeDesktop()
    {
        const StitchedDesktop desktop = stitchStreams(
            {{QRect(), solid(3840, 2160, Qt::red)}}, QRect(0, 0, 1920, 1080));
        QCOMPARE(desktop.virtualGeometry, QRect(0, 0, 1920, 1080));
        QCOMPARE(desktop.image.devicePixelRatio(), 2.0);
        QCOMPARE(desktop.image.size(), QSize(3840, 2160));
    }

    void streamsWithoutGeometryLineUpAfterTheKnownOnes()
    {
        const QList<StreamFrame> frames = resolveStreamGeometry(
            {{QRect(0, 0, 100, 50), solid(100, 50, Qt::red)},
             {QRect(), solid(30, 20, Qt::blue)},
             {QRect(), solid(40, 20, Qt::green)}},
            QRect(0, 0, 999, 999));
        QCOMPARE(frames.at(1).logical, QRect(100, 0, 30, 20));
        QCOMPARE(frames.at(2).logical, QRect(130, 0, 40, 20));
    }

    void nothingUsableGivesANullImage()
    {
        QVERIFY(stitchStreams({}, QRect()).image.isNull());
        QVERIFY(stitchStreams({{QRect(0, 0, 10, 10), QImage()}}, QRect()).image.isNull());
    }
};

QTEST_MAIN(tst_ScreencastStitch)
#include "tst_screencaststitch.moc"
