#include "app/RecordingWorkflow.h"
#include "app/CaptureWorkflow.h"
#include "app/Notifier.h"
#include "app/TrayMenu.h"
#include "app/UploadWorkflow.h"
#include "editor/video/VideoEditor.h"
#include "recording/RecordingController.h"
#include "recording/RecordingControls.h"
#include "recording/RecordingJournal.h"

#include <QAction>
#include <QEventLoop>
#include <QFileInfo>
#include <QLocale>
#include <QPushButton>
#include <QTimer>
#include <utility>

namespace App {

    // RecordingFactory picks the platform backend, so this flow is the same everywhere.
    RecordingWorkflow::RecordingWorkflow(TrayMenu &tray, CaptureWorkflow &capture,
                                         UploadWorkflow &upload, Notifier &notifier,
                                         QObject *parent)
        : RecordingWorkflow(std::make_unique<Recording::RecordingController>(), tray, capture,
                            upload, notifier, parent) {
    }

    RecordingWorkflow::RecordingWorkflow(std::unique_ptr<Recording::RecordingController> controller,
                                         TrayMenu &tray, CaptureWorkflow &capture,
                                         UploadWorkflow &upload, Notifier &notifier,
                                         QObject *parent)
        : QObject(parent)
          , m_tray(tray)
          , m_capture(capture)
          , m_upload(upload)
          , m_notifier(notifier)
          , m_controller(std::move(controller))
          , m_offerPrompt(&RecordingWorkflow::askAboutRecording) {
        connect(m_controller.get(), &Recording::RecordingController::recordingStateChanged,
                this, &RecordingWorkflow::onRecordingStateChanged);
        connect(m_controller.get(), &Recording::RecordingController::recordingFinished,
                this, &RecordingWorkflow::onRecordingFinished);
        connect(m_controller.get(), &Recording::RecordingController::partialRecordingKept,
                this, [this](const QString &path) { m_partialRecording = path; });
        connect(m_controller.get(), &Recording::RecordingController::recordingFailed,
                this, &RecordingWorkflow::onRecordingFailed);
        // Non-fatal setup problems (e.g. camera/mic access denied): a tray balloon,
        // not a modal box - the recording itself still proceeds.
        connect(m_controller.get(), &Recording::RecordingController::recordingWarning,
                this, [this](const QString &message) {
                    m_notifier.notify(tr("Snim"), message, QSystemTrayIcon::Warning);
                });

        if (!m_controller->isAvailable())
            m_tray.setRecordingUnavailable();
    }

    RecordingWorkflow::~RecordingWorkflow() = default;

    bool RecordingWorkflow::isRecording() const {
        return m_controller->isRecording();
    }

    void RecordingWorkflow::setOfferPrompt(OfferPrompt prompt) {
        m_offerPrompt = std::move(prompt);
    }

    void RecordingWorkflow::toggleAreaRecording() {
        if (m_controller->isRecording())
            m_controller->stop();
        else
            m_controller->recordArea();
    }

    void RecordingWorkflow::startWindowRecording() {
        m_controller->recordWindow();
    }

    void RecordingWorkflow::onRecordingStateChanged(bool recording) {
        m_tray.setRecordingActive(recording);

        if (recording) {
            if (!m_controls) {
                m_controls = new Recording::RecordingControls();
                connect(m_controls, &Recording::RecordingControls::stopRequested,
                        this, &RecordingWorkflow::toggleAreaRecording);
                connect(m_controls, &Recording::RecordingControls::pauseRequested,
                        m_controller.get(), &Recording::RecordingController::togglePause);
                connect(m_controller.get(), &Recording::RecordingController::recordingDuration,
                        m_controls, &Recording::RecordingControls::setElapsed);
                connect(m_controller.get(), &Recording::RecordingController::recordingPausedChanged,
                        m_controls, &Recording::RecordingControls::setPaused);
            }
            m_controls->setElapsed(0);
            m_controls->show();
            m_controls->raise();
        } else if (m_controls) {
            m_controls->hide();
            m_controls->deleteLater();
            m_controls = nullptr;
        }
    }

    void RecordingWorkflow::onRecordingFinished(const QString &tempPath) {
        // The recording was written to a temp file; open it in the trim editor,
        // which owns the file from here (save / copy / discard all clean it up).
        auto *editor = new Editor::Video::VideoEditor(tempPath);
        editor->setAttribute(Qt::WA_DeleteOnClose);
        connect(editor, &Editor::Video::VideoEditor::recordingSaved, this,
                [this](const QString &finalPath) {
                    m_notifier.notify(tr("Recording saved"), QFileInfo(finalPath).fileName(),
                                      QSystemTrayIcon::Information, 4000);
                });
        connect(editor, &Editor::Video::VideoEditor::uploadRequested,
                &m_upload, &UploadWorkflow::startUpload);
        connect(editor, &Editor::Video::VideoEditor::frameEditRequested, this,
                [this](const QPixmap &frame) { m_capture.openImageEditor(frame, {}); });
        editor->show();
        editor->raise();
        editor->activateWindow();
    }

    void RecordingWorkflow::onRecordingFailed(const QString &error) {
        const QString partial = std::exchange(m_partialRecording, QString());
        if (partial.isEmpty()) {
            QMessageBox::warning(nullptr, "Recording Failed", error);
            return;
        }
        offerRecording(partial, QMessageBox::Warning, tr("Recording Failed"),
                       tr("The recording stopped early: %1\n\nWhat was recorded until then was kept.")
                           .arg(error));
    }

    void RecordingWorkflow::offerUnsavedRecordings() {
        Recording::RecordingJournal::prune();
        const QStringList unsaved = Recording::RecordingJournal::recoverable();
        for (const QString &path : unsaved) {
            offerRecording(path, QMessageBox::Question, tr("Unsaved recording"),
                           tr("Snim closed before this recording was saved."));
        }
    }

    void RecordingWorkflow::offerRecording(const QString &path, QMessageBox::Icon icon,
                                           const QString &title, const QString &text) {
        switch (m_offerPrompt(path, icon, title, text)) {
        case OfferChoice::Open:
            onRecordingFinished(path);
            break;
        case OfferChoice::Discard:
            Recording::RecordingJournal::discard(path);
            break;
        case OfferChoice::Later:
            break;
        }
    }

    RecordingWorkflow::OfferChoice RecordingWorkflow::askAboutRecording(
            const QString &path, QMessageBox::Icon icon, const QString &title, const QString &text) {
        const QFileInfo info(path);
        QMessageBox box;
        box.setIcon(icon);
        box.setWindowTitle(title);
        box.setText(text);
        box.setInformativeText(tr("%1 (%2)").arg(info.fileName(),
                                                 QLocale().formattedDataSize(info.size())));
        QPushButton *open = box.addButton(tr("Open in editor"), QMessageBox::AcceptRole);
        QPushButton *discard = box.addButton(tr("Discard"), QMessageBox::DestructiveRole);
        // Later (or Esc) keeps it journaled, so the next start offers it again.
        box.addButton(tr("Later"), QMessageBox::RejectRole);
        box.setDefaultButton(open);
        box.exec();
        if (box.clickedButton() == open)
            return OfferChoice::Open;
        if (box.clickedButton() == discard)
            return OfferChoice::Discard;
        return OfferChoice::Later;
    }

    void RecordingWorkflow::finishBeforeQuit() {
        if (!m_controller->isActive())
            return;
        m_tray.quitAction()->setEnabled(false);
        Recording::RecordingController *controller = m_controller.get();
        // No editor while quitting: the finished file stays journaled for the next start.
        disconnect(controller, &Recording::RecordingController::recordingFinished,
                   this, &RecordingWorkflow::onRecordingFinished);
        disconnect(controller, &Recording::RecordingController::recordingFailed,
                   this, &RecordingWorkflow::onRecordingFailed);
        QEventLoop loop;
        bool done = false;   // stop() may already report, before the loop runs
        const auto finish = [&loop, &done] {
            done = true;
            loop.quit();
        };
        connect(controller, &Recording::RecordingController::recordingFinished, &loop, finish);
        connect(controller, &Recording::RecordingController::recordingFailed, &loop, finish);
        // Just past the Linux recorder's own 5 s EOS timeout.
        QTimer::singleShot(6000, &loop, &QEventLoop::quit);
        controller->stop();
        if (!done)
            loop.exec();
    }

} // namespace App
