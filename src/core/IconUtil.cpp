#include "core/IconUtil.h"

#include <QFile>
#include <QByteArray>
#include <QPixmap>
#include <QPainter>
#include <QSvgRenderer>
#include <QScreen>
#include <QGuiApplication>

namespace Core {

QIcon themedSvgIcon(const QString &svgPath, const QColor &color, int logicalSize)
{
    QFile file(svgPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "themedSvgIcon: failed to open" << svgPath;
        return QIcon();
    }
    QString svg = QString::fromUtf8(file.readAll());
    file.close();
    svg.replace("currentColor", color.name());

    // Rasterise at the highest screen scale (Retina = 2x/3x) and tag the pixmap with
    // that DPR, so the icon is drawn at native resolution rather than a 1x bitmap the
    // OS upscales. QIcon downscales crisply for any lower-DPI screen.
    qreal dpr = 1.0;
    const auto screens = QGuiApplication::screens();
    for (const QScreen *screen : screens)
        dpr = qMax(dpr, screen->devicePixelRatio());

    QSvgRenderer renderer(svg.toUtf8());
    QPixmap pixmap(qRound(logicalSize * dpr), qRound(logicalSize * dpr));
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    renderer.render(&painter);
    painter.end();
    pixmap.setDevicePixelRatio(dpr);

    return QIcon(pixmap);
}

} // namespace Core
