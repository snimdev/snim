#include <QtTest>

#include "recording/RecordingGeometry.h"
#include "screen/X11ScreenMap.h"

using Screen::X11ScreenMap::nativeRect;
using Screen::X11ScreenMap::toLogical;
using X11Screen = Screen::X11ScreenMap::Screen;

// Root-window pixels back to Qt's logical space: the inverse of the recorder's x11Grab.
class tst_X11ScreenMap : public QObject
{
    Q_OBJECT

private slots:
    void unitScaleIsTheIdentity()
    {
        const QVector<X11Screen> screens{{QRect(0, 0, 1920, 1080), 1.0}};
        QCOMPARE(toLogical(QRect(100, 50, 640, 360), screens), QRect(100, 50, 640, 360));
    }

    void aScreenMapsBackToItsGeometry()
    {
        // Xft.dpi 120 and 168 on a 1920x1080 monitor, as Qt rounds the logical size.
        for (const qreal dpr : {1.0, 1.25, 1.5, 1.75, 2.0}) {
            const X11Screen screen{QRect(0, 0, qRound(1920 / dpr), qRound(1080 / dpr)), dpr};
            QCOMPARE(nativeRect(screen), QRect(0, 0, 1920, 1080));
            QCOMPARE(toLogical(nativeRect(screen), {screen}), screen.geometry);
        }
    }

    void keepsEachScreensOrigin()
    {
        // QT_SCALE_FACTOR=2 over two 1920x1080 monitors: the second stays at x=1920.
        const QVector<X11Screen> screens{{QRect(0, 0, 960, 540), 2.0},
                                         {QRect(1920, 0, 960, 540), 2.0}};
        QCOMPARE(toLogical(QRect(1920 + 200, 100, 640, 360), screens),
                 QRect(2020, 50, 320, 180));
        QCOMPARE(toLogical(QRect(1920, 0, 1920, 1080), screens), screens.at(1).geometry);
    }

    void straddlesMixedScaleScreens()
    {
        const QVector<X11Screen> screens{{QRect(0, 0, 1920, 1080), 1.0},
                                         {QRect(1920, 0, 1920, 1080), 2.0}};
        // 100 px on the left screen, 200 px (100 logical) on the right one.
        QCOMPARE(toLogical(QRect(1820, 0, 300, 200), screens), QRect(1820, 0, 200, 100));
    }

    void invertsTheGrab()
    {
        // Any window's pixels, mapped to logical and grabbed again, land within a pixel.
        for (const qreal dpr : {1.25, 1.5, 1.75}) {
            const QVector<X11Screen> screens{
                {QRect(0, 0, qRound(2560 / dpr), qRound(1440 / dpr)), dpr}};
            for (int x = 0; x < 40; x += 3) {
                const QRect window(101 + x, 57 + 2 * x, 803 + x, 451 + x);
                const QRect logical = toLogical(window, screens);
                const Recording::X11Grab grab = Recording::x11Grab(logical, screens, true);
                QVERIFY(grab.valid);
                QVERIFY(qAbs(grab.rootPx.left() - window.left()) <= 1);
                QVERIFY(qAbs(grab.rootPx.top() - window.top()) <= 1);
                // The grab rounds its size down to even, so its far edge may sit one short.
                QVERIFY(qAbs(grab.rootPx.right() - window.right()) <= 2);
                QVERIFY(qAbs(grab.rootPx.bottom() - window.bottom()) <= 2);
            }
        }
    }

    void offEveryScreenIsEmpty()
    {
        const QVector<X11Screen> screens{{QRect(0, 0, 1920, 1080), 1.0}};
        QVERIFY(toLogical(QRect(4000, 0, 100, 100), screens).isEmpty());
        QVERIFY(toLogical(QRect(), screens).isEmpty());
        QVERIFY(toLogical(QRect(0, 0, 100, 100), {}).isEmpty());
    }

    void aCornerOffScreenUsesTheDominantScreen()
    {
        const QVector<X11Screen> screens{{QRect(0, 0, 1536, 864), 1.25}};
        // A window hanging off the bottom right still maps at the screen's scale.
        QCOMPARE(toLogical(QRect(1800, 1000, 400, 200), screens), QRect(1440, 800, 320, 160));
    }

    void anX11PortalStreamCropsAtTheScreenScale()
    {
        // The portal reports the 1920x1080 monitor in root pixels; Qt sees it at 1.25.
        const QVector<X11Screen> screens{{QRect(0, 0, 1536, 864), 1.25}};
        const QRect stream = toLogical(QRect(0, 0, 1920, 1080), screens);
        QCOMPARE(stream, QRect(0, 0, 1536, 864));
        const Recording::StreamCrop crop = Recording::portalStreamCrop(
            QRect(100, 100, 640, 360), stream, QSize(1920, 1080), true);
        QVERIFY(crop.valid);
        QCOMPARE(crop.cropPx, QRect(125, 125, 800, 450));
        QCOMPARE(crop.outputPx, QSize(800, 450));
    }
};

QTEST_MAIN(tst_X11ScreenMap)
#include "tst_x11screenmap.moc"
