#ifndef SCREEN_DESKTOPFRAMESOURCE_H
#define SCREEN_DESKTOPFRAMESOURCE_H

#include <QGuiApplication>
#include <QList>
#include <QObject>
#include <QPixmap>
#include <QRect>
#include <QScreen>
#include <QString>
#include <QtMath>

#include <algorithm>

namespace Screen {

/**
 * One frame of the whole virtual desktop, taken without any UI of Snim's own: the seam
 * shared by the capture strategies' full-desktop paths and the recording selector's
 * frozen frame. A source may still let the system ask for consent. grab() ends in
 * exactly one frameReady or frameFailed, possibly before it returns; a grab() while one
 * is in flight is ignored, and the pending one still answers.
 */
class DesktopFrameSource : public QObject
{
    Q_OBJECT

public:
    using QObject::QObject;
    ~DesktopFrameSource() override = default;

    virtual void grab() = 0;

signals:
    // pixmap carries its DPR; virtualGeometry is the logical desktop it covers. A pixmap
    // that does not span virtualGeometry (frameCoversGeometry) is a partial pick made in
    // a system dialog, such as a region or one monitor: take it as it is.
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

// Whether each screen lies inside one of the covered rects, give or take rounding.
inline bool coversEveryScreen(const QList<QRect> &covered, const QList<QRect> &screens)
{
    for (const QRect &screen : screens) {
        const int slack = qMax(2, qRound(qMax(screen.width(), screen.height()) * 0.01));
        const QRect inner = screen.adjusted(slack, slack, -slack, -slack);
        const bool inside = std::any_of(covered.cbegin(), covered.cend(),
                                        [&inner](const QRect &rect) { return rect.contains(inner); });
        if (!inside)
            return false;
    }
    return true;
}

} // namespace Screen

#endif // SCREEN_DESKTOPFRAMESOURCE_H
