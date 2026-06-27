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
        return create(defaultType(), parent);
    case SourceType::KWin:
#ifdef Q_OS_LINUX
        if (KWinFrameSource::isServiceRegistered()) {
            auto source = std::make_unique<KWinFrameSource>(parent);
            if (source->apiVersion() > 0)
                return source;
        }
#endif
        return nullptr;
    case SourceType::Screencast:
#if defined(Q_OS_LINUX) && defined(SNIM_HAVE_LINUX_RECORDER)
        if (ScreencastFrameSource::isSupported())
            return std::make_unique<ScreencastFrameSource>(parent);
#endif
        return nullptr;
    case SourceType::Screencopy:
#ifdef SNIM_HAVE_SCREENCOPY
        if (ScreencopyFrameSource::isAvailable())
            return std::make_unique<ScreencopyFrameSource>(parent);
#endif
        return nullptr;
    case SourceType::Wayland:
#ifdef Q_OS_LINUX
        return std::make_unique<PortalFrameSource>(parent);
#else
        return nullptr;
#endif
    case SourceType::Native:
        return std::make_unique<QtScreensFrameSource>(parent);
    }
    return nullptr;
}

SourceType FrameSourceFactory::defaultType()
{
    // SNIM_CAPTURE_STRATEGY=kwin|screencast|screencopy|wayland|native forces one, for testing.
    if (const auto forced = StrategySelection::parseOverride(qEnvironmentVariable("SNIM_CAPTURE_STRATEGY")))
        return *forced;

    StrategySelection::Probes probes;
#ifdef Q_OS_LINUX
    probes.kwin = [] { return KWinFrameSource::isServiceRegistered(); };
    probes.portal = [] {
        return isWaylandSession()
               && (PortalFrameSource::isPortalReachable() || PortalFrameSource::hasFallbackTool(false));
    };
#endif
#ifdef SNIM_HAVE_SCREENCOPY
    probes.screencopy = [] { return ScreencopyFrameSource::isAvailable(); };
#endif
#if defined(Q_OS_LINUX) && defined(SNIM_HAVE_LINUX_RECORDER)
    probes.screencast = [] { return ScreencastFrameSource::isSupported(); };
#endif
    // Without probes (macOS, Windows) this is always the native Qt capture.
    return StrategySelection::choose(qEnvironmentVariable("XDG_CURRENT_DESKTOP"),
                                     Core::Sandbox::isFlatpak(), probes);
}

} // namespace Screen
