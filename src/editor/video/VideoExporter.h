#ifndef EDITOR_VIDEO_VIDEOEXPORTER_H
#define EDITOR_VIDEO_VIDEOEXPORTER_H

#include <QObject>
#include <QString>
#include <memory>

namespace Editor::Video {

/**
 * Platform seam for cutting a recording down to a time range. The macOS
 * implementation remuxes with AVAssetExportSession (no re-encode); other
 * platforms get a stub that reports trimming as unavailable — untrimmed saves
 * are a plain file move and never touch this interface.
 */
class VideoExporter : public QObject
{
    Q_OBJECT

public:
    explicit VideoExporter(QObject *parent = nullptr) : QObject(parent) {}

    // Asynchronously write [inMs, outMs] of input to output (overwriting it).
    // Exactly one of finished()/failed() fires later, on this object's thread.
    virtual void trim(const QString &input, const QString &output,
                      qint64 inMs, qint64 outMs) = 0;
    virtual void cancel() {}                       // best-effort
    [[nodiscard]] virtual bool isAvailable() const = 0;

    // Platform pick, mirroring RecordingFactory's stub-is-universal layout.
    static std::unique_ptr<VideoExporter> create(QObject *parent = nullptr);

signals:
    void finished(const QString &outputPath);
    void failed(const QString &error);
};

} // namespace Editor::Video

#endif // EDITOR_VIDEO_VIDEOEXPORTER_H
