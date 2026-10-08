#ifndef RECORDING_RECORDINGOPTIONSBAR_H
#define RECORDING_RECORDINGOPTIONSBAR_H

#include <QWidget>

class QMenu;
class QPushButton;
class QToolButton;

namespace Record {

/**
 * CleanShot-style floating options pill shown during recording selection: camera /
 * microphone (each an icon toggle plus a slim ▾ device menu), system audio, FPS,
 * capture scale, plus
 * ● Record and ✕ Cancel. Every control writes Core::Settings immediately, so the
 * choices both apply to the imminent recording and persist as the new defaults.
 *
 * Pure Qt, fully portable. It never takes keyboard focus (the selection overlay
 * keeps Enter/Esc). Off macOS it is created as a CHILD of the AreaSelector overlay,
 * so no compositor can stack the overlay above it and move() works on Wayland; on
 * macOS it stays a Tool window floated above the shielding-level overlay.
 * Dragging the pill background moves the bar in both modes.
 */
class RecordingOptionsBar : public QWidget
{
    Q_OBJECT

public:
    explicit RecordingOptionsBar(QWidget *parent = nullptr);

    void setRecordVisible(bool visible);   // hidden in window-pick (the click commits)
    void attachToOverlay(QWidget *overlay);

signals:
    void cameraToggled(bool enabled);      // controller shows/hides the bubble live
    void recordRequested();
    void cancelRequested();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    [[nodiscard]] QToolButton *makeToggle(const QString &iconPath, const QString &tip,
                                          bool checked);
    [[nodiscard]] QWidget *makeSplitGroup(QToolButton *toggle, QMenu *menu,
                                          const QString &arrowName, const QString &tip);
    void applyCameraEnabled(bool on);
    void applyMicEnabled(bool on);
    void rebuildCameraMenu();
    void rebuildMicMenu();
    void repositionInParent();

    QToolButton *m_cameraButton = nullptr;
    QToolButton *m_micButton = nullptr;
    QToolButton *m_audioButton = nullptr;
    QToolButton *m_fpsButton = nullptr;
    QToolButton *m_scaleButton = nullptr;
    QPushButton *m_recordButton = nullptr;
    QMenu *m_cameraMenu = nullptr;
    QMenu *m_micMenu = nullptr;
    QPoint m_dragOffset;
    bool m_dragging = false;
    bool m_userMoved = false;   // a dragged bar is never re-positioned by showEvent
};

} // namespace Record

#endif // RECORDING_RECORDINGOPTIONSBAR_H
