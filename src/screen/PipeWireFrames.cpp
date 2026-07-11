#include "screen/PipeWireFrames.h"

#include "core/DynamicModule.h"

#include <QDebug>

#include <utility>

namespace Screen::PipeWireFrames {

namespace {

using GrabFunction = bool (*)(int, const quint32 *, int, int, QImage *, QString *);
using CanGrabFunction = bool (*)(QString *);

constexpr Core::DynamicModule kGrabber{kModuleBaseName, kModuleOverride, kGrabEntryPoint};
constexpr Core::DynamicModule kChecker{kModuleBaseName, kModuleOverride, kCanGrabEntryPoint};

} // namespace

bool canGrab(QString *missing)
{
    const QFunctionPointer check = kChecker.resolve();
    if (!kGrabber.resolve() || !check) {
        if (missing)
            *missing = QStringLiteral("the recorder module (libsnim-recorder-linux)");
        return false;
    }
    // Asking loads GStreamer, and its plugins do not change while the app runs.
    static const std::pair<bool, QString> answer = [check] {
        QString absent;
        const bool ok = reinterpret_cast<CanGrabFunction>(check)(&absent);
        if (!ok)
            qInfo().noquote() << "ScreenCast capture needs GStreamer's" << absent;
        return std::pair(ok, absent);
    }();
    if (missing)
        *missing = answer.second;
    return answer.first;
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
