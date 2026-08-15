#include <QtTest>
#include <QSignalSpy>
#include <QTemporaryFile>
#include <QThreadPool>

#include "upload/strategies/BlockingUploader.h"

using namespace Upload;

// The Template Method FTP and SFTP share, with a scripted transfer in place of a real
// transport: what reaches the worker, and how each outcome becomes one terminal signal.
class ScriptedUploader : public BlockingUploader
{
public:
    using Outcome = BlockingUploader::Outcome;
    // Named here, where the protected type is in reach: MSVC refuses it from outside.
    using Status = Outcome::Status;
    struct Seen { QByteArray body; QString remotePath; };

    ScriptedUploader(const UploadConfig &cfg, Outcome outcome)
        : BlockingUploader(cfg, nullptr), m_outcome(std::move(outcome)) {}

    std::shared_ptr<Seen> seen = std::make_shared<Seen>();

protected:
    Transfer transfer() const override
    {
        return [out = m_outcome, seen = seen](QIODevice &body, const Job &job,
                                              const std::atomic_bool &) {
            *seen = {body.readAll(), job.remotePath};
            return out;
        };
    }
    QUrl fallbackUrl(const QString &remotePath) const override
    {
        return QUrl(QStringLiteral("fake:") + remotePath);
    }

private:
    Outcome m_outcome;
};

using Status = ScriptedUploader::Status;

class tst_BlockingUploader : public QObject
{
    Q_OBJECT

private slots:
    void outcomeBecomesOneSignal_data()
    {
        QTest::addColumn<bool>("testing");
        QTest::addColumn<int>("status");
        QTest::addColumn<bool>("probeLeft");
        QTest::addColumn<QString>("base");
        QTest::addColumn<QString>("expected");   // signal: argument, or "" for none

        const int done = int(Status::Done), failedStatus = int(Status::Failed);
        QTest::newRow("uploaded") << false << done << false << "" << "uploaded: fake:/dir/";
        QTest::newRow("uploaded, base") << false << done << false << "https://cdn.example.com/x/"
                                        << "uploaded: https://cdn.example.com/x/dir/";
        QTest::newRow("upload failed") << false << failedStatus << false << "" << "failed: boom";
        QTest::newRow("tested") << true << done << false << ""
                                << "testFinished true: Connected: uploaded and removed a test file.";
        QTest::newRow("probe left") << true << done << true << ""
                                    << "testFinished true: Connected, but the test file /dir/";
        QTest::newRow("test failed") << true << failedStatus << false << ""
                                     << "testFinished false: boom";
        QTest::newRow("cancelled") << false << int(Status::Cancelled) << false << "" << "";
    }

    void outcomeBecomesOneSignal()
    {
        QFETCH(bool, testing);
        QFETCH(int, status);
        QFETCH(bool, probeLeft);
        QFETCH(QString, base);
        QFETCH(QString, expected);

        QTemporaryFile file;
        QVERIFY(file.open());
        file.write("pixels");
        file.close();

        UploadConfig cfg;
        cfg.remoteDir = QStringLiteral("/dir");
        cfg.publicBaseUrl = base;
        ScriptedUploader::Outcome outcome;
        outcome.status = Status(status);
        outcome.error = QStringLiteral("boom");
        outcome.probeLeft = probeLeft;
        ScriptedUploader up(cfg, outcome);

        QStringList got;
        connect(&up, &Uploader::uploaded, this,
                [&](const QUrl &url) { got << "uploaded: " + url.toString(); });
        connect(&up, &Uploader::failed, this, [&](const QString &m) { got << "failed: " + m; });
        connect(&up, &Uploader::testFinished, this, [&](bool ok, const QString &m) {
            got << QStringLiteral("testFinished %1: %2").arg(ok ? "true" : "false", m);
        });
        if (testing)
            up.testConnection();
        else
            up.upload(file.fileName(), QStringLiteral("shot.png"));
        up.upload(file.fileName(), QStringLiteral("again.png"));   // single-shot: ignored

        QThreadPool::globalInstance()->waitForDone();   // the worker has posted, if it will
        QCoreApplication::processEvents();
        QCOMPARE(got.size(), expected.isEmpty() ? 0 : 1);
        if (!expected.isEmpty())
            QVERIFY2(got.first().startsWith(expected), qPrintable(got.first()));
        QCOMPARE(up.seen->body, testing ? QByteArray("Snim connection test") : QByteArray("pixels"));
        QVERIFY(up.seen->remotePath.startsWith(QStringLiteral("/dir/")));
    }

    // An unreadable file never reaches the transport; a follow-up such as an SFTP pin
    // still runs when the uploader is gone by the time the worker reports.
    void missingFileAndOrphanedFollowUp()
    {
        ScriptedUploader::Outcome failing;
        ScriptedUploader missing(UploadConfig{}, failing);
        QSignalSpy failedSpy(&missing, &Uploader::failed);
        missing.upload(QStringLiteral("/nonexistent/shot.png"), QStringLiteral("shot.png"));
        QVERIFY(failedSpy.wait(1000));
        QCOMPARE(failedSpy.first().first().toString(), QStringLiteral("Could not open the file to upload."));
        QVERIFY(missing.seen->remotePath.isEmpty());

        bool followedUp = false;
        ScriptedUploader::Outcome done;
        done.status = Status::Done;
        done.onDone = [&followedUp] { followedUp = true; };
        auto *orphan = new ScriptedUploader(UploadConfig{}, done);
        orphan->testConnection();
        delete orphan;
        QTRY_VERIFY(followedUp);
    }
};

QTEST_MAIN(tst_BlockingUploader)
#include "tst_blockinguploader.moc"
