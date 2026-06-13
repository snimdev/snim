#include "recording/strategies/LinuxRecorderModule.h"

#include "core/DynamicModule.h"

#include <memory>

namespace Recording::LinuxRecorderModule {

namespace {

using CreateFunction = RecordingStrategy *(*)(QObject *);
using GrabFunction = bool (*)(int, const quint32 *, int, int, QImage *, QString *);
using MissingFunction = void (*)(QStringList *);

constexpr Core::DynamicModule kModule{kBaseName, "SNIM_RECORDER_MODULE", kEntryPoint};
constexpr Core::DynamicModule kGrabber{kBaseName, "SNIM_RECORDER_MODULE", kGrabEntryPoint};
constexpr Core::DynamicModule kMissing{kBaseName, "SNIM_RECORDER_MODULE", kMissingEntryPoint};

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
    if (!entry) {
        return {QStringLiteral("the recorder module or the GStreamer it loads "
                               "(libgstreamer1.0-0 and libgstreamer-plugins-base1.0-0; "
                               "Fedora: gstreamer1 and gstreamer1-plugins-base)")};
    }
    QStringList pieces;
    reinterpret_cast<MissingFunction>(entry)(&pieces);
    return pieces;
}

bool canGrabFrames()
{
    return kGrabber.resolve() != nullptr;
}

bool grabFrames(int pipewireFd, const QList<quint32> &nodeIds, int timeoutMs,
                QList<QImage> *frames, QString *error)
{
    const QFunctionPointer entry = kGrabber.resolve();
    if (!entry) {
        *error = QStringLiteral("The recorder module is not available");
        return false;
    }
    frames->resize(nodeIds.size());
    return reinterpret_cast<GrabFunction>(entry)(pipewireFd, nodeIds.constData(),
                                                 int(nodeIds.size()), timeoutMs,
                                                 frames->data(), error);
}

} // namespace Recording::LinuxRecorderModule
