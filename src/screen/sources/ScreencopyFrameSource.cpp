#include "ScreencopyFrameSource.h"
#include "screen/ScreencopyClient.h"

#include <QDebug>
#include <QElapsedTimer>
#include <QFutureWatcher>
#include <QtConcurrentRun>

namespace Screen {

namespace {

struct Copy {
    QImage image;   // null on failure
    QString error;
};

// Blocking: the compositor's answer, or its timeout, comes back on this thread.
Copy copyOutputs(Screencopy::Protocol protocol, const QList<Screencopy::ScreenSlot> &screens,
                 const QRect &virtualDesktop)
{
    Copy copy;
    QElapsedTimer timer;
    timer.start();
    const QList<Screencopy::OutputFrame> frames = Screencopy::captureOutputs(protocol, &copy.error);
    if (frames.isEmpty())
        return copy;
    copy.image = Screencopy::stitchFrames(frames, screens, virtualDesktop);
    if (copy.image.isNull())
        copy.error = QStringLiteral("the screencopy frames match no screen");
    else
        qDebug() << "Screencopy captured" << frames.size() << "outputs in" << timer.elapsed()
                 << "ms:" << copy.image.size() << "DPR" << copy.image.devicePixelRatio();
    return copy;
}

} // namespace

bool ScreencopyFrameSource::isAvailable()
{
    const Screencopy::Globals globals = Screencopy::advertisedGlobals();
    return Screencopy::pickProtocol(globals.ext, globals.wlr) != Screencopy::Protocol::None;
}

void ScreencopyFrameSource::grab()
{
    if (m_busy) {
        qDebug() << "Screencopy grab already in progress";
        return;
    }
    const Screencopy::Globals globals = Screencopy::advertisedGlobals();
    const Screencopy::Protocol protocol = Screencopy::pickProtocol(globals.ext, globals.wlr);
    if (protocol == Screencopy::Protocol::None) {
        emit frameFailed(QStringLiteral("the compositor offers no screencopy protocol"), false);
        return;
    }

    // Qt's screens are read here, on the GUI thread, never from the worker.
    QList<Screencopy::ScreenSlot> screens;
    for (QScreen *screen : QGuiApplication::screens())
        screens.append({screen->name(), screen->geometry()});
    const QRect virtualDesktop = qtVirtualDesktop();

    m_busy = true;
    auto *watcher = new QFutureWatcher<Copy>(this);
    connect(watcher, &QFutureWatcher<Copy>::finished, this, [this, watcher, virtualDesktop] {
        const Copy copy = watcher->result();
        watcher->deleteLater();
        m_busy = false;
        if (copy.image.isNull()) {
            emit frameFailed(copy.error, false);
            return;
        }
        QPixmap frame = QPixmap::fromImage(copy.image);
        frame.setDevicePixelRatio(copy.image.devicePixelRatio());
        emit frameReady(frame, virtualDesktop);
    });
    watcher->setFuture(QtConcurrent::run(copyOutputs, protocol, screens, virtualDesktop));
}

} // namespace Screen
