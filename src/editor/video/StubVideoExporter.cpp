#include "editor/video/StubVideoExporter.h"

#ifdef Q_OS_MACOS
#include "editor/video/MacVideoExporter.h"
#endif

#include <QMetaObject>

namespace Editor::Video {

void StubVideoExporter::trim(const QString &input, const QString &output,
                             qint64 inMs, qint64 outMs)
{
    Q_UNUSED(input); Q_UNUSED(output); Q_UNUSED(inMs); Q_UNUSED(outMs);
    // Deferred, like the real exporter — callers connect before the signal fires.
    QMetaObject::invokeMethod(this, [this] {
        emit failed(tr("Trimming is not supported on this platform."));
    }, Qt::QueuedConnection);
}

std::unique_ptr<VideoExporter> VideoExporter::create(QObject *parent)
{
#ifdef Q_OS_MACOS
    return std::make_unique<MacVideoExporter>(parent);
#else
    return std::make_unique<StubVideoExporter>(parent);
#endif
}

} // namespace Editor::Video
