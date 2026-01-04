#include <QtTest>
#include <QSignalSpy>
#include <QStandardPaths>

#include "upload/UploaderFactory.h"
#include "upload/Uploader.h"
#include "core/Settings.h"

using namespace Upload;

// Factory surface only — never calls a live upload() (same rule as the capture/record
// factory tests). With no configured bucket, Auto/S3 resolve to the inert stub, whose
// upload() fails deferred rather than hitting the network.
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

    void unconfiguredResolvesToStub()
    {
        QVERIFY(!UploaderFactory::isStrategyAvailable(UploaderFactory::StrategyType::S3));
        QVERIFY(UploaderFactory::isStrategyAvailable(UploaderFactory::StrategyType::Stub));
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
        // Asking for S3 while unconfigured must not crash or hit the network — it
        // degrades to the stub.
        auto up = UploaderFactory::create(UploaderFactory::StrategyType::S3);
        QVERIFY(up != nullptr);
        QVERIFY(!up->isConfigured());
    }
};

QTEST_MAIN(tst_UploaderFactory)
#include "tst_uploaderfactory.moc"
