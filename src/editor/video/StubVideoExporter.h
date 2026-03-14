#ifndef EDITOR_VIDEO_STUBVIDEOEXPORTER_H
#define EDITOR_VIDEO_STUBVIDEOEXPORTER_H

#include "editor/video/VideoExporter.h"

namespace Editor::Video {

/**
 * Universal fallback: trimming is reported as unavailable, asynchronously (to
 * match the real exporter's deferred completion). Untrimmed saves never get here.
 */
class StubVideoExporter : public VideoExporter
{
    Q_OBJECT

public:
    explicit StubVideoExporter(QObject *parent = nullptr) : VideoExporter(parent) {}

    void trim(const QString &input, const QString &output,
              qint64 inMs, qint64 outMs) override;
    [[nodiscard]] bool isAvailable() const override { return false; }
};

} // namespace Editor::Video

#endif // EDITOR_VIDEO_STUBVIDEOEXPORTER_H
