#include <QtTest>
#include <QSignalSpy>
#include <QStandardPaths>
#include <memory>

#include "recording/RecordingController.h"
#include "recording/RecordingJournal.h"
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
    WindowCapture windowCapture() const override { return capture; }

    void emitStarted() { m_running = true; emit started(); }
    void emitFinished(const QString &p) { m_running = false; emit finished(p); }
    void emitFailed(const QString &e) { m_running = false; emit failed(e); }
    void emitCancelled() { m_running = false; emit cancelled(); }

    RecordTarget lastTarget;
    QString lastPath;
    bool startCalled = false;
    bool stopCalled = false;
    bool m_running = false;
    WindowCapture capture = WindowCapture::WithOverlays;
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

    void cancelReturnsQuietlyToIdle()
    {
        auto fake = std::make_unique<FakeRecordingStrategy>();
        FakeRecordingStrategy *f = fake.get();
        RecordingController ctrl(std::move(fake));

        QSignalSpy failedSpy(&ctrl, &RecordingController::recordingFailed);
        QSignalSpy keptSpy(&ctrl, &RecordingController::partialRecordingKept);
        QSignalSpy cancelledSpy(&ctrl, &RecordingController::recordingCancelled);
        RecordTarget t;
        t.regionVirtual = QRect(0, 0, 50, 50);
        ctrl.startRecording(t);
        QVERIFY(ctrl.isActive());

        f->emitCancelled();
        QCOMPARE(cancelledSpy.count(), 1);
        QCOMPARE(failedSpy.count(), 0);
        QCOMPARE(keptSpy.count(), 0);
        QVERIFY(!ctrl.isActive());
        QVERIFY(!RecordingJournal::recoverable().contains(f->lastPath));

        // Idle again: the next attempt starts.
        f->startCalled = false;
        ctrl.startRecording(t);
        QVERIFY(f->startCalled);
    }

    void aSystemPickerChoosesTheWindowItself()
    {
        auto fake = std::make_unique<FakeRecordingStrategy>();
        FakeRecordingStrategy *f = fake.get();
        f->capture = RecordingStrategy::WindowCapture::SystemPicked;
        RecordingController ctrl(std::move(fake));

        ctrl.recordWindow();
        // Straight to the backend: no frozen frame, no window picker of Snim's own.
        QVERIFY(f->startCalled);
        QCOMPARE(f->lastTarget.kind, RecordTarget::Kind::Window);
        QVERIFY(f->lastTarget.systemPicker);
        QCOMPARE(f->lastTarget.windowId, quint64(0));
        QVERIFY(f->lastTarget.regionVirtual.isEmpty());
        QVERIFY(f->lastTarget.isValid());
        QVERIFY(ctrl.isActive());
        for (const QWidget *widget : QApplication::topLevelWidgets())
            QVERIFY(!widget->isVisible());

        // The stream is the window alone: no frame around a region, no camera bubble.
        f->emitStarted();
        for (const QWidget *widget : QApplication::topLevelWidgets())
            QVERIFY(!widget->isVisible());
        f->emitFinished(f->lastPath);
        QVERIFY(!ctrl.isActive());
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
