#include <QtTest>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
#include <QStandardPaths>

#include "upload/ProviderRegistry.h"
#include "upload/UploadConfig.h"
#include "upload/UploaderFactory.h"
#include "upload/UploadProfiles.h"
#include "core/Settings.h"

using namespace Upload;

// The upload provider registry: each ProviderSpec is self-describing, and the profile
// store reads and writes its fields under the JSON keys earlier builds used. Test-mode
// isolated settings; the keychain and the network are never touched.
class tst_ProviderRegistry : public QObject
{
    Q_OBJECT

    // A value for the field that is not its default, so a round trip proves it was stored.
    static QVariant sample(const FieldSpec &f)
    {
        switch (f.kind) {
        case FieldKind::Port:
            return 2121;
        case FieldKind::Flag:
            return !f.defaultValue.toBool();
        case FieldKind::Choice:
            for (const auto &choice : f.choices)
                if (choice.first != f.defaultValue.toString())
                    return choice.first;
            return f.defaultValue;
        case FieldKind::FilePath:
            return QStringLiteral("/home/darko/.ssh/") + f.key;
        case FieldKind::Text:
        case FieldKind::Secret:
            break;
        }
        return QStringLiteral("v-") + f.key;
    }

    static void providerRows()
    {
        QTest::addColumn<QString>("id");
        for (const ProviderSpec &spec : ProviderRegistry::providers())
            QTest::newRow(qPrintable(spec.id)) << spec.id;
    }

private slots:
    void initTestCase()
    {
        QCoreApplication::setOrganizationName("SnimTest");
        QCoreApplication::setApplicationName("tst_providerregistry");
        QStandardPaths::setTestModeEnabled(true);
    }

    void init()
    {
        Core::Settings::setUploadProfilesJson(QString());
        Core::Settings::setUploadDefaultProfileId(QString());
    }

    void idsAreTheBuiltInProviders()
    {
        QStringList ids;
        for (const ProviderSpec &spec : ProviderRegistry::providers())
            ids << spec.id;
        // Persisted as the profile type, and the Add menu's order.
        QCOMPARE(ids, (QStringList{"s3", "sftp", "ftp"}));
        for (const QString &id : ids)
            QCOMPARE(ProviderRegistry::find(id)->id, id);
    }

    void keychainServicesAreUniqueAndFrozen()
    {
        QSet<QString> services;
        for (const ProviderSpec &spec : ProviderRegistry::providers())
            services.insert(spec.keychainService);
        QCOMPARE(services.size(), ProviderRegistry::providers().size());
        // Existing keychain items were written under these ids.
        QCOMPARE(ProviderRegistry::find("s3")->keychainService, QStringLiteral("dev.snim.s3"));
        QCOMPARE(ProviderRegistry::find("sftp")->keychainService, QStringLiteral("dev.snim.sftp"));
        QCOMPARE(ProviderRegistry::find("ftp")->keychainService, QStringLiteral("dev.snim.ftp"));
    }

    void everySpecDescribesItself_data() { providerRows(); }
    void everySpecDescribesItself()
    {
        QFETCH(QString, id);
        const ProviderSpec &spec = *ProviderRegistry::find(id);
        QVERIFY(!spec.displayName.isEmpty());
        QVERIFY(!spec.addLabel.isEmpty());
        QVERIFY(!spec.namePlaceholder.isEmpty());
        QVERIFY(spec.isComplete);
        QVERIFY(spec.secretLabel);
        QVERIFY(spec.secretHint);
        // A backend is either built in or explains why not.
        QCOMPARE(bool(spec.create), spec.available());

        QSet<QString> keys;
        int secrets = 0;
        for (const FieldSpec &f : spec.fields) {
            if (f.kind == FieldKind::Secret) {
                ++secrets;
                continue;
            }
            QVERIFY(!f.key.isEmpty());
            QVERIFY2(!keys.contains(f.key), qPrintable(f.key));
            keys.insert(f.key);
            QVERIFY(!f.label.isEmpty());
            QCOMPARE(spec.field(f.key), &f);
            if (f.kind == FieldKind::Choice) {
                QVERIFY(!f.choices.isEmpty());
                const bool known = std::any_of(f.choices.cbegin(), f.choices.cend(),
                                               [&f](const auto &c) {
                                                   return c.first == f.defaultValue.toString();
                                               });
                QVERIFY2(known, qPrintable(f.key));
            }
        }
        QCOMPARE(secrets, 1);
        QVERIFY(!spec.field(QStringLiteral("publicBaseUrl")));   // shared, not a provider field
    }

    void unknownTypesReadAsS3()
    {
        QVERIFY(!ProviderRegistry::find("gopher"));
        QVERIFY(!ProviderRegistry::find(QString()));
        QVERIFY(!ProviderRegistry::find("SFTP"));
        QCOMPARE(UploadProfile().type, QStringLiteral("s3"));

        Core::Settings::setUploadProfilesJson(QStringLiteral(
            R"([{"id":"g1","type":"gopher","bucket":"shots"},)"
            R"({"id":"notype-1","bucket":"clips"}])"));
        QCOMPARE(UploadProfiles::byId("g1").type, QStringLiteral("s3"));
        QCOMPARE(UploadProfiles::byId("g1").text("bucket"), QStringLiteral("shots"));
        QCOMPARE(UploadProfiles::byId("notype-1").type, QStringLiteral("s3"));
        QCOMPARE(UploadProfiles::byId("notype-1").text("bucket"), QStringLiteral("clips"));
        // Nothing to upload with: an unknown type is never complete.
        UploadConfig c;
        c.enabled = true;
        c.type = QStringLiteral("gopher");
        QVERIFY(!c.isComplete());
        QVERIFY(!UploaderFactory::isAvailable("gopher"));
    }

    void everyFieldRoundTrips_data() { providerRows(); }
    void everyFieldRoundTrips()
    {
        QFETCH(QString, id);
        const ProviderSpec &spec = *ProviderRegistry::find(id);
        UploadProfile p;
        p.id = UploadProfiles::newId();
        p.name = "Round trip";
        p.type = id;
        p.publicBaseUrl = "https://cdn.example.com";
        for (const FieldSpec &f : spec.fields)
            if (f.kind != FieldKind::Secret)
                p.values.insert(f.key, sample(f));
        UploadProfiles::setAll({p}, p.id);

        // Flat, under each field's own key.
        const QJsonObject o =
            QJsonDocument::fromJson(Core::Settings::uploadProfilesJson().toUtf8())[0].toObject();
        QCOMPARE(o.value("type").toString(), id);
        const UploadProfile r = UploadProfiles::byId(p.id);
        QCOMPARE(r.type, id);
        QCOMPARE(r.publicBaseUrl, p.publicBaseUrl);
        for (const FieldSpec &f : spec.fields) {
            if (f.kind == FieldKind::Secret)
                continue;
            QVERIFY2(o.contains(f.key), qPrintable(f.key));
            QCOMPARE(r.value(f.key), sample(f));
        }
    }

    void missingKeysReadAsTheirDefaults_data() { providerRows(); }
    void missingKeysReadAsTheirDefaults()
    {
        QFETCH(QString, id);
        Core::Settings::setUploadProfilesJson(
            QStringLiteral(R"([{"id":"bare","type":"%1"}])").arg(id));
        const UploadProfile r = UploadProfiles::byId("bare");
        QCOMPARE(r.type, id);
        for (const FieldSpec &f : ProviderRegistry::find(id)->fields)
            if (f.kind != FieldKind::Secret)
                QCOMPARE(r.value(f.key), f.defaultValue);
    }

    // Exactly what earlier builds wrote (every key on every profile), plus a profile from
    // before the type key existed.
    void todaysJsonLoadsIntact()
    {
        Core::Settings::setUploadProfilesJson(QStringLiteral(R"([)"
            R"({"id":"s3-1","name":"R2","type":"s3","endpoint":"acct.r2.cloudflarestorage.com",)"
            R"("region":"auto","bucket":"clips","accessKeyId":"AKIA123","keyPrefix":"shots/",)"
            R"("forcePathStyle":true,"host":"","port":0,"username":"","remoteDir":"",)"
            R"("sftpAuthMode":"password","privateKeyPath":"","ftpsMode":"explicit",)"
            R"("publicBaseUrl":"https://pub-x.r2.dev"},)"
            R"({"id":"sftp-1","name":"Box","type":"sftp","endpoint":"","region":"","bucket":"",)"
            R"("accessKeyId":"","keyPrefix":"","forcePathStyle":false,"host":"sftp.example.com",)"
            R"("port":2222,"username":"darko","remoteDir":"/var/www/uploads",)"
            R"("sftpAuthMode":"key","privateKeyPath":"/home/darko/.ssh/id_ed25519",)"
            R"("ftpsMode":"explicit","publicBaseUrl":"https://cdn.example.com"},)"
            R"({"id":"ftp-1","name":"Anon","type":"ftp","endpoint":"","region":"","bucket":"",)"
            R"("accessKeyId":"","keyPrefix":"","forcePathStyle":false,"host":"ftp.example.com",)"
            R"("port":990,"username":"","remoteDir":"public_html/shots","sftpAuthMode":"password",)"
            R"("privateKeyPath":"","ftpsMode":"implicit","publicBaseUrl":""},)"
            R"({"id":"legacy-1","name":"Old S3","endpoint":"s3.amazonaws.com",)"
            R"("region":"eu-central-1","bucket":"shots","accessKeyId":"AKIALEGACY",)"
            R"("keyPrefix":"screenshots/","publicBaseUrl":"https://cdn.example.com",)"
            R"("forcePathStyle":true}])"));

        // Twice: as read, and after a rewrite in the current format.
        for (int pass = 0; pass < 2; ++pass) {
            const UploadProfile s3 = UploadProfiles::byId("s3-1");
            QCOMPARE(s3.type, QStringLiteral("s3"));
            QCOMPARE(s3.name, QStringLiteral("R2"));
            QCOMPARE(s3.text("endpoint"), QStringLiteral("acct.r2.cloudflarestorage.com"));
            QCOMPARE(s3.text("region"), QStringLiteral("auto"));
            QCOMPARE(s3.text("bucket"), QStringLiteral("clips"));
            QCOMPARE(s3.text("accessKeyId"), QStringLiteral("AKIA123"));
            QCOMPARE(s3.text("keyPrefix"), QStringLiteral("shots/"));
            QVERIFY(s3.flag("forcePathStyle"));
            QCOMPARE(s3.publicBaseUrl, QStringLiteral("https://pub-x.r2.dev"));

            const UploadProfile sftp = UploadProfiles::byId("sftp-1");
            QCOMPARE(sftp.type, QStringLiteral("sftp"));
            QCOMPARE(sftp.text("host"), QStringLiteral("sftp.example.com"));
            QCOMPARE(sftp.number("port"), 2222);
            QCOMPARE(sftp.text("username"), QStringLiteral("darko"));
            QCOMPARE(sftp.text("remoteDir"), QStringLiteral("/var/www/uploads"));
            QCOMPARE(sftp.text("sftpAuthMode"), QStringLiteral("key"));
            QCOMPARE(sftp.text("privateKeyPath"), QStringLiteral("/home/darko/.ssh/id_ed25519"));
            QCOMPARE(sftp.publicBaseUrl, QStringLiteral("https://cdn.example.com"));

            const UploadProfile ftp = UploadProfiles::byId("ftp-1");
            QCOMPARE(ftp.type, QStringLiteral("ftp"));
            QCOMPARE(ftp.text("host"), QStringLiteral("ftp.example.com"));
            QCOMPARE(ftp.number("port"), 990);
            QVERIFY(ftp.text("username").isEmpty());
            QCOMPARE(ftp.text("remoteDir"), QStringLiteral("public_html/shots"));
            QCOMPARE(ftp.text("ftpsMode"), QStringLiteral("implicit"));
            QVERIFY(ftp.publicBaseUrl.isEmpty());

            const UploadProfile legacy = UploadProfiles::byId("legacy-1");
            QCOMPARE(legacy.type, QStringLiteral("s3"));
            QCOMPARE(legacy.name, QStringLiteral("Old S3"));
            QCOMPARE(legacy.text("endpoint"), QStringLiteral("s3.amazonaws.com"));
            QCOMPARE(legacy.text("region"), QStringLiteral("eu-central-1"));
            QCOMPARE(legacy.text("bucket"), QStringLiteral("shots"));
            QCOMPARE(legacy.text("accessKeyId"), QStringLiteral("AKIALEGACY"));
            QCOMPARE(legacy.text("keyPrefix"), QStringLiteral("screenshots/"));
            QVERIFY(legacy.flag("forcePathStyle"));
            QCOMPARE(legacy.publicBaseUrl, QStringLiteral("https://cdn.example.com"));

            UploadProfiles::setAll(UploadProfiles::all(), "sftp-1");
        }
        QCOMPARE(UploadProfiles::all().size(), 4);
    }

    // Filled in (every setting, plus a secret), a provider gets its own backend, or a stub
    // that says it is not in this build; left empty, the generic stub.
    void factoryBuildsFromTheSpec_data() { providerRows(); }
    void factoryBuildsFromTheSpec()
    {
        QFETCH(QString, id);
        const ProviderSpec &spec = *ProviderRegistry::find(id);
        UploadConfig empty;
        empty.enabled = true;
        empty.type = id;
        QVERIFY(!empty.isComplete());
        const auto stub = UploaderFactory::createForConfig(empty);
        QCOMPARE(QString::fromLatin1(stub->metaObject()->className()),
                 QStringLiteral("Upload::StubUploader"));

        UploadConfig full = empty;
        for (const FieldSpec &f : spec.fields)
            if (f.kind != FieldKind::Secret)
                full.values.insert(f.key, sample(f));
        full.secretKey = QStringLiteral("sekrit");
        QVERIFY(full.isComplete());
        QCOMPARE(UploaderFactory::isAvailable(id), spec.available());
        auto up = UploaderFactory::createForConfig(full);
        const QString backend = QString::fromLatin1(up->metaObject()->className());
        if (spec.available()) {
            QVERIFY(backend != QStringLiteral("Upload::StubUploader"));
            return;
        }
        QCOMPARE(backend, QStringLiteral("Upload::StubUploader"));
        QSignalSpy failedSpy(up.get(), &Uploader::failed);
        up->upload(QStringLiteral("/tmp/whatever.png"), QStringLiteral("whatever.png"));
        QVERIFY(failedSpy.wait(1000));
        QCOMPARE(failedSpy.first().first().toString(),
                 QStringLiteral("%1 support is not included in this build.").arg(spec.displayName));
    }
};

QTEST_MAIN(tst_ProviderRegistry)
#include "tst_providerregistry.moc"
