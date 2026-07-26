#include "recording/RecordingFrameOverlay.h"
#include "recording/RecordingGeometry.h"
#include "screen/LayerShellSupport.h"

#include <QGuiApplication>
#include <QPainter>
#include <QRegion>
#include <QScreen>
#include <QWindow>

#include "screen/OverlayWindows.h"

namespace Recording {

namespace {
// Match the selection overlay's look (AreaSelector) so the frame reads as "this is
// the area you picked": same dim strength, same accent for the border.
constexpr int kDimAlpha = 120;
const QColor  kAccent(0, 150, 255);
constexpr int kBorderWidth = 2;
} // namespace

RecordingFrameOverlay::RecordingFrameOverlay(QWidget *parent)
    : QWidget(parent), m_x11(QGuiApplication::platformName() == QLatin1String("xcb"))
{
    // Click-through is essential: the user keeps working inside AND outside the
    // recorded region; this window must never swallow a click or take focus.
    Qt::WindowFlags flags = Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint
                            | Qt::WindowTransparentForInput;
    if (m_x11)   // unmanaged: a window manager frame would swallow every click
        flags |= Qt::X11BypassWindowManagerHint | Qt::WindowDoesNotAcceptFocus;
    setWindowFlags(flags);
    // ARGB on X11 too: marco drops a shadow into the hole of opaque unmanaged windows.
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setAttribute(Qt::WA_ShowWithoutActivating);
}

void RecordingFrameOverlay::showForRegion(const QRect &regionVirtual)
{
    // One display gets the frame, clamped to it: the one under the region's center (else
    // the first it touches, else the primary), which is all macOS records.
    QScreen *screen = nullptr;
    const QList<QScreen *> screens = QGuiApplication::screens();
    for (QScreen *cand : screens)
        if (cand->geometry().contains(regionVirtual.center())) { screen = cand; break; }
    if (!screen)
        for (QScreen *cand : screens)
            if (cand->geometry().intersects(regionVirtual)) { screen = cand; break; }
    if (!screen)
        screen = QGuiApplication::primaryScreen();
    if (!screen)
        return;

    const QRect screenGeo = screen->geometry();
    m_hole = displayLocalRect(regionVirtual, screenGeo)
                 .intersected(QRect(QPoint(0, 0), screenGeo.size()));
    if (m_hole.isEmpty())
        return;
    if (m_x11) {
        showRing(screen, regionVirtual);
        return;
    }

    // Before createWinId(): a layer surface binds its output when it is created.
    setScreen(screen);
    setGeometry(screenGeo);
    // The layer role has to be set before the surface is committed, so create the
    // window handle here and configure it while the widget is still hidden.
    createWinId();
    if (QWindow *wh = windowHandle())
        wh->setScreen(screen);
    applyLayerShell();
    show();
    update();
}

void RecordingFrameOverlay::showRing(QScreen *screen, const QRect &regionVirtual)
{
    // Border only: the dim needs a full-screen window, which xfwm4 frames to eat clicks.
    // It goes around the very pixels the X11 recorder reads, so it rounds as they do.
    QVector<X11Screen> screens;
    const QList<QScreen *> all = QGuiApplication::screens();
    for (const QScreen *each : all)
        screens.append(X11Screen{each->geometry(), each->devicePixelRatio()});
    const X11Screen here{screen->geometry(), screen->devicePixelRatio()};
    const QRect grabPx = x11Grab(regionVirtual, screens, true)
                             .rootPx.intersected(Screen::X11ScreenMap::nativeRect(here))
                             .translated(-here.geometry.topLeft());
    const QRect screenGeo = here.geometry;
    const FrameRing ring = x11FrameRing(grabPx, screenGeo.size(), here.dpr, kBorderWidth);
    if (!ring.valid)
        return;

    QRegion shape;
    for (const QRect &strip : ring.strips)
        shape += strip;
    setScreen(screen);
    setGeometry(ring.window.translated(screenGeo.topLeft()));
    createWinId();
    if (QWindow *wh = windowHandle())
        wh->setScreen(screen);
    setMask(shape);   // bounding shape: no pixel inside the region, input stays empty
    show();
    raise();   // WMs stack their frames below override-redirect windows mapped later
    update();
}

void RecordingFrameOverlay::applyLayerShell()
{
    // Zone -1 covers panels too: the surface must be the whole screen, or the
    // compositor shrinks it and m_hole (screen-local) no longer lands on the region.
    Screen::attachOverlayLayerSurface(windowHandle(), Screen::OverlayAnchorAll, -1,
                                      Screen::OverlayKeyboard::None);
}

void RecordingFrameOverlay::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, false);

    if (m_x11) {
        p.fillRect(rect(), kAccent);   // opaque: no compositor needed, the mask keeps the ring
        return;
    }

    // Dim everything around the recorded region; the region itself stays clear.
    for (const QRect &strip : surroundingRects(rect(), m_hole))
        p.fillRect(strip, QColor(0, 0, 0, kDimAlpha));

    p.setPen(QPen(kAccent, kBorderWidth));
    p.setBrush(Qt::NoBrush);
#ifdef Q_OS_MACOS
    // ScreenCaptureKit leaves our windows out of the capture, so the border can sit
    // just inside the hole edge (same style as the selector).
    p.drawRect(m_hole.adjusted(0, 0, -1, -1));
#else
    // The portal stream captures our own windows, so no pixel may be painted inside
    // the recorded region: this stroke covers exactly the kBorderWidth px outside it.
    constexpr int kOut = kBorderWidth / 2;
    p.drawRect(m_hole.adjusted(-kOut, -kOut, kOut, kOut));
#endif
}

void RecordingFrameOverlay::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    Screen::configureRecordingHud(this);   // float across Spaces, non-activating
    Screen::excludeFromCapture(this);
}

} // namespace Recording
