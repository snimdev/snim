#ifndef EDITOR_VIDEO_GSTVIDEOEXPORTER_H
#define EDITOR_VIDEO_GSTVIDEOEXPORTER_H

#include "editor/video/VideoExporter.h"

#include <QString>

#include <memory>
#include <optional>

class QTimer;

namespace Editor::Video {

/**
 * Linux trim exporter (a VideoExporter Strategy) on GStreamer. It decodes the
 * recording, seeks frame-accurately to the range before anything reaches the muxer,
 * and re-encodes it to H.264 (and AAC when the source has audio), so the cut lands on
 * the exact frame rather than the nearest keyframe. Lives in the dlopened
 * libsnim-video-linux.so; the header stays free of GStreamer headers.
 */
class GstVideoExporter : public VideoExporter
{
    Q_OBJECT

public:
    explicit GstVideoExporter(QObject *parent = nullptr);
    ~GstVideoExporter() override;

    void trim(const QString &input, const QString &output,
              qint64 inMs, qint64 outMs) override;
    void toGif(const QString &input, const QString &output,
               qint64 inMs, qint64 outMs, const AnimationParams &params) override;
    void cancel() override;
    [[nodiscard]] bool isAvailable() const override;

    // Backend hooks: invoked by the GStreamer callbacks once they have marshalled onto
    // this object's thread. The generation identifies the pipeline they came from, so a
    // callback queued before a teardown is dropped. Not for general use.
    void reportPadsComplete(quint64 generation, int branches, bool hasVideo);
    void reportBranchReady(quint64 generation);
    void reportEos(quint64 generation);
    void reportError(quint64 generation, const QString &error);

    struct Session;   // defined in the .cpp (holds the pipeline)

private:
    void failLater(const QString &error);
    void seekAndPlay();
    void updateProgress();
    void fail(const QString &error);
    void teardown();

    std::unique_ptr<Session> m_session;
    QTimer *m_progressTimer = nullptr;
    QTimer *m_watchdog = nullptr;

    QString m_outputPath;
    qint64 m_inMs = 0;
    qint64 m_outMs = 0;
    quint64 m_generation = 0;
    int m_branches = -1;         // -1 until decodebin has exposed every stream
    int m_readyBranches = 0;
    bool m_seeked = false;
    qint64 m_lastPtsMs = -1;

    mutable std::optional<bool> m_available;
};

} // namespace Editor::Video

#endif // EDITOR_VIDEO_GSTVIDEOEXPORTER_H
