#ifndef RECORDING_STUBRECORDINGSTRATEGY_H
#define RECORDING_STUBRECORDINGSTRATEGY_H

#include "recording/RecordingStrategy.h"

namespace Recording {

/**
 * Universal fallback backend for systems with no recorder yet (everything except
 * macOS for now, and macOS too old for ScreenCaptureKit). isAvailable() is false
 * and start() fails immediately, so the app builds everywhere and the Record
 * actions simply stay disabled where recording is unsupported.
 */
class StubRecordingStrategy : public RecordingStrategy
{
    Q_OBJECT

public:
    explicit StubRecordingStrategy(QObject *parent = nullptr) : RecordingStrategy(parent) {}

    void start(const RecordTarget &target, const QString &outputPath) override;
    void stop() override {}
    [[nodiscard]] bool isRecording() const override { return false; }
    [[nodiscard]] bool isAvailable() const override { return false; }
    [[nodiscard]] QString name() const override { return QStringLiteral("Unsupported"); }
};

} // namespace Recording

#endif // RECORDING_STUBRECORDINGSTRATEGY_H
