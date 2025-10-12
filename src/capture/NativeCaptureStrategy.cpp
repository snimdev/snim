#include "NativeCaptureStrategy.h"
#include "AreaSelector.h"
#include <QScreen>
#include <QApplication>
#include <QCursor>
#include <QDebug>
#include <QTimer>

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
    // First capture the full screen
    m_fullScreenshot = captureScreen();
    if (m_fullScreenshot.isNull()) {
        emit screenshotFailed("Failed to capture screen for area selection");
        return;
    }

    // Wait a moment for UI to settle, then show area selector
    QTimer::singleShot(100, [this]() {
        auto *selector = new AreaSelector();
        selector->setAttribute(Qt::WA_DeleteOnClose);
        selector->setScreenshot(m_fullScreenshot);
        selector->showFullScreen();

        connect(selector, &AreaSelector::areaSelected,
                this, &NativeCaptureStrategy::onAreaSelected);
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
    if (!area.isNull() && !m_fullScreenshot.isNull()) {
        QPixmap croppedScreenshot = m_fullScreenshot.copy(area);
        emit screenshotReady(croppedScreenshot);
    } else {
        emit screenshotFailed("Invalid area selected or no screenshot available");
    }

    // Clear the stored screenshot
    m_fullScreenshot = QPixmap();
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

} // namespace Capture
