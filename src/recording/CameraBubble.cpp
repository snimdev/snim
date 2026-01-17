#include "recording/CameraBubble.h"
#include "recording/LayerShellSupport.h"
#include "recording/RecordingGeometry.h"

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
#include <QWindow>
#include <QDebug>

#ifdef Q_OS_MACOS
#include "capture/MacOverlay.h"
#endif

namespace Recording {

namespace {
bool waylandSession()
{
    return QGuiApplication::platformName() == QLatin1String("wayland");
}
} // namespace

CameraBubble::CameraBubble(QWidget *parent)
    : QWidget(parent), m_layerMode(overlayLayerSurfacesAvailable())
{
    // The layer surface replaces the toplevel hints, and KWin withholds input from
    // layer surfaces carrying popup-like flags, so set these only off the layer path.
    if (!m_layerMode) {
        setWindowFlags(Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
        setAttribute(Qt::WA_ShowWithoutActivating);
    }
    setAttribute(Qt::WA_TranslucentBackground);
    setCursor(Qt::OpenHandCursor);
    m_circleRect = QRect(0, 0, kDefaultDiameter, kDefaultDiameter);
    resize(kDefaultDiameter, kDefaultDiameter);

    m_session = new QMediaCaptureSession(this);
    m_sink = new QVideoSink(this);
    m_session->setVideoSink(m_sink);
    connect(m_sink, &QVideoSink::videoFrameChanged, this, [this](const QVideoFrame &frame) {
        m_frame = frame.toImage();
        // Frame ticks repaint only the circle, not the whole fullscreen surface.
        update(m_circleRect.adjusted(-2, -2, 2, 2));
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
        qWarning() << "[CameraBubble] camera access denied, enable it in "
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
    QScreen *screen = QGuiApplication::screenAt(regionVirtual.center());
    if (!screen)
        screen = m_boundScreen ? m_boundScreen : QGuiApplication::primaryScreen();
    if (!screen)
        return;
    setScreenRelativePos(screen, bubbleParkPos(screen->geometry(), regionVirtual,
                                               m_circleRect.size(), kRegionMargin));
}

// The one place that knows how a position is applied: the circle's rect inside a static
// full-screen layer surface, move() on a plain toplevel.
void CameraBubble::setScreenRelativePos(QScreen *screen, const QPoint &posIn)
{
    if (!screen)
        return;
    QScreen *previous = m_boundScreen;
    m_boundScreen = screen;
    m_placed = true;

    if (!m_layerMode) {
        m_screenPos = posIn;
        move(screen->geometry().topLeft() + posIn);
        m_winStartPos = this->pos();
        return;
    }

    // The surface covers the whole output, so screen-local coords are widget-local ones
    // and the circle must stay inside them.
    const QRect oldCircle = m_circleRect;
    m_screenPos = clampBubbleTopLeft(QRect(QPoint(0, 0), screen->geometry().size()),
                                     posIn, m_circleRect.size());
    m_circleRect.moveTopLeft(m_screenPos);
    if (m_layerAttached && screen != previous) {
        // The output is bound when the surface is created, so a different screen means
        // a new surface: drop this one and map a fresh one in its place.
        const bool wasVisible = isVisible();
        hide();
        destroy();
        m_layerAttached = false;
        if (wasVisible)
            show();
        return;
    }
    updateCircleMask();
    // Repaint where the circle was too: the buffer keeps stale pixels otherwise.
    update(oldCircle.united(m_circleRect).adjusted(-2, -2, 2, 2));
}

void CameraBubble::updateCircleMask()
{
    // Only the circle takes input: everything else on the surface stays click-through.
    if (!m_layerMode) {
        setMask(QRegion(m_circleRect, QRegion::Ellipse));
        return;
    }
    // Widget-level masks also clip raster painting, which would make the stale
    // area around a moved circle unclearable; the window-level mask shapes the
    // Wayland input region only, leaving the whole surface paintable.
    clearMask();
    if (QWindow *handle = windowHandle())
        handle->setMask(QRegion(m_circleRect, QRegion::Ellipse));
}

void CameraBubble::ensurePlacement()
{
    if (m_placed)
        return;
    QScreen *screen = m_boundScreen;
    if (!screen)
        screen = QGuiApplication::screenAt(QCursor::pos());
    if (!screen)
        screen = QGuiApplication::primaryScreen();
    if (!screen)
        return;
    // Bottom-left of the screen's work area (Loom-style), in screen-local coords.
    const QRect avail = screen->availableGeometry().translated(-screen->geometry().topLeft());
    setScreenRelativePos(screen,
                         QPoint(avail.left() + 32, avail.bottom() - m_circleRect.height() - 32));
}

void CameraBubble::attachLayerSurface()
{
    ensurePlacement();
    QScreen *screen = m_boundScreen ? m_boundScreen : QGuiApplication::primaryScreen();
    if (!screen)
        return;
    setScreen(screen);   // a layer surface binds its output at creation
    setGeometry(screen->geometry());
    createWinId();
    QWindow *handle = windowHandle();
    if (!handle)
        return;
    handle->setScreen(screen);
    // Zone -1 so panels do not shift the surface and screen-local coords stay valid;
    // OnDemand because KWin withholds the pointer from keyboard-less layer surfaces.
    attachOverlayLayerSurface(handle, OverlayAnchorAll, /*exclusiveZone=*/-1,
                              OverlayKeyboard::OnDemand);
    m_layerAttached = true;
    m_circleRect.moveTopLeft(m_screenPos);
    updateCircleMask();
}

void CameraBubble::setVisible(bool visible)
{
    if (visible && m_layerMode && !m_layerAttached)
        attachLayerSurface();
    QWidget::setVisible(visible);
}

void CameraBubble::paintEvent(QPaintEvent *event)
{
    QPainter p(this);
    // Translucent windows do not auto-clear and Wayland ignores visual masks, so
    // stale circle pixels smear across the fullscreen surface without this.
    p.setCompositionMode(QPainter::CompositionMode_Clear);
    p.fillRect(event->rect(), Qt::transparent);
    p.setCompositionMode(QPainter::CompositionMode_SourceOver);
    p.setRenderHint(QPainter::Antialiasing);

    const int diameter = m_circleRect.width();
    const QRectF circle(m_circleRect.x() + 0.5, m_circleRect.y() + 0.5,
                        diameter - 1.0, m_circleRect.height() - 1.0);
    QPainterPath clip;
    clip.addEllipse(circle);
    p.fillPath(clip, QColor(20, 20, 24));   // backing while the first frame loads

    if (!m_frame.isNull()) {
        p.setClipPath(clip);
        // Mirror horizontally for a natural "selfie" view, then aspect-fill the circle.
        // mirrored(), not flipped(): the latter only exists since Qt 6.9 and 6.5 is the floor.
        const QImage img = m_frame.mirrored(true, false);
        const QImage scaled = img.scaled(m_circleRect.size(), Qt::KeepAspectRatioByExpanding,
                                         Qt::SmoothTransformation);
        p.drawImage(m_circleRect.topLeft()
                        + QPoint((diameter - scaled.width()) / 2,
                                 (m_circleRect.height() - scaled.height()) / 2),
                    scaled);
        p.setClipping(false);
    } else if (!m_statusText.isEmpty()) {
        // No feed and we know why (access denied / no device): say so in the circle
        // instead of sitting there silently black.
        QFont f = p.font();
        f.setPointSizeF(qMax(9.0, m_circleRect.height() / 16.0));
        p.setFont(f);
        p.setPen(QColor(200, 200, 205));
        const QRectF textRect = circle.adjusted(diameter / 8.0, 0, -diameter / 8.0, 0);
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
    const QPoint local = event->position().toPoint();
    if (!m_circleRect.contains(local))
        return;   // masked-out part of the full-screen surface: nothing to drag
    m_dragStartGlobal = event->globalPosition().toPoint();
    m_pressPos = local;
    m_pressOffset = m_pressPos - m_circleRect.topLeft();
    m_winStartPos = pos();
    m_winStartSize = m_circleRect.width();
    const bool onGrip = m_pressOffset.x() >= m_circleRect.width() - kGrip
                        && m_pressOffset.y() >= m_circleRect.height() - kGrip;
    m_drag = onGrip ? DragMode::Resize : DragMode::Move;
    setCursor(onGrip ? Qt::SizeFDiagCursor : Qt::ClosedHandCursor);

    // Wayland without layer shell: move() is ignored and global coordinates are
    // unreadable, so hand the move to the compositor and drop our own drag.
    if (!onGrip && !m_layerMode && waylandSession()) {
        m_drag = DragMode::None;
        setCursor(Qt::OpenHandCursor);
        m_userMoved = true;
        if (QWindow *handle = windowHandle())
            handle->startSystemMove();
    }
}

void CameraBubble::mouseMoveEvent(QMouseEvent *event)
{
    if (m_drag == DragMode::None)
        return;
    const QPoint local = event->position().toPoint();
    if (m_drag == DragMode::Move) {
        m_userMoved = true;   // the user picked a spot: stop auto-parking
        if (m_layerMode)
            setScreenRelativePos(m_boundScreen, local - m_pressOffset);
        else
            move(m_winStartPos + (event->globalPosition().toPoint() - m_dragStartGlobal));
        return;
    }
    // The static surface has no usable global coordinates; widget-local ones work
    // because its origin never moves.
    const QPoint delta = m_layerMode ? local - m_pressPos
                                     : event->globalPosition().toPoint() - m_dragStartGlobal;
    int d = qMax(m_winStartSize + qMax(delta.x(), delta.y()), kMinDiameter);
    if (!m_layerMode) {
        resize(d, d);   // kept square; resizeEvent re-masks to a circle
        return;
    }
    d = qMin(d, qMin(width(), height()));   // the surface is the screen: never grow past it
    m_circleRect.setSize(QSize(d, d));
    setScreenRelativePos(m_boundScreen, m_circleRect.topLeft());
}

void CameraBubble::mouseReleaseEvent(QMouseEvent *)
{
    m_drag = DragMode::None;
    setCursor(Qt::OpenHandCursor);
}

void CameraBubble::resizeEvent(QResizeEvent *)
{
    // Off the layer path the toplevel IS the circle; on it the widget grew to cover the
    // screen and the circle keeps the position the placement logic picked.
    if (m_layerMode)
        m_circleRect.moveTopLeft(m_screenPos);
    else
        m_circleRect = rect();
    updateCircleMask();
}

void CameraBubble::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);

    ensurePlacement();   // no-op once a region park or a drag has placed the bubble

#ifdef Q_OS_MACOS
    Capture::configureRecordingHud(this);   // float across Spaces, non-activating
#endif
}

} // namespace Recording
