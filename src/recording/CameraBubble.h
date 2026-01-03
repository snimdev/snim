#ifndef RECORDING_CAMERABUBBLE_H
#define RECORDING_CAMERABUBBLE_H

#include <QWidget>
#include <QImage>
#include <QByteArray>

class QMediaCaptureSession;
class QCamera;
class QVideoSink;

namespace Recording {

/**
 * A frameless, always-on-top circular live-webcam overlay (Loom/CleanShot style).
 * Draggable to reposition and resizable from the bottom-right; the window is masked
 * to a circle so it composites cleanly and the corners are click-through. It is a
 * real on-screen window, so ScreenCaptureKit can record it (the recorder keeps it
 * in the capture via exceptingWindows while excluding the rest of the app).
 */
class CameraBubble : public QWidget
{
    Q_OBJECT

public:
    explicit CameraBubble(QWidget *parent = nullptr);
    ~CameraBubble() override;

    void setCameraDevice(const QByteArray &deviceId);   // empty -> system default
    void startCamera();
    void stopCamera();

    // Park the bubble in the bottom-left of the recorded region (virtual-desktop
    // coords). No-op once the user has dragged it somewhere themselves.
    void moveToRegionCorner(const QRect &regionVirtual);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void showEvent(QShowEvent *event) override;

private:
    enum class DragMode { None, Move, Resize };

    QMediaCaptureSession *m_session = nullptr;
    QCamera *m_camera = nullptr;
    QVideoSink *m_sink = nullptr;
    QImage m_frame;
    QString m_statusText;   // drawn while there is no frame ("No camera access" etc.)

    DragMode m_drag = DragMode::None;
    QPoint m_dragStartGlobal;
    QPoint m_winStartPos;
    int    m_winStartSize = 0;
    bool   m_userMoved = false;   // user dragged it: stop auto-parking in the region

    static constexpr int kGrip = 22;          // bottom-right resize hot-zone (px)
    static constexpr int kMinDiameter = 96;
};

} // namespace Recording

#endif // RECORDING_CAMERABUBBLE_H
