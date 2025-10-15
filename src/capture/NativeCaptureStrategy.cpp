#include "NativeCaptureStrategy.h"
#include "AreaSelector.h"
#include <QScreen>
#include <QApplication>
#include <QCursor>
#include <QDebug>
#include <QTimer>
#include <QWindow>
#include <QPainter>
#include <QGuiApplication>

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

    // Wait a moment for UI to settle, then show area selector
    QTimer::singleShot(100, [this]() {
        showAreaSelector(m_fullScreenshot, m_virtualGeometry);
    });
}

void NativeCaptureStrategy::captureWindow()
{
    // For now, use full screen capture (could be enhanced with window selection)
    captureFullScreen();
}

bool NativeCaptureStrategy::isAvailable() const
{
    // Native Qt capture is always available
    return true;
}

void NativeCaptureStrategy::onAreaSelected(const QRect &area)
{
    if (area.isEmpty()) {
        // User cancelled (pressed Escape)
        qDebug() << "Area selection cancelled";
        m_fullScreenshot = QPixmap();
        m_virtualGeometry = QRect();
        return;
    }

    if (!m_fullScreenshot.isNull()) {
        // The area is in virtual desktop logical coordinates
        qreal dpr = m_fullScreenshot.devicePixelRatio();

        // Map from virtual desktop logical coordinates to screenshot physical coordinates
        QRect physicalArea(
            (area.x() - m_virtualGeometry.x()) * dpr,
            (area.y() - m_virtualGeometry.y()) * dpr,
            area.width() * dpr,
            area.height() * dpr
        );

        // Ensure the crop rect is within bounds
        physicalArea = physicalArea.intersected(m_fullScreenshot.rect());

        qDebug() << "Selected area (logical):" << area;
        qDebug() << "Physical area (screenshot coords):" << physicalArea;

        QPixmap finalScreenshot = m_fullScreenshot.copy(physicalArea);
        finalScreenshot.setDevicePixelRatio(dpr);

        qDebug() << "Final screenshot size:" << finalScreenshot.size();

        emit screenshotReady(finalScreenshot);
    } else {
        emit screenshotFailed("Invalid area selected or no screenshot available");
    }

    // Clear the stored screenshot
    m_fullScreenshot = QPixmap();
    m_virtualGeometry = QRect();
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

    // Determine the device pixel ratio (use the primary screen's DPR)
    qreal dpr = QGuiApplication::primaryScreen()->devicePixelRatio();

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

        // Draw this screen's screenshot at its offset position
        painter.drawPixmap(offset, screenPixmap);
    }

    painter.end();

    qDebug() << "Full screenshot size:" << fullScreenshot.size()
             << "DPR:" << fullScreenshot.devicePixelRatio();

    return fullScreenshot;
}

void NativeCaptureStrategy::showAreaSelector(const QPixmap &screenshot, const QRect &virtualGeometry)
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

        // Position the selector on this specific screen
        selector->setGeometry(screenGeometry);
        selector->setWindowState(Qt::WindowFullScreen);
        selector->windowHandle()->setScreen(screen);
        selector->showFullScreen();

        selectors->append(selector);
    }

    // Connect all selectors to the same handler - use a shared pointer approach
    for (auto *selector : *selectors) {
        connect(selector, &Capture::AreaSelector::areaSelected,
                this, [this, selectors](const QRect &area) {
                    // Disconnect and close all selectors immediately to prevent multiple triggers
                    for (auto *sel : *selectors) {
                        sel->blockSignals(true);  // Block any further signals
                        sel->disconnect();         // Disconnect all signals
                        sel->close();              // Close immediately
                        sel->deleteLater();        // Schedule for deletion
                    }

                    // Clear the list and delete it
                    selectors->clear();
                    delete selectors;

                    // Handle the selected area
                    onAreaSelected(area);
                });
    }
}

} // namespace Capture
