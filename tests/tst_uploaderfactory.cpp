#include <QtTest>
#include <QSignalSpy>
#include <QStandardPaths>

#include "upload/UploaderFactory.h"
#include "upload/Uploader.h"
#include "upload/UploadConfig.h"
#include "upload/UploadProfiles.h"
#include "core/Settings.h"

using namespace Upload;

// Factory surface only - never calls a live upload() (same rule as the capture/record
// factory tests). With no configured destination, Auto/S3 resolve to the inert stub,
// whose upload() fails deferred rather than hitting the network.
class tst_UploaderFactory : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        QCoreApplication::setOrganizationName("NiceshotTest");
        QCoreApplication::setApplicationName("tst_uploaderfactory");
        QStandardPaths::setTestModeEnabled(true);   // isolated, empty settings -> not configured
    }

    void init()
    {
        // Start every test from an empty store, independent of what a previous test (or
        // a previous run of this binary) left behind.
        Core::Settings::setUploadEnabled(false);
        Core::Settings::setUploadProfilesJson(QString());
        Core::Settings::setUploadDefaultProfileId(QString());
    }

    void availabilityIsCompileTime()
    {
        // Availability answers "was this backend built in", NOT "is it configured" - the
        // settings UI greys out types it could never run. S3/Stub/Auto are pure Qt, so
        // they are available even with nothing configured.
        QVERIFY(UploaderFactory::isStrategyAvailable(UploaderFactory::StrategyType::S3));
        QVERIFY(UploaderFactory::isStrategyAvailable(UploaderFactory::StrategyType::Stub));
        QVERIFY(UploaderFactory::isStrategyAvailable(UploaderFactory::StrategyType::Auto));
#ifdef HAVE_LIBSSH2
        QVERIFY(UploaderFactory::isStrategyAvailable(UploaderFactory::StrategyType::Sftp));
#else
        QVERIFY(!UploaderFactory::isStrategyAvailable(UploaderFactory::StrategyType::Sftp));
#endif
#ifdef HAVE_LIBCURL
        QVERIFY(UploaderFactory::isStrategyAvailable(UploaderFactory::StrategyType::Ftp));
#else
        QVERIFY(!UploaderFactory::isStrategyAvailable(UploaderFactory::StrategyType::Ftp));
#endif
    }

    void unconfiguredResolvesToStub()
    {
        QCOMPARE(UploaderFactory::getDefaultStrategyType(), UploaderFactory::StrategyType::Stub);

        auto up = UploaderFactory::create(UploaderFactory::StrategyType::Auto);
        QVERIFY(up != nullptr);
        QVERIFY(!up->isConfigured());
        QCOMPARE(up->name(), QStringLiteral("None"));   // the stub
    }

    void stubUploadFailsDeferred()
    {
        auto up = UploaderFactory::create(UploaderFactory::StrategyType::Stub);
        QSignalSpy failedSpy(up.get(), &Uploader::failed);
        QSignalSpy uploadedSpy(up.get(), &Uploader::uploaded);
        up->upload("/tmp/whatever.png", "whatever.png");
        QVERIFY(failedSpy.wait(1000));
        QCOMPARE(failedSpy.count(), 1);
        QCOMPARE(uploadedSpy.count(), 0);
    }

    void explicitS3WithoutConfigFallsToStub()
    {
        // Asking for S3 while unconfigured must not crash or hit the network - it
        // degrades to the stub.
        auto up = UploaderFactory::create(UploaderFactory::StrategyType::S3);
        QVERIFY(up != nullptr);
        QVERIFY(!up->isConfigured());
    }

    // Regression: Auto used to resolve from the DEFAULT profile even when a specific
    // profileId was passed, so uploading to a non-default destination via the ▾ menu
    // silently used the default's backend. Default here is an incomplete S3 profile;
    // profile B is a complete SFTP key-auth destination (key auth needs no keychain
    // secret, so this stays keychain-free and works on the stub-keychain platforms).
    void autoResolvesFromRequestedProfile()
    {
        Core::Settings::setUploadEnabled(true);

        UploadProfile a;
        a.id = UploadProfiles::newId();
        a.name = "Incomplete S3";               // no bucket/accessKeyId/secret
        UploadProfile b;
        b.id = UploadProfiles::newId();
        b.name = "Key-auth SFTP";
        b.type = ProviderType::Sftp;
        b.host = "sftp.example.com";
        b.username = "darko";
        b.sftpAuth = SftpAuthMode::PrivateKey;
        b.privateKeyPath = "/home/darko/.ssh/id_ed25519";
        UploadProfiles::setAll({a, b}, a.id);

        // The resolution itself: the default is incomplete, yet B is complete and SFTP.
        QVERIFY(!UploadConfig::forProfile(a.id).isComplete());
        QVERIFY(UploadConfig::forProfile(b.id).isComplete());
        QCOMPARE(UploaderFactory::getDefaultStrategyType(), UploaderFactory::StrategyType::Stub);

        auto up = UploaderFactory::create(UploaderFactory::StrategyType::Auto, nullptr, b.id);
        QVERIFY(up != nullptr);
#ifdef HAVE_LIBSSH2
        QCOMPARE(up->name(), QStringLiteral("SFTP"));
        QVERIFY(up->isConfigured());
#else
        // A compiled-out backend explains itself instead of claiming "not configured".
        QCOMPARE(up->name(), QStringLiteral("None"));
        QSignalSpy failedSpy(up.get(), &Uploader::failed);
        up->upload("/tmp/whatever.png", "whatever.png");
        QVERIFY(failedSpy.wait(1000));
        QVERIFY(failedSpy.first().first().toString().contains("not included in this build"));
#endif
    }

    // Same regression for the FTP backend, which a libcurl build resolves to a real
    // FtpUploader. An anonymous FTP destination (empty username) is complete without a
    // stored secret, so this too stays keychain-free. upload() is never called on the
    // real backend - no network from the test suite.
    void autoResolvesFtpFromRequestedProfile()
    {
        Core::Settings::setUploadEnabled(true);

        UploadProfile a;
        a.id = UploadProfiles::newId();
        a.name = "Incomplete S3";               // no bucket/accessKeyId/secret
        UploadProfile b;
        b.id = UploadProfiles::newId();
        b.name = "Anonymous FTP";
        b.type = ProviderType::Ftp;
        b.host = "ftp.example.com";
        b.remoteDir = "/pub/incoming";
        UploadProfiles::setAll({a, b}, a.id);

        QVERIFY(!UploadConfig::forProfile(a.id).isComplete());
        QVERIFY(UploadConfig::forProfile(b.id).isComplete());
        QCOMPARE(UploaderFactory::getDefaultStrategyType(), UploaderFactory::StrategyType::Stub);

        auto up = UploaderFactory::create(UploaderFactory::StrategyType::Auto, nullptr, b.id);
        QVERIFY(up != nullptr);
#ifdef HAVE_LIBCURL
        QCOMPARE(up->name(), QStringLiteral("FTP"));
        QVERIFY(up->isConfigured());
#else
        // A compiled-out backend explains itself instead of claiming "not configured".
        QCOMPARE(up->name(), QStringLiteral("None"));
        QSignalSpy failedSpy(up.get(), &Uploader::failed);
        up->upload("/tmp/whatever.png", "whatever.png");
        QVERIFY(failedSpy.wait(1000));
        QVERIFY(failedSpy.first().first().toString().contains("not included in this build"));
#endif
    }
};

QTEST_MAIN(tst_UploaderFactory)
#include "tst_uploaderfactory.moc"
