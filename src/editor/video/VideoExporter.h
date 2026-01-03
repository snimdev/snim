#ifndef EDITOR_VIDEO_VIDEOEXPORTER_H
#define EDITOR_VIDEO_VIDEOEXPORTER_H

#include "editor/video/GifParams.h"

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

    // Asynchronously encode [inMs, outMs] of input as an animated GIF at output
    // (overwriting it), per `params`. Same finished()/failed() contract as trim();
    // emits progress() per frame. The stub reports it as unavailable.
    virtual void toGif(const QString &input, const QString &output,
                       qint64 inMs, qint64 outMs, const GifParams &params) = 0;

    virtual void cancel() {}                       // best-effort
    [[nodiscard]] virtual bool isAvailable() const = 0;

    // Platform pick, mirroring RecordingFactory's stub-is-universal layout.
    static std::unique_ptr<VideoExporter> create(QObject *parent = nullptr);

signals:
    void finished(const QString &outputPath);
    void failed(const QString &error);
    void progress(int done, int total);           // GIF encode, per frame
};

} // namespace Editor::Video

#endif // EDITOR_VIDEO_VIDEOEXPORTER_H
