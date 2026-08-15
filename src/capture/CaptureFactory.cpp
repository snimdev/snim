#include "CaptureFactory.h"
#include "screen/sources/StrategySelection.h"
#include "strategies/FrameSourceCaptureStrategy.h"
#include "strategies/NativeCaptureStrategy.h"
#ifdef Q_OS_LINUX
#include "strategies/KWinCaptureStrategy.h"
#endif
#include <QtGlobal>
#include <QDebug>
#include <memory>

namespace Capture {

std::unique_ptr<CaptureStrategy> CaptureFactory::createStrategy(StrategyType type, QObject *parent)
{
    using Screen::FrameSourceFactory;
    if (type == StrategyType::Auto)
        type = FrameSourceFactory::defaultType();

    // A type this session does not offer hands over to the next in its chain, the last to Qt's grab.
    QList<StrategyType> chain = Screen::StrategySelection::frameSourceChain(type);
    while (!chain.isEmpty() && !FrameSourceFactory::isAvailable(chain.constFirst())) {
        const StrategyType missing = chain.takeFirst();
        qWarning().noquote() << FrameSourceFactory::typeName(missing)
                             << "capture is not available, falling back to"
                             << FrameSourceFactory::typeName(chain.value(0, StrategyType::Native));
    }

    std::unique_ptr<CaptureStrategy> strategy;
    if (chain.isEmpty() || chain.constFirst() == StrategyType::Native)
        strategy = std::make_unique<NativeCaptureStrategy>(parent);
#ifdef Q_OS_LINUX
    // KWin answers a refusal with its own interactive pick, never with the rest of the chain.
    else if (chain.constFirst() == StrategyType::KWin)
        strategy = std::make_unique<KWinCaptureStrategy>(parent);
#endif
    else
        strategy = std::make_unique<FrameSourceCaptureStrategy>(chain, parent);
    qDebug() << "Created" << strategy->name() << "capture strategy";
    return strategy;
}

} // namespace Capture
