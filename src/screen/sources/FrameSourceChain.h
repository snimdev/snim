#ifndef SCREEN_FRAMESOURCECHAIN_H
#define SCREEN_FRAMESOURCECHAIN_H

#include "FrameSourceFactory.h"

#include <QElapsedTimer>
#include <QList>
#include <QObject>
#include <QPixmap>
#include <QRect>
#include <QString>

#include <functional>
#include <memory>

namespace Screen {

class DesktopFrameSource;

/**
 * Chain of Responsibility over the full-desktop frame sources: each type in turn gets one
 * grab until one delivers. A type the factory does not offer, or a source that fails, passes
 * on to the next; a cancel ends the walk, so nothing else asks the user. Sources are made
 * per grab, so one that stops offering itself after a failure is skipped from then on.
 * Shared by the screenshot strategy and the frozen frame.
 */
class FrameSourceChain : public QObject
{
    Q_OBJECT

public:
    // Null for a type this desktop does not offer.
    using Factory = std::function<std::unique_ptr<DesktopFrameSource>(SourceType, QObject *)>;
    // The reason a delivered frame is turned down, or empty to take it.
    using FrameCheck = std::function<QString(const QPixmap &, const QRect &, SourceType)>;

    // label names the walk in the log.
    explicit FrameSourceChain(const QString &label, Factory factory = &FrameSourceFactory::create,
                              QObject *parent = nullptr);
    ~FrameSourceChain() override;

    // A turned-down frame counts as that source failing.
    void setFrameCheck(FrameCheck check) { m_check = std::move(check); }

    // Ends in one frameReady or failed, maybe before it returns; ignored while one is in flight.
    void grab(const QList<SourceType> &types);

    // Whether the walk that just failed ended on a type the factory did not offer.
    [[nodiscard]] bool endedOnMissingSource() const { return m_endedOnMissing; }

signals:
    void frameReady(const QPixmap &frame, const QRect &virtualGeometry);
    // reason: the last source's, or what was missing when none could be asked.
    void failed(const QString &reason, bool cancelled);

private:
    void tryNext();
    void sourceReady(const QPixmap &frame, const QRect &virtualGeometry);
    void sourceFailed(const QString &reason, bool cancelled);
    void dropSource();

    QString m_label;
    Factory m_factory;
    FrameCheck m_check;
    QList<SourceType> m_pending;
    DesktopFrameSource *m_source = nullptr;
    SourceType m_type = SourceType::Auto;
    QString m_lastReason;
    QElapsedTimer m_clock;
    bool m_busy = false;
    bool m_endedOnMissing = false;
};

} // namespace Screen

#endif // SCREEN_FRAMESOURCECHAIN_H
