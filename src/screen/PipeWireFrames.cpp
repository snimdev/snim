#include "screen/PipeWireFrames.h"

#include "core/DynamicModule.h"

namespace Screen::PipeWireFrames {

namespace {

using GrabFunction = bool (*)(int, const quint32 *, int, int, QImage *, QString *);

constexpr Core::DynamicModule kGrabber{kModuleBaseName, kModuleOverride, kGrabEntryPoint};

} // namespace

bool canGrab()
{
    return kGrabber.resolve() != nullptr;
}

bool grab(int pipewireFd, const QList<quint32> &nodeIds, int timeoutMs, QList<QImage> *frames,
          QString *error)
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

} // namespace Screen::PipeWireFrames
