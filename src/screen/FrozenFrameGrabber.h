#ifndef SCREEN_FROZENFRAMEGRABBER_H
#define SCREEN_FROZENFRAMEGRABBER_H

#include <QObject>
#include <QPixmap>
#include <QRect>
#include <QString>
#include <QVariantMap>
#include <functional>

namespace Screen {

/**
 * Freezes every screen into one virtual-desktop pixmap so a selection overlay has a
 * frozen frame to draw on.
 *
 * Everywhere but Wayland the frame comes from QScreen::grabWindow and the callback
 * runs synchronously, inside grab(). On Wayland grabWindow only ever returns black,
 * so the frame comes from a one-shot org.freedesktop.portal.Screenshot request and
 * the callback runs later, from the portal Response. A failed or cancelled grab
 * calls back with a null pixmap and an empty geometry.
 */
class FrozenFrameGrabber : public QObject
{
    Q_OBJECT

public:
    using Done = std::function<void(QPixmap frozen, QRect virtualGeometry)>;

    explicit FrozenFrameGrabber(QObject *parent = nullptr);

    void grab(Done done);

#ifdef Q_OS_LINUX
private slots:
    void handlePortalResponse(uint status, QVariantMap results);
#endif

private:
    static QPixmap grabAllScreens(QRect &virtualGeometryOut);
#ifdef Q_OS_LINUX
    bool requestPortalFrame();
    void disconnectPortalResponse();

    QString m_requestPath;   // portal Request object we are subscribed to
#endif

    Done m_done;             // non-null only while a portal request is in flight
};

} // namespace Screen

#endif // SCREEN_FROZENFRAMEGRABBER_H
