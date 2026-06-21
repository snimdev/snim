#include "screen/FrozenFrameGrabber.h"
#include "capture/StrategySelection.h"
#include "capture/sources/QtScreensFrameSource.h"

#include <QDebug>
#include <QGuiApplication>

#include <memory>

namespace Screen {

namespace {

#ifdef Q_OS_LINUX
using Type = Capture::CaptureFactory::StrategyType;

QString typeName(Type type)
{
    switch (type) {
    case Type::KWin: return QStringLiteral("KWin ScreenShot2");
    case Type::Screencast: return QStringLiteral("ScreenCast portal");
    case Type::Screencopy: return QStringLiteral("Wayland screencopy");
    case Type::Wayland: return QStringLiteral("Screenshot portal");
    case Type::Native: return QStringLiteral("Qt screen grab");
    case Type::Auto: break;
    }
    return QStringLiteral("automatic");
}
#endif

} // namespace

FrozenFrameGrabber::FrozenFrameGrabber(QObject *parent)
    : QObject(parent)
{
}

FrozenFrameGrabber::~FrozenFrameGrabber() = default;

void FrozenFrameGrabber::grab(Done done)
{
    if (!done)
        return;
    if (m_done)
        return;   // a grab is still in flight: ignore the re-entry
    m_lastError.clear();

#ifdef Q_OS_LINUX
    if (QGuiApplication::platformName() == QLatin1String("wayland")) {
        m_done = std::move(done);
        m_clock.start();
        m_lastReason.clear();
        const Type chosen = Capture::CaptureFactory::getDefaultStrategyType();
        m_chain.clear();
        QStringList names;
        for (const Type type : Capture::StrategySelection::frameSourceChain(chosen)) {
            m_chain.append(static_cast<int>(type));
            names.append(typeName(type));
        }
        qInfo().noquote() << "Frozen frame: trying" << names.join(QStringLiteral(", then "));
        tryNextSource();
        return;
    }
#endif

    // Synchronous on every non-Wayland platform (macOS, X11, offscreen).
    QRect virtualGeometry;
    const QPixmap frozen = Capture::QtScreensFrameSource::grabNow(&virtualGeometry);
    if (frozen.isNull())
        m_lastError = failureMessage(QString(), QString(), false);
    done(frozen, virtualGeometry);
}

QString FrozenFrameGrabber::failureMessage(const QString &currentDesktop, const QString &reason,
                                           bool cancelled)
{
    using Capture::StrategySelection::desktopIs;
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
    qWarning().noquote() << "Frozen frame: no frame for the selection:" << reason;
    finish({}, {});
}

#ifdef Q_OS_LINUX
void FrozenFrameGrabber::tryNextSource()
{
    while (!m_chain.isEmpty()) {
        const Type type = static_cast<Type>(m_chain.takeFirst());
        std::unique_ptr<Capture::DesktopFrameSource> source =
            Capture::CaptureFactory::createFrameSource(type, this);
        if (!source) {
            qInfo().noquote() << "Frozen frame:" << typeName(type) << "is not available here";
            if (m_lastReason.isEmpty())
                m_lastReason = tr("%1 is not available").arg(typeName(type));
            continue;
        }

        m_source = source.release();
        connect(m_source, &Capture::DesktopFrameSource::frameReady,
                this, &FrozenFrameGrabber::sourceReady);
        connect(m_source, &Capture::DesktopFrameSource::frameFailed,
                this, &FrozenFrameGrabber::sourceFailed);
        qInfo().noquote() << "Frozen frame: asking" << m_source->name();
        m_source->grab();
        return;
    }
    fail(m_lastReason, false);
}

void FrozenFrameGrabber::dropSource()
{
    if (!m_source)
        return;
    // It may still be inside its own signal: disconnect now, delete later.
    m_source->disconnect(this);
    m_source->deleteLater();
    m_source = nullptr;
}

void FrozenFrameGrabber::sourceReady(const QPixmap &frame, const QRect &virtualGeometry)
{
    const QString name = m_source ? m_source->name() : QString();
    dropSource();
    if (!Capture::frameCoversGeometry(frame.size(), frame.devicePixelRatio(), virtualGeometry)) {
        sourceFailed(tr("%1 returned %2x%3 pixels at scale %4, not the whole %5x%6 desktop")
                         .arg(name).arg(frame.width()).arg(frame.height())
                         .arg(frame.devicePixelRatio()).arg(virtualGeometry.width())
                         .arg(virtualGeometry.height()),
                     false);
        return;
    }
    qInfo().noquote() << "Frozen frame:" << name << "gave" << frame.width() << "x" << frame.height()
                      << "pixels at DPR" << frame.devicePixelRatio() << "over" << virtualGeometry
                      << "in" << m_clock.elapsed() << "ms";
    m_chain.clear();
    finish(frame, virtualGeometry);
}

void FrozenFrameGrabber::sourceFailed(const QString &reason, bool cancelled)
{
    const QString name = m_source ? m_source->name() : QString();
    dropSource();
    if (!name.isEmpty())
        qInfo().noquote() << "Frozen frame:" << name << "failed:" << reason;
    else
        qInfo().noquote() << "Frozen frame:" << reason;
    m_lastReason = reason;
    if (cancelled) {
        m_chain.clear();
        fail(reason, true);
        return;
    }
    tryNextSource();
}
#endif // Q_OS_LINUX

} // namespace Screen
