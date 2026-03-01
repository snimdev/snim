#include <QtTest>

#include <limits>

#include "editor/video/Timecode.h"

using namespace Editor::Video;

// The trim editor's time readout and frame-step math. Pure logic, no player involved.
class tst_Timecode : public QObject
{
    Q_OBJECT

private slots:
    void formatsMinutesSecondsTenths()
    {
        QCOMPARE(formatTimecode(0), QStringLiteral("0:00.0"));
        QCOMPARE(formatTimecode(999), QStringLiteral("0:00.9"));
        QCOMPARE(formatTimecode(1'000), QStringLiteral("0:01.0"));
        QCOMPARE(formatTimecode(59'999), QStringLiteral("0:59.9"));
        QCOMPARE(formatTimecode(60'000), QStringLiteral("1:00.0"));
        QCOMPARE(formatTimecode(65'300), QStringLiteral("1:05.3"));
    }

    void truncatesInsteadOfRounding()
    {
        QCOMPARE(formatTimecode(99), QStringLiteral("0:00.0"));
        QCOMPARE(formatTimecode(1'950), QStringLiteral("0:01.9"));
    }

    void minutesPastAnHour()
    {
        QCOMPARE(formatTimecode(61 * 60'000 + 5'300), QStringLiteral("61:05.3"));
    }

    void negativeClampsToZero()
    {
        QCOMPARE(formatTimecode(-1), QStringLiteral("0:00.0"));
        QCOMPARE(formatTimecode(-90'000), QStringLiteral("0:00.0"));
    }

    void frameStepFollowsTheRate()
    {
        QCOMPARE(frameStepMs(30.0), qint64(33));
        QCOMPARE(frameStepMs(60.0), qint64(17));
        QCOMPARE(frameStepMs(29.97), qint64(33));
        QCOMPARE(frameStepMs(24.0), qint64(42));
    }

    void frameStepFallsBackAndStaysPositive()
    {
        QCOMPARE(frameStepMs(0.0), qint64(33));
        QCOMPARE(frameStepMs(-25.0), qint64(33));
        QCOMPARE(frameStepMs(std::numeric_limits<double>::quiet_NaN()), qint64(33));
        QCOMPARE(frameStepMs(std::numeric_limits<double>::infinity()), qint64(33));
        QCOMPARE(frameStepMs(1e6), qint64(1));
    }
};

QTEST_MAIN(tst_Timecode)
#include "tst_timecode.moc"
