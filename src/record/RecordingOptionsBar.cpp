#include "record/RecordingOptionsBar.h"
#include "core/IconUtil.h"
#include "core/Settings.h"

#include <QActionGroup>
#include <QAudioDevice>
#include <QCameraDevice>
#include <QCursor>
#include <QEvent>
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

#include "screen/OverlayWindows.h"

#include <functional>

namespace Record {

namespace {
const QColor kIconColor(235, 235, 240);
const QColor kIconDim(150, 150, 158);

// One device menu entry; preferred is the one checked while no device is stored.
struct DeviceChoice {
    QString description;
    QByteArray id;
    bool preferred = false;
};

// "Off" leads the exclusive list, so the menu is the reliable way to turn the device off
// even where the icon toggle is awkward to hit. Picking a device hands its id on.
void fillDeviceMenu(QMenu *menu, QObject *context, const QString &offText,
                    const QString &noneText, bool enabled, const QByteArray &current,
                    const QList<DeviceChoice> &devices, const std::function<void()> &turnOff,
                    const std::function<void(const QByteArray &)> &pick)
{
    menu->clear();
    auto *group = new QActionGroup(menu);

    QAction *off = menu->addAction(offText);
    off->setCheckable(true);
    off->setChecked(!enabled);
    group->addAction(off);
    QObject::connect(off, &QAction::triggered, context, turnOff);
    menu->addSeparator();

    for (const DeviceChoice &device : devices) {
        QAction *a = menu->addAction(device.description);
        a->setCheckable(true);
        a->setChecked(enabled
                      && (device.id == current || (current.isEmpty() && device.preferred)));
        group->addAction(a);
        QObject::connect(a, &QAction::triggered, context, [pick, id = device.id] { pick(id); });
    }
    if (devices.isEmpty())
        menu->addAction(noneText)->setEnabled(false);
}
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
    if (parent)
        parent->installEventFilter(this);

    auto *pill = new QWidget(this);
    pill->setObjectName("optionsPill");
    pill->setStyleSheet(QStringLiteral(R"(
        #optionsPill { background-color: rgba(28,28,30,0.94); border-radius: 21px; }
        QToolButton { color: white; border: none; border-radius: 8px; padding: 5px; }
        QToolButton:checked { background-color: rgba(0,150,255,0.45); }
        QToolButton:hover { background-color: rgba(255,255,255,0.16); }
        QToolButton#cameraButton, QToolButton#micButton {
            border-top-right-radius: 0; border-bottom-right-radius: 0; }
        QToolButton#cameraArrow, QToolButton#micArrow {
            border-top-left-radius: 0; border-bottom-left-radius: 0;
            color: rgba(255,255,255,0.75); padding: 5px 3px; font-size: 10px; }
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

    // Camera + mic: the icon toggles, a separate slim ▾ button opens the device menu.
    // A QToolButton::MenuButtonPopup split button is not usable here: QStyleSheetStyle
    // puts the menu subcontrol over the right third of the centered icon, so clicking
    // the glyph opens the menu instead of toggling. Menus are rebuilt on open so
    // plugging a device in mid-selection just works.
    m_cameraButton = makeToggle(QStringLiteral(":/icons/icons/camera.svg"),
                                tr("Record webcam bubble"), Core::Settings::cameraEnabled());
    m_cameraButton->setObjectName(QStringLiteral("cameraButton"));
    m_cameraMenu = new QMenu(m_cameraButton);
    connect(m_cameraMenu, &QMenu::aboutToShow, this, &RecordingOptionsBar::rebuildCameraMenu);
    connect(m_cameraButton, &QToolButton::toggled, this,
            [this](bool on) { applyCameraEnabled(on); });
    row->addWidget(makeSplitGroup(m_cameraButton, m_cameraMenu,
                                  QStringLiteral("cameraArrow"), tr("Choose a camera")));

    m_micButton = makeToggle(QStringLiteral(":/icons/icons/mic.svg"),
                             tr("Record microphone"), Core::Settings::micEnabled());
    m_micButton->setObjectName(QStringLiteral("micButton"));
    m_micMenu = new QMenu(m_micButton);
    connect(m_micMenu, &QMenu::aboutToShow, this, &RecordingOptionsBar::rebuildMicMenu);
    connect(m_micButton, &QToolButton::toggled, this,
            [this](bool on) { applyMicEnabled(on); });
    row->addWidget(makeSplitGroup(m_micButton, m_micMenu,
                                  QStringLiteral("micArrow"), tr("Choose a microphone")));

    m_audioButton = makeToggle(QStringLiteral(":/icons/icons/speaker.svg"),
                               tr("Record system audio"), Core::Settings::systemAudioEnabled());
    connect(m_audioButton, &QToolButton::toggled, this,
            [](bool on) { Core::Settings::setSystemAudioEnabled(on); });
    row->addWidget(m_audioButton);

    auto *sep = new QLabel(QStringLiteral("|"), pill);
    row->addWidget(sep);

    // FPS menu (30/60): text button showing the current value.
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

// The arrow deliberately carries no QMenu of its own: setMenu() would make the style
// paint a second indicator next to the ▾ glyph.
QWidget *RecordingOptionsBar::makeSplitGroup(QToolButton *toggle, QMenu *menu,
                                             const QString &arrowName, const QString &tip)
{
    auto *group = new QWidget(this);
    auto *arrow = new QToolButton(group);
    arrow->setObjectName(arrowName);
    arrow->setFocusPolicy(Qt::NoFocus);
    arrow->setToolButtonStyle(Qt::ToolButtonTextOnly);
    arrow->setText(QStringLiteral("▾"));
    arrow->setToolTip(tip);
    connect(arrow, &QToolButton::clicked, this, [arrow, menu] {
        menu->popup(arrow->mapToGlobal(QPoint(0, arrow->height())));
    });

    auto *box = new QHBoxLayout(group);
    box->setContentsMargins(0, 0, 0, 0);
    box->setSpacing(0);
    box->addWidget(toggle);
    box->addWidget(arrow);
    return group;
}

// Single funnel for every camera state change (button, menu), so Settings, the button
// and the signal never drift apart and cameraToggled fires exactly once.
void RecordingOptionsBar::applyCameraEnabled(bool on)
{
    Core::Settings::setCameraEnabled(on);
    if (m_cameraButton->isChecked() != on) {
        const QSignalBlocker block(m_cameraButton);
        m_cameraButton->setChecked(on);
    }
    emit cameraToggled(on);
}

void RecordingOptionsBar::applyMicEnabled(bool on)
{
    Core::Settings::setMicEnabled(on);
    if (m_micButton->isChecked() != on) {
        const QSignalBlocker block(m_micButton);
        m_micButton->setChecked(on);
    }
}

void RecordingOptionsBar::setRecordVisible(bool visible)
{
    m_recordButton->setVisible(visible);
    adjustSize();
}

void RecordingOptionsBar::rebuildCameraMenu()
{
    QList<DeviceChoice> devices;
    const QList<QCameraDevice> cams = QMediaDevices::videoInputs();
    for (const QCameraDevice &cam : cams)
        devices.append({cam.description(), cam.id(), cam == cams.first()});
    fillDeviceMenu(m_cameraMenu, this, tr("Camera off"), tr("No camera found"),
                   Core::Settings::cameraEnabled(), Core::Settings::cameraDeviceId(), devices,
                   [this] { applyCameraEnabled(false); },
                   [this](const QByteArray &id) {
                       Core::Settings::setCameraDeviceId(id);
                       applyCameraEnabled(true);   // re-ensures the bubble on the new device
                   });
}

void RecordingOptionsBar::rebuildMicMenu()
{
    QList<DeviceChoice> devices;
    const QList<QAudioDevice> mics = QMediaDevices::audioInputs();
    for (const QAudioDevice &mic : mics)
        devices.append({mic.description(), mic.id(), mic.isDefault()});
    fillDeviceMenu(m_micMenu, this, tr("Microphone off"), tr("No microphone found"),
                   Core::Settings::micEnabled(), Core::Settings::micDeviceId(), devices,
                   [this] { applyMicEnabled(false); },
                   [this](const QByteArray &id) {
                       Core::Settings::setMicDeviceId(id);
                       applyMicEnabled(true);
                   });
}

void RecordingOptionsBar::repositionInParent()
{
    QWidget *parent = parentWidget();
    if (!parent)
        return;
    move((parent->width() - width()) / 2, parent->height() - height() - 24);
}

// On Wayland the cursor screen is only known once the pointer enters an overlay, so the
// controller reparents the bar to the overlay under the mouse.
void RecordingOptionsBar::attachToOverlay(QWidget *overlay)
{
    if (!overlay || overlay == parentWidget())
        return;
    if (QWidget *old = parentWidget())
        old->removeEventFilter(this);
    setParent(overlay);
    overlay->installEventFilter(this);
    m_userMoved = false;   // a drag position on another screen is meaningless
    show();                // setParent() hides the widget
    repositionInParent();
}

bool RecordingOptionsBar::eventFilter(QObject *watched, QEvent *event)
{
    // The compositor can resize the overlay after mapping.
    if (watched == parentWidget() && event->type() == QEvent::Resize) {
        QWidget *parent = parentWidget();
        if (!m_userMoved) {
            repositionInParent();
        } else {
            QPoint p = pos();
            p.setX(qBound(0, p.x(), qMax(0, parent->width() - width())));
            p.setY(qBound(0, p.y(), qMax(0, parent->height() - height())));
            move(p);
        }
    }
    return QWidget::eventFilter(watched, event);
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
        Screen::configureSelectionHud(this);   // above the shielding-level overlay
        Screen::excludeFromCapture(this);
        return;
    }

    // Child of the overlay: bottom-center of the parent.
    if (!m_userMoved)
        repositionInParent();
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

} // namespace Record
