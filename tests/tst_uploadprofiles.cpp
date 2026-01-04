#include <QtTest>
#include <QStandardPaths>

#include "upload/KnownHosts.h"
#include "upload/UploadProfiles.h"
#include "core/KeychainStore.h"
#include "core/Settings.h"

using namespace Upload;

// The multi-destination store: a JSON list of profiles + a default id in QSettings,
// the provider-type enum helpers, and the SFTP host-key pin store (same QSettings
// neighbourhood, same GUI-thread-only contract). Pure (test-mode isolated) settings,
// so the legacy migration never fires (no legacy bucket) and the keychain is only
// touched on remove() of a profile that has no secret.
class tst_UploadProfiles : public QObject
{
    Q_OBJECT

    static UploadProfile make(const QString &name, const QString &bucket)
    {
        UploadProfile p;
        p.id = UploadProfiles::newId();
        p.name = name;
        p.endpoint = "s3.amazonaws.com";
        p.region = "us-east-1";
        p.bucket = bucket;
        p.accessKeyId = "AKIA" + name;
        return p;
    }

private slots:
    void initTestCase()
    {
        QCoreApplication::setOrganizationName("NiceshotTest");
        QCoreApplication::setApplicationName("tst_uploadprofiles");
        QStandardPaths::setTestModeEnabled(true);
    }

    void init()
    {
        // Each test starts from an empty store.
        Core::Settings::setUploadProfilesJson(QString());
        Core::Settings::setUploadDefaultProfileId(QString());
        Core::Settings::setUploadKnownHostKeys(QString());   // ... including the host pins
    }

    void cleanup()
    {
        // Leave nothing behind for the next test (or the next run of this binary).
        Core::Settings::setUploadKnownHostKeys(QString());
    }

    void emptyByDefault()
    {
        QVERIFY(UploadProfiles::all().isEmpty());
        QVERIFY(UploadProfiles::defaultId().isEmpty());
        QVERIFY(UploadProfiles::defaultProfile().isNull());
    }

    void firstSaveBecomesDefault()
    {
        const UploadProfile a = make("A", "bucket-a");
        UploadProfiles::save(a);
        QCOMPARE(UploadProfiles::all().size(), 1);
        QCOMPARE(UploadProfiles::defaultId(), a.id);          // auto-default
        QCOMPARE(UploadProfiles::defaultProfile().bucket, QStringLiteral("bucket-a"));
    }

    void addUpdateByIdAndRoundTrip()
    {
        UploadProfile a = make("A", "bucket-a");
        UploadProfiles::save(a);
        UploadProfile b = make("B", "bucket-b");
        UploadProfiles::save(b);
        QCOMPARE(UploadProfiles::all().size(), 2);

        a.bucket = "bucket-a2";   // update by id, not append
        UploadProfiles::save(a);
        QCOMPARE(UploadProfiles::all().size(), 2);
        QCOMPARE(UploadProfiles::byId(a.id).bucket, QStringLiteral("bucket-a2"));

        // Fields survive the JSON round-trip.
        const UploadProfile rb = UploadProfiles::byId(b.id);
        QCOMPARE(rb.name, QStringLiteral("B"));
        QCOMPARE(rb.region, QStringLiteral("us-east-1"));
        QCOMPARE(rb.accessKeyId, QStringLiteral("AKIAB"));
    }

    void setDefaultExplicit()
    {
        const UploadProfile a = make("A", "bucket-a");
        const UploadProfile b = make("B", "bucket-b");
        UploadProfiles::save(a);
        UploadProfiles::save(b);
        QCOMPARE(UploadProfiles::defaultId(), a.id);   // a was first
        UploadProfiles::setDefault(b.id);
        QCOMPARE(UploadProfiles::defaultProfile().id, b.id);
    }

    void removeReassignsDefault()
    {
        const UploadProfile a = make("A", "bucket-a");
        const UploadProfile b = make("B", "bucket-b");
        UploadProfiles::save(a);   // default
        UploadProfiles::save(b);
        UploadProfiles::remove(a.id);   // removing the default
        QCOMPARE(UploadProfiles::all().size(), 1);
        QCOMPARE(UploadProfiles::defaultId(), b.id);    // reassigned to the survivor

        UploadProfiles::remove(b.id);   // last one
        QVERIFY(UploadProfiles::all().isEmpty());
        QVERIFY(UploadProfiles::defaultId().isEmpty()); // cleared
    }

    void byIdMissing()
    {
        QVERIFY(UploadProfiles::byId("nope").isNull());
        QVERIFY(UploadProfiles::byId(QString()).isNull());
    }

    // --- provider-type enum <-> string helpers -------------------------------

    void providerTypeStringRoundTrip()
    {
        QCOMPARE(providerTypeToString(ProviderType::S3), QStringLiteral("s3"));
        QCOMPARE(providerTypeToString(ProviderType::Sftp), QStringLiteral("sftp"));
        QCOMPARE(providerTypeToString(ProviderType::Ftp), QStringLiteral("ftp"));
        for (const ProviderType t : {ProviderType::S3, ProviderType::Sftp, ProviderType::Ftp})
            QCOMPARE(providerTypeFromString(providerTypeToString(t)), t);
        // Missing/unknown reads as S3, which is what makes pre-enum profiles round-trip.
        QCOMPARE(providerTypeFromString(QString()), ProviderType::S3);
        QCOMPARE(providerTypeFromString(""), ProviderType::S3);
        QCOMPARE(providerTypeFromString("gopher"), ProviderType::S3);
        QCOMPARE(providerTypeFromString("SFTP"), ProviderType::S3);   // the tokens are lowercase

        QCOMPARE(providerDisplayName(ProviderType::S3), QStringLiteral("S3"));
        QCOMPARE(providerDisplayName(ProviderType::Sftp), QStringLiteral("SFTP"));
        QCOMPARE(providerDisplayName(ProviderType::Ftp), QStringLiteral("FTP"));
    }

    void sftpAuthModeStringRoundTrip()
    {
        QCOMPARE(sftpAuthModeToString(SftpAuthMode::Password), QStringLiteral("password"));
        QCOMPARE(sftpAuthModeToString(SftpAuthMode::PrivateKey), QStringLiteral("key"));
        for (const SftpAuthMode m : {SftpAuthMode::Password, SftpAuthMode::PrivateKey})
            QCOMPARE(sftpAuthModeFromString(sftpAuthModeToString(m)), m);
        QCOMPARE(sftpAuthModeFromString(QString()), SftpAuthMode::Password);
        QCOMPARE(sftpAuthModeFromString("nonsense"), SftpAuthMode::Password);
    }

    void ftpEncryptionStringRoundTrip()
    {
        QCOMPARE(ftpEncryptionToString(FtpEncryption::None), QStringLiteral("none"));
        QCOMPARE(ftpEncryptionToString(FtpEncryption::Explicit), QStringLiteral("explicit"));
        QCOMPARE(ftpEncryptionToString(FtpEncryption::Implicit), QStringLiteral("implicit"));
        for (const FtpEncryption e : {FtpEncryption::None, FtpEncryption::Explicit,
                                      FtpEncryption::Implicit})
            QCOMPARE(ftpEncryptionFromString(ftpEncryptionToString(e)), e);
        // Missing/unknown must land on the SECURE default, never on plain FTP.
        QCOMPARE(ftpEncryptionFromString(QString()), FtpEncryption::Explicit);
        QCOMPARE(ftpEncryptionFromString("nonsense"), FtpEncryption::Explicit);
    }

    void keychainServicesAreDistinctPerType()
    {
        const QString s3 = keychainServiceFor(ProviderType::S3);
        const QString sftp = keychainServiceFor(ProviderType::Sftp);
        const QString ftp = keychainServiceFor(ProviderType::Ftp);
        QVERIFY(!s3.isEmpty());
        QVERIFY(!sftp.isEmpty());
        QVERIFY(!ftp.isEmpty());
        QVERIFY(s3 != sftp);
        QVERIFY(s3 != ftp);
        QVERIFY(sftp != ftp);
        // Frozen for compatibility: existing keychain items were written under this id.
        QCOMPARE(s3, QStringLiteral("com.darkog.niceshot.s3"));
        QCOMPARE(s3, Core::KeychainStore::s3Service());
        QCOMPARE(sftp, Core::KeychainStore::sftpService());
        QCOMPARE(ftp, Core::KeychainStore::ftpService());
    }

    // --- JSON round-trip of every field, per type ----------------------------

    void setAllRoundTripsEveryFieldForAllTypes()
    {
        UploadProfile s3;
        s3.id = UploadProfiles::newId();
        s3.name = "R2";
        s3.type = ProviderType::S3;
        s3.endpoint = "acct.r2.cloudflarestorage.com";
        s3.region = "auto";
        s3.bucket = "clips";
        s3.accessKeyId = "AKIA123";
        s3.keyPrefix = "screenshots/";
        s3.forcePathStyle = true;
        s3.publicBaseUrl = "https://pub-x.r2.dev";

        UploadProfile sftp;
        sftp.id = UploadProfiles::newId();
        sftp.name = "Box";
        sftp.type = ProviderType::Sftp;
        sftp.host = "sftp.example.com";
        sftp.port = 2222;
        sftp.username = "darko";
        sftp.remoteDir = "/var/www/uploads";
        sftp.sftpAuth = SftpAuthMode::PrivateKey;
        sftp.privateKeyPath = "/home/darko/.ssh/id_ed25519";
        sftp.publicBaseUrl = "https://cdn.example.com";

        UploadProfile ftp;
        ftp.id = UploadProfiles::newId();
        ftp.name = "Legacy host";
        ftp.type = ProviderType::Ftp;
        ftp.host = "ftp.example.com";
        ftp.port = 990;
        ftp.username = "";                              // anonymous
        ftp.remoteDir = "public_html/shots";
        ftp.ftpEncryption = FtpEncryption::Implicit;

        UploadProfiles::setAll({s3, sftp, ftp}, sftp.id);
        QCOMPARE(UploadProfiles::all().size(), 3);
        QCOMPARE(UploadProfiles::defaultId(), sftp.id);

        const UploadProfile rs3 = UploadProfiles::byId(s3.id);
        QCOMPARE(rs3.type, ProviderType::S3);
        QCOMPARE(rs3.name, QStringLiteral("R2"));
        QCOMPARE(rs3.endpoint, QStringLiteral("acct.r2.cloudflarestorage.com"));
        QCOMPARE(rs3.region, QStringLiteral("auto"));
        QCOMPARE(rs3.bucket, QStringLiteral("clips"));
        QCOMPARE(rs3.accessKeyId, QStringLiteral("AKIA123"));
        QCOMPARE(rs3.keyPrefix, QStringLiteral("screenshots/"));
        QVERIFY(rs3.forcePathStyle);
        QCOMPARE(rs3.publicBaseUrl, QStringLiteral("https://pub-x.r2.dev"));

        const UploadProfile rsftp = UploadProfiles::byId(sftp.id);
        QCOMPARE(rsftp.type, ProviderType::Sftp);
        QCOMPARE(rsftp.host, QStringLiteral("sftp.example.com"));
        QCOMPARE(rsftp.port, 2222);
        QCOMPARE(rsftp.username, QStringLiteral("darko"));
        QCOMPARE(rsftp.remoteDir, QStringLiteral("/var/www/uploads"));
        QCOMPARE(rsftp.sftpAuth, SftpAuthMode::PrivateKey);
        QCOMPARE(rsftp.privateKeyPath, QStringLiteral("/home/darko/.ssh/id_ed25519"));
        QCOMPARE(rsftp.publicBaseUrl, QStringLiteral("https://cdn.example.com"));
        QCOMPARE(rsftp.ftpEncryption, FtpEncryption::Explicit);   // untouched default

        const UploadProfile rftp = UploadProfiles::byId(ftp.id);
        QCOMPARE(rftp.type, ProviderType::Ftp);
        QCOMPARE(rftp.host, QStringLiteral("ftp.example.com"));
        QCOMPARE(rftp.port, 990);
        QVERIFY(rftp.username.isEmpty());
        QCOMPARE(rftp.remoteDir, QStringLiteral("public_html/shots"));
        QCOMPARE(rftp.ftpEncryption, FtpEncryption::Implicit);
        QCOMPARE(rftp.sftpAuth, SftpAuthMode::Password);          // untouched default
    }

    // --- back-compat with profiles written before the type discriminator -----

    void legacyProfileWithoutTypeReadsAsS3()
    {
        // Exactly the shape the pre-SFTP/FTP build persisted: no "type", no host/port/
        // remoteDir/sftpAuthMode/privateKeyPath/ftpsMode keys at all.
        Core::Settings::setUploadProfilesJson(QStringLiteral(
            R"([{"id":"legacy-1","name":"Old S3","endpoint":"s3.amazonaws.com",)"
            R"("region":"eu-central-1","bucket":"shots","accessKeyId":"AKIALEGACY",)"
            R"("keyPrefix":"screenshots/","publicBaseUrl":"https://cdn.example.com",)"
            R"("forcePathStyle":true}])"));

        const QVector<UploadProfile> v = UploadProfiles::all();
        QCOMPARE(v.size(), 1);
        const UploadProfile &p = v.first();
        QCOMPARE(p.type, ProviderType::S3);          // the whole point
        QCOMPARE(p.id, QStringLiteral("legacy-1"));
        QCOMPARE(p.name, QStringLiteral("Old S3"));
        QCOMPARE(p.endpoint, QStringLiteral("s3.amazonaws.com"));
        QCOMPARE(p.region, QStringLiteral("eu-central-1"));
        QCOMPARE(p.bucket, QStringLiteral("shots"));
        QCOMPARE(p.accessKeyId, QStringLiteral("AKIALEGACY"));
        QCOMPARE(p.keyPrefix, QStringLiteral("screenshots/"));
        QCOMPARE(p.publicBaseUrl, QStringLiteral("https://cdn.example.com"));
        QVERIFY(p.forcePathStyle);
        // The absent new fields land on their defaults.
        QVERIFY(p.host.isEmpty());
        QCOMPARE(p.port, 0);
        QVERIFY(p.username.isEmpty());
        QVERIFY(p.remoteDir.isEmpty());
        QVERIFY(p.privateKeyPath.isEmpty());
        QCOMPARE(p.sftpAuth, SftpAuthMode::Password);
        QCOMPARE(p.ftpEncryption, FtpEncryption::Explicit);
        // ... and rewriting it keeps the S3 identity.
        UploadProfiles::save(p);
        QCOMPARE(UploadProfiles::byId("legacy-1").type, ProviderType::S3);
        QCOMPARE(UploadProfiles::byId("legacy-1").bucket, QStringLiteral("shots"));
    }

    void unknownTypeReadsAsS3()
    {
        Core::Settings::setUploadProfilesJson(QStringLiteral(
            R"([{"id":"g1","name":"Gopher","type":"gopher","bucket":"shots"}])"));
        const QVector<UploadProfile> v = UploadProfiles::all();
        QCOMPARE(v.size(), 1);
        QCOMPARE(v.first().type, ProviderType::S3);
        QCOMPARE(v.first().bucket, QStringLiteral("shots"));
    }

    // --- KnownHosts: the SFTP host-key pin store -----------------------------

    void knownHostsEmptyStoreHasNoPin()
    {
        QVERIFY(!KnownHosts::lookup("sftp.example.com", 22).has_value());
    }

    void knownHostsRememberThenLookup()
    {
        const QString fp = "SHA256:abcdefghijklmnopqrstuvwxyz0123456789ABCDEFG";
        KnownHosts::remember("sftp.example.com", 22, fp);
        const auto got = KnownHosts::lookup("sftp.example.com", 22);
        QVERIFY(got.has_value());
        QCOMPARE(*got, fp);
        // A different host is a different pin.
        QVERIFY(!KnownHosts::lookup("other.example.com", 22).has_value());
    }

    void knownHostsKeyIncludesThePort()
    {
        // Two SSH daemons on one machine can hold different keys, so the port is part
        // of the key. (Callers resolve 0 -> 22 before calling; the store is literal.)
        KnownHosts::remember("sftp.example.com", 22, "SHA256:aaa");
        QVERIFY(!KnownHosts::lookup("sftp.example.com", 2222).has_value());
        KnownHosts::remember("sftp.example.com", 2222, "SHA256:bbb");
        QCOMPARE(*KnownHosts::lookup("sftp.example.com", 22), QStringLiteral("SHA256:aaa"));
        QCOMPARE(*KnownHosts::lookup("sftp.example.com", 2222), QStringLiteral("SHA256:bbb"));
    }

    void knownHostsRememberOverwrites()
    {
        KnownHosts::remember("sftp.example.com", 22, "SHA256:aaa");
        KnownHosts::remember("sftp.example.com", 22, "SHA256:bbb");
        QCOMPARE(*KnownHosts::lookup("sftp.example.com", 22), QStringLiteral("SHA256:bbb"));
    }

    void knownHostsIgnoresEmptyInput()
    {
        KnownHosts::remember(QString(), 22, "SHA256:aaa");
        KnownHosts::remember("sftp.example.com", 22, QString());
        QVERIFY(!KnownHosts::lookup(QString(), 22).has_value());
        QVERIFY(!KnownHosts::lookup("sftp.example.com", 22).has_value());
        QVERIFY(Core::Settings::uploadKnownHostKeys().isEmpty());   // nothing written at all
    }
};

QTEST_MAIN(tst_UploadProfiles)
#include "tst_uploadprofiles.moc"
