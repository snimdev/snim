#ifndef RECORDING_LINUXRECORDINGSTRATEGY_H
#define RECORDING_LINUXRECORDINGSTRATEGY_H

#include "recording/PauseAwareClock.h"
#include "recording/RecordingGeometry.h"
#include "recording/RecordingStrategy.h"
#include "recording/RecordTarget.h"
#include "recording/strategies/LinuxPipeline.h"

#include <QRect>
#include <QString>
#include <QStringList>
#include <QVector>

#include <functional>
#include <memory>
#include <optional>

typedef struct _GstElement GstElement;
typedef struct _GstPad GstPad;
typedef struct _GThreadPool GThreadPool;

class QTimer;

namespace Screen {
class ScreenCastPortalSession;
} // namespace Screen

namespace Recording {

struct StrategyLink;

/**
 * Linux recording backend: a GStreamer pipeline that encodes the selection to H.264.
 * In an X11 session ximagesrc reads it straight off the root window; elsewhere an
 * xdg-desktop-portal ScreenCast stream is cropped down to it. The header stays free of
 * GStreamer headers, so the rest of the codebase includes this like any other class;
 * everything else lives in the .cpp.
 */
class LinuxRecordingStrategy : public RecordingStrategy
{
    Q_OBJECT

public:
    explicit LinuxRecordingStrategy(QObject *parent = nullptr);
    ~LinuxRecordingStrategy() override;

    void start(const RecordTarget &target, const QString &outputPath) override;
    void stop() override;
    void pause() override;
    void resume() override;
    [[nodiscard]] bool isPaused() const override { return m_paused; }
    [[nodiscard]] bool isRecording() const override { return m_starting || m_recording; }
    [[nodiscard]] bool isAvailable() const override;
    // What this host lacks to record, named with the packages to install. Empty when none.
    [[nodiscard]] static QStringList missingPieces();
    // For `snim --self-test`: the elements this session's recording and ScreenCast
    // screenshots need, without asking the portal. `found` ends with the encoder.
    static void checkElements(QStringList *found, QStringList *missing);
    [[nodiscard]] QString name() const override
    {
        return m_source == LinuxPipeline::VideoSource::X11 ? QStringLiteral("X11/GStreamer")
                                                           : QStringLiteral("Portal/GStreamer");
    }

    // Backend hooks: invoked by the GStreamer callbacks once they have marshalled onto
    // this object's thread. The generation identifies the pipeline they came from, so a
    // callback queued before a teardown is dropped. Not for general use.
    void reportStarted(quint64 generation);
    void reportEos(quint64 generation);
    void reportError(quint64 generation, const QString &error);

private:
    void handleSessionReady(quint32 nodeId, const QRect &streamRectLogical, int pipewireFd);
    void handleSessionFailed(const QString &error);
    void handleSessionClosed();

    void startX11();
    bool buildPipeline(quint32 nodeId, QString *error);
    void configurePortalSource(GstElement *src, GstElement *crop, GstElement *outcaps,
                               quint32 nodeId);
    void startPipeline();
    void setValvesDropping(bool drop);
    // Runs blocking GStreamer calls off the GUI thread, in submission order.
    void runOnPipelineThread(std::function<void()> task);
    void teardown();
    void fail(const QString &error);

    Screen::ScreenCastPortalSession *m_session = nullptr;
    GstElement *m_pipeline = nullptr;
    GThreadPool *m_runner = nullptr;
    std::shared_ptr<StrategyLink> m_link;   // the current pipeline's way back to this object
    QTimer *m_durationTimer = nullptr;
    QTimer *m_firstFrameTimer = nullptr;
    QTimer *m_eosTimer = nullptr;

    const LinuxPipeline::VideoSource m_source;
    X11Grab m_x11Grab;

    RecordTarget m_target;
    QString m_outputPath;
    QRect m_streamRect;          // null when the portal sends no stream geometry
    QVector<StreamSource> m_screens;   // snapshot taken on the GUI thread, for the probe
    StreamSource m_virtualDesktop;
    int m_pipewireFd = -1;
    quint64 m_generation = 0;

    QVector<GstElement *> m_valves;   // owned refs, all closed while paused
    QVector<GstPad *> m_valvePads;    // owned refs, the valve src pads carrying the offset
    quint64 m_pauseStartRt = 0;       // GstClockTime, kept gst-free for this header
    quint64 m_pausedTotal = 0;
    PauseAwareClock m_elapsed;        // the timer's clock: buffer timestamps vary by portal

    bool m_starting = false;
    bool m_recording = false;
    bool m_stopping = false;
    bool m_paused = false;

    mutable std::optional<bool> m_available;
};

} // namespace Recording

#endif // RECORDING_LINUXRECORDINGSTRATEGY_H
