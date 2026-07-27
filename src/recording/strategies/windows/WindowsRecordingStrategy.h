#ifndef RECORDING_WINDOWSRECORDINGSTRATEGY_H
#define RECORDING_WINDOWSRECORDINGSTRATEGY_H

#include "recording/RecordingStrategy.h"

#include <memory>

class QTimer;

namespace Recording {

/**
 * Windows recording backend and a Mediator: WgcFrameSource (screen), two
 * WasapiAudioSources (system audio, microphone), a PcmMixBuffer and a
 * PauseAwareClock never talk to each other; everything they produce funnels
 * through this class onto one serial encoder thread that feeds FfmpegEncoder.
 * start() and stop() return at once; started() follows the first frame and
 * finished() the written file, as on macOS.
 */
class WindowsRecordingStrategy : public RecordingStrategy
{
    Q_OBJECT

public:
    explicit WindowsRecordingStrategy(QObject *parent = nullptr);
    ~WindowsRecordingStrategy() override;

    void start(const RecordTarget &target, const QString &outputPath) override;
    void stop() override;
    void pause() override;
    void resume() override;
    [[nodiscard]] bool isPaused() const override { return m_paused; }
    [[nodiscard]] bool isRecording() const override { return m_active; }
    [[nodiscard]] bool isAvailable() const override;
    [[nodiscard]] QString name() const override { return QStringLiteral("Graphics Capture"); }
    // Graphics Capture records the window's own surface, which never holds the bubble.
    [[nodiscard]] WindowCapture windowCapture() const override { return WindowCapture::Alone; }

    // Why recording cannot run on this system, or empty when it can.
    [[nodiscard]] static QString unavailableReason();

private:
    struct Engine;   // capture sources and the encoder thread, defined in the .cpp

    void reportStarted(quint64 generation);
    void reportFinished(quint64 generation, const QString &path);
    void reportFailed(quint64 generation, const QString &error);
    void reportClosed(quint64 generation);
    void requestFinish(bool report);

    std::unique_ptr<Engine> m_engine;
    QTimer *m_durationTimer = nullptr;
    quint64 m_generation = 0;
    bool m_active = false;
    bool m_stopping = false;
    bool m_paused = false;
};

} // namespace Recording

#endif // RECORDING_WINDOWSRECORDINGSTRATEGY_H
