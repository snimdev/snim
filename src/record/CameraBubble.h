#ifndef RECORDING_CAMERABUBBLE_H
#define RECORDING_CAMERABUBBLE_H

#include <QWidget>
#include <QImage>
#include <QByteArray>

class QMediaCaptureSession;
class QCamera;
class QScreen;
class QVideoSink;

namespace Record {

/**
 * A frameless, always-on-top circular live-webcam overlay (Loom/CleanShot style).
 * Draggable to reposition and resizable from the bottom-right; the window is masked
 * to a circle so it composites cleanly and the corners are click-through. It is a
 * real on-screen window, so ScreenCaptureKit can record it (the recorder keeps it
 * in the capture via exceptingWindows while excluding the rest of the app).
 *
 * Positioning has two modes. On X11 and macOS it is a plain toplevel the size of the
 * circle, placed with move() in virtual-desktop coordinates. On Wayland with
 * LayerShellQt it is a static full-screen overlay layer surface bound to one output:
 * the surface never moves after mapping (margin updates take an async configure
 * round-trip, which made drags run away and flicker), and the circle is an inner rect
 * moved with plain widget-local math, with the window mask following it so everything
 * outside the circle stays click-through. Both modes go through
 * setScreenRelativePos(), so callers only ever deal in screen + local position.
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
    // coords), binding it to the screen holding the region's center. No-op once the
    // user has dragged it somewhere themselves.
    void moveToRegionCorner(const QRect &regionVirtual);

    void setVisible(bool visible) override;   // attaches the layer surface before mapping

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void showEvent(QShowEvent *event) override;

private:
    enum class DragMode { None, Move, Resize };

    void setScreenRelativePos(QScreen *screen, const QPoint &pos);
    void ensurePlacement();
    void attachLayerSurface();
    void updateCircleMask();

    QMediaCaptureSession *m_session = nullptr;
    QCamera *m_camera = nullptr;
    QVideoSink *m_sink = nullptr;
    QImage m_frame;
    QString m_statusText;   // drawn while there is no frame ("No camera access" etc.)

    DragMode m_drag = DragMode::None;
    QPoint m_dragStartGlobal;
    QPoint m_pressPos;            // press point in widget coords
    QPoint m_pressOffset;         // press point relative to the circle's top-left
    QPoint m_winStartPos;
    int    m_winStartSize = 0;
    bool   m_userMoved = false;   // user dragged it: stop auto-parking in the region

    const bool m_layerMode;       // Wayland layer surface instead of a plain toplevel
    bool    m_layerAttached = false;
    bool    m_placed = false;     // a position was chosen: showEvent must not override
    QScreen *m_boundScreen = nullptr;
    QPoint   m_screenPos;         // circle top-left within m_boundScreen
    // The circle in widget coords. Off the layer path it is the whole widget; on it the
    // widget covers the screen and only this rect moves.
    QRect    m_circleRect;

    static constexpr int kGrip = 22;          // bottom-right resize hot-zone (px)
    static constexpr int kMinDiameter = 96;
    static constexpr int kDefaultDiameter = 180;
    static constexpr int kRegionMargin = 16;  // inset from the recorded region's corner
};

} // namespace Record

#endif // RECORDING_CAMERABUBBLE_H
