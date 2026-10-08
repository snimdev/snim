#include <QtTest>
#include <QFile>
#include <QSettings>
#include <QStandardPaths>
#include <QTemporaryDir>

#include "core/Settings.h"
#include "record/RecordingJournal.h"

using namespace Record;

class tst_RecordingJournal : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        QCoreApplication::setOrganizationName("SnimTest");
        QCoreApplication::setApplicationName("tst_recordingjournal");
        QStandardPaths::setTestModeEnabled(true);   // redirect QSettings to a throwaway store
        QVERIFY(m_dir.isValid());
    }

    void init()
    {
        QSettings().clear();
    }

    void offersOnlyFilesWithFootage()
    {
        const QString footage = makeFile(QStringLiteral("footage.mp4"), 64 * 1024);
        const QString headers = makeFile(QStringLiteral("headers.mp4"),
                                         RecordingJournal::kMinimumBytes - 1);
        const QString gone = m_dir.filePath(QStringLiteral("gone.mp4"));
        RecordingJournal::add(footage);
        RecordingJournal::add(headers);
        RecordingJournal::add(gone);

        QCOMPARE(RecordingJournal::recoverable(), QStringList{footage});
        QVERIFY(RecordingJournal::isRecoverable(footage));
        QVERIFY(!RecordingJournal::isRecoverable(headers));
        QVERIFY(!RecordingJournal::isRecoverable(gone));
    }

    void keepsTheThresholdInclusive()
    {
        const QString path = makeFile(QStringLiteral("edge.mp4"), RecordingJournal::kMinimumBytes);
        RecordingJournal::add(path);
        QCOMPARE(RecordingJournal::recoverable(), QStringList{path});
    }

    void addsEachPathOnce()
    {
        const QString path = makeFile(QStringLiteral("once.mp4"), 4096);
        RecordingJournal::add(path);
        RecordingJournal::add(path);
        RecordingJournal::add(QString());
        QCOMPARE(Core::Settings::recordingsInProgress(), QStringList{path});
    }

    void forgetsWhatTheEditorTook()
    {
        const QString saved = makeFile(QStringLiteral("saved.mp4"), 4096);
        const QString kept = makeFile(QStringLiteral("kept.mp4"), 4096);
        RecordingJournal::add(saved);
        RecordingJournal::add(kept);

        // Saving moves the temp file away, discarding deletes it: either way it is gone.
        QVERIFY(QFile::rename(saved, m_dir.filePath(QStringLiteral("elsewhere.mp4"))));
        QCOMPARE(RecordingJournal::recoverable(), QStringList{kept});
    }

    void pruneDropsGoneAndEmptyFiles()
    {
        const QString footage = makeFile(QStringLiteral("prune-footage.mp4"), 4096);
        const QString headers = makeFile(QStringLiteral("prune-headers.mp4"), 100);
        const QString gone = m_dir.filePath(QStringLiteral("prune-gone.mp4"));
        RecordingJournal::add(footage);
        RecordingJournal::add(headers);
        RecordingJournal::add(gone);

        RecordingJournal::prune();

        QCOMPARE(Core::Settings::recordingsInProgress(), QStringList{footage});
        QVERIFY(QFile::exists(footage));
        QVERIFY(!QFile::exists(headers));
    }

    void discardDeletesTheFileAndEntry()
    {
        const QString first = makeFile(QStringLiteral("discard-1.mp4"), 4096);
        const QString second = makeFile(QStringLiteral("discard-2.mp4"), 4096);
        RecordingJournal::add(first);
        RecordingJournal::add(second);

        RecordingJournal::discard(first);

        QVERIFY(!QFile::exists(first));
        QCOMPARE(Core::Settings::recordingsInProgress(), QStringList{second});
        QCOMPARE(RecordingJournal::recoverable(), QStringList{second});
    }

private:
    QString makeFile(const QString &name, qint64 bytes)
    {
        const QString path = m_dir.filePath(name);
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly))
            return {};
        file.write(QByteArray(bytes, 'x'));
        return path;
    }

    QTemporaryDir m_dir;
};

QTEST_GUILESS_MAIN(tst_RecordingJournal)
#include "tst_recordingjournal.moc"
