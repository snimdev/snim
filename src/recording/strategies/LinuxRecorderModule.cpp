#include "recording/strategies/LinuxRecorderModule.h"

#include "core/DynamicModule.h"

#include <memory>

namespace Recording::LinuxRecorderModule {

namespace {

using CreateFunction = RecordingStrategy *(*)(QObject *);
using MissingFunction = void (*)(QStringList *);
using CheckFunction = void (*)(QStringList *, QStringList *);

using Screen::PipeWireFrames::kModuleOverride;

constexpr Core::DynamicModule kModule{kBaseName, kModuleOverride, kEntryPoint};
constexpr Core::DynamicModule kMissing{kBaseName, kModuleOverride, kMissingEntryPoint};
constexpr Core::DynamicModule kCheck{kBaseName, kModuleOverride, kCheckEntryPoint};

QString moduleMissing()
{
    return QStringLiteral("the recorder module or the GStreamer it loads "
                          "(libgstreamer1.0-0 and libgstreamer-plugins-base1.0-0; "
                          "Fedora: gstreamer1 and gstreamer1-plugins-base)");
}

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

QStringList missingPieces()
{
    const QFunctionPointer entry = kMissing.resolve();
    if (!entry)
        return {moduleMissing()};
    QStringList pieces;
    reinterpret_cast<MissingFunction>(entry)(&pieces);
    return pieces;
}

ElementCheck checkElements()
{
    ElementCheck check;
    const QFunctionPointer entry = kCheck.resolve();
    if (!entry)
        check.missing << moduleMissing();
    else
        reinterpret_cast<CheckFunction>(entry)(&check.found, &check.missing);
    return check;
}

} // namespace Recording::LinuxRecorderModule
