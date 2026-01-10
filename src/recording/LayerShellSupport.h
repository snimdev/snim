#ifndef RECORDING_LAYERSHELLSUPPORT_H
#define RECORDING_LAYERSHELLSUPPORT_H

#include <QGuiApplication>
#include <QMargins>
#include <QSize>
#include <QWindow>

#ifdef NICESHOT_HAVE_LAYER_SHELL
#include <LayerShellQt/window.h>
#endif

namespace Recording {

/**
 * Wayland overlay-layer helper shared by the recording/selection overlays. Without
 * a layer surface these windows are ordinary toplevels: the compositor ignores
 * move(), shrinks them to the work area and lets other windows cover them.
 * Every entry point is a no-op unless built against LayerShellQt and running on
 * Wayland, so the X11/macOS paths keep their own positioning logic.
 */

enum OverlayAnchor {
    OverlayAnchorTop    = 0x1,
    OverlayAnchorBottom = 0x2,
    OverlayAnchorLeft   = 0x4,
    OverlayAnchorRight  = 0x8,
    OverlayAnchorAll    = OverlayAnchorTop | OverlayAnchorBottom
                          | OverlayAnchorLeft | OverlayAnchorRight,
};

enum class OverlayKeyboard {
    None,        // clicks still work, focus is never stolen
    Exclusive,   // grabs the keyboard while shown (Enter/Esc driven overlays)
};

/** True when attachOverlayLayerSurface() will actually do something. */
inline bool overlayLayerSurfacesAvailable()
{
#ifdef NICESHOT_HAVE_LAYER_SHELL
    return QGuiApplication::platformName() == QLatin1String("wayland");
#else
    return false;
#endif
}

/**
 * Turns @p w into an overlay-layer surface. Must be called after createWinId() and
 * before show(): the layer role has to be set before the surface is committed.
 * @p exclusiveZone is -1 for full-screen coverage (ignores panels), 0 otherwise.
 * An invalid @p desiredSize leaves the size to the anchors.
 */
inline void attachOverlayLayerSurface(QWindow *w, int anchors, int exclusiveZone,
                                      OverlayKeyboard keyboard,
                                      const QMargins &margins = QMargins(),
                                      const QSize &desiredSize = QSize())
{
#ifdef NICESHOT_HAVE_LAYER_SHELL
    if (!w || QGuiApplication::platformName() != QLatin1String("wayland"))
        return;

    // Idempotent: get() returns the object already attached to this QWindow.
    LayerShellQt::Window *ls = LayerShellQt::Window::get(w);
    if (!ls)
        return;

    LayerShellQt::Window::Anchors lsAnchors;
    if (anchors & OverlayAnchorTop)    lsAnchors |= LayerShellQt::Window::AnchorTop;
    if (anchors & OverlayAnchorBottom) lsAnchors |= LayerShellQt::Window::AnchorBottom;
    if (anchors & OverlayAnchorLeft)   lsAnchors |= LayerShellQt::Window::AnchorLeft;
    if (anchors & OverlayAnchorRight)  lsAnchors |= LayerShellQt::Window::AnchorRight;

    ls->setLayer(LayerShellQt::Window::LayerOverlay);
    ls->setAnchors(lsAnchors);
    ls->setExclusiveZone(exclusiveZone);
    ls->setKeyboardInteractivity(keyboard == OverlayKeyboard::Exclusive
                                     ? LayerShellQt::Window::KeyboardInteractivityExclusive
                                     : LayerShellQt::Window::KeyboardInteractivityNone);
    ls->setMargins(margins);
    if (desiredSize.isValid())
        ls->setDesiredSize(desiredSize);
    // The output is taken from QWindow::screen() by default, so callers pick the
    // screen with QWindow::setScreen() before getting here.
#else
    Q_UNUSED(w);
    Q_UNUSED(anchors);
    Q_UNUSED(exclusiveZone);
    Q_UNUSED(keyboard);
    Q_UNUSED(margins);
    Q_UNUSED(desiredSize);
#endif
}

} // namespace Recording

#endif // RECORDING_LAYERSHELLSUPPORT_H
