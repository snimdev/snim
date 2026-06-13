#include "ScreencopyCaptureStrategy.h"
#include "capture/ScreencopyClient.h"

#include <QDebug>
#include <QElapsedTimer>
#include <QGuiApplication>
#include <QScreen>
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
    const Screencopy::Globals globals = Screencopy::advertisedGlobals();
    const Screencopy::Protocol protocol = Screencopy::pickProtocol(globals.ext, globals.wlr);

    QElapsedTimer timer;
    timer.start();
    QString error;
    const QList<Screencopy::OutputFrame> frames = Screencopy::captureOutputs(protocol, &error);
    if (frames.isEmpty()) {
        qWarning() << "Screencopy failed:" << error << "- falling back to the portal";
        return false;
    }

    QList<Screencopy::ScreenSlot> screens;
    QRect virtualDesktop;
    for (QScreen *screen : QGuiApplication::screens()) {
        screens.append({screen->name(), screen->geometry()});
        virtualDesktop = virtualDesktop.united(screen->geometry());
    }

    const QImage stitched = Screencopy::stitchFrames(frames, screens, virtualDesktop);
    if (stitched.isNull()) {
        qWarning() << "Screencopy frames match no screen - falling back to the portal";
        return false;
    }
    qDebug() << "Screencopy captured" << frames.size() << "outputs in" << timer.elapsed() << "ms:"
             << stitched.size() << "DPR" << stitched.devicePixelRatio();

    QPixmap screenshot = QPixmap::fromImage(stitched);
    screenshot.setDevicePixelRatio(stitched.devicePixelRatio());

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
