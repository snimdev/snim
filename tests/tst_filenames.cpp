#include <QtTest>

#include "core/FileNames.h"

using namespace Core;

// The shared default file name pattern for screenshots and recordings.
class tst_FileNames : public QObject
{
    Q_OBJECT

private slots:
    void screenshotName()
    {
        const QDateTime when(QDate(2026, 9, 22), QTime(20, 24, 37));
        QCOMPARE(screenshotFileName(when, QStringLiteral("png")),
                 QStringLiteral("snimshoot-2026-09-22_20-24-37.png"));
        QCOMPARE(screenshotFileName(when, QStringLiteral("jpg")),
                 QStringLiteral("snimshoot-2026-09-22_20-24-37.jpg"));
    }

    void recordingName()
    {
        const QDateTime when(QDate(2026, 9, 22), QTime(20, 24, 37));
        QCOMPARE(recordingFileName(when, QStringLiteral("mp4")),
                 QStringLiteral("snimcapture-2026-09-22_20-24-37.mp4"));
    }

    void zeroPadsFields()
    {
        const QDateTime when(QDate(2026, 1, 2), QTime(3, 4, 5));
        QCOMPARE(screenshotFileName(when, QStringLiteral("png")),
                 QStringLiteral("snimshoot-2026-01-02_03-04-05.png"));
    }

    void stripsLeadingDotAndAllowsNoExtension()
    {
        const QDateTime when(QDate(2026, 9, 22), QTime(20, 24, 37));
        QCOMPARE(recordingFileName(when, QStringLiteral(".webp")),
                 QStringLiteral("snimcapture-2026-09-22_20-24-37.webp"));
        QCOMPARE(timestampedFileName(QStringLiteral("x"), when, QString()),
                 QStringLiteral("x-2026-09-22_20-24-37"));
    }

    void isFileSafe()
    {
        const QString name = screenshotFileName(QDateTime::currentDateTime(), QStringLiteral("png"));
        QVERIFY(!name.contains(QLatin1Char(':')));
        QVERIFY(!name.contains(QLatin1Char(' ')));
    }
};

QTEST_MAIN(tst_FileNames)
#include "tst_filenames.moc"
