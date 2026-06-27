#include "CaptureFactory.h"
#include "strategies/NativeCaptureStrategy.h"
#ifdef Q_OS_LINUX
#include "strategies/KWinCaptureStrategy.h"
#include "strategies/WaylandCaptureStrategy.h"
#endif
#if defined(Q_OS_LINUX) && defined(SNIM_HAVE_LINUX_RECORDER)
#include "strategies/ScreencastCaptureStrategy.h"
#endif
#ifdef SNIM_HAVE_SCREENCOPY
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

CaptureFactory::StrategyType CaptureFactory::getDefaultStrategyType()
{
    return Screen::FrameSourceFactory::defaultType();
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
