#include <QtTest>
#include <QFile>
#include <QStandardPaths>
#include <QTemporaryDir>

#include "app/Notifier.h"
#include "app/UploadWorkflow.h"
#include "core/Settings.h"

using App::UploadWorkflow;

// Records every balloon instead of showing one.
class FakeNotifier : public App::Notifier
{
public:
    struct Note {
        QString title;
        QString text;
        QSystemTrayIcon::MessageIcon icon;
        int msecs;
    };

    void notify(const QString &title, const QString &text, QSystemTrayIcon::MessageIcon icon,
                int msecs) override
    {
        notes.append({title, text, icon, msecs});
    }

    QList<Note> notes;
};

// No destination is configured, so every upload resolves to the inert stub, whose
// failure arrives queued: an upload stays active until the event loop runs.
class tst_UploadWorkflow : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        QCoreApplication::setOrganizationName("SnimTest");
        QCoreApplication::setApplicationName("tst_uploadworkflow");
        QStandardPaths::setTestModeEnabled(true);   // isolated, empty settings -> not configured
        QVERIFY(m_dir.isValid());
    }

    void init()
    {
        Core::Settings::setUploadEnabled(false);
        Core::Settings::setUploadProfilesJson(QString());
        Core::Settings::setUploadDefaultProfileId(QString());
    }

    void secondUploadIsRefusedWhileOneIsActive()
    {
        FakeNotifier notifier;
        UploadWorkflow workflow(notifier);
        const QString first = makeFile(QStringLiteral("first.png"));
        const QString second = makeFile(QStringLiteral("second.png"));

        workflow.startUpload(first, QStringLiteral("first.png"), false, QString());
        QVERIFY(workflow.isUploading());
        QCOMPARE(notifier.notes.size(), 1);
        QCOMPARE(notifier.notes.at(0).text, QStringLiteral("first.png"));

        workflow.startUpload(second, QStringLiteral("second.png"), true, QString());
        QCOMPARE(notifier.notes.size(), 2);
        QCOMPARE(notifier.notes.at(1).title, QStringLiteral("Upload"));
        QCOMPARE(notifier.notes.at(1).text, QStringLiteral("An upload is already in progress."));
        QCOMPARE(notifier.notes.at(1).icon, QSystemTrayIcon::Warning);
        QVERIFY(!QFile::exists(second));   // refused, and asked to clean up
        QVERIFY(QFile::exists(first));     // the active upload's file is untouched

        QTRY_VERIFY(!workflow.isUploading());
        QCOMPARE(notifier.notes.size(), 3);
        QCOMPARE(notifier.notes.at(2).title, QStringLiteral("Upload failed"));
        QVERIFY(QFile::exists(first));     // not a temp file, so it stays
    }

    void refusedUploadKeepsAFileItDoesNotOwn()
    {
        FakeNotifier notifier;
        UploadWorkflow workflow(notifier);
        const QString first = makeFile(QStringLiteral("owned-first.png"));
        const QString second = makeFile(QStringLiteral("owned-second.png"));

        workflow.startUpload(first, QStringLiteral("owned-first.png"), false, QString());
        workflow.startUpload(second, QStringLiteral("owned-second.png"), false, QString());
        QCOMPARE(notifier.notes.at(1).title, QStringLiteral("Upload"));
        QVERIFY(QFile::exists(second));
        QTRY_VERIFY(!workflow.isUploading());
    }

    void finishedUploadFreesTheSlotAndItsTempFile()
    {
        FakeNotifier notifier;
        UploadWorkflow workflow(notifier);
        const QString temp = makeFile(QStringLiteral("temp.png"));

        workflow.startUpload(temp, QStringLiteral("temp.png"), true, QString());
        QTRY_VERIFY(!workflow.isUploading());
        QVERIFY(!QFile::exists(temp));

        // The slot is free again: the next request starts instead of being refused.
        const QString next = makeFile(QStringLiteral("next.png"));
        workflow.startUpload(next, QStringLiteral("next.png"), true, QString());
        QVERIFY(workflow.isUploading());
        QCOMPARE(notifier.notes.last().title, notifier.notes.at(0).title);
        QTRY_VERIFY(!workflow.isUploading());
        QVERIFY(!QFile::exists(next));
    }

private:
    QString makeFile(const QString &name)
    {
        const QString path = m_dir.filePath(name);
        QFile file(path);
        if (file.open(QIODevice::WriteOnly))
            file.write(QByteArray(512, 'x'));
        return path;
    }

    QTemporaryDir m_dir;
};

QTEST_MAIN(tst_UploadWorkflow)
#include "tst_uploadworkflow.moc"
