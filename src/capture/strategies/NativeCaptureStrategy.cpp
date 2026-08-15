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
    // The screen under the pointer, else the primary one.
    QScreen *screen = QApplication::screenAt(QCursor::pos());
    if (!screen)
        screen = QApplication::primaryScreen();
    if (!screen)
        return {};

    // Window 0 is exactly this screen at its own DPR, so no logical crop applies.
    const QPixmap screenshot = screen->grabWindow(0);
    qDebug() << "Native capture - Screen:" << screen->name() << "Geometry:" << screen->geometry()
             << "Final screenshot size:" << screenshot.size() << "DPR:" << screenshot.devicePixelRatio();
    return screenshot;
}

} // namespace Capture
