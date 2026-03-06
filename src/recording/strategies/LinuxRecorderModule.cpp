#include "recording/strategies/LinuxRecorderModule.h"

#include "core/DynamicModule.h"

#include <memory>

namespace Recording::LinuxRecorderModule {

namespace {

using CreateFunction = RecordingStrategy *(*)(QObject *);

constexpr Core::DynamicModule kModule{kBaseName, "SNIM_RECORDER_MODULE", kEntryPoint};

} // namespace

QStringList candidatePaths(const QString &binDir)
{
    return kModule.candidatePaths(binDir);
}

RecordingStrategy *create(QObject *parent)
{
    const QFunctionPointer entry = kModule.resolve();
    if (!entry)
        return nullptr;
    return reinterpret_cast<CreateFunction>(entry)(parent);
}

bool isAvailable()
{
    const std::unique_ptr<RecordingStrategy> probe(create());
    return probe && probe->isAvailable();
}

} // namespace Recording::LinuxRecorderModule
