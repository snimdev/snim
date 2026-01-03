#include "recording/CameraBubble.h"

#include <QMediaCaptureSession>
#include <QCamera>
#include <QCameraDevice>
#include <QMediaDevices>
#include <QVideoSink>
#include <QVideoFrame>
#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QResizeEvent>
#include <QShowEvent>
#include <QRegion>
#include <QScreen>
#include <QGuiApplication>
#include <QPermissions>
#include <QDebug>

#ifdef Q_OS_MACOS
#include "capture/MacOverlay.h"
#endif

namespace Recording {

CameraBubble::CameraBubble(QWidget *parent)
    : QWidget(parent)
{
    setWindowFlags(Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_ShowWithoutActivating);
    setCursor(Qt::OpenHandCursor);
    resize(180, 180);

    m_session = new QMediaCaptureSession(this);
    m_sink = new QVideoSink(this);
    m_session->setVideoSink(m_sink);
    connect(m_sink, &QVideoSink::videoFrameChanged, this, [this](const QVideoFrame &frame) {
        m_frame = frame.toImage();
        update();
    });
}

CameraBubble::~CameraBubble()
{
    stopCamera();
}

void CameraBubble::setCameraDevice(const QByteArray &deviceId)
{
    QCameraDevice chosen;
    const QList<QCameraDevice> cams = QMediaDevices::videoInputs();
    for (const QCameraDevice &c : cams) {
        if (c.id() == deviceId) { chosen = c; break; }
    }
    if (chosen.isNull())
        chosen = QMediaDevices::defaultVideoInput();

    const bool wasActive = m_camera && m_camera->isActive();
    delete m_camera;
    m_camera = chosen.isNull() ? nullptr : new QCamera(chosen, this);
    if (m_camera) {
        qInfo() << "[CameraBubble] using camera:" << chosen.description();
        connect(m_camera, &QCamera::errorOccurred, this, [](QCamera::Error e, const QString &msg) {
            if (e != QCamera::NoError)
                qWarning() << "[CameraBubble] camera error:" << msg;
        });
    } else {
        qWarning() << "[CameraBubble] no camera device for id" << deviceId;
    }
    m_session->setCamera(m_camera);
    if (m_camera && wasActive)
        m_camera->start();
}

void CameraBubble::startCamera()
{
    // Qt 6 requires the camera permission to be requested explicitly via the QPermission
    // API; QCamera::start() alone does NOT prompt and just yields no frames. Request it,
    // then re-enter once the user responds.
    QCameraPermission cameraPermission;
    const Qt::PermissionStatus status = qApp->checkPermission(cameraPermission);
    qInfo() << "[CameraBubble] camera permission:"
            << (status == Qt::PermissionStatus::Granted     ? "Granted"
                : status == Qt::PermissionStatus::Denied    ? "Denied"
                                                            : "Undetermined");
    switch (status) {
    case Qt::PermissionStatus::Undetermined:
        qApp->requestPermission(cameraPermission, this, [this](const QPermission &permission) {
            qInfo() << "[CameraBubble] permission answered:"
                    << (permission.status() == Qt::PermissionStatus::Granted ? "Granted" : "Denied");
            if (permission.status() == Qt::PermissionStatus::Granted) {
                startCamera();
            } else {
                m_statusText = tr("No camera access");
                update();
            }
        });
        return;
    case Qt::PermissionStatus::Denied:
        qWarning() << "[CameraBubble] camera access denied — enable it in "
                      "System Settings > Privacy & Security > Camera";
        m_statusText = tr("No camera access");
        update();
        return;
    case Qt::PermissionStatus::Granted:
        break;
    }

    if (!m_camera) {
        const QCameraDevice def = QMediaDevices::defaultVideoInput();
        if (!def.isNull()) {
            m_camera = new QCamera(def, this);
            connect(m_camera, &QCamera::errorOccurred, this, [](QCamera::Error e, const QString &msg) {
                if (e != QCamera::NoError)
                    qWarning() << "[CameraBubble] camera error:" << msg;
            });
            m_session->setCamera(m_camera);
        }
    }
    if (m_camera) {
        m_camera->start();
        qInfo() << "[CameraBubble] start() called; active =" << m_camera->isActive();
        m_statusText.clear();
    } else {
        qWarning() << "[CameraBubble] no camera available to start";
        m_statusText = tr("No camera");
    }
    update();
}

void CameraBubble::stopCamera()
{
    if (m_camera)
        m_camera->stop();
}

void CameraBubble::moveToRegionCorner(const QRect &regionVirtual)
{
    if (m_userMoved || regionVirtual.isEmpty())
        return;
    constexpr int kMargin = 16;
    // Bottom-left inside the region; clamp so tiny regions still pin the bubble
    // to their corner instead of pushing it out the top/right.
    const int x = regionVirtual.left() + kMargin;
    const int y = qMax(regionVirtual.top(),
                       regionVirtual.bottom() - height() - kMargin);
    move(x, y);
    m_winStartPos = pos();   // showEvent must not snap it back to the screen corner
}

void CameraBubble::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const QRectF circle(0.5, 0.5, width() - 1.0, height() - 1.0);
    QPainterPath clip;
    clip.addEllipse(circle);
    p.fillPath(clip, QColor(20, 20, 24));   // backing while the first frame loads

    if (!m_frame.isNull()) {
        p.setClipPath(clip);
        // Mirror horizontally for a natural "selfie" view, then aspect-fill the circle.
        const QImage img = m_frame.flipped(Qt::Horizontal);
        const QImage scaled = img.scaled(size(), Qt::KeepAspectRatioByExpanding,
                                         Qt::SmoothTransformation);
        p.drawImage(QPoint((width() - scaled.width()) / 2, (height() - scaled.height()) / 2), scaled);
        p.setClipping(false);
    } else if (!m_statusText.isEmpty()) {
        // No feed and we know why (access denied / no device): say so in the circle
        // instead of sitting there silently black.
        QFont f = p.font();
        f.setPointSizeF(qMax(9.0, height() / 16.0));
        p.setFont(f);
        p.setPen(QColor(200, 200, 205));
        const QRectF textRect = circle.adjusted(width() / 8.0, 0, -width() / 8.0, 0);
        p.drawText(textRect, Qt::AlignCenter | Qt::TextWordWrap, m_statusText);
    }

    QPen ring(QColor(255, 255, 255, 230), 3);
    p.setPen(ring);
    p.setBrush(Qt::NoBrush);
    p.drawEllipse(circle.adjusted(1.5, 1.5, -1.5, -1.5));
}

void CameraBubble::mousePressEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton)
        return;
    m_dragStartGlobal = event->globalPosition().toPoint();
    m_winStartPos = pos();
    m_winStartSize = width();
    const QPoint local = event->position().toPoint();
    const bool onGrip = local.x() >= width() - kGrip && local.y() >= height() - kGrip;
    m_drag = onGrip ? DragMode::Resize : DragMode::Move;
    setCursor(onGrip ? Qt::SizeFDiagCursor : Qt::ClosedHandCursor);
}

void CameraBubble::mouseMoveEvent(QMouseEvent *event)
{
    if (m_drag == DragMode::None)
        return;
    const QPoint delta = event->globalPosition().toPoint() - m_dragStartGlobal;
    if (m_drag == DragMode::Move) {
        m_userMoved = true;   // the user picked a spot: stop auto-parking
        move(m_winStartPos + delta);
    } else {
        const int d = qMax(m_winStartSize + qMax(delta.x(), delta.y()), kMinDiameter);
        resize(d, d);   // kept square; resizeEvent re-masks to a circle
    }
}

void CameraBubble::mouseReleaseEvent(QMouseEvent *)
{
    m_drag = DragMode::None;
    setCursor(Qt::OpenHandCursor);
}

void CameraBubble::resizeEvent(QResizeEvent *)
{
    // Mask the window to a circle so the corners are transparent + click-through.
    setMask(QRegion(0, 0, width(), height(), QRegion::Ellipse));
}

void CameraBubble::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);

    // Default placement: bottom-left of the screen under the cursor (Loom-style).
    if (m_winStartPos.isNull()) {
        QScreen *screen = QGuiApplication::screenAt(QCursor::pos());
        if (!screen)
            screen = QGuiApplication::primaryScreen();
        if (screen) {
            const QRect avail = screen->availableGeometry();
            move(avail.left() + 32, avail.bottom() - height() - 32);
        }
    }

#ifdef Q_OS_MACOS
    Capture::configureRecordingHud(this);   // float across Spaces, non-activating
#endif
}

} // namespace Recording
