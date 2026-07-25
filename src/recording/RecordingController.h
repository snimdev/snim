#ifndef RECORDING_RECORDINGCONTROLLER_H
#define RECORDING_RECORDINGCONTROLLER_H

#include "recording/RecordTarget.h"
#include "screen/FrozenFrameGrabber.h"

#include <QObject>
#include <QString>
#include <functional>
#include <memory>

namespace Recording {

class RecordingStrategy;
class CameraBubble;
class RecordingFrameOverlay;

/**
 * Cross-platform orchestration for screen recording. Owns the platform
 * RecordingStrategy (picked by RecordingFactory) and drives the lifecycle:
 * startRecording(target) -> the backend records to a timestamped file in the
 * recordings folder -> stop() finalizes it -> recordingFinished(path).
 *
 * This class is deliberately UI-free and platform-free so it is unit-testable with
 * a fake strategy; the app layer (App::RecordingWorkflow) owns the tray/notification/Stop
 * widget and reacts to the signals here. Selection (showing the AreaSelector to
 * build a RecordTarget) is layered on top in a later phase.
 */
class RecordingController : public QObject
{
    Q_OBJECT

public:
    explicit RecordingController(QObject *parent = nullptr);
    // Test seam: inject a strategy (e.g. a fake) instead of the platform default.
    explicit RecordingController(std::unique_ptr<RecordingStrategy> strategy,
                                 QObject *parent = nullptr);
    ~RecordingController() override;

    // Show the AreaSelector to pick a region, then record it.
    void recordArea();

    // Show the AreaSelector in window-pick mode, then record the chosen window. A backend
    // whose system picker chooses windows (the ScreenCast portal) skips the selector.
    void recordWindow();

    // Record an already-resolved target (the shared core; also the test entry point).
    void startRecording(const RecordTarget &target);
    void stop();
    void togglePause();   // pause if recording, resume if paused

    [[nodiscard]] bool isRecording() const { return m_state == State::Recording; }
    // Starting or recording: a stop() now ends in recordingFinished, recordingFailed or
    // recordingCancelled.
    [[nodiscard]] bool isActive() const
    {
        return m_state == State::Starting || m_state == State::Recording;
    }
    [[nodiscard]] bool isAvailable() const;

signals:
    void recordingStateChanged(bool recording);   // drives tray text + the Stop widget
    void recordingFinished(const QString &path);   // success: file written (temp path)
    void recordingFailed(const QString &error);
    void recordingCancelled();                     // backed out before capture began: no error
    void partialRecordingKept(const QString &path);   // precedes recordingFailed when footage survived
    void recordingDuration(qint64 ms);             // elapsed, forwarded to the Stop widget
    void recordingPausedChanged(bool paused);      // pause/resume, for the controls
    void recordingWarning(const QString &message); // non-fatal (e.g. camera denied); recording proceeds

private:
    void wireStrategy();
    void presentSelection(bool windowPick);   // freeze screens + show per-screen AreaSelectors

    // If camera/mic permission is still Undetermined, fire the system prompt and
    // return true: `done` runs once the user answers (callers bail out and re-enter
    // through it). False = nothing to ask, proceed synchronously. Runs only while no
    // shielding overlay is up (the prompt would open underneath it).
    bool resolveInputPermissions(const std::function<void()> &done);

    // Post-teardown half of a selection: re-resolve permissions (bar toggles may
    // have enabled inputs mid-selection), refresh the camera bubble, park it in the
    // region corner, build the RecordTarget, and start the strategy.
    void beginRecordingForSelection(const QRect &area, quint64 windowId, bool isWindow);

    // Show the webcam overlay if enabled in Settings; a region parks it first, which
    // also picks the screen its Wayland layer surface binds to.
    void ensureCameraBubble(const QRect &regionVirtual = QRect());
    void destroyCameraBubble();
    void showFrameOverlay();                   // border + dim around the recorded region
    void destroyFrameOverlay();
    [[nodiscard]] quint64 cameraBubbleWindowId() const;
    [[nodiscard]] QString makeOutputPath() const;

    enum class State { Idle, Selecting, Starting, Recording };

    std::unique_ptr<RecordingStrategy> m_strategy;
    State m_state = State::Idle;
    QString m_outputPath;                     // file of the recording in progress
    CameraBubble *m_cameraBubble = nullptr;   // shown while a camera-enabled recording is set up/running
    RecordingFrameOverlay *m_frameOverlay = nullptr;   // recording frame, shown while recording a region
    QRect m_activeRegion;                     // region of the recording being started (virtual coords)
    // Owned by value: it can never outlive the controller, so its callback may
    // capture a plain `this`. Async on Wayland, synchronous everywhere else.
    Screen::FrozenFrameGrabber m_frameGrabber;

    // Permission requests fired for the current selection attempt. Guarantees the
    // pre-selection TCC gate asks each permission at most ONCE per attempt, even if
    // the answer leaves the status unchanged, so it can never request in a loop.
    bool m_askedCameraPermission = false;
    bool m_askedMicPermission = false;
};

} // namespace Recording

#endif // RECORDING_RECORDINGCONTROLLER_H
