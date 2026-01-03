#include "recording/RecordingFrameOverlay.h"
#include "recording/RecordingGeometry.h"

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

    setGeometry(screenGeo);
    if (QWindow *wh = windowHandle())
        wh->setScreen(screen);
    show();
    update();
}

void RecordingFrameOverlay::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, false);

    // Dim everything around the recorded region; the region itself stays clear.
    for (const QRect &strip : surroundingRects(rect(), m_hole))
        p.fillRect(strip, QColor(0, 0, 0, kDimAlpha));

    // Accent border drawn just inside the hole edge (same style as the selector).
    p.setPen(QPen(kAccent, kBorderWidth));
    p.setBrush(Qt::NoBrush);
    p.drawRect(m_hole.adjusted(0, 0, -1, -1));
}

void RecordingFrameOverlay::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
#ifdef Q_OS_MACOS
    Capture::configureRecordingHud(this);   // float across Spaces, non-activating
#endif
}

} // namespace Recording
