#ifndef RECORDING_RECORDINGCONTROLS_H
#define RECORDING_RECORDINGCONTROLS_H

#include <QWidget>

class QLabel;
class QPushButton;

namespace Recording {

/**
 * Small frameless always-on-top control shown while recording: a red dot, the
 * elapsed time, and a Stop button. This is the dependable stop affordance for a
 * tray-only (LSUIElement) app, where the menu can be hard to reach mid-capture.
 * Owned by the app layer; shown on recording start, hidden on stop.
 */
class RecordingControls : public QWidget
{
    Q_OBJECT

public:
    explicit RecordingControls(QWidget *parent = nullptr);

public slots:
    void setElapsed(qint64 ms);
    void setPaused(bool paused);   // updates the Pause/Resume button label

signals:
    void stopRequested();
    void pauseRequested();         // toggle pause/resume

protected:
    void showEvent(QShowEvent *event) override;   // place top-center, float above Spaces (macOS)

private:
    QLabel *m_time = nullptr;
    QPushButton *m_pauseButton = nullptr;
    QPushButton *m_stopButton = nullptr;
};

} // namespace Recording

#endif // RECORDING_RECORDINGCONTROLS_H
