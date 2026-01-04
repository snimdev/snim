#include <QtTest>
#include <QStandardPaths>

#include "upload/UploadProfiles.h"
#include "core/Settings.h"

using namespace Upload;

// The multi-destination store: a JSON list of profiles + a default id in QSettings.
// Pure (test-mode isolated) settings, so the legacy migration never fires (no legacy
// bucket) and the keychain is only touched on remove() of a profile that has no secret.
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
};

QTEST_MAIN(tst_UploadProfiles)
#include "tst_uploadprofiles.moc"
