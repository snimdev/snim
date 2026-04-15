#include <QtTest>

#include "recording/PauseAwareClock.h"

using namespace Recording;

class tst_PauseAwareClock : public QObject
{
    Q_OBJECT

private slots:
    void mapsRelativeToTheStart()
    {
        PauseAwareClock clock;
        QCOMPARE(clock.toOutput(1000), qint64(-1));   // not started
        clock.start(1000);
        QVERIFY(clock.isStarted());
        QCOMPARE(clock.toOutput(999), qint64(-1));
        QCOMPARE(clock.toOutput(1000), qint64(0));
        QCOMPARE(clock.toOutput(4500), qint64(3500));
    }

    void dropsAndSkipsPausedSpans()
    {
        PauseAwareClock clock;
        clock.start(0);
        clock.pause(1000);
        QVERIFY(clock.isPaused());
        QCOMPARE(clock.toOutput(999), qint64(999));
        QCOMPARE(clock.toOutput(1000), qint64(-1));
        QCOMPARE(clock.toOutput(5000), qint64(-1));   // still paused
        clock.resume(3000);
        QVERIFY(!clock.isPaused());
        QCOMPARE(clock.toOutput(2999), qint64(-1));
        QCOMPARE(clock.toOutput(3000), qint64(1000));
        QCOMPARE(clock.toOutput(4000), qint64(2000));

        clock.pause(5000);
        clock.resume(6000);
        QCOMPARE(clock.toOutput(5500), qint64(-1));
        QCOMPARE(clock.toOutput(6500), qint64(3500));
    }

    void lateBuffersLandBeforeThePause()
    {
        PauseAwareClock clock;
        clock.start(0);
        clock.pause(1000);
        clock.resume(2000);
        // Captured before the pause but delivered after the resume.
        QCOMPARE(clock.toOutput(900), qint64(900));
    }

    void redundantCallsAreIgnored()
    {
        PauseAwareClock clock;
        clock.pause(10);                 // before start
        clock.resume(20);
        QVERIFY(!clock.isPaused());
        clock.start(100);
        clock.resume(150);               // not paused
        clock.pause(200);
        clock.pause(400);                // already paused: keeps 200
        clock.resume(300);
        QCOMPARE(clock.toOutput(300), qint64(100));
        clock.pause(350);
        clock.resume(350);               // empty span
        QCOMPARE(clock.toOutput(400), qint64(200));
    }

    void elapsedHoldsWhilePaused()
    {
        PauseAwareClock clock;
        QCOMPARE(clock.elapsed(500), qint64(0));
        clock.start(1000);
        QCOMPARE(clock.elapsed(1500), qint64(500));
        clock.pause(2000);
        QCOMPARE(clock.elapsed(9000), qint64(1000));
        clock.resume(5000);
        QCOMPARE(clock.elapsed(6000), qint64(2000));
    }

    void startResetsPauses()
    {
        PauseAwareClock clock;
        clock.start(0);
        clock.pause(10);
        clock.start(100);
        QVERIFY(!clock.isPaused());
        QCOMPARE(clock.toOutput(150), qint64(50));
        clock.reset();
        QVERIFY(!clock.isStarted());
        QCOMPARE(clock.toOutput(150), qint64(-1));
    }

    void nowIsMonotonic()
    {
        const qint64 a = PauseAwareClock::nowUs();
        const qint64 b = PauseAwareClock::nowUs();
        QVERIFY(b >= a);
    }
};

QTEST_MAIN(tst_PauseAwareClock)
#include "tst_pauseawareclock.moc"
