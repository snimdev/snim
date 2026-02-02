#include "editor/video/VideoExporter.h"

#include "editor/video/StubVideoExporter.h"

#ifdef Q_OS_MACOS
#include "editor/video/MacVideoExporter.h"
#endif

namespace Editor::Video {

VideoExporter::~VideoExporter() = default;

std::unique_ptr<VideoExporter> VideoExporter::create(QObject *parent)
{
#ifdef Q_OS_MACOS
    return std::make_unique<MacVideoExporter>(parent);
#else
    return std::make_unique<StubVideoExporter>(parent);
#endif
}

} // namespace Editor::Video
