#include "ScreencopyFrameSource.h"
#include "screen/ScreencopyClient.h"

#include <QDebug>
#include <QElapsedTimer>

namespace Capture {

QPixmap ScreencopyFrameSource::grabNow(QRect *virtualGeometryOut, QString *error)
{
    const Screen::Screencopy::Globals globals = Screen::Screencopy::advertisedGlobals();
    const Screen::Screencopy::Protocol protocol = Screen::Screencopy::pickProtocol(globals.ext, globals.wlr);
    if (protocol == Screen::Screencopy::Protocol::None) {
        *error = QStringLiteral("the compositor offers no screencopy protocol");
        return {};
    }

    QElapsedTimer timer;
    timer.start();
    const QList<Screen::Screencopy::OutputFrame> frames = Screen::Screencopy::captureOutputs(protocol, error);
    if (frames.isEmpty())
        return {};

    QList<Screen::Screencopy::ScreenSlot> screens;
    for (QScreen *screen : QGuiApplication::screens())
        screens.append({screen->name(), screen->geometry()});
    const QRect virtualDesktop = qtVirtualDesktop();

    const QImage stitched = Screen::Screencopy::stitchFrames(frames, screens, virtualDesktop);
    if (stitched.isNull()) {
        *error = QStringLiteral("the screencopy frames match no screen");
        return {};
    }
    qDebug() << "Screencopy captured" << frames.size() << "outputs in" << timer.elapsed() << "ms:"
             << stitched.size() << "DPR" << stitched.devicePixelRatio();

    QPixmap frame = QPixmap::fromImage(stitched);
    frame.setDevicePixelRatio(stitched.devicePixelRatio());
    if (virtualGeometryOut)
        *virtualGeometryOut = virtualDesktop;
    return frame;
}

void ScreencopyFrameSource::grab()
{
    QRect virtualGeometry;
    QString error;
    const QPixmap frame = grabNow(&virtualGeometry, &error);
    if (frame.isNull())
        emit frameFailed(error, false);
    else
        emit frameReady(frame, virtualGeometry);
}

} // namespace Capture
