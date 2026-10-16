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
        c.type = "s3";
        c.values = {{"endpoint", "s3.amazonaws.com"}, {"region", "us-east-1"},
                    {"bucket", "shots"}, {"accessKeyId", "AKIA123"}};
        c.secretKey = "s3cret";
        return c;
    }

    static UploadConfig sftpPassword()
    {
        UploadConfig c;
        c.enabled = true;
        c.type = "sftp";
        c.values = {{"host", "sftp.example.com"}, {"username", "darko"},
                    {"sftpAuthMode", "password"}};
        c.secretKey = "hunter2";
        return c;
    }

    static UploadConfig sftpKey()
    {
        UploadConfig c;
        c.enabled = true;
        c.type = "sftp";
        c.values = {{"host", "sftp.example.com"}, {"username", "darko"},
                    {"sftpAuthMode", "key"}, {"privateKeyPath", "/home/darko/.ssh/id_ed25519"}};
        return c;   // no secretKey: the passphrase is optional
    }

    static UploadConfig ftpAnonymous()
    {
        UploadConfig c;
        c.enabled = true;
        c.type = "ftp";
        c.values = {{"host", "ftp.example.com"}};
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
        for (const char *key : {"endpoint", "region", "bucket", "accessKeyId"}) {
            auto c = s3();
            c.values[key] = QString();
            QVERIFY2(!c.isComplete(), key);
        }
        { auto c = s3(); c.secretKey.clear();   QVERIFY(!c.isComplete()); }
    }

    void s3IgnoresOptionalFields()
    {
        auto c = s3();
        c.values["keyPrefix"] = QString();
        c.publicBaseUrl.clear();
        c.values["forcePathStyle"] = true;
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
        c.values["privateKeyPath"] = "/home/darko/.ssh/id_ed25519";
        QVERIFY(!c.isComplete());
    }

    void sftpKeyModeNeedsAKeyPathButNoPassphrase()
    {
        auto c = sftpKey();
        QVERIFY(c.secretKey.isEmpty());
        QVERIFY(c.isComplete());          // empty passphrase is legitimate
        c.secretKey = "passphrase";       // ... and so is a set one
        QVERIFY(c.isComplete());
        c.values["privateKeyPath"] = QString();
        QVERIFY(!c.isComplete());         // the key file itself is mandatory
    }

    void sftpAlwaysNeedsHostAndUsername()
    {
        { auto c = sftpPassword(); c.values.remove("host");     QVERIFY(!c.isComplete()); }
        { auto c = sftpPassword(); c.values.remove("username"); QVERIFY(!c.isComplete()); }
        { auto c = sftpKey();      c.values.remove("host");     QVERIFY(!c.isComplete()); }
        // Unlike FTP, an empty SFTP username is never "anonymous".
        { auto c = sftpKey();      c.values.remove("username"); QVERIFY(!c.isComplete()); }
    }

    // --- FTP -----------------------------------------------------------------

    void ftpAnonymousNeedsOnlyAHost()
    {
        QVERIFY(ftpAnonymous().isComplete());
        auto c = ftpAnonymous();
        c.values["host"] = QString();
        QVERIFY(!c.isComplete());
    }

    void ftpNamedUserNeedsAPassword()
    {
        auto c = ftpAnonymous();
        c.values["username"] = "bob";
        QVERIFY(!c.isComplete());
        c.secretKey = "hunter2";
        QVERIFY(c.isComplete());
    }

    void ftpCompletenessIgnoresEncryptionMode()
    {
        for (const char *mode : {"none", "explicit", "implicit"}) {
            auto c = ftpAnonymous();
            c.values["ftpsMode"] = mode;
            QVERIFY(c.isComplete());
        }
    }

    // --- the type picks the field group; nothing leaks across ----------------

    void typeSelectsItsOwnFieldGroup()
    {
        // Fully filled S3 credentials say nothing about an SFTP/FTP destination.
        { auto c = s3(); c.type = "sftp"; QVERIFY(!c.isComplete()); }
        { auto c = s3(); c.type = "ftp";  QVERIFY(!c.isComplete()); }
        // ... and vice versa.
        { auto c = sftpPassword(); c.type = "s3"; QVERIFY(!c.isComplete()); }
        // An FTP host is not an S3 endpoint.
        { auto c = ftpAnonymous(); c.type = "s3"; QVERIFY(!c.isComplete()); }
        // No provider, no destination.
        { auto c = s3(); c.type = "gopher"; QVERIFY(!c.isComplete()); }
    }

    // --- forProfile() snapshotting -------------------------------------------

    void forProfileCopiesEveryFieldAndTheGlobalToggle()
    {
        UploadProfile p;
        p.id = UploadProfiles::newId();
        p.name = "Box";
        p.type = "sftp";
        p.values = {{"host", "sftp.example.com"}, {"port", 2222}, {"username", "darko"},
                    {"remoteDir", "/var/www/uploads"}, {"sftpAuthMode", "key"},
                    {"privateKeyPath", "/home/darko/.ssh/id_ed25519"}};
        p.publicBaseUrl = "https://cdn.example.com";
        UploadProfiles::setAll({p}, p.id);

        Core::Settings::setUploadEnabled(true);
        const UploadConfig c = UploadConfig::forProfile(p.id);
        QVERIFY(c.enabled);                       // from the global toggle, not the profile
        QCOMPARE(c.type, QStringLiteral("sftp"));
        QCOMPARE(c.text("host"), QStringLiteral("sftp.example.com"));
        QCOMPARE(c.number("port"), 2222);
        QCOMPARE(c.text("username"), QStringLiteral("darko"));
        QCOMPARE(c.text("remoteDir"), QStringLiteral("/var/www/uploads"));
        QCOMPARE(c.text("sftpAuthMode"), QStringLiteral("key"));
        QCOMPARE(c.text("privateKeyPath"), QStringLiteral("/home/darko/.ssh/id_ed25519"));
        QCOMPARE(c.publicBaseUrl, QStringLiteral("https://cdn.example.com"));
        QVERIFY(c.isComplete());                  // key auth: complete without a keychain

        // An empty id means "the default profile" - same snapshot here.
        QCOMPARE(UploadConfig::forProfile(QString()).text("host"), QStringLiteral("sftp.example.com"));

        Core::Settings::setUploadEnabled(false);
        QVERIFY(!UploadConfig::forProfile(p.id).isComplete());
    }

    void forProfileUnknownIdIsAnEmptyIncompleteConfig()
    {
        Core::Settings::setUploadEnabled(true);
        const UploadConfig c = UploadConfig::forProfile("does-not-exist");
        QCOMPARE(c.type, QStringLiteral("s3"));   // the null profile's default type
        QVERIFY(c.values.isEmpty());
        QVERIFY(c.text("bucket").isEmpty());
        QVERIFY(c.secretKey.isEmpty());           // no id -> no keychain lookup at all
        QVERIFY(!c.isComplete());
    }
};

QTEST_MAIN(tst_UploadConfig)
#include "tst_uploadconfig.moc"
