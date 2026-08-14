#include "screen/sources/FrameSourceChain.h"

#include "screen/sources/DesktopFrameSource.h"

#include <QDebug>
#include <QStringList>

namespace Screen {

FrameSourceChain::FrameSourceChain(const QString &label, Factory factory, QObject *parent)
    : QObject(parent)
    , m_label(label + QLatin1Char(':'))
    , m_factory(std::move(factory))
{
}

FrameSourceChain::~FrameSourceChain() = default;

void FrameSourceChain::grab(const QList<SourceType> &types)
{
    if (m_busy) {
        qDebug().noquote() << m_label << "a grab is already in flight";
        return;
    }
    m_busy = true;
    m_pending = types;
    m_lastReason.clear();
    m_endedOnMissing = false;
    m_clock.start();

    QStringList names;
    for (const SourceType type : types)
        names.append(FrameSourceFactory::typeName(type));
    qInfo().noquote() << m_label << "trying" << names.join(QStringLiteral(", then "));
    tryNext();
}

void FrameSourceChain::tryNext()
{
    while (!m_pending.isEmpty()) {
        const SourceType type = m_pending.takeFirst();
        std::unique_ptr<DesktopFrameSource> source = m_factory(type, this);
        if (!source) {
            qInfo().noquote() << m_label << FrameSourceFactory::typeName(type)
                              << "is not available here";
            if (m_lastReason.isEmpty())
                m_lastReason = tr("%1 is not available").arg(FrameSourceFactory::typeName(type));
            m_endedOnMissing = true;
            continue;
        }

        m_endedOnMissing = false;
        m_type = type;
        m_source = source.release();
        connect(m_source, &DesktopFrameSource::frameReady, this, &FrameSourceChain::sourceReady);
        connect(m_source, &DesktopFrameSource::frameFailed, this, &FrameSourceChain::sourceFailed);
        qInfo().noquote() << m_label << "asking" << FrameSourceFactory::typeName(type);
        m_source->grab();
        return;
    }
    m_busy = false;
    emit failed(m_lastReason, false);
}

void FrameSourceChain::dropSource()
{
    if (!m_source)
        return;
    // It may still be inside its own signal: disconnect now, delete later.
    m_source->disconnect(this);
    m_source->deleteLater();
    m_source = nullptr;
}

void FrameSourceChain::sourceReady(const QPixmap &frame, const QRect &virtualGeometry)
{
    dropSource();
    if (m_check) {
        if (const QString refusal = m_check(frame, virtualGeometry, m_type); !refusal.isEmpty()) {
            sourceFailed(refusal, false);
            return;
        }
    }
    qInfo().noquote() << m_label << FrameSourceFactory::typeName(m_type) << "gave" << frame.width()
                      << "x" << frame.height() << "pixels at DPR" << frame.devicePixelRatio()
                      << "over" << virtualGeometry << "in" << m_clock.elapsed() << "ms";
    m_pending.clear();
    m_busy = false;
    emit frameReady(frame, virtualGeometry);
}

void FrameSourceChain::sourceFailed(const QString &reason, bool cancelled)
{
    dropSource();
    qInfo().noquote() << m_label << FrameSourceFactory::typeName(m_type) << "failed:" << reason;
    m_lastReason = reason;
    if (cancelled) {
        m_pending.clear();
        m_busy = false;
        emit failed(reason, true);
        return;
    }
    tryNext();
}

} // namespace Screen
