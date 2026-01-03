#include <QtTest>

#include "editor/video/TrimState.h"

using Editor::Video::TrimState;

// The trim editor's cut math: clamping rules, the minimum-span rule, and the
// timeline's px<->ms mapping. Pure logic, no player or widgets involved.
class tst_TrimState : public QObject
{
    Q_OBJECT

private slots:
    void durationResetsToKeepEverything()
    {
        TrimState s;
        QCOMPARE(s.durationMs(), qint64(0));
        QVERIFY(!s.isTrimmed());
        QVERIFY(!s.isTrimmable());

        s.setDurationMs(10'000);
        QCOMPARE(s.inMs(), qint64(0));
        QCOMPARE(s.outMs(), qint64(10'000));
        QVERIFY(!s.isTrimmed());                 // full range = save as-is
        QVERIFY(s.isTrimmable());

        s.setInMs(2'000);
        s.setDurationMs(8'000);                  // player refined its estimate
        QCOMPARE(s.inMs(), qint64(0));           // range reset, nothing stale
        QCOMPARE(s.outMs(), qint64(8'000));
    }

    void clampingAndMinimumSpan()
    {
        TrimState s;
        s.setDurationMs(10'000);

        s.setInMs(-50);                          // below the start
        QCOMPARE(s.inMs(), qint64(0));
        s.setOutMs(12'000);                      // past the end
        QCOMPARE(s.outMs(), qint64(10'000));

        s.setInMs(9'950);                        // would leave < kMinSpanMs
        QCOMPARE(s.inMs(), 10'000 - TrimState::kMinSpanMs);
        s.setInMs(1'000);
        s.setOutMs(1'050);                       // same from the other side
        QCOMPARE(s.outMs(), 1'000 + TrimState::kMinSpanMs);

        QVERIFY(s.isTrimmed());
        QCOMPARE(s.trimmedDurationMs(), TrimState::kMinSpanMs);
    }

    void shortClipsAreNeverTrimmable()
    {
        TrimState s;
        s.setDurationMs(TrimState::kMinSpanMs - 1);
        QVERIFY(!s.isTrimmable());
        s.setInMs(50);                           // ignored
        s.setOutMs(100);                         // ignored
        QCOMPARE(s.inMs(), qint64(0));
        QCOMPARE(s.outMs(), s.durationMs());
        QVERIFY(!s.isTrimmed());                 // always the plain-move save path
    }

    void pxMsMappingRoundTrips()
    {
        TrimState s;
        s.setDurationMs(10'000);

        QCOMPARE(s.msToX(0, 500), 0);
        QCOMPARE(s.msToX(10'000, 500), 500);
        QCOMPARE(s.msToX(5'000, 500), 250);
        QCOMPARE(s.xToMs(250, 500), qint64(5'000));
        QCOMPARE(s.xToMs(-10, 500), qint64(0));         // off the left edge
        QCOMPARE(s.xToMs(600, 500), qint64(10'000));    // off the right edge

        // Round trip within one pixel's worth of time.
        const qint64 ms = s.xToMs(s.msToX(3'333, 500), 500);
        QVERIFY(qAbs(ms - 3'333) <= 10'000 / 500);
    }

    void zeroGuards()
    {
        TrimState s;                             // duration 0
        QCOMPARE(s.xToMs(100, 500), qint64(0));
        QCOMPARE(s.msToX(100, 500), 0);
        s.setDurationMs(10'000);
        QCOMPARE(s.xToMs(100, 0), qint64(0));    // degenerate track width
        QCOMPARE(s.msToX(100, -5), 0);
    }
};

QTEST_MAIN(tst_TrimState)
#include "tst_trimstate.moc"
