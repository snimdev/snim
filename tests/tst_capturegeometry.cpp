#include <QtTest>

#include "capture/CaptureGeometry.h"

using namespace Capture;

// The virtual-desktop -> physical-pixmap crop shared by every capture strategy.
// Pure geometry, so it runs headless: the multi-monitor and HiDPI cases that are
// impossible to exercise by hand (a 2x screen, a monitor left of the primary, a
// selection dragged past the frozen frame) are locked down here instead.
class tst_CaptureGeometry : public QObject
{
    Q_OBJECT

private slots:
    void cropsAtDevicePixelRatio()
    {
        // 100x100 logical desktop captured at 2x -> a 200x200 physical pixmap.
        const QRect virt(0, 0, 100, 100);
        QCOMPARE(physicalCropRect(QRect(10, 10, 20, 20), virt, 2.0, QSize(200, 200)),
                 QRect(20, 20, 40, 40));
        QCOMPARE(physicalCropRect(QRect(10, 10, 20, 20), virt, 1.0, QSize(100, 100)),
                 QRect(10, 10, 20, 20));   // 1x is a straight pass-through

        QPixmap shot(200, 200);
        shot.fill(Qt::black);
        shot.setDevicePixelRatio(2.0);
        const QPixmap cropped = cropVirtualArea(shot, virt, QRect(10, 10, 20, 20));
        QCOMPARE(cropped.size(), QSize(40, 40));            // physical pixels
        QCOMPARE(cropped.devicePixelRatio(), 2.0);          // DPR restored
    }

    void mapsVirtualOffset()
    {
        // Two 1920x1080 screens with the secondary to the LEFT: the virtual origin
        // is negative, so the offset subtraction must not be assumed to be a no-op.
        const QRect virt(-1920, 0, 3840, 1080);
        QCOMPARE(physicalCropRect(QRect(-1920, 0, 100, 50), virt, 1.0, QSize(3840, 1080)),
                 QRect(0, 0, 100, 50));                     // top-left of the left screen
        QCOMPARE(physicalCropRect(QRect(-1820, 10, 10, 10), virt, 1.0, QSize(3840, 1080)),
                 QRect(100, 10, 10, 10));
        QCOMPARE(physicalCropRect(QRect(0, 0, 10, 10), virt, 1.0, QSize(3840, 1080)),
                 QRect(1920, 0, 10, 10));                   // primary starts mid-pixmap

        // Negative origin and 2x together: the offset is subtracted before scaling.
        const QRect hidpi(-100, -50, 400, 300);
        QCOMPARE(physicalCropRect(QRect(-90, -40, 10, 10), hidpi, 2.0, QSize(800, 600)),
                 QRect(20, 20, 20, 20));
    }

    void clampsOutOfBounds()
    {
        // A selection dragged past the frozen frame is intersected, never overflowing.
        const QRect virt(0, 0, 100, 100);
        QCOMPARE(physicalCropRect(QRect(80, 80, 40, 40), virt, 1.0, QSize(100, 100)),
                 QRect(80, 80, 20, 20));
        QCOMPARE(physicalCropRect(QRect(80, 80, 40, 40), virt, 2.0, QSize(200, 200)),
                 QRect(160, 160, 40, 40));

        QPixmap shot(100, 100);
        shot.fill(Qt::black);
        QCOMPARE(cropVirtualArea(shot, virt, QRect(80, 80, 40, 40)).size(), QSize(20, 20));
    }

    void fullyOutOfBoundsYieldsNull()
    {
        // QPixmap::copy() treats a NULL rect as "copy everything", so a selection that
        // clamps away to nothing must be rejected before it reaches copy(), or the user
        // silently gets the whole desktop instead of their selection.
        const QRect virt(0, 0, 100, 100);
        QVERIFY(physicalCropRect(QRect(500, 500, 50, 50), virt, 1.0, QSize(100, 100)).isEmpty());

        QPixmap shot(100, 100);
        shot.fill(Qt::black);
        const QPixmap cropped = cropVirtualArea(shot, virt, QRect(500, 500, 50, 50));
        QVERIFY2(cropped.isNull(),
                 qPrintable(QStringLiteral("got a %1x%2 pixmap instead of nothing")
                                .arg(cropped.width()).arg(cropped.height())));
    }

    void emptySelectionYieldsNull()
    {
        const QRect virt(0, 0, 100, 100);
        QVERIFY(physicalCropRect(QRect(), virt, 2.0, QSize(200, 200)).isEmpty());
        QVERIFY(physicalCropRect(QRect(10, 10, 0, 0), virt, 2.0, QSize(200, 200)).isEmpty());

        QPixmap shot(200, 200);
        shot.fill(Qt::black);
        shot.setDevicePixelRatio(2.0);
        QVERIFY(cropVirtualArea(shot, virt, QRect()).isNull());        // nothing selected
        QVERIFY(cropVirtualArea(QPixmap(), virt, QRect(0, 0, 10, 10)).isNull()); // no frame
    }
};

QTEST_MAIN(tst_CaptureGeometry)
#include "tst_capturegeometry.moc"
