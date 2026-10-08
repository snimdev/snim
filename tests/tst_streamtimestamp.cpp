#include <QtTest>

#include "record/StreamTimestamp.h"

using namespace Record;

namespace {
constexpr quint64 kNone = StreamTimestamp::kNone;
constexpr quint64 kSecond = 1'000'000'000;
constexpr quint64 kBase = 5000 * kSecond;   // the pipeline went to PLAYING at this clock time
}

class tst_StreamTimestamp : public QObject
{
    Q_OBJECT

private slots:
    void trustsAStampNearTheClock()
    {
        const quint64 now = kBase + 2 * kSecond;
        QCOMPARE(StreamTimestamp::runningTime(now - 15'000'000, now, kBase, kNone),
                 2 * kSecond - 15'000'000);
    }

    void usesArrivalForAStampOfZero()
    {
        const quint64 now = kBase + kSecond;
        QCOMPARE(StreamTimestamp::runningTime(0, now, kBase, kNone), kSecond);
    }

    void usesArrivalForAForeignClock()
    {
        const quint64 now = kBase + kSecond;
        // A producer counting from its own start, minutes away from the pipeline clock.
        QCOMPARE(StreamTimestamp::runningTime(576 * kSecond, now, kBase, kNone), kSecond);
        QCOMPARE(StreamTimestamp::runningTime(now + 10 * kSecond, now, kBase, kNone), kSecond);
    }

    void usesArrivalWithoutAStamp()
    {
        QCOMPARE(StreamTimestamp::runningTime(kNone, kBase + 3, kBase, kNone), quint64(3));
    }

    void neverGoesBackwards()
    {
        const quint64 now = kBase + kSecond;
        QCOMPARE(StreamTimestamp::runningTime(now - 500, now, kBase, kSecond),
                 kSecond + 1);
        QCOMPARE(StreamTimestamp::runningTime(now, now, kBase, kSecond), kSecond + 1);
    }

    void holdsBeforeTheBaseTime()
    {
        QCOMPARE(StreamTimestamp::runningTime(kBase - 1, kBase - 1, kBase, kNone), quint64(0));
        QCOMPARE(StreamTimestamp::runningTime(kBase - 1, kBase - 1, kBase, 7), quint64(7));
    }
};

QTEST_MAIN(tst_StreamTimestamp)
#include "tst_streamtimestamp.moc"
