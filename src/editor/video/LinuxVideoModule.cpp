#include "editor/video/LinuxVideoModule.h"

#include "core/DynamicModule.h"

namespace Editor::Video::LinuxVideoModule {

namespace {

using CreateFunction = VideoExporter *(*)(QObject *);

constexpr Core::DynamicModule kModule{kBaseName, "SNIM_VIDEO_MODULE", kEntryPoint};

} // namespace

VideoExporter *create(QObject *parent)
{
    const QFunctionPointer entry = kModule.resolve();
    if (!entry)
        return nullptr;
    return reinterpret_cast<CreateFunction>(entry)(parent);
}

} // namespace Editor::Video::LinuxVideoModule
