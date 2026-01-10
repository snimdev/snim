#include "recording/RecordingOptionsBar.h"
#include "core/IconUtil.h"
#include "core/Settings.h"

#include <QActionGroup>
#include <QAudioDevice>
#include <QCameraDevice>
#include <QCursor>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QMediaDevices>
#include <QMenu>
#include <QMouseEvent>
#include <QPushButton>
#include <QScreen>
#include <QShowEvent>
#include <QToolButton>
#include <QWindow>

#ifdef Q_OS_MACOS
#include "capture/MacOverlay.h"
#endif

namespace Recording {

namespace {
const QColor kIconColor(235, 235, 240);
const QColor kIconDim(150, 150, 158);
} // namespace

RecordingOptionsBar::RecordingOptionsBar(QWidget *parent)
    : QWidget(parent)
{
    // Window-only flags: as a child of the overlay these would force it back into a
    // separate top-level, which is exactly the stacking bug they caused.
    if (isWindow()) {
        setWindowFlags(Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
        setAttribute(Qt::WA_TranslucentBackground);
        setAttribute(Qt::WA_ShowWithoutActivating);   // keyboard stays on the overlay
    }
    setFocusPolicy(Qt::NoFocus);

    auto *pill = new QWidget(this);
    pill->setObjectName("optionsPill");
    pill->setStyleSheet(QStringLiteral(R"(
        #optionsPill { background-color: rgba(28,28,30,0.94); border-radius: 21px; }
        QToolButton { color: white; border: none; border-radius: 8px; padding: 5px; }
        QToolButton:checked { background-color: rgba(0,150,255,0.45); }
        QToolButton:hover { background-color: rgba(255,255,255,0.16); }
        QToolButton::menu-button { border: none; width: 12px; }
        QLabel { color: rgba(255,255,255,0.35); }
        QPushButton#recordButton { color: white; background-color: #e0392b;
                                   border: none; border-radius: 14px;
                                   padding: 5px 16px; font-size: 13px; }
        QPushButton#recordButton:hover { background-color: #f0493b; }
        QPushButton#cancelButton { color: rgba(255,255,255,0.85); background: transparent;
                                   border: none; border-radius: 11px;
                                   padding: 4px 10px; font-size: 13px; }
        QPushButton#cancelButton:hover { background-color: rgba(255,255,255,0.16); }
    )"));

    auto *row = new QHBoxLayout(pill);
    row->setContentsMargins(12, 7, 10, 7);
    row->setSpacing(6);

    // Camera + mic: click toggles, the ▾ side opens the device menu. Menus are
    // rebuilt on open so plugging a device in mid-selection just works.
    m_cameraButton = makeToggle(QStringLiteral(":/icons/icons/camera.svg"),
                                tr("Record webcam bubble"), Core::Settings::cameraEnabled());
    m_cameraMenu = new QMenu(m_cameraButton);
    m_cameraButton->setMenu(m_cameraMenu);
    m_cameraButton->setPopupMode(QToolButton::MenuButtonPopup);
    connect(m_cameraMenu, &QMenu::aboutToShow, this, &RecordingOptionsBar::rebuildCameraMenu);
    connect(m_cameraButton, &QToolButton::toggled, this, [this](bool on) {
        Core::Settings::setCameraEnabled(on);
        emit cameraToggled(on);
    });
    row->addWidget(m_cameraButton);

    m_micButton = makeToggle(QStringLiteral(":/icons/icons/mic.svg"),
                             tr("Record microphone"), Core::Settings::micEnabled());
    m_micMenu = new QMenu(m_micButton);
    m_micButton->setMenu(m_micMenu);
    m_micButton->setPopupMode(QToolButton::MenuButtonPopup);
    connect(m_micMenu, &QMenu::aboutToShow, this, &RecordingOptionsBar::rebuildMicMenu);
    connect(m_micButton, &QToolButton::toggled, this,
            [](bool on) { Core::Settings::setMicEnabled(on); });
    row->addWidget(m_micButton);

    m_audioButton = makeToggle(QStringLiteral(":/icons/icons/speaker.svg"),
                               tr("Record system audio"), Core::Settings::systemAudioEnabled());
    connect(m_audioButton, &QToolButton::toggled, this,
            [](bool on) { Core::Settings::setSystemAudioEnabled(on); });
    row->addWidget(m_audioButton);

    auto *sep = new QLabel(QStringLiteral("|"), pill);
    row->addWidget(sep);

    // FPS menu (30/60) — text button showing the current value.
    m_fpsButton = new QToolButton(pill);
    m_fpsButton->setFocusPolicy(Qt::NoFocus);
    m_fpsButton->setToolButtonStyle(Qt::ToolButtonTextOnly);
    m_fpsButton->setText(QStringLiteral("%1 fps").arg(Core::Settings::recordingFps()));
    m_fpsButton->setToolTip(tr("Frame rate"));
    auto *fpsMenu = new QMenu(m_fpsButton);
    auto *fpsGroup = new QActionGroup(fpsMenu);
    for (int fps : {30, 60}) {
        QAction *a = fpsMenu->addAction(QStringLiteral("%1 fps").arg(fps));
        a->setCheckable(true);
        a->setChecked(Core::Settings::recordingFps() == fps);
        fpsGroup->addAction(a);
        connect(a, &QAction::triggered, this, [this, fps] {
            Core::Settings::setRecordingFps(fps);
            m_fpsButton->setText(QStringLiteral("%1 fps").arg(fps));
        });
    }
    m_fpsButton->setMenu(fpsMenu);
    m_fpsButton->setPopupMode(QToolButton::InstantPopup);
    row->addWidget(m_fpsButton);

    // Capture scale: native (2x on Retina) vs logical pixels (1x).
    m_scaleButton = new QToolButton(pill);
    m_scaleButton->setFocusPolicy(Qt::NoFocus);
    m_scaleButton->setToolButtonStyle(Qt::ToolButtonTextOnly);
    m_scaleButton->setCheckable(true);
    m_scaleButton->setChecked(Core::Settings::recordingRetina());
    m_scaleButton->setText(Core::Settings::recordingRetina() ? QStringLiteral("2×")
                                                             : QStringLiteral("1×"));
    m_scaleButton->setToolTip(tr("Capture resolution: 2× = native (Retina), 1× = standard"));
    connect(m_scaleButton, &QToolButton::toggled, this, [this](bool retina) {
        Core::Settings::setRecordingRetina(retina);
        m_scaleButton->setText(retina ? QStringLiteral("2×") : QStringLiteral("1×"));
    });
    row->addWidget(m_scaleButton);

    row->addSpacing(4);

    m_recordButton = new QPushButton(tr("● Record"), pill);
    m_recordButton->setObjectName("recordButton");
    m_recordButton->setFocusPolicy(Qt::NoFocus);
    m_recordButton->setToolTip(tr("Start recording the selected area (Enter)"));
    connect(m_recordButton, &QPushButton::clicked, this, &RecordingOptionsBar::recordRequested);
    row->addWidget(m_recordButton);

    auto *cancelButton = new QPushButton(QStringLiteral("✕"), pill);
    cancelButton->setObjectName("cancelButton");
    cancelButton->setFocusPolicy(Qt::NoFocus);
    cancelButton->setToolTip(tr("Cancel (Esc)"));
    connect(cancelButton, &QPushButton::clicked, this, &RecordingOptionsBar::cancelRequested);
    row->addWidget(cancelButton);

    auto *outer = new QHBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->addWidget(pill);
    adjustSize();
}

QToolButton *RecordingOptionsBar::makeToggle(const QString &iconPath, const QString &tip,
                                             bool checked)
{
    auto *button = new QToolButton(this);
    button->setFocusPolicy(Qt::NoFocus);
    button->setCheckable(true);
    button->setChecked(checked);
    button->setToolTip(tip);
    button->setIcon(Core::themedSvgIcon(iconPath, kIconColor, 20));
    button->setIconSize(QSize(20, 20));
    return button;
}

void RecordingOptionsBar::setRecordVisible(bool visible)
{
    m_recordButton->setVisible(visible);
    adjustSize();
}

void RecordingOptionsBar::rebuildCameraMenu()
{
    m_cameraMenu->clear();
    auto *group = new QActionGroup(m_cameraMenu);
    const QByteArray current = Core::Settings::cameraDeviceId();
    const QList<QCameraDevice> cams = QMediaDevices::videoInputs();
    for (const QCameraDevice &cam : cams) {
        QAction *a = m_cameraMenu->addAction(cam.description());
        a->setCheckable(true);
        a->setChecked(cam.id() == current || (current.isEmpty() && cam == cams.first()));
        group->addAction(a);
        const QByteArray id = cam.id();
        connect(a, &QAction::triggered, this, [this, id] {
            Core::Settings::setCameraDeviceId(id);
            if (m_cameraButton->isChecked())
                emit cameraToggled(true);   // re-ensure the bubble on the new device
        });
    }
    if (cams.isEmpty())
        m_cameraMenu->addAction(tr("No camera found"))->setEnabled(false);
}

void RecordingOptionsBar::rebuildMicMenu()
{
    m_micMenu->clear();
    auto *group = new QActionGroup(m_micMenu);
    const QByteArray current = Core::Settings::micDeviceId();
    const QList<QAudioDevice> mics = QMediaDevices::audioInputs();
    for (const QAudioDevice &mic : mics) {
        QAction *a = m_micMenu->addAction(mic.description());
        a->setCheckable(true);
        a->setChecked(mic.id() == current || (current.isEmpty() && mic.isDefault()));
        group->addAction(a);
        const QByteArray id = mic.id();
        connect(a, &QAction::triggered, this,
                [id] { Core::Settings::setMicDeviceId(id); });
    }
    if (mics.isEmpty())
        m_micMenu->addAction(tr("No microphone found"))->setEnabled(false);
}

void RecordingOptionsBar::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    adjustSize();

    if (isWindow()) {
        // Bottom-center of the screen under the cursor (where selection started).
        QScreen *screen = QGuiApplication::screenAt(QCursor::pos());
        if (!screen)
            screen = QGuiApplication::primaryScreen();
        if (screen && !m_userMoved) {
            const QRect avail = screen->availableGeometry();
            move(avail.center().x() - width() / 2, avail.bottom() - height() - 24);
        }
#ifdef Q_OS_MACOS
        Capture::configureSelectionHud(this);   // above the shielding-level overlay
#endif
        return;
    }

    // Child of the overlay: bottom-center of the parent.
    QWidget *parent = parentWidget();
    if (parent && !m_userMoved)
        move((parent->width() - width()) / 2, parent->height() - height() - 24);
}

void RecordingOptionsBar::mousePressEvent(QMouseEvent *event)
{
    // Child buttons consume their own presses, so anything reaching us is pill
    // background: start a move.
    if (event->button() != Qt::LeftButton) {
        QWidget::mousePressEvent(event);
        return;
    }
    if (isWindow()) {
        if (QWindow *handle = windowHandle())
            handle->startSystemMove();   // the only move that works on Wayland
        event->accept();
        return;
    }
    m_dragging = true;
    m_dragOffset = mapFromGlobal(event->globalPosition().toPoint());
    event->accept();
}

void RecordingOptionsBar::mouseMoveEvent(QMouseEvent *event)
{
    QWidget *parent = parentWidget();
    if (!m_dragging || !parent) {
        QWidget::mouseMoveEvent(event);
        return;
    }
    QPoint pos = parent->mapFromGlobal(event->globalPosition().toPoint()) - m_dragOffset;
    pos.setX(qBound(0, pos.x(), qMax(0, parent->width() - width())));
    pos.setY(qBound(0, pos.y(), qMax(0, parent->height() - height())));
    move(pos);
    m_userMoved = true;
    event->accept();
}

void RecordingOptionsBar::mouseReleaseEvent(QMouseEvent *event)
{
    m_dragging = false;
    QWidget::mouseReleaseEvent(event);
}

} // namespace Recording
