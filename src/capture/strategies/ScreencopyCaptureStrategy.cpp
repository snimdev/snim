#include "ScreencopyCaptureStrategy.h"
#include "screen/ScreencopyClient.h"
#include "screen/sources/ScreencopyFrameSource.h"

#include <QDebug>
#include <QTimer>

namespace Capture {

ScreencopyCaptureStrategy::ScreencopyCaptureStrategy(QObject *parent)
    : WaylandCaptureStrategy(parent)
{
}

bool ScreencopyCaptureStrategy::isScreencopyAvailable()
{
    const Screen::Screencopy::Globals globals = Screen::Screencopy::advertisedGlobals();
    return Screen::Screencopy::pickProtocol(globals.ext, globals.wlr) != Screen::Screencopy::Protocol::None;
}

bool ScreencopyCaptureStrategy::isAvailable() const
{
    return isScreencopyAvailable();
}

void ScreencopyCaptureStrategy::captureFullScreen()
{
    if (!captureOutputs(false))
        WaylandCaptureStrategy::captureFullScreen();
}

void ScreencopyCaptureStrategy::captureArea()
{
    if (!captureOutputs(true))
        WaylandCaptureStrategy::captureArea();
}

bool ScreencopyCaptureStrategy::captureOutputs(bool showSelector)
{
    QRect virtualDesktop;
    QString error;
    const QPixmap screenshot = Screen::ScreencopyFrameSource::grabNow(&virtualDesktop, &error);
    if (screenshot.isNull()) {
        qWarning() << "Screencopy failed:" << error << "- falling back to the portal";
        return false;
    }

    if (!showSelector) {
        emit screenshotReady(screenshot);
        return true;
    }
    // The frame is already taken; the overlay waits a tick so a dismissed tray menu finishes.
    QTimer::singleShot(0, this, [this, screenshot, virtualDesktop]() {
        showAreaSelector(screenshot, virtualDesktop);
    });
    return true;
}

} // namespace Capture
