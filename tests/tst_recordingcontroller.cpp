#include <QtTest>
#include <QSignalSpy>
#include <QStandardPaths>
#include <memory>

#include "recording/RecordingController.h"
#include "recording/RecordingStrategy.h"
#include "recording/RecordTarget.h"

using namespace Recording;

// A fake backend so the controller's state machine is testable headlessly, with no
// ScreenCaptureKit and no real screen grab.
class FakeRecordingStrategy : public RecordingStrategy
{
    Q_OBJECT
public:
    void start(const RecordTarget &target, const QString &outputPath) override
    {
        lastTarget = target; lastPath = outputPath; startCalled = true;
    }
    void stop() override { stopCalled = true; }
    bool isRecording() const override { return m_running; }
    bool isAvailable() const override { return true; }
    QString name() const override { return QStringLiteral("Fake"); }

    void emitStarted() { m_running = true; emit started(); }
    void emitFinished(const QString &p) { m_running = false; emit finished(p); }
    void emitFailed(const QString &e) { m_running = false; emit failed(e); }

    RecordTarget lastTarget;
    QString lastPath;
    bool startCalled = false;
    bool stopCalled = false;
    bool m_running = false;
};

class tst_RecordingController : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        QCoreApplication::setOrganizationName("SnimTest");
        QCoreApplication::setApplicationName("tst_recordingcontroller");
        QStandardPaths::setTestModeEnabled(true);   // isolate the recordings folder
    }

    void startThenStopLifecycle()
    {
        auto fake = std::make_unique<FakeRecordingStrategy>();
        FakeRecordingStrategy *f = fake.get();
        RecordingController ctrl(std::move(fake));

        QSignalSpy stateSpy(&ctrl, &RecordingController::recordingStateChanged);
        QSignalSpy finishedSpy(&ctrl, &RecordingController::recordingFinished);

        RecordTarget t;
        t.regionVirtual = QRect(0, 0, 200, 150);

        ctrl.startRecording(t);
        QVERIFY(f->startCalled);
        QVERIFY(f->lastPath.endsWith(".mp4"));       // output path built from Settings
        QVERIFY(!ctrl.isRecording());                // not until the backend signals started()

        f->emitStarted();
        QCOMPARE(stateSpy.count(), 1);
        QCOMPARE(stateSpy.at(0).at(0).toBool(), true);
        QVERIFY(ctrl.isRecording());

        ctrl.stop();
        QVERIFY(f->stopCalled);

        f->emitFinished("/tmp/out.mp4");
        QCOMPARE(finishedSpy.count(), 1);
        QCOMPARE(finishedSpy.at(0).at(0).toString(), QStringLiteral("/tmp/out.mp4"));
        QVERIFY(!ctrl.isRecording());
    }

    void ignoresInvalidTarget()
    {
        auto fake = std::make_unique<FakeRecordingStrategy>();
        FakeRecordingStrategy *f = fake.get();
        RecordingController ctrl(std::move(fake));

        ctrl.startRecording(RecordTarget{});         // empty/invalid -> no-op
        QVERIFY(!f->startCalled);
        QVERIFY(!ctrl.isRecording());
    }

    void failurePropagates()
    {
        auto fake = std::make_unique<FakeRecordingStrategy>();
        FakeRecordingStrategy *f = fake.get();
        RecordingController ctrl(std::move(fake));

        QSignalSpy failedSpy(&ctrl, &RecordingController::recordingFailed);
        RecordTarget t;
        t.regionVirtual = QRect(0, 0, 50, 50);
        ctrl.startRecording(t);
        f->emitFailed("boom");
        QCOMPARE(failedSpy.count(), 1);
        QVERIFY(!ctrl.isRecording());
    }

    void failureReportsTheFootageThatSurvived()
    {
        auto fake = std::make_unique<FakeRecordingStrategy>();
        FakeRecordingStrategy *f = fake.get();
        RecordingController ctrl(std::move(fake));

        QSignalSpy keptSpy(&ctrl, &RecordingController::partialRecordingKept);
        QSignalSpy failedSpy(&ctrl, &RecordingController::recordingFailed);
        RecordTarget t;
        t.regionVirtual = QRect(0, 0, 50, 50);

        // Nothing on disk: a plain failure.
        ctrl.startRecording(t);
        f->emitFailed("boom");
        QCOMPARE(keptSpy.count(), 0);
        QCOMPARE(failedSpy.count(), 1);

        ctrl.startRecording(t);
        QFile file(f->lastPath);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(QByteArray(4096, 'x'));
        file.close();
        f->emitFailed("boom");
        QCOMPARE(keptSpy.count(), 1);
        QCOMPARE(keptSpy.at(0).at(0).toString(), f->lastPath);
        QCOMPARE(failedSpy.count(), 2);
        QFile::remove(f->lastPath);
    }
};

QTEST_MAIN(tst_RecordingController)
#include "tst_recordingcontroller.moc"
