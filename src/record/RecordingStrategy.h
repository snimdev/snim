#ifndef RECORDING_RECORDINGSTRATEGY_H
#define RECORDING_RECORDINGSTRATEGY_H

#include "record/RecordTarget.h"

#include <QObject>
#include <QString>

namespace Record {

/**
 * Abstract base for platform screen-recording backends: the seam that keeps
 * recording platform-agnostic. RecordingFactory picks one per OS: ScreenCaptureKit on
 * macOS, GStreamer (ScreenCast portal or X11) on Linux, Windows.Graphics.Capture on
 * Windows. Everything above it
 * (selection, lifecycle, settings, UI) is shared, platform-neutral code.
 *
 * start()/stop() are asynchronous: a backend emits started() once capture is
 * actually running (on macOS, after the screen-recording permission prompt) and
 * finished() once the output file is fully written and closed. A backend whose system
 * picker the user dismisses emits cancelled() instead of failed().
 */
class RecordingStrategy : public QObject
{
    Q_OBJECT

public:
    explicit RecordingStrategy(QObject *parent = nullptr) : QObject(parent) {}
    ~RecordingStrategy() override;

    // Begin recording the target into outputPath.
    virtual void start(const RecordTarget &target, const QString &outputPath) = 0;
    // Stop and finalize the file (emits finished() or failed()).
    virtual void stop() = 0;

    // Pause/resume. Paused time is excluded from the output (no frozen segment).
    // Default no-ops for backends that don't support it.
    virtual void pause() {}
    virtual void resume() {}
    [[nodiscard]] virtual bool isPaused() const { return false; }

    [[nodiscard]] virtual bool isRecording() const = 0;
    // Whether this backend can run here (platform + OS version). Does not assert
    // permission, which on macOS can only be known after the first capture call.
    [[nodiscard]] virtual bool isAvailable() const = 0;
    [[nodiscard]] virtual QString name() const = 0;

    // What a window recording holds, and who picks the window.
    enum class WindowCapture {
        WithOverlays,   // the picked window plus Snim's camera bubble
        Alone,          // the picked window only: a bubble would never be recorded
        SystemPicked,   // alone, and the system's own picker chooses it (the portal's)
    };
    [[nodiscard]] virtual WindowCapture windowCapture() const
    {
        return WindowCapture::WithOverlays;
    }

signals:
    void started();                        // capture is running
    void finished(const QString &path);    // file written and closed
    void failed(const QString &error);     // permission denied / setup / writer error
    void cancelled();                      // the user backed out before capture began
    void durationChanged(qint64 ms);       // recorded time (pauses excluded), for the label
    void pausedChanged(bool paused);       // pause/resume toggled
};

} // namespace Record

#endif // RECORDING_RECORDINGSTRATEGY_H
