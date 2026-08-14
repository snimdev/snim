#ifndef SCREEN_FROZENFRAMEGRABBER_H
#define SCREEN_FROZENFRAMEGRABBER_H

#include <QObject>
#include <QPixmap>
#include <QRect>
#include <QString>
#include <functional>

namespace Screen {

class FrameSourceChain;

/**
 * Freezes every screen into one virtual-desktop pixmap so a selection overlay has a
 * frozen frame to draw on.
 *
 * Outside a Wayland session the frame comes from QScreen::grabWindow and the callback
 * runs synchronously, inside grab(). In one, grabWindow returns black even through
 * XWayland, so a FrameSourceChain walks the sources the screenshot strategy would pick
 * (KWin, ScreenCast, screencopy), then the Screenshot portal, until one covers the whole
 * desktop; the callback may then run later. A failed or cancelled grab calls back with a
 * null pixmap and an empty geometry, and lastError() says why.
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

    // Whether the last grab failed because the user dismissed the system's dialog.
    [[nodiscard]] bool wasCancelled() const { return m_cancelled; }

    // The failure text for a desktop named like XDG_CURRENT_DESKTOP.
    [[nodiscard]] static QString failureMessage(const QString &currentDesktop,
                                                const QString &reason, bool cancelled);

private:
    void finish(const QPixmap &frozen, const QRect &virtualGeometry);
    void fail(const QString &reason, bool cancelled);

    FrameSourceChain *m_chain;
    Done m_done;             // non-null only while an asynchronous grab is in flight
    QString m_lastError;
    bool m_cancelled = false;
};

} // namespace Screen

#endif // SCREEN_FROZENFRAMEGRABBER_H
