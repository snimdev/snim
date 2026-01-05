#ifndef RECORDING_LINUXRECORDINGSTRATEGY_H
#define RECORDING_LINUXRECORDINGSTRATEGY_H

#include "recording/RecordingStrategy.h"
#include "recording/RecordTarget.h"

#include <QRect>
#include <QString>

#include <optional>

typedef struct _GstElement GstElement;

class QTimer;

namespace Recording {

class ScreenCastPortalSession;

/**
 * Linux recording backend: an xdg-desktop-portal ScreenCast stream fed into a
 * GStreamer pipeline that crops the selection out of the shared monitor and encodes
 * it to H.264. The header stays free of GStreamer headers, so the rest of the
 * codebase includes this like any other class; everything else lives in the .cpp.
 */
class LinuxRecordingStrategy : public RecordingStrategy
{
    Q_OBJECT

public:
    explicit LinuxRecordingStrategy(QObject *parent = nullptr);
    ~LinuxRecordingStrategy() override;

    void start(const RecordTarget &target, const QString &outputPath) override;
    void stop() override;
    [[nodiscard]] bool isRecording() const override { return m_starting || m_recording; }
    [[nodiscard]] bool isAvailable() const override;
    [[nodiscard]] QString name() const override { return QStringLiteral("Portal/GStreamer"); }

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

    bool buildPipeline(quint32 nodeId, QString *error);
    void teardown();
    void fail(const QString &error);

    ScreenCastPortalSession *m_session = nullptr;
    GstElement *m_pipeline = nullptr;
    QTimer *m_durationTimer = nullptr;
    QTimer *m_eosTimer = nullptr;

    RecordTarget m_target;
    QString m_outputPath;
    QRect m_streamRect;          // resolved on the GUI thread when the session is ready
    int m_pipewireFd = -1;
    quint64 m_generation = 0;
    bool m_starting = false;
    bool m_recording = false;
    bool m_stopping = false;

    mutable std::optional<bool> m_available;
};

} // namespace Recording

#endif // RECORDING_LINUXRECORDINGSTRATEGY_H
