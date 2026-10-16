#include <QtTest>
#include <QStandardPaths>

#include "upload/KnownHosts.h"
#include "upload/UploadProfiles.h"
#include "core/Settings.h"

using namespace Upload;

// The multi-destination store: a JSON list of profiles + a default id in QSettings,
// the provider-type helpers, and the SFTP host-key pin store (same QSettings
// neighbourhood, same GUI-thread-only contract). Pure (test-mode isolated) settings,
// and the keychain is never touched.
class tst_UploadProfiles : public QObject
{
    Q_OBJECT

    static UploadProfile make(const QString &name, const QString &bucket)
    {
        UploadProfile p;
        p.id = UploadProfiles::newId();
        p.name = name;
        p.values = {{"endpoint", "s3.amazonaws.com"}, {"region", "us-east-1"},
                    {"bucket", bucket}, {"accessKeyId", "AKIA" + name}};
        return p;
    }

private slots:
    void initTestCase()
    {
        QCoreApplication::setOrganizationName("SnimTest");
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
        QVERIFY(UploadProfiles::defaultProfile().id.isEmpty());
    }

    void setAllFallsBackToTheFirstProfile()
    {
        const UploadProfile a = make("A", "bucket-a");
        const UploadProfile b = make("B", "bucket-b");
        UploadProfiles::setAll({a, b}, QString());
        QCOMPARE(UploadProfiles::defaultId(), a.id);
        UploadProfiles::setAll({a, b}, "missing");
        QCOMPARE(UploadProfiles::defaultProfile().text("bucket"), QStringLiteral("bucket-a"));
    }

    void setAllClearsTheDefaultWithTheList()
    {
        const UploadProfile a = make("A", "bucket-a");
        UploadProfiles::setAll({a}, a.id);
        UploadProfiles::setAll({}, a.id);
        QVERIFY(UploadProfiles::all().isEmpty());
        QVERIFY(UploadProfiles::defaultId().isEmpty());
    }

    void byIdMissing()
    {
        QVERIFY(UploadProfiles::byId("nope").id.isEmpty());
        QVERIFY(UploadProfiles::byId(QString()).id.isEmpty());
    }

    // --- provider-type helpers ---------------------------------------------

    void providerDisplayNames()
    {
        QCOMPARE(providerDisplayName("s3"), QStringLiteral("S3"));
        QCOMPARE(providerDisplayName("sftp"), QStringLiteral("SFTP"));
        QCOMPARE(providerDisplayName("ftp"), QStringLiteral("FTP"));
        QVERIFY(providerDisplayName("gopher").isEmpty());
    }

    void keychainServicesAreDistinctPerType()
    {
        const QString s3 = keychainServiceFor("s3");
        const QString sftp = keychainServiceFor("sftp");
        const QString ftp = keychainServiceFor("ftp");
        QVERIFY(!s3.isEmpty());
        QVERIFY(!sftp.isEmpty());
        QVERIFY(!ftp.isEmpty());
        QVERIFY(s3 != sftp);
        QVERIFY(s3 != ftp);
        QVERIFY(sftp != ftp);
        // Frozen for compatibility: existing keychain items were written under this id.
        QCOMPARE(s3, QStringLiteral("dev.snim.s3"));
        QCOMPARE(sftp, QStringLiteral("dev.snim.sftp"));
        QCOMPARE(ftp, QStringLiteral("dev.snim.ftp"));
    }

    // --- JSON round-trip of every field, per type ----------------------------

    void setAllRoundTripsEveryFieldForAllTypes()
    {
        UploadProfile s3;
        s3.id = UploadProfiles::newId();
        s3.name = "R2";
        s3.type = "s3";
        s3.values = {{"endpoint", "acct.r2.cloudflarestorage.com"}, {"region", "auto"},
                     {"bucket", "clips"}, {"accessKeyId", "AKIA123"},
                     {"keyPrefix", "screenshots/"}, {"forcePathStyle", true}};
        s3.publicBaseUrl = "https://pub-x.r2.dev";

        UploadProfile sftp;
        sftp.id = UploadProfiles::newId();
        sftp.name = "Box";
        sftp.type = "sftp";
        sftp.values = {{"host", "sftp.example.com"}, {"port", 2222}, {"username", "darko"},
                       {"remoteDir", "/var/www/uploads"}, {"sftpAuthMode", "key"},
                       {"privateKeyPath", "/home/darko/.ssh/id_ed25519"}};
        sftp.publicBaseUrl = "https://cdn.example.com";

        UploadProfile ftp;
        ftp.id = UploadProfiles::newId();
        ftp.name = "Legacy host";
        ftp.type = "ftp";
        ftp.values = {{"host", "ftp.example.com"}, {"port", 990},
                      {"username", ""},                         // anonymous
                      {"remoteDir", "public_html/shots"}, {"ftpsMode", "implicit"}};

        UploadProfiles::setAll({s3, sftp, ftp}, sftp.id);
        QCOMPARE(UploadProfiles::all().size(), 3);
        QCOMPARE(UploadProfiles::defaultId(), sftp.id);

        const UploadProfile rs3 = UploadProfiles::byId(s3.id);
        QCOMPARE(rs3.type, QStringLiteral("s3"));
        QCOMPARE(rs3.name, QStringLiteral("R2"));
        QCOMPARE(rs3.text("endpoint"), QStringLiteral("acct.r2.cloudflarestorage.com"));
        QCOMPARE(rs3.text("region"), QStringLiteral("auto"));
        QCOMPARE(rs3.text("bucket"), QStringLiteral("clips"));
        QCOMPARE(rs3.text("accessKeyId"), QStringLiteral("AKIA123"));
        QCOMPARE(rs3.text("keyPrefix"), QStringLiteral("screenshots/"));
        QVERIFY(rs3.flag("forcePathStyle"));
        QCOMPARE(rs3.publicBaseUrl, QStringLiteral("https://pub-x.r2.dev"));

        const UploadProfile rsftp = UploadProfiles::byId(sftp.id);
        QCOMPARE(rsftp.type, QStringLiteral("sftp"));
        QCOMPARE(rsftp.text("host"), QStringLiteral("sftp.example.com"));
        QCOMPARE(rsftp.number("port"), 2222);
        QCOMPARE(rsftp.text("username"), QStringLiteral("darko"));
        QCOMPARE(rsftp.text("remoteDir"), QStringLiteral("/var/www/uploads"));
        QCOMPARE(rsftp.text("sftpAuthMode"), QStringLiteral("key"));
        QCOMPARE(rsftp.text("privateKeyPath"), QStringLiteral("/home/darko/.ssh/id_ed25519"));
        QCOMPARE(rsftp.publicBaseUrl, QStringLiteral("https://cdn.example.com"));

        const UploadProfile rftp = UploadProfiles::byId(ftp.id);
        QCOMPARE(rftp.type, QStringLiteral("ftp"));
        QCOMPARE(rftp.text("host"), QStringLiteral("ftp.example.com"));
        QCOMPARE(rftp.number("port"), 990);
        QVERIFY(rftp.text("username").isEmpty());
        QCOMPARE(rftp.text("remoteDir"), QStringLiteral("public_html/shots"));
        QCOMPARE(rftp.text("ftpsMode"), QStringLiteral("implicit"));
    }

    // Choices persist as their tokens, and a token outside the list reads as the default.
    void choicesPersistAsTheirTokens()
    {
        UploadProfile sftp;
        sftp.id = "s1";
        sftp.type = "sftp";
        sftp.values = {{"sftpAuthMode", "key"}};
        UploadProfile ftp;
        ftp.id = "f1";
        ftp.type = "ftp";
        ftp.values = {{"ftpsMode", "none"}};
        UploadProfiles::setAll({sftp, ftp}, sftp.id);
        const QString json = Core::Settings::uploadProfilesJson();
        QVERIFY(json.contains(QStringLiteral(R"("sftpAuthMode":"key")")));
        QVERIFY(json.contains(QStringLiteral(R"("ftpsMode":"none")")));

        Core::Settings::setUploadProfilesJson(QStringLiteral(
            R"([{"id":"s1","type":"sftp","sftpAuthMode":"nonsense"},)"
            R"({"id":"f1","type":"ftp","ftpsMode":"nonsense"},{"id":"f2","type":"ftp"}])"));
        QCOMPARE(UploadProfiles::byId("s1").text("sftpAuthMode"), QStringLiteral("password"));
        // Missing/unknown must land on the SECURE default, never on plain FTP.
        QCOMPARE(UploadProfiles::byId("f1").text("ftpsMode"), QStringLiteral("explicit"));
        QCOMPARE(UploadProfiles::byId("f2").text("ftpsMode"), QStringLiteral("explicit"));
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
        QCOMPARE(p.type, QStringLiteral("s3"));     // the whole point
        QCOMPARE(p.id, QStringLiteral("legacy-1"));
        QCOMPARE(p.name, QStringLiteral("Old S3"));
        QCOMPARE(p.text("endpoint"), QStringLiteral("s3.amazonaws.com"));
        QCOMPARE(p.text("region"), QStringLiteral("eu-central-1"));
        QCOMPARE(p.text("bucket"), QStringLiteral("shots"));
        QCOMPARE(p.text("accessKeyId"), QStringLiteral("AKIALEGACY"));
        QCOMPARE(p.text("keyPrefix"), QStringLiteral("screenshots/"));
        QCOMPARE(p.publicBaseUrl, QStringLiteral("https://cdn.example.com"));
        QVERIFY(p.flag("forcePathStyle"));
        // Only the provider's own settings are kept.
        QCOMPARE(p.values.size(), 6);
        // ... and rewriting it keeps the S3 identity.
        UploadProfiles::setAll({p}, p.id);
        QCOMPARE(UploadProfiles::byId("legacy-1").type, QStringLiteral("s3"));
        QCOMPARE(UploadProfiles::byId("legacy-1").text("bucket"), QStringLiteral("shots"));
    }

    void unknownTypeReadsAsS3()
    {
        Core::Settings::setUploadProfilesJson(QStringLiteral(
            R"([{"id":"g1","name":"Gopher","type":"gopher","bucket":"shots"},)"
            R"({"id":"u1","type":"SFTP","host":"h"}])"));
        const QVector<UploadProfile> v = UploadProfiles::all();
        QCOMPARE(v.size(), 2);
        QCOMPARE(v.first().type, QStringLiteral("s3"));
        QCOMPARE(v.first().text("bucket"), QStringLiteral("shots"));
        QCOMPARE(v.last().type, QStringLiteral("s3"));   // the tokens are lowercase
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
