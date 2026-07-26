#include "recording/RecordingFactory.h"
#include "recording/strategies/StubRecordingStrategy.h"
#ifdef SNIM_HAVE_MAC_RECORDER
#include "recording/strategies/MacRecordingStrategy.h"
#endif
#ifdef SNIM_HAVE_LINUX_RECORDER
#include "recording/strategies/LinuxRecorderModule.h"
#endif
#ifdef SNIM_HAVE_WIN_RECORDER
#include "recording/strategies/windows/WindowsRecordingStrategy.h"
#endif

#include <QtGlobal>
#include <QDebug>

namespace Recording {

std::unique_ptr<RecordingStrategy> RecordingFactory::createStrategy(QObject *parent)
{
#if defined(SNIM_HAVE_MAC_RECORDER)
    auto strategy = std::make_unique<MacRecordingStrategy>(parent);
    if (strategy->isAvailable()) {
        qDebug() << "Created ScreenCaptureKit recording strategy";
        return strategy;
    }
    qWarning() << "ScreenCaptureKit recorder unavailable (needs macOS 12.3+), using stub";
#elif defined(SNIM_HAVE_LINUX_RECORDER)
    // The backend lives in a dlopened module, so a host without GStreamer (or
    // without the module) degrades here exactly like an unavailable backend.
    std::unique_ptr<RecordingStrategy> strategy(LinuxRecorderModule::create(parent));
    if (strategy && strategy->isAvailable()) {
        qDebug().noquote() << "Created" << strategy->name() << "recording strategy";
        return strategy;
    }
    qWarning().noquote() << "GStreamer recorder unavailable, using stub. Missing:"
                         << LinuxRecorderModule::missingPieces().join(QStringLiteral("; "));
#elif defined(SNIM_HAVE_WIN_RECORDER)
    auto strategy = std::make_unique<WindowsRecordingStrategy>(parent);
    if (strategy->isAvailable()) {
        qDebug() << "Created Graphics Capture recording strategy";
        return strategy;
    }
    qWarning() << "Graphics Capture recorder unavailable (needs Windows 10 2004+), using stub";
#endif
    return std::make_unique<StubRecordingStrategy>(parent);
}

} // namespace Recording
