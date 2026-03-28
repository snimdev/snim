#include <QtTest>

#include "capture/WinScreenMap.h"

using namespace Capture;
using WinScreenMap::Screen;

// Physical window rects (DWM frame bounds) to Qt logical space. Pure, so the
// mixed-DPI layouts nobody can reproduce by hand run on every host.
class tst_WinScreenMap : public QObject
{
    Q_OBJECT

    // A 100% primary, a 150% screen to its right, and a 150% screen up and to the
    // left (negative origin). Qt keeps each origin and scales only the extent.
    static QVector<Screen> layout()
    {
        return {
            { QRect(0, 0, 1920, 1080), QPoint(0, 0), 1.0 },
            { QRect(1920, 0, 2560, 1440), QPoint(1920, 0), 1.5 },
            { QRect(-2560, -200, 2560, 1440), QPoint(-2560, -200), 1.5 },
        };
    }

private slots:
    void passesThroughAtOneHundredPercent()
    {
        QCOMPARE(WinScreenMap::toLogical(QRect(100, 100, 800, 600), layout()),
                 QRect(100, 100, 800, 600));
    }

    void scalesFromTheScreenOrigin()
    {
        // 150 physical px into the right screen is 100 logical px past its origin.
        QCOMPARE(WinScreenMap::toLogical(QRect(2070, 150, 900, 600), layout()),
                 QRect(2020, 100, 600, 400));
    }

    void handlesNegativeOrigins()
    {
        QCOMPARE(WinScreenMap::toLogical(QRect(-2410, -50, 1500, 900), layout()),
                 QRect(-2460, -100, 1000, 600));
    }

    void mapsEachEdgeThroughItsOwnScreen()
    {
        // Straddles 100% and 150%: the left edge stays put on the primary, the right
        // edge lands where the 150% screen draws it.
        QCOMPARE(WinScreenMap::toLogical(QRect(1800, 300, 420, 300), layout()),
                 QRect(1800, 300, 320, 100));
    }

    void offScreenCornerUsesTheDominantScreen()
    {
        // The top-left sits below the shorter primary; most of the rect is on the 150% screen.
        QCOMPARE(WinScreenMap::toLogical(QRect(1700, 1200, 400, 100), layout()),
                 QRect(1773, 800, 267, 67));
    }

    void roundsOutward()
    {
        // 1 physical px at 150% is 2/3 logical px: never collapse to nothing.
        const QRect r = WinScreenMap::toLogical(QRect(1921, 1, 1, 1), layout());
        QVERIFY(!r.isEmpty());
        QVERIFY(r.contains(QPoint(1920, 0)));
    }

    void rejectsRectsOffEveryScreen()
    {
        QVERIFY(WinScreenMap::toLogical(QRect(10000, 10000, 100, 100), layout()).isEmpty());
        QVERIFY(WinScreenMap::toLogical(QRect(), layout()).isEmpty());
        QVERIFY(WinScreenMap::toLogical(QRect(0, 0, 100, 100), {}).isEmpty());
    }
};

QTEST_MAIN(tst_WinScreenMap)
#include "tst_winscreenmap.moc"
