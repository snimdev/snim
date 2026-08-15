#include <QtTest>
#include <QSignalSpy>
#include <QStandardPaths>

#include "upload/UploaderFactory.h"
#include "upload/Uploader.h"
#include "upload/UploadConfig.h"
#include "upload/UploadProfiles.h"
#include "core/Settings.h"

using namespace Upload;

Q_DECLARE_METATYPE(Upload::ProviderType)

// Factory surface only - never a live upload() on a real backend, so no network. Every
// destination here is complete without a keychain secret (S3 is built by hand), so the
// profile-store cases stay keychain-free and work on the stub-keychain platforms too.
class tst_UploaderFactory : public QObject
{
    Q_OBJECT

    static QString backend(const std::unique_ptr<Uploader> &up)
    {
        return QString::fromLatin1(up->metaObject()->className());
    }

    // SFTP with key auth and anonymous FTP need no stored secret.
    static UploadConfig complete(ProviderType type)
    {
        UploadConfig c;
        c.id = UploadProfiles::newId();
        c.name = QStringLiteral("B");
        c.enabled = true;
        c.type = type;
        c.host = QStringLiteral("files.example.com");
        c.username = type == ProviderType::Sftp ? QStringLiteral("darko") : QString();
        c.sftpAuth = SftpAuthMode::PrivateKey;
        c.privateKeyPath = QStringLiteral("/home/darko/.ssh/id_ed25519");
        c.endpoint = QStringLiteral("s3.example.com");
        c.region = QStringLiteral("us-east-1");
        c.bucket = QStringLiteral("shots");
        c.accessKeyId = QStringLiteral("AKIA");
        c.secretKey = QStringLiteral("sekrit");
        return c;
    }

    static void backendRows(bool withS3)
    {
        QTest::addColumn<ProviderType>("type");
        QTest::addColumn<QString>("expected");
        if (withS3)
            QTest::newRow("s3") << ProviderType::S3 << "Upload::S3Uploader";
#ifdef HAVE_LIBSSH2
        QTest::newRow("sftp") << ProviderType::Sftp << "Upload::SftpUploader";
#else
        QTest::newRow("sftp") << ProviderType::Sftp << "Upload::StubUploader";
#endif
#ifdef HAVE_LIBCURL
        QTest::newRow("ftp") << ProviderType::Ftp << "Upload::FtpUploader";
#else
        QTest::newRow("ftp") << ProviderType::Ftp << "Upload::StubUploader";
#endif
    }

private slots:
    void initTestCase()
    {
        QCoreApplication::setOrganizationName("SnimTest");
        QCoreApplication::setApplicationName("tst_uploaderfactory");
        QStandardPaths::setTestModeEnabled(true);
    }

    void init()
    {
        Core::Settings::setUploadEnabled(false);
        Core::Settings::setUploadProfilesJson(QString());
        Core::Settings::setUploadDefaultProfileId(QString());
    }

    // Availability is compile time only; a compiled-out backend explains itself instead
    // of claiming "not configured".
    void picksTheBackendPerType_data() { backendRows(true); }
    void picksTheBackendPerType()
    {
        QFETCH(ProviderType, type);
        QFETCH(QString, expected);
        const bool stub = expected == QStringLiteral("Upload::StubUploader");
        QCOMPARE(UploaderFactory::isAvailable(type), !stub);

        auto up = UploaderFactory::createForConfig(complete(type));
        QCOMPARE(backend(up), expected);
        if (stub) {
            QSignalSpy failedSpy(up.get(), &Uploader::failed);
            up->upload(QStringLiteral("/tmp/whatever.png"), QStringLiteral("whatever.png"));
            QVERIFY(failedSpy.wait(1000));
            QVERIFY(failedSpy.first().first().toString().contains("not included in this build"));
        }
    }

    // Nothing usable, saved or typed: the inert stub, whose upload and test both fail
    // deferred (a caller may connect after the call) and never reach the network.
    void incompleteYieldsTheStub()
    {
        auto saved = UploaderFactory::create();   // empty store
        auto typed = UploaderFactory::createForConfig(UploadConfig{});
        for (Uploader *up : {saved.get(), typed.get()}) {
            QCOMPARE(QString::fromLatin1(up->metaObject()->className()),
                     QStringLiteral("Upload::StubUploader"));
            QSignalSpy failedSpy(up, &Uploader::failed);
            QSignalSpy testSpy(up, &Uploader::testFinished);
            up->upload(QStringLiteral("/tmp/whatever.png"), QStringLiteral("whatever.png"));
            up->testConnection();
            QCOMPARE(failedSpy.count() + testSpy.count(), 0);
            QTRY_COMPARE(failedSpy.count(), 1);
            QTRY_COMPARE(testSpy.count(), 1);
            QCOMPARE(failedSpy.first().first().toString(), QStringLiteral("Upload is not configured."));
            QCOMPARE(testSpy.first().first().toBool(), false);
        }
    }

    // Regression: the backend comes from the REQUESTED profile, not the default one (the
    // ▾ menu uploads to a non-default destination). The default here is incomplete.
    void createUsesTheRequestedProfile_data() { backendRows(false); }
    void createUsesTheRequestedProfile()
    {
        QFETCH(ProviderType, type);
        QFETCH(QString, expected);
        Core::Settings::setUploadEnabled(true);
        UploadProfile incomplete;
        incomplete.id = UploadProfiles::newId();
        const UploadConfig requested = complete(type);
        UploadProfiles::setAll({incomplete, requested}, incomplete.id);

        QCOMPARE(backend(UploaderFactory::create()), QStringLiteral("Upload::StubUploader"));
        QCOMPARE(backend(UploaderFactory::create(requested.id)), expected);
    }
};

QTEST_MAIN(tst_UploaderFactory)
#include "tst_uploaderfactory.moc"
