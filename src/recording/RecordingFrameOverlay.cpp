#include "recording/RecordingFrameOverlay.h"
#include "recording/RecordingGeometry.h"
#include "recording/LayerShellSupport.h"

#include <QGuiApplication>
#include <QPainter>
#include <QScreen>
#include <QWindow>

#ifdef Q_OS_MACOS
#include "capture/MacOverlay.h"
#endif

namespace Recording {

namespace {
// Match the selection overlay's look (AreaSelector) so the frame reads as "this is
// the area you picked": same dim strength, same accent for the border.
constexpr int kDimAlpha = 120;
const QColor  kAccent(0, 150, 255);
constexpr int kBorderWidth = 2;
} // namespace

RecordingFrameOverlay::RecordingFrameOverlay(QWidget *parent)
    : QWidget(parent)
{
    // Click-through is essential: the user keeps working inside AND outside the
    // recorded region; this window must never swallow a click or take focus.
    setWindowFlags(Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint
                   | Qt::WindowTransparentForInput);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setAttribute(Qt::WA_ShowWithoutActivating);
}

void RecordingFrameOverlay::showForRegion(const QRect &regionVirtual)
{
    // The recorder captures one display: the one under the region's center (with a
    // first-intersecting / primary fallback). Mirror that choice here, and clamp the
    // hole to the screen exactly like the strategy clamps its sourceRect.
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

void RecordingFrameOverlay::applyLayerShell()
{
    // Zone -1 covers panels too: the surface must be the whole screen, or the
    // compositor shrinks it and m_hole (screen-local) no longer lands on the region.
    attachOverlayLayerSurface(windowHandle(), OverlayAnchorAll, -1, OverlayKeyboard::None);
}

void RecordingFrameOverlay::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, false);

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
#ifdef Q_OS_MACOS
    Capture::configureRecordingHud(this);   // float across Spaces, non-activating
#endif
}

} // namespace Recording
