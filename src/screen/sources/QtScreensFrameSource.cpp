#include "QtScreensFrameSource.h"

#include <QList>
#include <QPainter>
#include <algorithm>

namespace Screen {

QPixmap QtScreensFrameSource::grabNow(QRect *virtualGeometryOut)
{
    const QList<QScreen*> screens = QGuiApplication::screens();
    if (screens.isEmpty())
        return {};

    const QRect virtualDesktop = qtVirtualDesktop();
    if (virtualGeometryOut)
        *virtualGeometryOut = virtualDesktop;

    qreal dpr = 1.0;
    for (QScreen *s : screens)
        dpr = std::max(dpr, s->devicePixelRatio());

    QPixmap full(virtualDesktop.size() * dpr);
    full.setDevicePixelRatio(dpr);
    full.fill(Qt::black);
    QPainter painter(&full);
    for (QScreen *s : screens) {
        const QRect geo = s->geometry();
        const QPixmap shot = s->grabWindow(0);
        const QRect destLogical(geo.topLeft() - virtualDesktop.topLeft(), geo.size());
        painter.drawPixmap(destLogical, shot, shot.rect());
    }
    painter.end();
    return full;
}

void QtScreensFrameSource::grab()
{
    QRect virtualGeometry;
    const QPixmap frame = grabNow(&virtualGeometry);
    if (frame.isNull())
        emit frameFailed(QStringLiteral("no screens to grab"), false);
    else
        emit frameReady(frame, virtualGeometry);
}

} // namespace Screen
