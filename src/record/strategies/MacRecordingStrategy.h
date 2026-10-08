#ifndef RECORDING_MACRECORDINGSTRATEGY_H
#define RECORDING_MACRECORDINGSTRATEGY_H

#include "record/RecordingStrategy.h"
#include <memory>

namespace Record {

/**
 * macOS recording backend built on ScreenCaptureKit (capture) + AVAssetWriter
 * (H.264 / MP4 encoding). The header stays pure C++: all Objective-C++ and the
 * ScreenCaptureKit object graph live behind an opaque pimpl in the .mm, so the
 * rest of the (Qt/C++) codebase includes this like any other class.
 */
class MacRecordingStrategy : public RecordingStrategy
{
    Q_OBJECT

public:
    explicit MacRecordingStrategy(QObject *parent = nullptr);
    ~MacRecordingStrategy() override;

    void start(const RecordTarget &target, const QString &outputPath) override;
    void stop() override;
    void pause() override;
    void resume() override;
    [[nodiscard]] bool isPaused() const override;
    [[nodiscard]] bool isRecording() const override;
    [[nodiscard]] bool isAvailable() const override;
    [[nodiscard]] QString name() const override { return QStringLiteral("ScreenCaptureKit"); }

    // Backend hooks: invoked by the ScreenCaptureKit implementation (already hopped
    // onto this object's thread) to surface state as signals. Not for general use.
    void reportStarted();
    void reportFinished(const QString &path);
    void reportFailed(const QString &error);
    void reportDuration(qint64 ms);

    struct Impl;   // defined in the .mm (holds the ScreenCaptureKit object graph)

private:
    std::unique_ptr<Impl> d;
};

} // namespace Record

#endif // RECORDING_MACRECORDINGSTRATEGY_H
