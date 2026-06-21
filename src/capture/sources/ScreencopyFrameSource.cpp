#include "ScreencopyFrameSource.h"
#include "capture/ScreencopyClient.h"

#include <QDebug>
#include <QElapsedTimer>

namespace Capture {

QPixmap ScreencopyFrameSource::grabNow(QRect *virtualGeometryOut, QString *error)
{
    const Screencopy::Globals globals = Screencopy::advertisedGlobals();
    const Screencopy::Protocol protocol = Screencopy::pickProtocol(globals.ext, globals.wlr);
    if (protocol == Screencopy::Protocol::None) {
        *error = QStringLiteral("the compositor offers no screencopy protocol");
        return {};
    }

    QElapsedTimer timer;
    timer.start();
    const QList<Screencopy::OutputFrame> frames = Screencopy::captureOutputs(protocol, error);
    if (frames.isEmpty())
        return {};

    QList<Screencopy::ScreenSlot> screens;
    for (QScreen *screen : QGuiApplication::screens())
        screens.append({screen->name(), screen->geometry()});
    const QRect virtualDesktop = qtVirtualDesktop();

    const QImage stitched = Screencopy::stitchFrames(frames, screens, virtualDesktop);
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
