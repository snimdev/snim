#include <QtTest>
#include <QAction>
#include <QFile>
#include <QSettings>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <memory>

#include "app/CaptureWorkflow.h"
#include "app/Notifier.h"
#include "app/RecordingWorkflow.h"
#include "app/TrayMenu.h"
#include "app/UploadWorkflow.h"
#include "editor/video/VideoEditor.h"
#include "record/RecordingController.h"
#include "record/RecordingJournal.h"
#include "record/RecordingStrategy.h"
#include "record/RecordTarget.h"

using namespace App;
using Record::RecordingController;
using Record::RecordingJournal;
using Record::RecordTarget;

// A fake backend, as in tst_recordingcontroller: no screen grab, no encoder.
class FakeRecordingStrategy : public Record::RecordingStrategy
{
    Q_OBJECT
public:
    void start(const RecordTarget &, const QString &outputPath) override { lastPath = outputPath; }
    void stop() override
    {
        stopCalled = true;
        if (finishOnStop)
            emitFinished(lastPath);
        if (cancelOnStop)
            emitCancelled();
    }
    bool isRecording() const override { return m_running; }
    bool isAvailable() const override { return available; }
    QString name() const override { return QStringLiteral("Fake"); }

    void emitStarted() { m_running = true; emit started(); }
    void emitFinished(const QString &p) { m_running = false; emit finished(p); }
    void emitFailed(const QString &e) { m_running = false; emit failed(e); }
    void emitCancelled() { m_running = false; emit cancelled(); }

    QString lastPath;
    bool available = true;
    bool finishOnStop = false;
    bool cancelOnStop = false;
    bool stopCalled = false;
    bool m_running = false;
};

class FakeNotifier : public Notifier
{
public:
    void notify(const QString &title, const QString &, QSystemTrayIcon::MessageIcon, int) override
    {
        titles.append(title);
    }
    QStringList titles;
};

struct Offer {
    QString path;
    QMessageBox::Icon icon;
    QString title;
    QString text;
};

class tst_RecordingWorkflow : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        QCoreApplication::setOrganizationName("SnimTest");
        QCoreApplication::setApplicationName("tst_recordingworkflow");
        QStandardPaths::setTestModeEnabled(true);   // throwaway settings and recordings folder
        QVERIFY(m_dir.isValid());
    }

    void init()
    {
        QSettings().clear();
        m_tray = std::make_unique<TrayMenu>();
        m_upload = std::make_unique<UploadWorkflow>(m_notifier);
        m_capture = std::make_unique<CaptureWorkflow>(*m_tray, *m_upload);
        m_offers.clear();
    }

    void cleanup()
    {
        m_workflow.reset();
        m_capture.reset();
        m_upload.reset();
        m_tray.reset();
    }

    void unsavedRecordingsAreOfferedAtStartup()
    {
        const QString discarded = makeFile(QStringLiteral("discarded.mp4"));
        const QString kept = makeFile(QStringLiteral("kept.mp4"));
        RecordingJournal::add(discarded);
        RecordingJournal::add(kept);

        makeWorkflow();
        m_workflow->setOfferPrompt([this](const QString &path, QMessageBox::Icon icon,
                                          const QString &title, const QString &text) {
            m_offers.append({path, icon, title, text});
            return m_offers.size() == 1 ? RecordingWorkflow::OfferChoice::Discard
                                        : RecordingWorkflow::OfferChoice::Later;
        });

        m_workflow->offerUnsavedRecordings();

        QCOMPARE(m_offers.size(), 2);
        QCOMPARE(m_offers.at(0).path, discarded);
        QCOMPARE(m_offers.at(1).path, kept);
        QCOMPARE(m_offers.at(0).icon, QMessageBox::Question);
        QCOMPARE(m_offers.at(0).title, QStringLiteral("Unsaved recording"));
        QVERIFY(!QFile::exists(discarded));
        // Later keeps it journaled, so the next start offers it again.
        QCOMPARE(RecordingJournal::recoverable(), QStringList{kept});
        QVERIFY(!editorOpen());
    }

    void recordingStateRelabelsTheTray()
    {
        FakeRecordingStrategy *fake = makeWorkflow();
        m_workflow->setOfferPrompt(laterPrompt());

        m_controller->startRecording(regionTarget());
        fake->emitStarted();
        QVERIFY(m_workflow->isRecording());
        QCOMPARE(m_tray->recordAreaAction()->text(), QStringLiteral("Stop Recording"));
        QVERIFY(m_tray->recordAreaAction()->isEnabled());   // it is the Stop now
        QVERIFY(!m_tray->recordWindowAction()->isEnabled());

        // Footage on disk, so the failure below offers it instead of a modal warning.
        writeFootage(fake->lastPath);
        fake->emitFailed(QStringLiteral("boom"));

        QVERIFY(!m_workflow->isRecording());
        QCOMPARE(m_tray->recordAreaAction()->text(), QStringLiteral("Record Area"));
        QVERIFY(m_tray->recordWindowAction()->isEnabled());

        QCOMPARE(m_offers.size(), 1);
        QCOMPARE(m_offers.at(0).path, fake->lastPath);
        QCOMPARE(m_offers.at(0).icon, QMessageBox::Warning);
        QCOMPARE(m_offers.at(0).title, QStringLiteral("Recording Failed"));
        QVERIFY(m_offers.at(0).text.contains(QStringLiteral("boom")));
        QVERIFY(RecordingJournal::isRecoverable(fake->lastPath));
        RecordingJournal::discard(fake->lastPath);
    }

    void unavailableRecorderGreysOutTheTray()
    {
        auto fake = std::make_unique<FakeRecordingStrategy>();
        fake->available = false;
        m_controller = new RecordingController(std::move(fake));
        m_workflow = std::make_unique<RecordingWorkflow>(std::unique_ptr<RecordingController>(m_controller),
                                                         *m_tray, *m_capture, *m_upload, m_notifier);

        QVERIFY(!m_tray->recordAreaAction()->isEnabled());
        QVERIFY(!m_tray->recordWindowAction()->isEnabled());
        QCOMPARE(m_tray->recordAreaAction()->text(), QStringLiteral("Record Area (unavailable)"));
        QVERIFY(!m_tray->recordAreaAction()->toolTip().isEmpty());
    }

    void quitFinishesTheRecordingWithoutAnEditor()
    {
        FakeRecordingStrategy *fake = makeWorkflow();
        m_workflow->setOfferPrompt(laterPrompt());
        m_controller->startRecording(regionTarget());
        fake->emitStarted();
        writeFootage(fake->lastPath);
        fake->finishOnStop = true;   // reports during stop(), so no wait

        m_workflow->finishBeforeQuit();

        QVERIFY(fake->stopCalled);
        QVERIFY(!m_workflow->isRecording());
        QVERIFY(!m_tray->quitAction()->isEnabled());
        QVERIFY(!editorOpen());
        QVERIFY(m_offers.isEmpty());
        // The finished file stays journaled for the next start.
        QVERIFY(RecordingJournal::isRecoverable(fake->lastPath));
        RecordingJournal::discard(fake->lastPath);
    }

    void aCancelledPickerShowsNothing()
    {
        FakeRecordingStrategy *fake = makeWorkflow();
        m_workflow->setOfferPrompt(laterPrompt());
        m_controller->startRecording(regionTarget());

        // A failure here would block on a modal box; a cancel returns straight to idle.
        fake->emitCancelled();
        QVERIFY(!m_workflow->isRecording());
        QVERIFY(m_offers.isEmpty());
        QVERIFY(!editorOpen());
        QCOMPARE(m_tray->recordAreaAction()->text(), QStringLiteral("Record Area"));
        QVERIFY(!RecordingJournal::recoverable().contains(fake->lastPath));
    }

    void quitDuringStartDoesNotWaitOutTheTimeout()
    {
        FakeRecordingStrategy *fake = makeWorkflow();
        m_controller->startRecording(regionTarget());
        fake->cancelOnStop = true;   // stopped before the first frame

        QElapsedTimer clock;
        clock.start();
        m_workflow->finishBeforeQuit();
        QVERIFY(fake->stopCalled);
        QVERIFY(clock.elapsed() < 1000);
        QVERIFY(!m_controller->isActive());
    }

private:
    FakeRecordingStrategy *makeWorkflow()
    {
        auto fake = std::make_unique<FakeRecordingStrategy>();
        FakeRecordingStrategy *raw = fake.get();
        m_controller = new RecordingController(std::move(fake));
        m_workflow = std::make_unique<RecordingWorkflow>(std::unique_ptr<RecordingController>(m_controller),
                                                         *m_tray, *m_capture, *m_upload, m_notifier);
        return raw;
    }

    RecordingWorkflow::OfferPrompt laterPrompt()
    {
        return [this](const QString &path, QMessageBox::Icon icon, const QString &title,
                      const QString &text) {
            m_offers.append({path, icon, title, text});
            return RecordingWorkflow::OfferChoice::Later;
        };
    }

    static RecordTarget regionTarget()
    {
        RecordTarget target;
        target.regionVirtual = QRect(0, 0, 200, 150);
        return target;
    }

    static void writeFootage(const QString &path)
    {
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(QByteArray(64 * 1024, 'x'));
    }

    QString makeFile(const QString &name)
    {
        const QString path = m_dir.filePath(name);
        QFile file(path);
        if (file.open(QIODevice::WriteOnly))
            file.write(QByteArray(64 * 1024, 'x'));
        return path;
    }

    static bool editorOpen()
    {
        const auto widgets = QApplication::topLevelWidgets();
        return std::any_of(widgets.cbegin(), widgets.cend(), [](const QWidget *w) {
            return qobject_cast<const Editor::Video::VideoEditor *>(w) != nullptr;
        });
    }

    QTemporaryDir m_dir;
    FakeNotifier m_notifier;
    std::unique_ptr<TrayMenu> m_tray;
    std::unique_ptr<UploadWorkflow> m_upload;
    std::unique_ptr<CaptureWorkflow> m_capture;
    RecordingController *m_controller = nullptr;   // owned by m_workflow
    std::unique_ptr<RecordingWorkflow> m_workflow;
    QList<Offer> m_offers;
};

QTEST_MAIN(tst_RecordingWorkflow)
#include "tst_recordingworkflow.moc"
