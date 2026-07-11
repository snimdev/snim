#ifndef SCREEN_FROZENFRAMEGRABBER_H
#define SCREEN_FROZENFRAMEGRABBER_H

#include <QElapsedTimer>
#include <QList>
#include <QObject>
#include <QPixmap>
#include <QRect>
#include <QString>
#include <functional>

namespace Screen {

class DesktopFrameSource;

/**
 * Freezes every screen into one virtual-desktop pixmap so a selection overlay has a
 * frozen frame to draw on.
 *
 * Outside a Wayland session the frame comes from QScreen::grabWindow and the callback
 * runs synchronously, inside grab(). In one, grabWindow returns black even through
 * XWayland, so the frame comes from the same frame sources the screenshot strategy
 * would pick (KWin, ScreenCast, screencopy), then the Screenshot portal, tried in order
 * until one delivers; the callback may then run later. A failed or cancelled grab calls
 * back with a null pixmap and an empty geometry, and lastError() says why.
 */
class FrozenFrameGrabber : public QObject
{
    Q_OBJECT

public:
    using Done = std::function<void(QPixmap frozen, QRect virtualGeometry)>;

    explicit FrozenFrameGrabber(QObject *parent = nullptr);
    ~FrozenFrameGrabber() override;

    void grab(Done done);

    // Why the last grab failed, worded for the user; empty after a success.
    [[nodiscard]] QString lastError() const { return m_lastError; }

    // The failure text for a desktop named like XDG_CURRENT_DESKTOP.
    [[nodiscard]] static QString failureMessage(const QString &currentDesktop,
                                                const QString &reason, bool cancelled);

private:
    void finish(const QPixmap &frozen, const QRect &virtualGeometry);
    void fail(const QString &reason, bool cancelled);
#ifdef Q_OS_LINUX
    void tryNextSource();
    void sourceReady(const QPixmap &frame, const QRect &virtualGeometry);
    void sourceFailed(const QString &reason, bool cancelled);
    void dropSource();

    QList<int> m_chain;   // SourceType values still to try
    DesktopFrameSource *m_source = nullptr;
    QString m_sourceName;
    QString m_lastReason;
    QElapsedTimer m_clock;
#endif

    Done m_done;             // non-null only while an asynchronous grab is in flight
    QString m_lastError;
};

} // namespace Screen

#endif // SCREEN_FROZENFRAMEGRABBER_H
