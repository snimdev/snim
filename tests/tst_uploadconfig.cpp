#include <QtTest>
#include <QStandardPaths>

#include "upload/UploadConfig.h"
#include "upload/UploadProfiles.h"
#include "core/Settings.h"

using namespace Upload;

// UploadConfig::isComplete() is the per-type "is this destination filled in" gate the
// editors call before starting an upload. It is PURE CONFIG: it never asks whether the
// backend was compiled in (that is UploaderFactory::isAvailable), so every
// assertion here holds with or without HAVE_LIBCURL / HAVE_LIBSSH2.
//
// The matrix builds UploadConfig structs by hand - no keychain, no profile store, no
// network. Only the two forProfile() cases at the end go through the (test-mode
// isolated) settings store.
class tst_UploadConfig : public QObject
{
    Q_OBJECT

    static UploadConfig s3()
    {
        UploadConfig c;
        c.enabled = true;
        c.type = ProviderType::S3;
        c.endpoint = "s3.amazonaws.com";
        c.region = "us-east-1";
        c.bucket = "shots";
        c.accessKeyId = "AKIA123";
        c.secretKey = "s3cret";
        return c;
    }

    static UploadConfig sftpPassword()
    {
        UploadConfig c;
        c.enabled = true;
        c.type = ProviderType::Sftp;
        c.host = "sftp.example.com";
        c.username = "darko";
        c.sftpAuth = SftpAuthMode::Password;
        c.secretKey = "hunter2";
        return c;
    }

    static UploadConfig sftpKey()
    {
        UploadConfig c;
        c.enabled = true;
        c.type = ProviderType::Sftp;
        c.host = "sftp.example.com";
        c.username = "darko";
        c.sftpAuth = SftpAuthMode::PrivateKey;
        c.privateKeyPath = "/home/darko/.ssh/id_ed25519";
        return c;   // no secretKey: the passphrase is optional
    }

    static UploadConfig ftpAnonymous()
    {
        UploadConfig c;
        c.enabled = true;
        c.type = ProviderType::Ftp;
        c.host = "ftp.example.com";
        return c;   // empty username = anonymous, no password needed
    }

private slots:
    void initTestCase()
    {
        QCoreApplication::setOrganizationName("SnimTest");
        QCoreApplication::setApplicationName("tst_uploadconfig");
        QStandardPaths::setTestModeEnabled(true);
    }

    void init()
    {
        Core::Settings::setUploadEnabled(false);
        Core::Settings::setUploadProfilesJson(QString());
        Core::Settings::setUploadDefaultProfileId(QString());
    }

    // --- the global toggle wins over everything ------------------------------

    void disabledIsNeverComplete()
    {
        for (UploadConfig c : {s3(), sftpPassword(), sftpKey(), ftpAnonymous()}) {
            QVERIFY(c.isComplete());     // sanity: the fixtures really are complete
            c.enabled = false;
            QVERIFY(!c.isComplete());
        }
    }

    // --- S3 ------------------------------------------------------------------

    void s3NeedsEveryCredentialField()
    {
        QVERIFY(s3().isComplete());
        { auto c = s3(); c.endpoint.clear();    QVERIFY(!c.isComplete()); }
        { auto c = s3(); c.region.clear();      QVERIFY(!c.isComplete()); }
        { auto c = s3(); c.bucket.clear();      QVERIFY(!c.isComplete()); }
        { auto c = s3(); c.accessKeyId.clear(); QVERIFY(!c.isComplete()); }
        { auto c = s3(); c.secretKey.clear();   QVERIFY(!c.isComplete()); }
    }

    void s3IgnoresOptionalFields()
    {
        auto c = s3();
        c.keyPrefix.clear();
        c.publicBaseUrl.clear();
        c.forcePathStyle = true;
        QVERIFY(c.isComplete());
    }

    // --- SFTP ----------------------------------------------------------------

    void sftpPasswordModeNeedsASecret()
    {
        QVERIFY(sftpPassword().isComplete());
        auto c = sftpPassword();
        c.secretKey.clear();
        QVERIFY(!c.isComplete());
        // A key path is irrelevant while the mode says password.
        c.privateKeyPath = "/home/darko/.ssh/id_ed25519";
        QVERIFY(!c.isComplete());
    }

    void sftpKeyModeNeedsAKeyPathButNoPassphrase()
    {
        auto c = sftpKey();
        QVERIFY(c.secretKey.isEmpty());
        QVERIFY(c.isComplete());          // empty passphrase is legitimate
        c.secretKey = "passphrase";       // ... and so is a set one
        QVERIFY(c.isComplete());
        c.privateKeyPath.clear();
        QVERIFY(!c.isComplete());         // the key file itself is mandatory
    }

    void sftpAlwaysNeedsHostAndUsername()
    {
        { auto c = sftpPassword(); c.host.clear();     QVERIFY(!c.isComplete()); }
        { auto c = sftpPassword(); c.username.clear(); QVERIFY(!c.isComplete()); }
        { auto c = sftpKey();      c.host.clear();     QVERIFY(!c.isComplete()); }
        // Unlike FTP, an empty SFTP username is never "anonymous".
        { auto c = sftpKey();      c.username.clear(); QVERIFY(!c.isComplete()); }
    }

    // --- FTP -----------------------------------------------------------------

    void ftpAnonymousNeedsOnlyAHost()
    {
        QVERIFY(ftpAnonymous().isComplete());
        auto c = ftpAnonymous();
        c.host.clear();
        QVERIFY(!c.isComplete());
    }

    void ftpNamedUserNeedsAPassword()
    {
        auto c = ftpAnonymous();
        c.username = "bob";
        QVERIFY(!c.isComplete());
        c.secretKey = "hunter2";
        QVERIFY(c.isComplete());
    }

    void ftpCompletenessIgnoresEncryptionMode()
    {
        for (const FtpEncryption e : {FtpEncryption::None, FtpEncryption::Explicit,
                                      FtpEncryption::Implicit}) {
            auto c = ftpAnonymous();
            c.ftpEncryption = e;
            QVERIFY(c.isComplete());
        }
    }

    // --- the type picks the field group; nothing leaks across ----------------

    void typeSelectsItsOwnFieldGroup()
    {
        // Fully filled S3 credentials say nothing about an SFTP/FTP destination.
        { auto c = s3(); c.type = ProviderType::Sftp; QVERIFY(!c.isComplete()); }
        { auto c = s3(); c.type = ProviderType::Ftp;  QVERIFY(!c.isComplete()); }
        // ... and vice versa.
        { auto c = sftpPassword(); c.type = ProviderType::S3; QVERIFY(!c.isComplete()); }
        // An FTP host is not an S3 endpoint.
        { auto c = ftpAnonymous(); c.type = ProviderType::S3; QVERIFY(!c.isComplete()); }
    }

    // --- forProfile() snapshotting -------------------------------------------

    void forProfileCopiesEveryFieldAndTheGlobalToggle()
    {
        UploadProfile p;
        p.id = UploadProfiles::newId();
        p.name = "Box";
        p.type = ProviderType::Sftp;
        p.host = "sftp.example.com";
        p.port = 2222;
        p.username = "darko";
        p.remoteDir = "/var/www/uploads";
        p.sftpAuth = SftpAuthMode::PrivateKey;
        p.privateKeyPath = "/home/darko/.ssh/id_ed25519";
        p.ftpEncryption = FtpEncryption::Implicit;
        p.publicBaseUrl = "https://cdn.example.com";
        p.keyPrefix = "screenshots/";
        p.forcePathStyle = true;
        UploadProfiles::setAll({p}, p.id);

        Core::Settings::setUploadEnabled(true);
        const UploadConfig c = UploadConfig::forProfile(p.id);
        QVERIFY(c.enabled);                       // from the global toggle, not the profile
        QCOMPARE(c.type, ProviderType::Sftp);
        QCOMPARE(c.host, QStringLiteral("sftp.example.com"));
        QCOMPARE(c.port, 2222);
        QCOMPARE(c.username, QStringLiteral("darko"));
        QCOMPARE(c.remoteDir, QStringLiteral("/var/www/uploads"));
        QCOMPARE(c.sftpAuth, SftpAuthMode::PrivateKey);
        QCOMPARE(c.privateKeyPath, QStringLiteral("/home/darko/.ssh/id_ed25519"));
        QCOMPARE(c.ftpEncryption, FtpEncryption::Implicit);
        QCOMPARE(c.publicBaseUrl, QStringLiteral("https://cdn.example.com"));
        QCOMPARE(c.keyPrefix, QStringLiteral("screenshots/"));
        QVERIFY(c.forcePathStyle);
        QVERIFY(c.isComplete());                  // key auth: complete without a keychain

        // An empty id means "the default profile" - same snapshot here.
        QCOMPARE(UploadConfig::forProfile(QString()).host, QStringLiteral("sftp.example.com"));

        Core::Settings::setUploadEnabled(false);
        QVERIFY(!UploadConfig::forProfile(p.id).isComplete());
    }

    void forProfileUnknownIdIsAnEmptyIncompleteConfig()
    {
        Core::Settings::setUploadEnabled(true);
        const UploadConfig c = UploadConfig::forProfile("does-not-exist");
        QCOMPARE(c.type, ProviderType::S3);       // the null profile's default type
        QVERIFY(c.host.isEmpty());
        QVERIFY(c.bucket.isEmpty());
        QVERIFY(c.secretKey.isEmpty());           // no id -> no keychain lookup at all
        QVERIFY(!c.isComplete());
    }
};

QTEST_MAIN(tst_UploadConfig)
#include "tst_uploadconfig.moc"
