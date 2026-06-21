#include "ScreencastCaptureStrategy.h"

#include <QDebug>

namespace Capture {

ScreencastCaptureStrategy::ScreencastCaptureStrategy(QObject *parent)
    : WaylandCaptureStrategy(parent)
    , m_source(new ScreencastFrameSource(this))
{
    connect(m_source, &ScreencastFrameSource::sourcePickerExpected,
            this, &ScreencastCaptureStrategy::sourcePickerExpected);
    connect(m_source, &DesktopFrameSource::frameReady, this,
            [this](const QPixmap &frame, const QRect &virtualGeometry) {
                m_busy = false;
                if (m_showSelector)
                    showAreaSelector(frame, virtualGeometry);
                else
                    emit screenshotReady(frame);
            });
    connect(m_source, &DesktopFrameSource::frameFailed, this,
            [this](const QString &reason, bool) { fallBack(reason); });
}

ScreencastCaptureStrategy::~ScreencastCaptureStrategy() = default;

void ScreencastCaptureStrategy::captureFullScreen()
{
    begin(false);
}

void ScreencastCaptureStrategy::captureArea()
{
    begin(true);
}

void ScreencastCaptureStrategy::captureWindow()
{
    begin(false);
}

bool ScreencastCaptureStrategy::isAvailable() const
{
    return isSupported();
}

void ScreencastCaptureStrategy::begin(bool showSelector)
{
    if (m_busy) {
        qDebug() << "ScreenCast capture already in progress";
        return;
    }
    m_busy = true;
    m_showSelector = showSelector;
    m_source->grab();
}

void ScreencastCaptureStrategy::fallBack(const QString &reason)
{
    m_busy = false;
    qWarning() << "ScreenCast capture failed (" << reason << "), using the Screenshot portal";
    if (m_showSelector)
        WaylandCaptureStrategy::captureArea();
    else
        WaylandCaptureStrategy::captureFullScreen();
}

} // namespace Capture
