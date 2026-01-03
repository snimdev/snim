#include "NativeCaptureStrategy.h"
#include "../AreaSelector.h"
#include "../WindowEnumerator.h"
#ifdef Q_OS_MACOS
#include "../MacOverlay.h"
#endif
#include <QScreen>
#include <QApplication>
#include <QCursor>
#include <QDebug>
#include <QTimer>
#include <QWindow>
#include <QPainter>
#include <QGuiApplication>
#include <QClipboard>
#include <QFileDialog>
#include <QStandardPaths>
#include <algorithm>

namespace Capture {

NativeCaptureStrategy::NativeCaptureStrategy(QObject *parent)
    : CaptureStrategy(parent)
{
}

void NativeCaptureStrategy::captureFullScreen()
{
    QPixmap screenshot = captureScreen();
    if (!screenshot.isNull()) {
        emit screenshotReady(screenshot);
    } else {
        emit screenshotFailed("Failed to capture screen with native method");
    }
}

void NativeCaptureStrategy::captureArea()
{
    // Capture all screens
    m_fullScreenshot = captureAllScreens();
    if (m_fullScreenshot.isNull()) {
        emit screenshotFailed("Failed to capture screens for area selection");
        return;
    }

    // The frame is already grabbed above; present the overlay on the next event
    // loop tick (so the tray menu's dismissal finishes) with no perceptible delay.
    QTimer::singleShot(0, [this]() {
        showAreaSelector(m_fullScreenshot, m_virtualGeometry);
    });
}

void NativeCaptureStrategy::captureWindow()
{
    // Freeze all screens, then present an interactive window picker: hovering
    // highlights the window under the cursor and a click captures it. Falls back
    // to highlighting the screen under the cursor where window enumeration is
    // unavailable (see WindowEnumerator).
    m_fullScreenshot = captureAllScreens();
    if (m_fullScreenshot.isNull()) {
        emit screenshotFailed("Failed to capture screens for window selection");
        return;
    }

    const QVector<QRect> windows = enumerateWindows();
    QTimer::singleShot(0, [this, windows]() {
        showAreaSelector(m_fullScreenshot, m_virtualGeometry, /*windowPick=*/true, windows);
    });
}

bool NativeCaptureStrategy::isAvailable() const
{
    // Native Qt capture is always available
    return true;
}

QPixmap NativeCaptureStrategy::cropSelection(const QRect &area) const
{
    if (m_fullScreenshot.isNull() || area.isEmpty())
        return QPixmap();

    // area is in virtual-desktop logical coords; map to physical pixmap coords.
    const qreal dpr = m_fullScreenshot.devicePixelRatio();
    QRect physicalArea(
        (area.x() - m_virtualGeometry.x()) * dpr,
        (area.y() - m_virtualGeometry.y()) * dpr,
        area.width()  * dpr,
        area.height() * dpr);
    physicalArea = physicalArea.intersected(m_fullScreenshot.rect());

    qDebug() << "Selected area (logical):" << area
             << "Physical area:" << physicalArea;

    QPixmap finalScreenshot = m_fullScreenshot.copy(physicalArea);
    finalScreenshot.setDevicePixelRatio(dpr);
    return finalScreenshot;
}

void NativeCaptureStrategy::onAreaSelected(const QRect &area)
{
    if (area.isEmpty()) {
        // User cancelled (pressed Escape)
        qDebug() << "Area selection cancelled";
    } else {
        const QPixmap finalScreenshot = cropSelection(area);
        if (!finalScreenshot.isNull())
            emit screenshotReady(finalScreenshot);
        else
            emit screenshotFailed("Invalid area selected or no screenshot available");
    }

    // Clear the stored screenshot
    m_fullScreenshot = QPixmap();
    m_virtualGeometry = QRect();
}

void NativeCaptureStrategy::onCopyRequested(const QRect &area)
{
    const QPixmap cropped = cropSelection(area);
    if (!cropped.isNull()) {
        QGuiApplication::clipboard()->setPixmap(cropped);
        qDebug() << "Selection copied to clipboard:" << cropped.size();
    }
    m_fullScreenshot = QPixmap();
    m_virtualGeometry = QRect();
}

void NativeCaptureStrategy::onSaveRequested(const QRect &area)
{
    const QPixmap cropped = cropSelection(area);
    m_fullScreenshot = QPixmap();
    m_virtualGeometry = QRect();
    if (cropped.isNull())
        return;

    // The overlay is already torn down, so this dialog isn't hidden behind it.
    const QString fileName = QFileDialog::getSaveFileName(
        nullptr, "Save Screenshot",
        QStandardPaths::writableLocation(QStandardPaths::PicturesLocation) + "/screenshot.png",
        "Image Files (*.png *.jpg *.bmp)");
    if (!fileName.isEmpty())
        cropped.save(fileName);
}

QPixmap NativeCaptureStrategy::captureScreen()
{
    // Get the screen where the cursor is currently located
    QPoint cursorPos = QCursor::pos();
    QScreen *currentScreen = QApplication::screenAt(cursorPos);

    // Fall back to primary screen if cursor screen detection fails
    if (!currentScreen) {
        currentScreen = QApplication::primaryScreen();
    }

    if (currentScreen) {
        QPixmap screenshot = currentScreen->grabWindow(0);

        // If the screenshot is larger than the screen, crop it to the screen size
        QRect screenGeometry = currentScreen->geometry();
        if (screenshot.size() != screenGeometry.size()) {
            qDebug() << "Screenshot size" << screenshot.size()
                     << "doesn't match screen size" << screenGeometry.size();

            QPoint screenOffset = screenGeometry.topLeft();
            QRect cropRect(screenOffset, screenGeometry.size());
            screenshot = screenshot.copy(cropRect);
        }

        qDebug() << "Native capture - Screen:" << currentScreen->name()
                 << "Geometry:" << screenGeometry
                 << "Final screenshot size:" << screenshot.size();

        return screenshot;
    }

    return QPixmap();
}

QPixmap NativeCaptureStrategy::captureAllScreens()
{
    QList<QScreen*> screens = QGuiApplication::screens();
    if (screens.isEmpty()) {
        qDebug() << "No screens available";
        return QPixmap();
    }

    // Calculate virtual desktop geometry (union of all screen geometries)
    QRect virtualDesktop;
    for (QScreen *screen : screens) {
        virtualDesktop = virtualDesktop.united(screen->geometry());
    }

    m_virtualGeometry = virtualDesktop;

    qDebug() << "Capturing all screens - Virtual desktop:" << virtualDesktop;

    // Composite at the highest DPR among all screens so the sharpest display
    // isn't downscaled. Using a single DPR is fine because each screen is drawn
    // into an explicit logical destination rect below (DPI-correct regardless of
    // the individual screen's own DPR).
    qreal dpr = 1.0;
    for (QScreen *screen : screens) {
        dpr = std::max(dpr, screen->devicePixelRatio());
    }

    // Create a pixmap large enough to hold all screens
    QPixmap fullScreenshot(virtualDesktop.size() * dpr);
    fullScreenshot.setDevicePixelRatio(dpr);
    fullScreenshot.fill(Qt::black);

    // Paint each screen onto the full screenshot
    QPainter painter(&fullScreenshot);

    for (QScreen *screen : screens) {
        QRect screenGeometry = screen->geometry();
        QPixmap screenPixmap = screen->grabWindow(0);

        // Calculate position relative to virtual desktop
        QPoint offset = screenGeometry.topLeft() - virtualDesktop.topLeft();

        qDebug() << "  - Screen:" << screen->name()
                 << "Geometry:" << screenGeometry
                 << "Offset:" << offset
                 << "Screenshot size:" << screenPixmap.size();

        // Draw into the screen's LOGICAL destination rect, scaling its raw pixels
        // to fit. This stays correct on mixed-DPI setups (e.g. a 2x Retina screen
        // next to a 1x external display), where drawing the pixmap 1:1 at the
        // offset would misalign/mis-scale the secondary screen.
        const QRect destLogical(offset, screenGeometry.size());
        painter.drawPixmap(destLogical, screenPixmap, screenPixmap.rect());
    }

    painter.end();

    qDebug() << "Full screenshot size:" << fullScreenshot.size()
             << "DPR:" << fullScreenshot.devicePixelRatio();

    return fullScreenshot;
}

void NativeCaptureStrategy::showAreaSelector(const QPixmap &screenshot, const QRect &virtualGeometry,
                                             bool windowPick, const QVector<QRect> &windows)
{
    qDebug() << "Virtual desktop geometry:" << virtualGeometry;
    qDebug() << "Screenshot size:" << screenshot.size() << "DPR:" << screenshot.devicePixelRatio();
    qDebug() << "All screens:";

    QList<QScreen*> screens = QGuiApplication::screens();
    QList<Capture::AreaSelector*> *selectors = new QList<Capture::AreaSelector*>();

    // Create one AreaSelector widget per screen
    for (QScreen *screen : screens) {
        QRect screenGeometry = screen->geometry();
        qDebug() << "  - Creating selector for" << screen->name() << screenGeometry;

        auto *selector = new Capture::AreaSelector();
        selector->setScreenshot(screenshot);
        selector->setVirtualGeometry(virtualGeometry);
        selector->setScreenOffset(screenGeometry.topLeft());
        if (windowPick) {
            selector->setMode(Capture::AreaSelector::Mode::WindowPick);
            selector->setWindows(windows);
        }
        // Action toolbar only for normal area capture (not window-pick, not OCR).
        selector->setActionsEnabled(quickActionsEnabled() && !windowPick);

        // Present as a borderless overlay covering the screen. We deliberately
        // avoid showFullScreen()/Qt::WindowFullScreen: on macOS that triggers the
        // native fullscreen transition (Spaces zoom + menu-bar slide), which is
        // the distracting animation we want gone. A plain sized window appears
        // instantly; configureOverlayWindow() then raises it above the menu bar.
        selector->setGeometry(screenGeometry);
        selector->winId(); // ensure the native window exists before placing it
        if (QWindow *wh = selector->windowHandle())
            wh->setScreen(screen);
        selector->show();
        selector->raise();
        selector->activateWindow();
#ifdef Q_OS_MACOS
        configureOverlayWindow(selector);
#endif

        selectors->append(selector);
    }

    // Each terminal action tears down ALL per-screen selectors, then routes:
    //  - areaSelected -> crop & open editor (or empty = cancel)
    //  - copyRequested -> crop & copy to clipboard
    //  - saveRequested -> crop & save to file (after teardown, so no overlay covers the dialog)
    for (auto *selector : *selectors) {
        connect(selector, &Capture::AreaSelector::areaSelected,
                this, [this, selectors](const QRect &area) {
                    teardownSelectors(selectors);
                    onAreaSelected(area);
                });
        connect(selector, &Capture::AreaSelector::copyRequested,
                this, [this, selectors](const QRect &area) {
                    teardownSelectors(selectors);
                    onCopyRequested(area);
                });
        connect(selector, &Capture::AreaSelector::saveRequested,
                this, [this, selectors](const QRect &area) {
                    teardownSelectors(selectors);
                    onSaveRequested(area);
                });
        // Multi-monitor: mirror the live selection to every other overlay so a
        // selection spanning screens is drawn on all of them.
        connect(selector, &Capture::AreaSelector::liveStateChanged, this,
                [selectors, selector](const QRect &sel, int phase, int mode, const QPoint &cursor) {
                    for (auto *other : *selectors)
                        if (other != selector)
                            other->applyPeerState(sel, phase, mode, cursor);
                });
    }
}

void NativeCaptureStrategy::teardownSelectors(QList<AreaSelector*> *selectors)
{
    for (auto *sel : *selectors) {
        sel->blockSignals(true);  // prevent re-entry from other selectors
        sel->disconnect();
        sel->close();
        sel->deleteLater();
    }
    selectors->clear();
    delete selectors;
}

} // namespace Capture
