#ifndef CAPTURE_CAPTUREFACTORY_H
#define CAPTURE_CAPTUREFACTORY_H

#include "screen/sources/FrameSourceFactory.h"
#include "strategies/CaptureStrategy.h"
#include <memory>

namespace Capture {

/**
 * Creates the appropriate capture strategy for the current system
 */
class CaptureFactory
{
public:
    // Strategies follow the frame source types, so they share the type and its selection.
    using StrategyType = Screen::SourceType;

    /**
     * Create the best available capture strategy for the current system
     */
    static std::unique_ptr<CaptureStrategy> createStrategy(
        StrategyType type = StrategyType::Auto,
        QObject *parent = nullptr
    );

private:
    CaptureFactory() = default; // Static class
};

} // namespace Capture

#endif // CAPTURE_CAPTUREFACTORY_H
