#include "screen/sources/FrameSourceFactory.h"

#include "core/Sandbox.h"
#include "screen/sources/QtScreensFrameSource.h"
#include "screen/sources/StrategySelection.h"
#ifdef Q_OS_LINUX
#include "screen/sources/KWinFrameSource.h"
#include "screen/sources/PortalFrameSource.h"
#endif
#if defined(Q_OS_LINUX) && defined(SNIM_HAVE_LINUX_RECORDER)
#include "screen/sources/ScreencastFrameSource.h"
#endif
#ifdef SNIM_HAVE_SCREENCOPY
#include "screen/sources/ScreencopyFrameSource.h"
#endif

namespace Screen {

std::unique_ptr<DesktopFrameSource> FrameSourceFactory::create(SourceType type, QObject *parent)
{
    switch (type) {
    case SourceType::Auto:
        break;
    case SourceType::KWin:
#ifdef Q_OS_LINUX
        if (isAvailable(type))
            return std::make_unique<KWinFrameSource>(parent);
#endif
        break;
    case SourceType::Screencast:
#if defined(Q_OS_LINUX) && defined(SNIM_HAVE_LINUX_RECORDER)
        if (isAvailable(type))
            return std::make_unique<ScreencastFrameSource>(parent);
#endif
        break;
    case SourceType::Screencopy:
#ifdef SNIM_HAVE_SCREENCOPY
        if (isAvailable(type))
            return std::make_unique<ScreencopyFrameSource>(parent);
#endif
        break;
    case SourceType::Portal:
#ifdef Q_OS_LINUX
        return std::make_unique<PortalFrameSource>(parent);
#else
        break;
#endif
    case SourceType::Native:
        return std::make_unique<QtScreensFrameSource>(parent);
    }
    return nullptr;
}

bool FrameSourceFactory::isAvailable(SourceType type)
{
    switch (type) {
    case SourceType::Auto:
        break;
    case SourceType::KWin:
#ifdef Q_OS_LINUX
        return KWinFrameSource::isServiceRegistered() && KWinFrameSource::apiVersion() > 0;
#else
        break;
#endif
    case SourceType::Screencast:
#if defined(Q_OS_LINUX) && defined(SNIM_HAVE_LINUX_RECORDER)
        return ScreencastFrameSource::isSupported();
#else
        break;
#endif
    case SourceType::Screencopy:
#ifdef SNIM_HAVE_SCREENCOPY
        return ScreencopyFrameSource::isAvailable();
#else
        break;
#endif
    case SourceType::Portal:
#ifdef Q_OS_LINUX
        return isWaylandSession() && PortalFrameSource::isPortalReachable();
#else
        break;
#endif
    case SourceType::Native:
        return true;
    }
    return false;
}

SourceType FrameSourceFactory::defaultType()
{
    // SNIM_CAPTURE_STRATEGY=kwin|screencast|screencopy|portal|native forces one, for testing.
    if (const auto forced = StrategySelection::parseOverride(qEnvironmentVariable("SNIM_CAPTURE_STRATEGY")))
        return *forced;

    // Without these sources (macOS, Windows) this is always the native Qt capture.
    const auto offered = [](SourceType type) { return [type] { return isAvailable(type); }; };
    StrategySelection::Probes probes;
    probes.kwin = offered(SourceType::KWin);
    probes.screencopy = offered(SourceType::Screencopy);
    probes.screencast = offered(SourceType::Screencast);
    probes.portal = offered(SourceType::Portal);
    return StrategySelection::choose(qEnvironmentVariable("XDG_CURRENT_DESKTOP"),
                                     Core::Sandbox::isFlatpak(), probes);
}

QString FrameSourceFactory::typeName(SourceType type)
{
    switch (type) {
    case SourceType::KWin: return QStringLiteral("KWin ScreenShot2");
    case SourceType::Screencast: return QStringLiteral("ScreenCast portal");
    case SourceType::Screencopy: return QStringLiteral("Wayland screencopy");
    case SourceType::Portal: return QStringLiteral("Screenshot portal");
    case SourceType::Native: return QStringLiteral("Qt screen grab");
    case SourceType::Auto: break;
    }
    return QStringLiteral("automatic");
}

} // namespace Screen
