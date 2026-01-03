#include "recording/RecordingControls.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScreen>
#include <QGuiApplication>
#include <QShowEvent>
#include <QTime>

#ifdef Q_OS_MACOS
#include "capture/MacOverlay.h"
#endif

namespace Recording {

RecordingControls::RecordingControls(QWidget *parent)
    : QWidget(parent)
{
    setWindowFlags(Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_ShowWithoutActivating);

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
    m_time->setMinimumWidth(44);

    m_pauseButton = new QPushButton(tr("Pause"), pill);
    m_pauseButton->setObjectName("pauseButton");
    connect(m_pauseButton, &QPushButton::clicked, this, &RecordingControls::pauseRequested);

    m_stopButton = new QPushButton(tr("Stop"), pill);
    m_stopButton->setObjectName("stopButton");
    connect(m_stopButton, &QPushButton::clicked, this, &RecordingControls::stopRequested);

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
    QScreen *screen = QGuiApplication::screenAt(QCursor::pos());
    if (!screen)
        screen = QGuiApplication::primaryScreen();
    if (screen) {
        const QRect avail = screen->availableGeometry();
        move(avail.center().x() - width() / 2, avail.top() + 12);
    }

#ifdef Q_OS_MACOS
    // Persistent, non-activating HUD: stays visible across Spaces and when the user
    // clicks other apps mid-recording, without stealing focus from them.
    Capture::configureRecordingHud(this);
#endif
}

} // namespace Recording
