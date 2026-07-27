#include "ScreencastCaptureStrategy.h"

#include <QDebug>

namespace Capture {

ScreencastCaptureStrategy::ScreencastCaptureStrategy(QObject *parent)
    : WaylandCaptureStrategy(parent)
    , m_source(new Screen::ScreencastFrameSource(this))
{
    connect(m_source, &Screen::ScreencastFrameSource::sourcePickerExpected,
            this, &ScreencastCaptureStrategy::sourcePickerExpected);
    connect(m_source, &Screen::DesktopFrameSource::frameReady, this,
            [this](const QPixmap &frame, const QRect &virtualGeometry) {
                m_busy = false;
                deliverFrame(frame, virtualGeometry, m_showSelector);
            });
    connect(m_source, &Screen::DesktopFrameSource::frameFailed, this,
            [this](const QString &reason, bool cancelled) {
                if (!cancelled) {
                    fallBack(reason);
                    return;
                }
                // The user dismissed the picker: asking again through the portal would nag.
                m_busy = false;
                qInfo() << "ScreenCast capture cancelled:" << reason;
                emit screenshotCancelled();
            });
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
    begin(true);
}

bool ScreencastCaptureStrategy::isAvailable() const
{
    return Screen::ScreencastFrameSource::isSupported();
}

void ScreencastCaptureStrategy::begin(bool showSelector)
{
    if (m_busy) {
        qDebug() << "ScreenCast capture already in progress";
        return;
    }
    m_showSelector = showSelector;
    if (Screen::ScreencastFrameSource::hasFailedThisRun()) {
        usePortal();
        return;
    }
    m_busy = true;
    m_source->grab();
}

void ScreencastCaptureStrategy::fallBack(const QString &reason)
{
    m_busy = false;
    qWarning() << "ScreenCast capture failed (" << reason << "), using the Screenshot portal";
    usePortal();
}

void ScreencastCaptureStrategy::usePortal()
{
    if (m_showSelector)
        WaylandCaptureStrategy::captureArea();
    else
        WaylandCaptureStrategy::captureFullScreen();
}

} // namespace Capture
