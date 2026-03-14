#ifndef EDITOR_VIDEO_VIDEOEXPORTER_H
#define EDITOR_VIDEO_VIDEOEXPORTER_H

#include <QObject>
#include <QString>
#include <memory>

namespace Editor::Video {

/**
 * Platform seam (Strategy) for cutting a recording down to a time range. The macOS
 * implementation remuxes with AVAssetExportSession (no re-encode); Linux re-encodes
 * through GStreamer in a dlopened module; anything else, or a Linux host missing the
 * plugins, gets a stub that reports trimming as unavailable, since untrimmed saves
 * are a plain file move and never touch this interface.
 */
class VideoExporter : public QObject
{
    Q_OBJECT

public:
    explicit VideoExporter(QObject *parent = nullptr) : QObject(parent) {}
    ~VideoExporter() override;

    // Asynchronously write [inMs, outMs] of input to output (overwriting it).
    // Exactly one of finished()/failed() fires later, on this object's thread.
    virtual void trim(const QString &input, const QString &output,
                      qint64 inMs, qint64 outMs) = 0;

    // Best-effort stop of a running trim. Silent, so no finished()/failed() follows.
    virtual void cancel() {}

    [[nodiscard]] virtual bool isAvailable() const = 0;

    // Platform pick, mirroring RecordingFactory's stub-is-universal layout.
    static std::unique_ptr<VideoExporter> create(QObject *parent = nullptr);

signals:
    void finished(const QString &outputPath);
    void failed(const QString &error);
    void progress(int done, int total);           // ms of the trimmed range
};

} // namespace Editor::Video

#endif // EDITOR_VIDEO_VIDEOEXPORTER_H
