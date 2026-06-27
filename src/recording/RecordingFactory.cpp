#include "recording/RecordingFactory.h"
#include "recording/strategies/StubRecordingStrategy.h"
#ifdef SNIM_HAVE_MAC_RECORDER
#include "recording/strategies/MacRecordingStrategy.h"
#endif
#ifdef SNIM_HAVE_LINUX_RECORDER
#include "recording/strategies/LinuxRecorderModule.h"
#endif
#ifdef SNIM_HAVE_WIN_RECORDER
#include "recording/strategies/windows/WindowsRecordingStrategy.h"
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
#ifdef SNIM_HAVE_MAC_RECORDER
            auto strategy = std::make_unique<MacRecordingStrategy>(parent);
            if (strategy->isAvailable()) {
                qDebug() << "Created ScreenCaptureKit recording strategy";
                return strategy;
            }
            qWarning() << "ScreenCaptureKit recorder unavailable (needs macOS 12.3+), using stub";
#endif
        }
        [[fallthrough]];

        case StrategyType::Linux: {
#ifdef SNIM_HAVE_LINUX_RECORDER
            // The backend lives in a dlopened module, so a host without GStreamer (or
            // without the module) degrades here exactly like an unavailable backend.
            std::unique_ptr<RecordingStrategy> strategy(LinuxRecorderModule::create(parent));
            if (strategy && strategy->isAvailable()) {
                qDebug().noquote() << "Created" << strategy->name() << "recording strategy";
                return strategy;
            }
            qWarning().noquote() << "GStreamer recorder unavailable, using stub. Missing:"
                                 << LinuxRecorderModule::missingPieces().join(QStringLiteral("; "));
#endif
        }
        [[fallthrough]];

        case StrategyType::Windows: {
#ifdef SNIM_HAVE_WIN_RECORDER
            auto strategy = std::make_unique<WindowsRecordingStrategy>(parent);
            if (strategy->isAvailable()) {
                qDebug() << "Created Graphics Capture recording strategy";
                return strategy;
            }
            qWarning() << "Graphics Capture recorder unavailable (needs Windows 10 2004+), using stub";
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
#if defined(SNIM_HAVE_MAC_RECORDER)
    return StrategyType::Mac;
#elif defined(SNIM_HAVE_LINUX_RECORDER)
    return StrategyType::Linux;
#elif defined(SNIM_HAVE_WIN_RECORDER)
    return StrategyType::Windows;
#else
    return StrategyType::Stub;
#endif
}

bool RecordingFactory::isStrategyAvailable(StrategyType type)
{
    switch (type) {
        case StrategyType::Mac: {
#ifdef SNIM_HAVE_MAC_RECORDER
            auto strategy = std::make_unique<MacRecordingStrategy>();
            return strategy->isAvailable();
#else
            return false;
#endif
        }
        case StrategyType::Linux: {
#ifdef SNIM_HAVE_LINUX_RECORDER
            // Built with Linux recording support is not the same as usable here: the
            // module still has to load and find its portal, plugins and encoder.
            return LinuxRecorderModule::isAvailable();
#else
            return false;
#endif
        }
        case StrategyType::Windows: {
#ifdef SNIM_HAVE_WIN_RECORDER
            return WindowsRecordingStrategy().isAvailable();
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
