#include "CaptureFactory.h"
#include "StrategySelection.h"
#include "core/Sandbox.h"
#include "screen/sources/QtScreensFrameSource.h"
#include "strategies/NativeCaptureStrategy.h"
#ifdef Q_OS_LINUX
#include "screen/sources/KWinFrameSource.h"
#include "screen/sources/PortalFrameSource.h"
#include "strategies/KWinCaptureStrategy.h"
#include "strategies/WaylandCaptureStrategy.h"
#endif
#if defined(Q_OS_LINUX) && defined(SNIM_HAVE_LINUX_RECORDER)
#include "strategies/ScreencastCaptureStrategy.h"
#endif
#ifdef SNIM_HAVE_SCREENCOPY
#include "screen/sources/ScreencopyFrameSource.h"
#include "strategies/ScreencopyCaptureStrategy.h"
#endif
#include <QtGlobal>
#include <QDebug>
#include <memory>

namespace Capture {

std::unique_ptr<CaptureStrategy> CaptureFactory::createStrategy(StrategyType type, QObject *parent)
{
    if (type == StrategyType::Auto) {
        type = getDefaultStrategyType();
    }

    switch (type) {
        case StrategyType::KWin: {
#ifdef Q_OS_LINUX
            auto strategy = std::make_unique<KWinCaptureStrategy>(parent);
            if (strategy->isAvailable()) {
                qDebug() << "Created KWin capture strategy";
                return strategy;
            }
            qWarning() << "KWin strategy requested but not available, falling back to Wayland";
#endif
        }
        [[fallthrough]];

        case StrategyType::Wayland: {
#ifdef Q_OS_LINUX
            auto strategy = std::make_unique<WaylandCaptureStrategy>(parent);
            if (strategy->isAvailable()) {
                qDebug() << "Created Wayland capture strategy";
                return strategy;
            }
            qWarning() << "Wayland strategy requested but not available, falling back to native";
#endif
        }
        [[fallthrough]];

        case StrategyType::Native: {
            auto strategy = std::make_unique<NativeCaptureStrategy>(parent);
            qDebug() << "Created native capture strategy";
            return strategy;
        }

        case StrategyType::Screencast: {
#if defined(Q_OS_LINUX) && defined(SNIM_HAVE_LINUX_RECORDER)
            auto strategy = std::make_unique<ScreencastCaptureStrategy>(parent);
            if (strategy->isAvailable()) {
                qDebug() << "Created ScreenCast capture strategy";
                return strategy;
            }
#endif
            qWarning() << "ScreenCast strategy requested but not available, falling back to Wayland";
            return createStrategy(StrategyType::Wayland, parent);
        }

        case StrategyType::Screencopy: {
#ifdef SNIM_HAVE_SCREENCOPY
            auto strategy = std::make_unique<ScreencopyCaptureStrategy>(parent);
            if (strategy->isAvailable()) {
                qDebug() << "Created Wayland screencopy capture strategy";
                return strategy;
            }
            qWarning() << "Screencopy strategy requested but not available, falling back to Wayland";
#endif
            return createStrategy(StrategyType::Wayland, parent);
        }

        default:
            qWarning() << "Unknown strategy type, using native as fallback";
            return std::make_unique<NativeCaptureStrategy>(parent);
    }
}

std::unique_ptr<Screen::DesktopFrameSource> CaptureFactory::createFrameSource(StrategyType type, QObject *parent)
{
    switch (type) {
    case StrategyType::Auto:
        return createFrameSource(getDefaultStrategyType(), parent);
    case StrategyType::KWin:
#ifdef Q_OS_LINUX
        if (KWinCaptureStrategy::isKWinAvailable()) {
            auto source = std::make_unique<Screen::KWinFrameSource>(parent);
            if (source->apiVersion() > 0)
                return source;
        }
#endif
        return nullptr;
    case StrategyType::Screencast:
#if defined(Q_OS_LINUX) && defined(SNIM_HAVE_LINUX_RECORDER)
        if (Screen::ScreencastFrameSource::isSupported())
            return std::make_unique<Screen::ScreencastFrameSource>(parent);
#endif
        return nullptr;
    case StrategyType::Screencopy:
#ifdef SNIM_HAVE_SCREENCOPY
        if (ScreencopyCaptureStrategy::isScreencopyAvailable())
            return std::make_unique<Screen::ScreencopyFrameSource>(parent);
#endif
        return nullptr;
    case StrategyType::Wayland:
#ifdef Q_OS_LINUX
        return std::make_unique<Screen::PortalFrameSource>(parent);
#else
        return nullptr;
#endif
    case StrategyType::Native:
        return std::make_unique<Screen::QtScreensFrameSource>(parent);
    }
    return nullptr;
}

CaptureFactory::StrategyType CaptureFactory::getDefaultStrategyType()
{
    // SNIM_CAPTURE_STRATEGY=kwin|screencast|screencopy|wayland|native forces one, for testing.
    if (const auto forced = StrategySelection::parseOverride(qEnvironmentVariable("SNIM_CAPTURE_STRATEGY")))
        return *forced;

    StrategySelection::Probes probes;
#ifdef Q_OS_LINUX
    probes.kwin = [] { return KWinCaptureStrategy::isKWinAvailable(); };
    probes.portal = [] {
        return WaylandCaptureStrategy::isWaylandSession() && WaylandCaptureStrategy().isAvailable();
    };
#endif
#ifdef SNIM_HAVE_SCREENCOPY
    probes.screencopy = [] { return ScreencopyCaptureStrategy::isScreencopyAvailable(); };
#endif
#if defined(Q_OS_LINUX) && defined(SNIM_HAVE_LINUX_RECORDER)
    probes.screencast = [] { return ScreencastCaptureStrategy::isSupported(); };
#endif
    // Without probes (macOS, Windows) this is always the native Qt capture.
    return StrategySelection::choose(qEnvironmentVariable("XDG_CURRENT_DESKTOP"),
                                     Core::Sandbox::isFlatpak(), probes);
}

bool CaptureFactory::isStrategyAvailable(StrategyType type)
{
    switch (type) {
        case StrategyType::KWin: {
#ifdef Q_OS_LINUX
            return KWinCaptureStrategy::isKWinAvailable();
#else
            return false;
#endif
        }
        case StrategyType::Wayland: {
#ifdef Q_OS_LINUX
            auto strategy = std::make_unique<WaylandCaptureStrategy>();
            return strategy->isAvailable();
#else
            return false;
#endif
        }
        case StrategyType::Screencast: {
#if defined(Q_OS_LINUX) && defined(SNIM_HAVE_LINUX_RECORDER)
            return ScreencastCaptureStrategy::isSupported();
#else
            return false;
#endif
        }
        case StrategyType::Screencopy: {
#ifdef SNIM_HAVE_SCREENCOPY
            return ScreencopyCaptureStrategy::isScreencopyAvailable();
#else
            return false;
#endif
        }
        case StrategyType::Native: {
            auto strategy = std::make_unique<NativeCaptureStrategy>();
            return strategy->isAvailable();
        }
        case StrategyType::Auto:
            return true; // Auto is always available since it falls back
        default:
            return false;
    }
}

} // namespace Capture
