#ifndef EDITOR_VIDEO_FFMPEGVIDEOEXPORTER_H
#define EDITOR_VIDEO_FFMPEGVIDEOEXPORTER_H

#include "editor/video/VideoExporter.h"

#include <QFuture>
#include <QString>

#include <memory>

namespace Editor::Video {

/**
 * Trim exporter (a VideoExporter Strategy) on the statically linked FFmpeg layer. It
 * re-encodes through FfmpegTrimTranscoder on a worker thread, so the cut lands on the
 * exact frame, and reports back on this object's thread. cancel() waits for the worker
 * (it stops within a packet) and removes the partial output.
 */
class FfmpegVideoExporter : public VideoExporter
{
    Q_OBJECT

public:
    explicit FfmpegVideoExporter(QObject *parent = nullptr);
    ~FfmpegVideoExporter() override;

    void trim(const QString &input, const QString &output,
              qint64 inMs, qint64 outMs) override;
    void cancel() override;
    [[nodiscard]] bool isAvailable() const override;

private:
    struct Job;

    void failLater(const QString &error);
    void complete(quint64 generation, bool ok, const QString &error);

    std::shared_ptr<Job> m_job;
    QFuture<void> m_worker;
    quint64 m_generation = 0;
};

} // namespace Editor::Video

#endif // EDITOR_VIDEO_FFMPEGVIDEOEXPORTER_H
