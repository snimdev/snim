#ifndef RECORDING_RECORDINGOPTIONSBAR_H
#define RECORDING_RECORDINGOPTIONSBAR_H

#include <QWidget>

class QMenu;
class QPushButton;
class QToolButton;

namespace Recording {

/**
 * CleanShot-style floating options pill shown during recording selection: camera /
 * microphone (each with a device menu), system audio, FPS, capture scale, plus
 * ● Record and ✕ Cancel. Every control writes Core::Settings immediately, so the
 * choices both apply to the imminent recording and persist as the new defaults.
 *
 * Pure Qt, fully portable. It never takes keyboard focus (the selection overlay
 * keeps Enter/Esc); on macOS a window-level helper floats it above the overlay,
 * elsewhere WindowStaysOnTopHint + being shown after the overlay is enough.
 */
class RecordingOptionsBar : public QWidget
{
    Q_OBJECT

public:
    explicit RecordingOptionsBar(QWidget *parent = nullptr);

    void setRecordVisible(bool visible);   // hidden in window-pick (the click commits)

signals:
    void cameraToggled(bool enabled);      // controller shows/hides the bubble live
    void recordRequested();
    void cancelRequested();

protected:
    void showEvent(QShowEvent *event) override;

private:
    [[nodiscard]] QToolButton *makeToggle(const QString &iconPath, const QString &tip,
                                          bool checked);
    void rebuildCameraMenu();
    void rebuildMicMenu();

    QToolButton *m_cameraButton = nullptr;
    QToolButton *m_micButton = nullptr;
    QToolButton *m_audioButton = nullptr;
    QToolButton *m_fpsButton = nullptr;
    QToolButton *m_scaleButton = nullptr;
    QPushButton *m_recordButton = nullptr;
    QMenu *m_cameraMenu = nullptr;
    QMenu *m_micMenu = nullptr;
};

} // namespace Recording

#endif // RECORDING_RECORDINGOPTIONSBAR_H
