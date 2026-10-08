#include <QRegion>
#include <QtTest>

#include "record/RecordingGeometry.h"

using namespace Record;

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

    void windowStreamKeepsTheWholeWindow()
    {
        // A 640x400 window on a 2x output streams 1280x800 pixels.
        const StreamCrop crop = windowStreamCrop(QSize(640, 400), QSize(1280, 800), true);
        QVERIFY(crop.valid);
        QCOMPARE(crop.cropPx, QRect(0, 0, 1280, 800));
        QCOMPARE(crop.outputPx, QSize(1280, 800));
        QCOMPARE(windowStreamCrop(QSize(640, 400), QSize(1280, 800), false).outputPx,
                 QSize(640, 400));
    }

    void windowStreamWithoutGeometryKeepsItsPixels()
    {
        // No size from the portal, and an odd one from the window: even, never cropped off.
        const StreamCrop crop = windowStreamCrop(QSize(), QSize(801, 451), false);
        QVERIFY(crop.valid);
        QCOMPARE(crop.cropPx, QRect(0, 0, 800, 450));
        QCOMPARE(crop.outputPx, QSize(800, 450));
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

    // The webcam bubble's park position. Screen-local because a Wayland layer surface
    // is positioned by margins against its own output.
    void bubbleParksInsideTheRegion()
    {
        const QRect screen(0, 0, 1920, 1080);
        const QSize bubble(180, 180);
        // Bottom-left inside the region, inset by the margin on both axes.
        QCOMPARE(bubbleParkPos(screen, QRect(100, 100, 800, 600), bubble, 16),
                 QPoint(116, 503));   // 699 - 180 - 16
    }

    void bubbleParkIsLocalToTheSecondaryScreen()
    {
        const QRect screen(1920, 0, 1920, 1080);
        const QSize bubble(180, 180);
        // Same region shape one screen over: the margins must not carry the 1920 offset.
        QCOMPARE(bubbleParkPos(screen, QRect(2020, 100, 800, 600), bubble, 16),
                 QPoint(116, 503));
    }

    void bubbleParkClampsToTheScreen()
    {
        const QRect screen(0, 0, 1920, 1080);
        const QSize bubble(180, 180);
        // Region straddling the right edge: the bubble is pulled back onto the screen.
        QCOMPARE(bubbleParkPos(screen, QRect(1850, 900, 400, 300), bubble, 16),
                 QPoint(1740, 900));   // x clamped to 1920-180, y to 1080-180
        // A region hanging off the left: never a negative margin.
        QCOMPARE(bubbleParkPos(screen, QRect(-300, -200, 400, 400), bubble, 16),
                 QPoint(0, 3));   // x clamped from -284, y is the region's own inset
    }

    void bubbleParkPinsTinyRegionsToTheirTop()
    {
        const QRect screen(0, 0, 1920, 1080);
        const QSize bubble(180, 180);
        // Region shorter than the bubble: pinned to the region's own top edge.
        QCOMPARE(bubbleParkPos(screen, QRect(400, 300, 300, 80), bubble, 16),
                 QPoint(416, 300));
    }

    // Dragging the circle inside the full-screen layer surface: pure clamping, so the
    // drag never needs the compositor to tell it where the surface ended up.
    void bubbleDragStaysInsideTheScreen()
    {
        const QRect bounds(0, 0, 1920, 1080);
        const QSize bubble(180, 180);
        QCOMPARE(clampBubbleTopLeft(bounds, QPoint(700, 400), bubble), QPoint(700, 400));
        QCOMPARE(clampBubbleTopLeft(bounds, QPoint(-50, -80), bubble), QPoint(0, 0));
        QCOMPARE(clampBubbleTopLeft(bounds, QPoint(5000, 5000), bubble), QPoint(1740, 900));
        // Exactly flush with the bottom-right edge is still inside.
        QCOMPARE(clampBubbleTopLeft(bounds, QPoint(1740, 900), bubble), QPoint(1740, 900));
    }

    void bubbleDragClampsOversizedBubblesToTheOrigin()
    {
        // A circle bigger than the screen pins to the top-left instead of going negative.
        QCOMPARE(clampBubbleTopLeft(QRect(0, 0, 160, 120), QPoint(40, 40), QSize(180, 180)),
                 QPoint(0, 0));
    }

    void bubbleDragRespectsNonZeroBounds()
    {
        // Same math against bounds that do not start at the origin.
        const QRect bounds(100, 50, 800, 600);
        const QSize bubble(200, 200);
        QCOMPARE(clampBubbleTopLeft(bounds, QPoint(0, 0), bubble), QPoint(100, 50));
        QCOMPARE(clampBubbleTopLeft(bounds, QPoint(5000, 5000), bubble), QPoint(700, 450));
    }

    void x11GrabAtUnitScale()
    {
        const X11Grab grab = x11Grab(QRect(100, 50, 640, 360),
                                     {X11Screen{QRect(0, 0, 3840, 1080), 1.0}}, true);
        QVERIFY(grab.valid);
        QCOMPARE(grab.rootPx, QRect(100, 50, 640, 360));
        QCOMPARE(grab.outputPx, QSize(640, 360));
    }

    void x11GrabFullScreenOfTheSecondMonitor()
    {
        const QVector<X11Screen> screens{X11Screen{QRect(0, 0, 1920, 1080), 1.0},
                                         X11Screen{QRect(1920, 0, 1920, 1080), 1.0}};
        const X11Grab grab = x11Grab(QRect(1920, 0, 1920, 1080), screens, true);
        QVERIFY(grab.valid);
        QCOMPARE(grab.rootPx, QRect(1920, 0, 1920, 1080));
        QCOMPARE(grab.rootPx.right(), 3839);   // ximagesrc's inclusive endx
    }

    void x11GrabScalesFromTheScreenOrigin()
    {
        // QT_SCALE_FACTOR=2 over two 1920x1080 monitors: Qt keeps the second monitor at
        // x=1920 and halves only its size, so the logical desktop has a gap.
        const QVector<X11Screen> screens{X11Screen{QRect(0, 0, 960, 540), 2.0},
                                         X11Screen{QRect(1920, 0, 960, 540), 2.0}};
        const X11Grab grab = x11Grab(QRect(2020, 50, 320, 180), screens, true);
        QVERIFY(grab.valid);
        QCOMPARE(grab.rootPx, QRect(1920 + 200, 100, 640, 360));
        QCOMPARE(grab.outputPx, QSize(640, 360));

        const X11Grab logical = x11Grab(QRect(2020, 50, 320, 180), screens, false);
        QCOMPARE(logical.rootPx, grab.rootPx);
        QCOMPARE(logical.outputPx, QSize(320, 180));
    }

    void x11GrabFractionalScale()
    {
        // Xft.dpi 144: a 2560x1440 monitor shows as 1707x960 logical at 1.5.
        const X11Grab grab = x11Grab(QRect(0, 0, 1707, 960),
                                     {X11Screen{QRect(0, 0, 1707, 960), 1.5}}, true);
        QVERIFY(grab.valid);
        QCOMPARE(grab.rootPx, QRect(0, 0, 2560, 1440));
        QCOMPARE(grab.outputPx, QSize(2560, 1440));
    }

    void x11GrabSpansMixedScaleMonitors()
    {
        // Left 1920x1080 at 1x, right 3840x2160 at 2x placed at x=1920.
        const QVector<X11Screen> screens{X11Screen{QRect(0, 0, 1920, 1080), 1.0},
                                         X11Screen{QRect(1920, 0, 1920, 1080), 2.0}};
        const X11Grab grab = x11Grab(QRect(1820, 0, 200, 100), screens, true);
        QVERIFY(grab.valid);
        // 100 px on the left screen, 100 logical = 200 px on the right one.
        QCOMPARE(grab.rootPx, QRect(1820, 0, 300, 200));
    }

    void x11GrabClipsToTheScreens()
    {
        const QVector<X11Screen> screens{X11Screen{QRect(0, 0, 1920, 1080), 1.0}};
        const X11Grab grab = x11Grab(QRect(-100, 1000, 400, 300), screens, true);
        QVERIFY(grab.valid);
        QCOMPARE(grab.rootPx, QRect(0, 1000, 300, 80));

        QVERIFY(!x11Grab(QRect(2000, 0, 100, 100), screens, true).valid);
        QVERIFY(!x11Grab(QRect(), screens, true).valid);
    }

    void x11GrabRoundsDownToEven()
    {
        const X11Grab grab = x11Grab(QRect(11, 13, 301, 201),
                                     {X11Screen{QRect(0, 0, 1920, 1080), 1.0}}, true);
        QVERIFY(grab.valid);
        QCOMPARE(grab.rootPx, QRect(11, 13, 300, 200));
        QCOMPARE(grab.outputPx, QSize(300, 200));
    }

    void x11WindowKeepsItsFrameAndDropsTheRest()
    {
        // A 530x354 frame window with 10 px invisible borders around a 510x334 window.
        const X11WindowGrab grab = x11WindowGrab(QSize(530, 354), QMargins(10, 10, 10, 10),
                                                 QSize(408, 267), true);
        QVERIFY(grab.valid);
        QCOMPARE(grab.cropPx, QMargins(10, 10, 10, 10));
        QCOMPARE(grab.outputPx, QSize(510, 334));
        // Logical size at 1.25 without retinaCapture, evened out.
        QCOMPARE(x11WindowGrab(QSize(530, 354), QMargins(10, 10, 10, 10), QSize(408, 267),
                               false).outputPx,
                 QSize(408, 266));
    }

    void x11WindowEvensOutOddSizesByCropping()
    {
        const X11WindowGrab grab = x11WindowGrab(QSize(501, 301), QMargins(), QSize(), true);
        QVERIFY(grab.valid);
        QCOMPARE(grab.cropPx, QMargins(0, 0, 1, 1));
        QCOMPARE(grab.outputPx, QSize(500, 300));
        QVERIFY(!x11WindowGrab(QSize(20, 20), QMargins(10, 10, 10, 10), QSize(), true).valid);
    }

    void x11FrameRingHugsTheGrab()
    {
        const QRect grab(200, 150, 640, 360);
        const FrameRing ring = x11FrameRing(grab, QSize(2560, 1440), 1.0, 2);
        QVERIFY(ring.valid);
        QCOMPARE(ring.window, QRect(198, 148, 644, 364));

        // The strips are exactly the window minus the grab: no pixel inside it.
        QRegion shape;
        for (const QRect &strip : ring.strips)
            shape += strip;
        const QRect localGrab = grab.translated(-ring.window.topLeft());
        QCOMPARE(shape, QRegion(QRect(QPoint(0, 0), ring.window.size())) - localGrab);
        QCOMPARE(shape.boundingRect(), QRect(0, 0, 644, 364));
    }

    void x11FrameRingClipsAtTheScreenEdge()
    {
        // A grab touching the left and top edges keeps only its right and bottom border.
        const QSize screen(1920, 1080);
        const FrameRing ring = x11FrameRing(QRect(0, 0, 400, 300), screen, 1.0, 2);
        QVERIFY(ring.valid);
        QCOMPARE(ring.window, QRect(0, 0, 402, 302));
        QRegion shape;
        for (const QRect &strip : ring.strips)
            shape += strip;
        QCOMPARE(shape, QRegion(0, 0, 402, 302) - QRegion(0, 0, 400, 300));

        // A grab reaching the far edges loses the border there.
        const FrameRing far = x11FrameRing(QRect(1780, 900, 140, 180), screen, 1.0, 2);
        QVERIFY(far.valid);
        QCOMPARE(far.window, QRect(1778, 898, 142, 182));
    }

    void x11FrameRingNeverCoversTheScreen()
    {
        const QSize screen(1920, 1080);
        QVERIFY(!x11FrameRing(QRect(QPoint(0, 0), screen), screen, 1.0, 2).valid);
        QVERIFY(!x11FrameRing(QRect(1, 1, 1918, 1078), screen, 1.0, 2).valid);
        QVERIFY(!x11FrameRing(QRect(), screen, 1.0, 2).valid);
        QVERIFY(!x11FrameRing(QRect(10, 10, 100, 100), screen, 1.0, 0).valid);
        // One edge with room left is enough for a (partial) ring.
        QVERIFY(x11FrameRing(QRect(0, 0, 1920, 1000), screen, 1.0, 2).valid);
    }

    void x11FrameRingStaysOutOfTheGrabWhenScaled()
    {
        // Xft.dpi 120, 144, 168, 192 and 216 on a 2560x1440 monitor.
        for (const qreal dpr : {1.25, 1.5, 1.75, 2.0, 2.25}) {
            const QSize logical(qRound(2560 / dpr), qRound(1440 / dpr));
            const X11Screen screen{QRect(QPoint(0, 0), logical), dpr};
            for (int offset = 0; offset < 24; ++offset) {
                const QRect region(97 + offset, 61 + 3 * offset, 641 - offset, 359 + offset);
                const X11Grab grab = x11Grab(region, {screen}, true);
                QVERIFY(grab.valid);
                const FrameRing ring = x11FrameRing(grab.rootPx, logical, dpr, 2);
                QVERIFY(ring.valid);

                const QRegion shape = qtNativeShape(ring, dpr);
                QVERIFY2(!shape.intersects(grab.rootPx),
                         qPrintable(QStringLiteral("dpr %1 region %2,%3").arg(dpr)
                                        .arg(region.x()).arg(region.y())));
                // Every side keeps a visible border, at most one scale step from the grab.
                const QRect box = shape.boundingRect();
                const int step = qCeil(dpr);
                QVERIFY(grab.rootPx.left() - box.left() >= 1);
                QVERIFY(grab.rootPx.top() - box.top() >= 1);
                QVERIFY(box.right() - grab.rootPx.right() >= 1);
                QVERIFY(box.bottom() - grab.rootPx.bottom() >= 1);
                const QRect touching = grab.rootPx.adjusted(-step, -step, step, step);
                QVERIFY(shape.intersects(QRect(touching.left(), grab.rootPx.top(),
                                               step, grab.rootPx.height())));
                QVERIFY(shape.intersects(QRect(grab.rootPx.right() + 1, grab.rootPx.top(),
                                               step, grab.rootPx.height())));
                QVERIFY(shape.intersects(QRect(grab.rootPx.left(), touching.top(),
                                               grab.rootPx.width(), step)));
                QVERIFY(shape.intersects(QRect(grab.rootPx.left(), grab.rootPx.bottom() + 1,
                                               grab.rootPx.width(), step)));
            }
        }
    }

private:
    // Where Qt 6 puts a shaped top-level in screen pixels: toNativeWindowGeometry rounds
    // the window's position and size separately, toNativeLocalRegion maps a mask by edges.
    static QRegion qtNativeShape(const FrameRing &ring, qreal dpr)
    {
        const auto px = [dpr](int v) { return qRound(v * dpr); };
        const QRect window(px(ring.window.x()), px(ring.window.y()),
                           px(ring.window.width()), px(ring.window.height()));
        QRegion shape;
        for (const QRect &strip : ring.strips) {
            const QRect native(QPoint(px(strip.x()), px(strip.y())),
                               QPoint(px(strip.x() + strip.width()) - 1,
                                      px(strip.y() + strip.height()) - 1));
            shape += native.translated(window.topLeft()).intersected(window);
        }
        return shape;
    }
};

QTEST_MAIN(tst_RecordingGeometry)
#include "tst_recordinggeometry.moc"
