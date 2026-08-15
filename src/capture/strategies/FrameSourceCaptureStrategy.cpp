#include "FrameSourceCaptureStrategy.h"

#include "screen/sources/DesktopFrameSource.h"
#include "screen/sources/FrameSourceFactory.h"

#include <QDebug>
#include <QStringList>

namespace Capture {

namespace {

using Screen::FrameSourceFactory;
using Screen::SourceType;

// The portal is asked only when it answers, so a walk ending without it means no method at all.
std::unique_ptr<Screen::DesktopFrameSource> createOffered(SourceType type, QObject *parent)
{
    return FrameSourceFactory::isAvailable(type) ? FrameSourceFactory::create(type, parent) : nullptr;
}

} // namespace

FrameSourceCaptureStrategy::FrameSourceCaptureStrategy(const QList<SourceType> &types,
                                                       QObject *parent)
    : FrameSourceCaptureStrategy(types, &createOffered, parent)
{
}

FrameSourceCaptureStrategy::FrameSourceCaptureStrategy(const QList<SourceType> &types,
                                                       Screen::FrameSourceChain::Factory factory,
                                                       QObject *parent)
    : CaptureStrategy(parent)
    , m_types(types)
    , m_chain(new Screen::FrameSourceChain(QStringLiteral("Capture"), std::move(factory), this))
{
    connect(m_chain, &Screen::FrameSourceChain::frameReady, this,
            [this](const QPixmap &frame, const QRect &virtualGeometry) {
                deliverFrame(frame, virtualGeometry, m_showSelector);
            });
    connect(m_chain, &Screen::FrameSourceChain::failed, this, &FrameSourceCaptureStrategy::walkFailed);
    connect(m_chain, &Screen::FrameSourceChain::sourcePickerExpected,
            this, &FrameSourceCaptureStrategy::sourcePickerExpected);
}

void FrameSourceCaptureStrategy::captureFullScreen()
{
    grab(false);
}

void FrameSourceCaptureStrategy::captureArea()
{
    grab(true);
}

void FrameSourceCaptureStrategy::captureWindow()
{
    grab(true);
}

QString FrameSourceCaptureStrategy::name() const
{
    QStringList names;
    for (const SourceType type : m_types)
        names.append(FrameSourceFactory::typeName(type));
    return QStringLiteral("Frame sources (%1)").arg(names.join(QStringLiteral(", then ")));
}

void FrameSourceCaptureStrategy::grab(bool showSelector)
{
    if (m_chain->isBusy()) {
        qDebug() << "Capture already in progress";
        return;
    }
    m_showSelector = showSelector;
    m_chain->grab(m_types);
}

void FrameSourceCaptureStrategy::walkFailed(const QString &reason, bool cancelled)
{
    if (cancelled) {
        qInfo() << "Capture cancelled:" << reason;
        emit screenshotCancelled();
        return;
    }
    // Every chain ends at the Screenshot portal, so its failure is the one the user sees.
    if (m_chain->endedOnMissingSource())
        emit screenshotFailed(QStringLiteral(
            "No screenshot method available: the Screenshot portal needs xdg-desktop-portal "
            "and a backend for this desktop."));
    else
        emit screenshotFailed(QStringLiteral("Portal screenshot failed: %1").arg(reason));
}

} // namespace Capture
