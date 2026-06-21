#include "ScreencopyCaptureStrategy.h"
#include "capture/ScreencopyClient.h"
#include "capture/sources/ScreencopyFrameSource.h"

#include <QDebug>
#include <QTimer>

namespace Capture {

ScreencopyCaptureStrategy::ScreencopyCaptureStrategy(QObject *parent)
    : WaylandCaptureStrategy(parent)
{
}

bool ScreencopyCaptureStrategy::isScreencopyAvailable()
{
    const Screencopy::Globals globals = Screencopy::advertisedGlobals();
    return Screencopy::pickProtocol(globals.ext, globals.wlr) != Screencopy::Protocol::None;
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
    const QPixmap screenshot = ScreencopyFrameSource::grabNow(&virtualDesktop, &error);
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
