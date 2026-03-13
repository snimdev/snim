#include "editor/video/StubVideoExporter.h"

#include <QMetaObject>

namespace Editor::Video {

void StubVideoExporter::trim(const QString &input, const QString &output,
                             qint64 inMs, qint64 outMs)
{
    Q_UNUSED(input); Q_UNUSED(output); Q_UNUSED(inMs); Q_UNUSED(outMs);
    // Deferred, like the real exporter: callers connect before the signal fires.
    QMetaObject::invokeMethod(this, [this] {
#ifdef Q_OS_LINUX
        emit failed(tr("Trimming is not available (missing GStreamer plugins)."));
#else
        emit failed(tr("Trimming is not supported on this platform."));
#endif
    }, Qt::QueuedConnection);
}

void StubVideoExporter::toGif(const QString &input, const QString &output,
                              qint64 inMs, qint64 outMs, const AnimationParams &params)
{
    Q_UNUSED(input); Q_UNUSED(output); Q_UNUSED(inMs); Q_UNUSED(outMs); Q_UNUSED(params);
    QMetaObject::invokeMethod(this, [this] {
        emit failed(tr("Exporting to GIF is not supported on this platform."));
    }, Qt::QueuedConnection);
}

} // namespace Editor::Video
