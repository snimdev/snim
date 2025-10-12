#include "CaptureFactory.h"
#include "WaylandCaptureStrategy.h"
#include "NativeCaptureStrategy.h"
#include <QDebug>

namespace Capture {

std::unique_ptr<CaptureStrategy> CaptureFactory::createStrategy(StrategyType type, QObject *parent)
{
    if (type == StrategyType::Auto) {
        type = getDefaultStrategyType();
    }

    switch (type) {
        case StrategyType::Wayland: {
            auto strategy = std::make_unique<WaylandCaptureStrategy>(parent);
            if (strategy->isAvailable()) {
                qDebug() << "Created Wayland capture strategy";
                return strategy;
            }
            qWarning() << "Wayland strategy requested but not available, falling back to native";
            // Fall through to native
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
    // Prefer Wayland if we're in a Wayland session and it's available
    if (WaylandCaptureStrategy::isWaylandSession()) {
        auto waylandStrategy = std::make_unique<WaylandCaptureStrategy>();
        if (waylandStrategy->isAvailable()) {
            return StrategyType::Wayland;
        }
    }

    // Fall back to native Qt capture
    return StrategyType::Native;
}

bool CaptureFactory::isStrategyAvailable(StrategyType type)
{
    switch (type) {
        case StrategyType::Wayland: {
            auto strategy = std::make_unique<WaylandCaptureStrategy>();
            return strategy->isAvailable();
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
