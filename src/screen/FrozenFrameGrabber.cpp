#include "screen/FrozenFrameGrabber.h"
#include "screen/sources/DesktopFrameSource.h"
#include "screen/sources/FrameSourceChain.h"
#include "screen/sources/FrameSourceFactory.h"
#include "screen/sources/QtScreensFrameSource.h"
#include "screen/sources/StrategySelection.h"

#include <QDebug>

namespace Screen {

FrozenFrameGrabber::FrozenFrameGrabber(QObject *parent)
    : QObject(parent)
    , m_chain(new FrameSourceChain(QStringLiteral("Frozen frame"), &FrameSourceFactory::create, this))
{
    // The selector draws over the whole desktop, so a partial pick passes on to the next source.
    m_chain->setFrameCheck([](const QPixmap &frame, const QRect &virtualGeometry, SourceType type) {
        if (frameCoversGeometry(frame.size(), frame.devicePixelRatio(), virtualGeometry))
            return QString();
        return tr("%1 returned %2x%3 pixels at scale %4, not the whole %5x%6 desktop")
            .arg(FrameSourceFactory::typeName(type)).arg(frame.width()).arg(frame.height())
            .arg(frame.devicePixelRatio()).arg(virtualGeometry.width())
            .arg(virtualGeometry.height());
    });
    connect(m_chain, &FrameSourceChain::frameReady, this,
            [this](const QPixmap &frame, const QRect &virtualGeometry) {
        // A ScreenCast pick of fewer monitors still works; the rest stay black under the overlay.
        if (const QRect desktop = qtVirtualDesktop(); !virtualGeometry.contains(desktop))
            qInfo().noquote() << "Frozen frame: it misses part of the" << desktop << "desktop";
        finish(frame, virtualGeometry);
    });
    connect(m_chain, &FrameSourceChain::failed, this, &FrozenFrameGrabber::fail);
}

FrozenFrameGrabber::~FrozenFrameGrabber() = default;

void FrozenFrameGrabber::grab(Done done)
{
    if (!done)
        return;
    if (m_done)
        return;   // a grab is still in flight: ignore the re-entry
    m_lastError.clear();
    m_cancelled = false;

    if (isWaylandSession()) {
        m_done = std::move(done);
        m_chain->grab(StrategySelection::frameSourceChain(FrameSourceFactory::defaultType()));
        return;
    }

    // Synchronous outside a Wayland session (macOS, Windows, X11).
    QRect virtualGeometry;
    const QPixmap frozen = QtScreensFrameSource::grabNow(&virtualGeometry);
    if (frozen.isNull())
        m_lastError = failureMessage(QString(), QString(), false);
    done(frozen, virtualGeometry);
}

QString FrozenFrameGrabber::failureMessage(const QString &currentDesktop, const QString &reason,
                                           bool cancelled)
{
    using StrategySelection::desktopIs;
    if (cancelled)
        return tr("Snim needs a picture of your screens to show the recording selection, and "
                  "that request was cancelled. Start the recording again and allow it when "
                  "the system asks.");

    const QString lead = reason.isEmpty()
        ? tr("Could not capture the screen for selection.")
        : tr("Could not capture the screen for selection (%1).").arg(reason);
    if (desktopIs(currentDesktop, QStringLiteral("GNOME")))
        return lead + ' '
               + tr("GNOME has to ask you once which screens Snim may see: start the recording "
                    "again and choose your screens in the dialog that opens.");
    if (desktopIs(currentDesktop, QStringLiteral("KDE")))
        return lead + ' '
               + tr("Allow Snim to take screenshots when Plasma asks, or use \"Set up desktop "
                    "integration...\" in the tray menu, then try again.");
    if (!currentDesktop.isEmpty())
        return lead + ' '
               + tr("Check that xdg-desktop-portal and a portal backend for this desktop are "
                    "running, then try again.");
    return lead;
}

void FrozenFrameGrabber::finish(const QPixmap &frozen, const QRect &virtualGeometry)
{
    const Done done = std::move(m_done);
    m_done = nullptr;
    if (done)
        done(frozen, virtualGeometry);
}

void FrozenFrameGrabber::fail(const QString &reason, bool cancelled)
{
    m_lastError = failureMessage(qEnvironmentVariable("XDG_CURRENT_DESKTOP"), reason, cancelled);
    m_cancelled = cancelled;
    qWarning().noquote() << "Frozen frame: no frame for the selection:" << reason;
    finish({}, {});
}

} // namespace Screen
