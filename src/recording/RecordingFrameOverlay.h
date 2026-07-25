#ifndef RECORDING_RECORDINGFRAMEOVERLAY_H
#define RECORDING_RECORDINGFRAMEOVERLAY_H

#include <QRect>
#include <QWidget>

namespace Recording {

/**
 * The "recording frame": while a region recording runs, dims the recorded screen
 * outside the region (same black veil as the selection overlay) and draws a 2px
 * accent border around it, so it is always obvious what is being captured. The
 * window is fully click-through. macOS excludes the app's own windows from the
 * capture; the xdg-desktop-portal stream does not, so on every other platform
 * nothing is painted inside the region and the border sits just outside it.
 * On X11 it is only the border: an override-redirect window shaped to the ring, so no
 * window manager frames it (a frame would swallow clicks) and no compositor is needed.
 */
class RecordingFrameOverlay : public QWidget
{
    Q_OBJECT

public:
    explicit RecordingFrameOverlay(QWidget *parent = nullptr);

    // Cover the screen containing regionVirtual (virtual-desktop logical coords)
    // and frame the part of the region that screen actually records. X11 covers the
    // border only.
    void showForRegion(const QRect &regionVirtual);

protected:
    void paintEvent(QPaintEvent *event) override;
    void showEvent(QShowEvent *event) override;

private:
    // Wayland: turn the window into an always-on-top, full-screen layer surface.
    // No-op on X11 and where LayerShellQt is missing.
    void applyLayerShell();

    // X11: map just the shaped border ring around the region's grabbed pixels on screen.
    void showRing(QScreen *screen, const QRect &regionVirtual);

    QRect m_hole;   // recorded region in widget-local coordinates (screen-local on X11)
    const bool m_x11;
};

} // namespace Recording

#endif // RECORDING_RECORDINGFRAMEOVERLAY_H
