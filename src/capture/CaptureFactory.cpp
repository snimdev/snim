#include "CaptureFactory.h"
#include "core/Sandbox.h"
#include "strategies/NativeCaptureStrategy.h"
#ifdef Q_OS_LINUX
#include "strategies/KWinCaptureStrategy.h"
#include "strategies/WaylandCaptureStrategy.h"
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

        default:
            qWarning() << "Unknown strategy type, using native as fallback";
            return std::make_unique<NativeCaptureStrategy>(parent);
    }
}

CaptureFactory::StrategyType CaptureFactory::getDefaultStrategyType()
{
#ifdef Q_OS_LINUX
    // Prefer KWin on KDE Plasma; it never authorizes a sandboxed app, so a Flatpak uses the portal.
    if (!Core::Sandbox::isFlatpak() && KWinCaptureStrategy::isKWinAvailable()) {
        return StrategyType::KWin;
    }

    // Fall back to Wayland portal if we're in a Wayland session
    if (WaylandCaptureStrategy::isWaylandSession()) {
        auto waylandStrategy = std::make_unique<WaylandCaptureStrategy>();
        if (waylandStrategy->isAvailable()) {
            return StrategyType::Wayland;
        }
    }
#endif

    // Fall back to native Qt capture (the only strategy on non-Linux platforms)
    return StrategyType::Native;
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
