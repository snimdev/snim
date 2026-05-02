#ifndef APP_RECORDINGWORKFLOW_H
#define APP_RECORDINGWORKFLOW_H

#include <QMessageBox>
#include <QObject>
#include <QString>
#include <functional>
#include <memory>

namespace Recording {
class RecordingController;
class RecordingControls;
} // namespace Recording

namespace App {

class CaptureWorkflow;
class Notifier;
class TrayMenu;
class UploadWorkflow;

/**
 * Mediator for the screen recording flow: owns the RecordingController and the Stop
 * controls, relabels the tray while a session runs, and opens each finished recording
 * in the video editor, whose uploads go to the UploadWorkflow and frame edits to the
 * CaptureWorkflow. Also offers what a crash or a failed recording left unsaved.
 */
class RecordingWorkflow : public QObject
{
    Q_OBJECT

public:
    enum class OfferChoice { Open, Discard, Later };
    using OfferPrompt = std::function<OfferChoice(const QString &path, QMessageBox::Icon icon,
                                                  const QString &title, const QString &text)>;

    RecordingWorkflow(TrayMenu &tray, CaptureWorkflow &capture, UploadWorkflow &upload,
                      Notifier &notifier, QObject *parent = nullptr);
    // Test seam: drive a controller built around a fake strategy.
    RecordingWorkflow(std::unique_ptr<Recording::RecordingController> controller, TrayMenu &tray,
                      CaptureWorkflow &capture, UploadWorkflow &upload, Notifier &notifier,
                      QObject *parent = nullptr);
    ~RecordingWorkflow() override;

    [[nodiscard]] bool isRecording() const;

    // Test seam: answers the "open, discard or later" question instead of a modal box.
    void setOfferPrompt(OfferPrompt prompt);

    // Stops an active recording and waits (bounded) for it to finalize. No editor opens:
    // the finished file stays journaled for the next start.
    void finishBeforeQuit();

public slots:
    void toggleAreaRecording();                              // Record Area / Stop (toggles)
    void startWindowRecording();                             // Record Window (disabled while recording)

    // Recordings a crash or a failure left unsaved: open each in the editor, or discard it.
    void offerUnsavedRecordings();

private slots:
    void onRecordingStateChanged(bool recording);
    void onRecordingFinished(const QString &path);
    void onRecordingFailed(const QString &error);

private:
    void offerRecording(const QString &path, QMessageBox::Icon icon, const QString &title,
                        const QString &text);
    static OfferChoice askAboutRecording(const QString &path, QMessageBox::Icon icon,
                                         const QString &title, const QString &text);

    TrayMenu &m_tray;
    CaptureWorkflow &m_capture;
    UploadWorkflow &m_upload;
    Notifier &m_notifier;
    std::unique_ptr<Recording::RecordingController> m_controller;
    Recording::RecordingControls *m_controls = nullptr;   // shown only while recording
    QString m_partialRecording;   // footage the failing recording kept, until recordingFailed
    OfferPrompt m_offerPrompt;
};

} // namespace App

#endif // APP_RECORDINGWORKFLOW_H
