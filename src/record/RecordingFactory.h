#ifndef RECORDING_RECORDINGFACTORY_H
#define RECORDING_RECORDINGFACTORY_H

#include "record/RecordingStrategy.h"
#include <memory>

namespace Record {

/**
 * Creates the recording backend for the current system, mirroring CaptureFactory:
 * ScreenCaptureKit on macOS, the portal on Linux, Graphics Capture on Windows, and the
 * stub where none of them can run. Each OS builds one backend, so adding a platform is
 * a new strategy class plus one branch here.
 */
class RecordingFactory
{
public:
    static std::unique_ptr<RecordingStrategy> createStrategy(QObject *parent = nullptr);

private:
    RecordingFactory() = default; // Static class
};

} // namespace Record

#endif // RECORDING_RECORDINGFACTORY_H
