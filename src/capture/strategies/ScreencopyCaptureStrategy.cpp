#include "ScreencopyCaptureStrategy.h"
#include "screen/sources/ScreencopyFrameSource.h"

#include <QDebug>

namespace Capture {

ScreencopyCaptureStrategy::ScreencopyCaptureStrategy(QObject *parent)
    : WaylandCaptureStrategy(parent)
    , m_source(new Screen::ScreencopyFrameSource(this))
{
    connect(m_source, &Screen::DesktopFrameSource::frameReady, this,
            [this](const QPixmap &frame, const QRect &virtualGeometry) {
                m_busy = false;
                deliverFrame(frame, virtualGeometry, m_showSelector);
            });
    connect(m_source, &Screen::DesktopFrameSource::frameFailed, this, [this](const QString &reason) {
        m_busy = false;
        qWarning() << "Screencopy failed:" << reason << "- falling back to the portal";
        if (m_showSelector)
            WaylandCaptureStrategy::captureArea();
        else
            WaylandCaptureStrategy::captureFullScreen();
    });
}

bool ScreencopyCaptureStrategy::isAvailable() const
{
    return Screen::ScreencopyFrameSource::isAvailable();
}

void ScreencopyCaptureStrategy::captureFullScreen()
{
    begin(false);
}

void ScreencopyCaptureStrategy::captureArea()
{
    begin(true);
}

void ScreencopyCaptureStrategy::begin(bool showSelector)
{
    if (m_busy) {
        qDebug() << "Screencopy capture already in progress";
        return;
    }
    m_busy = true;
    m_showSelector = showSelector;
    m_source->grab();
}

} // namespace Capture
