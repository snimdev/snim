#include "editor/video/VideoExporter.h"

#include "editor/video/StubVideoExporter.h"

#ifdef Q_OS_MACOS
#include "editor/video/MacVideoExporter.h"
#endif
#ifdef SNIM_HAVE_LINUX_VIDEO_EXPORTER
#include "editor/video/LinuxVideoModule.h"
#endif

#include <QDebug>

namespace Editor::Video {

VideoExporter::~VideoExporter() = default;

std::unique_ptr<VideoExporter> VideoExporter::create(QObject *parent)
{
#ifdef Q_OS_MACOS
    return std::make_unique<MacVideoExporter>(parent);
#else
#ifdef SNIM_HAVE_LINUX_VIDEO_EXPORTER
    // The backend lives in a dlopened module, so a host without GStreamer (or without
    // the module) degrades here exactly like an unsupported platform.
    std::unique_ptr<VideoExporter> exporter(LinuxVideoModule::create(parent));
    if (exporter && exporter->isAvailable()) {
        qDebug() << "Created GStreamer trim exporter";
        return exporter;
    }
    qWarning() << "GStreamer trim exporter unavailable (needs the distribution's GStreamer, "
                  "an H.264 decoder and an H.264 encoder), using stub";
#endif
    return std::make_unique<StubVideoExporter>(parent);
#endif
}

} // namespace Editor::Video
