#ifndef CAPTURE_CAPTUREFACTORY_H
#define CAPTURE_CAPTUREFACTORY_H

#include "strategies/CaptureStrategy.h"
#include <memory>

namespace Capture {

/**
 * Creates the appropriate capture strategy for the current system
 */
class CaptureFactory
{
public:
    enum class StrategyType {
        Auto,           // Automatically select best available strategy
        KWin,           // KWin ScreenShot2 D-Bus (preferred on KDE Plasma)
        Wayland,        // Force Wayland strategy
        Screencast,     // One frame from a restored ScreenCast portal session
        Screencopy,     // Native Wayland screencopy (wlroots-family compositors)
        Native          // Force native Qt strategy
    };

    /**
     * Create the best available capture strategy for the current system
     */
    static std::unique_ptr<CaptureStrategy> createStrategy(
        StrategyType type = StrategyType::Auto,
        QObject *parent = nullptr
    );

    /**
     * Get the default strategy type for the current system
     */
    static StrategyType getDefaultStrategyType();

    /**
     * Check if a specific strategy type is available
     */
    static bool isStrategyAvailable(StrategyType type);

private:
    CaptureFactory() = default; // Static class
};

} // namespace Capture

#endif // CAPTURE_CAPTUREFACTORY_H
