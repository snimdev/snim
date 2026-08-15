#include "NativeCaptureStrategy.h"
#include "screen/WindowEnumerator.h"
#include "screen/sources/QtScreensFrameSource.h"
#include <QApplication>
#include <QCursor>
#include <QDebug>
#include <QScreen>
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
    grabAndSelect(false);
}

void NativeCaptureStrategy::captureWindow()
{
    // Hover highlights the window under the cursor, a click takes it (see WindowEnumerator).
    grabAndSelect(true);
}

void NativeCaptureStrategy::grabAndSelect(bool windowPick)
{
    SelectorOptions options;
    QRect virtualGeometry;
#ifdef Q_OS_WIN
    // Mixed DPIs are common on Windows: a single-screen area crops its own screen's grab.
    const QPixmap frame = Screen::QtScreensFrameSource::grabNow(&virtualGeometry, &options.screenGrabs);
#else
    const QPixmap frame = Screen::QtScreensFrameSource::grabNow(&virtualGeometry);
#endif
    if (frame.isNull()) {
        emit screenshotFailed(windowPick ? "Failed to capture screens for window selection"
                                         : "Failed to capture screens for area selection");
        return;
    }
    if (windowPick) {
        options.windowPick = true;
        options.windows = Screen::enumerateWindowInfos();
    }

    // The frame is already grabbed; the next tick lets the tray menu's dismissal finish.
    QTimer::singleShot(0, this, [this, frame, virtualGeometry, options] {
        showAreaSelector(frame, virtualGeometry, options);
    });
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
