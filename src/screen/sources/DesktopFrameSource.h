#ifndef SCREEN_DESKTOPFRAMESOURCE_H
#define SCREEN_DESKTOPFRAMESOURCE_H

#include <QGuiApplication>
#include <QObject>
#include <QPixmap>
#include <QRect>
#include <QScreen>
#include <QString>
#include <QtMath>

namespace Screen {

/**
 * One frame of the whole virtual desktop, taken without any UI of Snim's own: the seam
 * shared by the capture strategies' full-desktop paths and the recording selector's
 * frozen frame. A source may still let the system ask for consent. grab() ends in
 * exactly one frameReady or frameFailed, possibly before it returns.
 */
class DesktopFrameSource : public QObject
{
    Q_OBJECT

public:
    using QObject::QObject;
    ~DesktopFrameSource() override = default;

    [[nodiscard]] virtual QString name() const = 0;
    virtual void grab() = 0;

signals:
    // pixmap carries its DPR; virtualGeometry is the logical desktop it covers.
    void frameReady(const QPixmap &pixmap, const QRect &virtualGeometry);
    // cancelled: the user dismissed a system prompt, so nothing else should ask again.
    void frameFailed(const QString &reason, bool cancelled);
};

// Whether Snim runs in a Wayland session, by the environment it was started with.
inline bool isWaylandSession()
{
    return qEnvironmentVariable("XDG_SESSION_TYPE") == QLatin1String("wayland")
           || !qEnvironmentVariableIsEmpty("WAYLAND_DISPLAY");
}

// The logical union of every screen Qt knows.
inline QRect qtVirtualDesktop()
{
    QRect desktop;
    for (const QScreen *screen : QGuiApplication::screens())
        desktop = desktop.united(screen->geometry());
    return desktop;
}

// Whether a frame's logical size spans its geometry, give or take rounding.
inline bool frameCoversGeometry(const QSize &pixels, qreal dpr, const QRect &geometry)
{
    if (pixels.isEmpty() || geometry.isEmpty() || dpr <= 0)
        return false;
    const auto close = [](qreal logical, int expected) {
        return qAbs(logical - expected) <= qMax(2.0, expected * 0.01);
    };
    return close(pixels.width() / dpr, geometry.width())
           && close(pixels.height() / dpr, geometry.height());
}

} // namespace Screen

#endif // SCREEN_DESKTOPFRAMESOURCE_H
