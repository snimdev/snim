#ifndef RECORDING_RECORDINGFACTORY_H
#define RECORDING_RECORDINGFACTORY_H

#include "recording/RecordingStrategy.h"
#include <memory>

namespace Recording {

/**
 * Creates the recording backend for the current system, mirroring CaptureFactory.
 * Auto picks the best available (ScreenCaptureKit on macOS, the portal on Linux,
 * Graphics Capture on Windows; the stub where none of them can run). Adding a
 * platform is a new strategy class plus one branch here.
 */
class RecordingFactory
{
public:
    enum class StrategyType {
        Auto,       // best available for this system
        Mac,        // ScreenCaptureKit (macOS 12.3+)
        Linux,      // portal ScreenCast + GStreamer
        Windows,    // Graphics Capture + WASAPI + FFmpeg (Windows 10 2004+)
        Stub        // unsupported-platform fallback
    };

    static std::unique_ptr<RecordingStrategy> createStrategy(
        StrategyType type = StrategyType::Auto,
        QObject *parent = nullptr
    );

    static StrategyType getDefaultStrategyType();
    static bool isStrategyAvailable(StrategyType type);

private:
    RecordingFactory() = default; // Static class
};

} // namespace Recording

#endif // RECORDING_RECORDINGFACTORY_H
