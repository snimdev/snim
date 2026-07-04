#include "CaptureFactory.h"
#include "strategies/NativeCaptureStrategy.h"
#ifdef Q_OS_LINUX
#include "strategies/KWinCaptureStrategy.h"
#include "strategies/WaylandCaptureStrategy.h"
#endif
#if defined(Q_OS_LINUX) && defined(SNIM_HAVE_LINUX_RECORDER)
#include "strategies/ScreencastCaptureStrategy.h"
#endif
#ifdef SNIM_HAVE_SCREENCOPY
#include "strategies/ScreencopyCaptureStrategy.h"
#endif
#include <QtGlobal>
#include <QDebug>
#include <memory>

namespace Capture {

namespace {

using Type = CaptureFactory::StrategyType;

// The strategy for exactly this type; null where it is not built.
std::unique_ptr<CaptureStrategy> makeStrategy(Type type, QObject *parent)
{
    switch (type) {
    case Type::KWin:
#ifdef Q_OS_LINUX
        return std::make_unique<KWinCaptureStrategy>(parent);
#else
        break;
#endif
    case Type::Portal:
#ifdef Q_OS_LINUX
        return std::make_unique<WaylandCaptureStrategy>(parent);
#else
        break;
#endif
    case Type::Screencast:
#if defined(Q_OS_LINUX) && defined(SNIM_HAVE_LINUX_RECORDER)
        return std::make_unique<ScreencastCaptureStrategy>(parent);
#else
        break;
#endif
    case Type::Screencopy:
#ifdef SNIM_HAVE_SCREENCOPY
        return std::make_unique<ScreencopyCaptureStrategy>(parent);
#else
        break;
#endif
    case Type::Native:
        return std::make_unique<NativeCaptureStrategy>(parent);
    case Type::Auto:
        break;
    }
    return nullptr;
}

// What takes over when a type is not available here; Native always is.
Type fallbackFor(Type type)
{
    switch (type) {
    case Type::KWin:
    case Type::Screencast:
    case Type::Screencopy:
        return Type::Portal;
    case Type::Portal:
    case Type::Native:
    case Type::Auto:
        break;
    }
    return Type::Native;
}

} // namespace

std::unique_ptr<CaptureStrategy> CaptureFactory::createStrategy(StrategyType type, QObject *parent)
{
    if (type == StrategyType::Auto)
        type = getDefaultStrategyType();

    for (;;) {
        auto strategy = makeStrategy(type, parent);
        if (strategy && (type == Type::Native || strategy->isAvailable())) {
            qDebug() << "Created" << strategy->name() << "capture strategy";
            return strategy;
        }
        const Type next = fallbackFor(type);
        qWarning().noquote() << Screen::FrameSourceFactory::typeName(type)
                             << "capture is not available, falling back to"
                             << Screen::FrameSourceFactory::typeName(next);
        type = next;
    }
}

CaptureFactory::StrategyType CaptureFactory::getDefaultStrategyType()
{
    return Screen::FrameSourceFactory::defaultType();
}

bool CaptureFactory::isStrategyAvailable(StrategyType type)
{
    if (type == StrategyType::Auto)
        return true;   // Auto always lands on a strategy, Native at worst
    const auto strategy = makeStrategy(type, nullptr);
    return strategy && strategy->isAvailable();
}

} // namespace Capture
