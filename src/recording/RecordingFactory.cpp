#include "recording/RecordingFactory.h"
#include "recording/strategies/StubRecordingStrategy.h"
#ifdef NICESHOT_HAVE_MAC_RECORDER
#include "recording/strategies/MacRecordingStrategy.h"
#endif

#include <QtGlobal>
#include <QDebug>

namespace Recording {

std::unique_ptr<RecordingStrategy> RecordingFactory::createStrategy(StrategyType type, QObject *parent)
{
    if (type == StrategyType::Auto)
        type = getDefaultStrategyType();

    switch (type) {
        case StrategyType::Mac: {
#ifdef NICESHOT_HAVE_MAC_RECORDER
            auto strategy = std::make_unique<MacRecordingStrategy>(parent);
            if (strategy->isAvailable()) {
                qDebug() << "Created ScreenCaptureKit recording strategy";
                return strategy;
            }
            qWarning() << "ScreenCaptureKit recorder unavailable (needs macOS 12.3+), using stub";
#endif
        }
        [[fallthrough]];

        case StrategyType::Stub:
        default:
            return std::make_unique<StubRecordingStrategy>(parent);
    }
}

RecordingFactory::StrategyType RecordingFactory::getDefaultStrategyType()
{
#ifdef NICESHOT_HAVE_MAC_RECORDER
    return StrategyType::Mac;
#else
    return StrategyType::Stub;
#endif
}

bool RecordingFactory::isStrategyAvailable(StrategyType type)
{
    switch (type) {
        case StrategyType::Mac: {
#ifdef NICESHOT_HAVE_MAC_RECORDER
            auto strategy = std::make_unique<MacRecordingStrategy>();
            return strategy->isAvailable();
#else
            return false;
#endif
        }
        case StrategyType::Stub:
            return true;   // always a valid fallback
        case StrategyType::Auto:
            return true;   // Auto always resolves (falls back to stub)
        default:
            return false;
    }
}

} // namespace Recording
