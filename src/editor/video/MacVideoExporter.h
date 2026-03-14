#ifndef EDITOR_VIDEO_MACVIDEOEXPORTER_H
#define EDITOR_VIDEO_MACVIDEOEXPORTER_H

#include "editor/video/VideoExporter.h"
#include <memory>

namespace Editor::Video {

/**
 * macOS trim exporter built on AVAssetExportSession. Uses the passthrough preset:
 * a remux of the selected time range with no re-encode (fast and lossless; the cut
 * snaps to safe sample boundaries), falling back to a re-encoding preset when
 * passthrough is unsupported for the asset. The header stays pure C++; all
 * AVFoundation state lives behind the pimpl in the .mm (compiled with ARC).
 */
class MacVideoExporter : public VideoExporter
{
    Q_OBJECT

public:
    explicit MacVideoExporter(QObject *parent = nullptr);
    ~MacVideoExporter() override;

    void trim(const QString &input, const QString &output,
              qint64 inMs, qint64 outMs) override;
    void cancel() override;
    [[nodiscard]] bool isAvailable() const override { return true; }

    // Backend hook: called on the Qt thread to end an export (success/cancel/failure).
    // Not for general use.
    void completeExport(bool ok, const QString &path, const QString &error);

    struct Impl;   // defined in the .mm (holds the AVAssetExportSession)

private:
    std::unique_ptr<Impl> d;
};

} // namespace Editor::Video

#endif // EDITOR_VIDEO_MACVIDEOEXPORTER_H
