#include "record/RecordingControls.h"
#include "screen/LayerShellSupport.h"

#include <QFontMetrics>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScreen>
#include <QGuiApplication>
#include <QShowEvent>
#include <QTime>
#include <QWindow>

#include "screen/OverlayWindows.h"

namespace Record {

RecordingControls::RecordingControls(QWidget *parent)
    : QWidget(parent)
{
    // The layer surface replaces the toplevel hints, and KWin withholds input from
    // layer surfaces carrying popup-like flags, so set these only off the layer path.
    if (!Screen::overlayLayerSurfacesAvailable()) {
        setWindowFlags(Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
        setAttribute(Qt::WA_ShowWithoutActivating);
    }
    setAttribute(Qt::WA_TranslucentBackground);

    auto *pill = new QWidget(this);
    pill->setObjectName("recordingPill");
    pill->setStyleSheet(
        "#recordingPill { background-color: rgba(28,28,30,0.92); border-radius: 18px; }"
        "QLabel { color: white; font-size: 13px; }"
        "QPushButton { color: white; border: none; border-radius: 11px;"
        "              padding: 3px 12px; font-size: 12px; }"
        "QPushButton#stopButton { background-color: #e0392b; }"
        "QPushButton#stopButton:hover { background-color: #f0493b; }"
        "QPushButton#pauseButton { background-color: rgba(255,255,255,0.18); }"
        "QPushButton#pauseButton:hover { background-color: rgba(255,255,255,0.30); }");

    auto *dot = new QLabel(QStringLiteral("●"), pill);   // red bullet
    dot->setStyleSheet("color: #e0392b; font-size: 14px;");

    m_time = new QLabel(QStringLiteral("00:00"), pill);
    m_time->setAlignment(Qt::AlignCenter);

    m_pauseButton = new QPushButton(tr("Pause"), pill);
    m_pauseButton->setObjectName("pauseButton");
    connect(m_pauseButton, &QPushButton::clicked, this, &RecordingControls::pauseRequested);

    m_stopButton = new QPushButton(tr("Stop"), pill);
    m_stopButton->setObjectName("stopButton");
    connect(m_stopButton, &QPushButton::clicked, this, &RecordingControls::stopRequested);

    // A Wayland layer surface keeps the size we ask for at map time, so the pill must
    // never grow later: reserve the widest clock and the wider Pause/Resume label now.
    // ensurePolished() first, so the stylesheet fonts are the ones being measured.
    ensurePolished();
    m_time->setMinimumWidth(QFontMetrics(m_time->font())
                                .horizontalAdvance(QStringLiteral("00:00:00")) + 4);
    const int pauseWidth = m_pauseButton->sizeHint().width();
    m_pauseButton->setText(tr("Resume"));
    const int resumeWidth = m_pauseButton->sizeHint().width();
    m_pauseButton->setText(tr("Pause"));
    m_pauseButton->setMinimumWidth(qMax(pauseWidth, resumeWidth));

    auto *row = new QHBoxLayout(pill);
    row->setContentsMargins(12, 6, 8, 6);
    row->setSpacing(8);
    row->addWidget(dot);
    row->addWidget(m_time);
    row->addWidget(m_pauseButton);
    row->addWidget(m_stopButton);

    auto *outer = new QHBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->addWidget(pill);
    adjustSize();
}

void RecordingControls::setVisible(bool visible)
{
    if (visible && !m_layerSurface && Screen::overlayLayerSurfacesAvailable()) {
        // Anchored to the top edge only: the compositor then centers the surface
        // along that edge, which replaces the move() the Wayland session ignores.
        QScreen *screen = QGuiApplication::screenAt(QCursor::pos());
        if (!screen)
            screen = QGuiApplication::primaryScreen();
        if (screen)
            setScreen(screen);   // a layer surface binds its output at creation
        createWinId();
        if (QWindow *wh = windowHandle(); wh && screen)
            wh->setScreen(screen);
        Screen::attachOverlayLayerSurface(windowHandle(), Screen::OverlayAnchorTop,
                                          /*exclusiveZone=*/0, Screen::OverlayKeyboard::OnDemand,
                                          QMargins(0, 12, 0, 0), sizeHint());
        m_layerSurface = true;
    }
    QWidget::setVisible(visible);
}

void RecordingControls::setElapsed(qint64 ms)
{
    const QTime t = QTime(0, 0).addMSecs(static_cast<int>(ms));
    m_time->setText(t.hour() > 0 ? t.toString("hh:mm:ss") : t.toString("mm:ss"));
}

void RecordingControls::setPaused(bool paused)
{
    if (m_pauseButton)
        m_pauseButton->setText(paused ? tr("Resume") : tr("Pause"));
}

void RecordingControls::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);

    // Top-center of the screen under the cursor (falls back to the primary screen).
    // The layer surface places itself, and move() would be ignored there anyway.
    if (!m_layerSurface) {
        QScreen *screen = QGuiApplication::screenAt(QCursor::pos());
        if (!screen)
            screen = QGuiApplication::primaryScreen();
        if (screen) {
            const QRect avail = screen->availableGeometry();
            move(avail.center().x() - width() / 2, avail.top() + 12);
        }
    }

    // Persistent, non-activating HUD: stays visible across Spaces and when the user
    // clicks other apps mid-recording, without stealing focus from them.
    Screen::configureRecordingHud(this);
    Screen::excludeFromCapture(this);
}

} // namespace Record
