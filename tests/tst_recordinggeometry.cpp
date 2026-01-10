#include <QtTest>

#include "recording/RecordingGeometry.h"

using namespace Recording;

class tst_RecordingGeometry : public QObject
{
    Q_OBJECT

private slots:
    void portalCropIdentityScale()
    {
        // X11 portal: buffer size == logical size, so the crop is just the local rect.
        const QRect stream(1920, 0, 1920, 1080);
        const StreamCrop c = portalStreamCrop(QRect(1920 + 100, 50, 200, 100),
                                              stream, QSize(1920, 1080), true);
        QVERIFY(c.valid);
        QCOMPARE(c.cropPx, QRect(100, 50, 200, 100));
        QCOMPARE(c.outputPx, QSize(200, 100));
    }

    void portalCropFractionalScale()
    {
        // 2560x1440 buffer over a 1707x960 logical rect: 1.4997x by X, 1.5x by Y.
        const QRect stream(0, 0, 1707, 960);
        const StreamCrop c = portalStreamCrop(QRect(100, 50, 401, 301),
                                              stream, QSize(2560, 1440), true);
        QVERIFY(c.valid);
        QCOMPARE(c.cropPx, QRect(150, 75, 600, 452));       // 601.4 -> 601 -> even 600
        QCOMPARE(c.outputPx, QSize(600, 452));
    }

    void portalCropStraddlingEdge()
    {
        // Region hanging off the right edge: only the intersection is cropped.
        const QRect stream(0, 0, 1920, 1080);
        const StreamCrop c = portalStreamCrop(QRect(1800, 100, 400, 200),
                                              stream, QSize(1920, 1080), true);
        QVERIFY(c.valid);
        QCOMPARE(c.cropPx, QRect(1800, 100, 120, 200));
        QCOMPARE(c.outputPx, QSize(120, 200));
    }

    void portalCropOutsideStream()
    {
        const QRect stream(0, 0, 1920, 1080);
        QVERIFY(!portalStreamCrop(QRect(2000, 0, 300, 300), stream,
                                  QSize(1920, 1080), true).valid);
        QVERIFY(!portalStreamCrop(QRect(), stream, QSize(1920, 1080), true).valid);
        QVERIFY(!portalStreamCrop(QRect(0, 0, 100, 100), stream, QSize(), true).valid);

        // Intersection thinner than 2px rounds down to zero: nothing encodable.
        QVERIFY(!portalStreamCrop(QRect(1919, 100, 200, 200), stream,
                                  QSize(1920, 1080), true).valid);
    }

    void portalCropRoundsDownToEven()
    {
        const QRect stream(0, 0, 1920, 1080);
        const StreamCrop c = portalStreamCrop(QRect(11, 21, 101, 51),
                                              stream, QSize(1920, 1080), true);
        QVERIFY(c.valid);
        QCOMPARE(c.cropPx, QRect(11, 21, 100, 50));         // origin keeps its odd offset
        QCOMPARE(c.outputPx, QSize(100, 50));
    }

    void portalCropWithoutRetinaDownscales()
    {
        // 2x display, retina off: crop stays native, output falls back to logical size.
        const QRect stream(0, 0, 1280, 720);
        const StreamCrop c = portalStreamCrop(QRect(0, 0, 101, 51),
                                              stream, QSize(2560, 1440), false);
        QVERIFY(c.valid);
        QCOMPARE(c.cropPx, QRect(0, 0, 202, 102));
        QCOMPARE(c.outputPx, QSize(100, 50));
    }

    void resolveStreamRectWorkspaceShare()
    {
        const QVector<StreamSource> screens{{QRect(0, 0, 2560, 1440), QSize(2560, 1440)},
                                            {QRect(2560, 0, 2560, 1440), QSize(2560, 1440)}};
        const StreamSource desktop{QRect(0, 0, 5120, 1440), QSize(5120, 1440)};
        QCOMPARE(resolveStreamRect(QSize(5120, 1440), screens, desktop,
                                   QRect(2700, 200, 800, 500)),
                 QRect(0, 0, 5120, 1440));
    }

    void resolveStreamRectUniqueMonitor()
    {
        const QVector<StreamSource> screens{{QRect(0, 0, 2560, 1440), QSize(2560, 1440)},
                                            {QRect(2560, 0, 1920, 1080), QSize(1920, 1080)}};
        const StreamSource desktop{QRect(0, 0, 4480, 1440), QSize(4480, 1440)};

        // Only one screen has this pixel size, so the region's place cannot change it.
        QCOMPARE(resolveStreamRect(QSize(1920, 1080), screens, desktop, QRect(10, 10, 100, 100)),
                 QRect(2560, 0, 1920, 1080));
        QCOMPARE(resolveStreamRect(QSize(1920, 1080), screens, desktop,
                                   QRect(2600, 10, 100, 100)),
                 QRect(2560, 0, 1920, 1080));
    }

    void resolveStreamRectAmbiguousPicksRegionScreen()
    {
        const QVector<StreamSource> screens{{QRect(0, 0, 2560, 1440), QSize(2560, 1440)},
                                            {QRect(2560, 0, 2560, 1440), QSize(2560, 1440)}};
        const StreamSource desktop{QRect(0, 0, 5120, 1440), QSize(5120, 1440)};
        QCOMPARE(resolveStreamRect(QSize(2560, 1440), screens, desktop,
                                   QRect(2700, 200, 800, 500)),
                 QRect(2560, 0, 2560, 1440));
    }

    void resolveStreamRectAmbiguousFallsBackToFirst()
    {
        const QVector<StreamSource> screens{{QRect(0, 0, 2560, 1440), QSize(2560, 1440)},
                                            {QRect(2560, 0, 2560, 1440), QSize(2560, 1440)}};
        const StreamSource desktop{QRect(0, 0, 5120, 1440), QSize(5120, 1440)};
        QCOMPARE(resolveStreamRect(QSize(2560, 1440), screens, desktop,
                                   QRect(-900, -900, 100, 100)),
                 QRect(0, 0, 2560, 1440));
    }

    void resolveStreamRectHiDpi()
    {
        // 1.5x scaling: the caps match the pixel size, the answer is the logical rect.
        const QVector<StreamSource> screens{{QRect(0, 0, 1707, 960), QSize(2560, 1440)}};
        const StreamSource desktop{QRect(0, 0, 1707, 960), QSize(2560, 1440)};
        QCOMPARE(resolveStreamRect(QSize(2560, 1440), screens, desktop, QRect(10, 10, 100, 100)),
                 QRect(0, 0, 1707, 960));
    }

    void resolveStreamRectNoMatchUsesRegionScreen()
    {
        const QVector<StreamSource> screens{{QRect(0, 0, 2560, 1440), QSize(2560, 1440)},
                                            {QRect(2560, 0, 1920, 1080), QSize(1920, 1080)}};
        const StreamSource desktop{QRect(0, 0, 4480, 1440), QSize(4480, 1440)};
        QCOMPARE(resolveStreamRect(QSize(1234, 777), screens, desktop, QRect(2700, 200, 80, 50)),
                 QRect(2560, 0, 1920, 1080));

        // Nothing matches and the region is nowhere: no guess left.
        QVERIFY(resolveStreamRect(QSize(1234, 777), screens, desktop,
                                  QRect(-900, -900, 10, 10)).isEmpty());
    }

    void portalCropOnWorkspaceShareIsUnscaled()
    {
        // End to end: workspace share of two 2560x1440 monitors, selection on the second.
        const QVector<StreamSource> screens{{QRect(0, 0, 2560, 1440), QSize(2560, 1440)},
                                            {QRect(2560, 0, 2560, 1440), QSize(2560, 1440)}};
        const StreamSource desktop{QRect(0, 0, 5120, 1440), QSize(5120, 1440)};
        const QRect region(2700, 200, 800, 500);

        const QRect stream = resolveStreamRect(QSize(5120, 1440), screens, desktop, region);
        const StreamCrop c = portalStreamCrop(region, stream, QSize(5120, 1440), true);
        QVERIFY(c.valid);
        QCOMPARE(c.cropPx, QRect(2700, 200, 800, 500));
        QCOMPARE(c.outputPx, QSize(800, 500));
    }
};

QTEST_MAIN(tst_RecordingGeometry)
#include "tst_recordinggeometry.moc"
