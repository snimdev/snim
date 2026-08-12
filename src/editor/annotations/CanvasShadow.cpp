#include "CanvasShadow.h"
#include <QImage>
#include <QPainter>
#include <QTransform>
#include <QVector>
#include <cmath>

namespace Editor {

namespace {
// Logical pixels: a 16 px CSS-style blur (sigma 8) dropped 4 px.
constexpr qreal kSigma = 8.0;
constexpr qreal kOffsetY = 4.0;
constexpr qreal kLightAlpha = 0.24;
constexpr qreal kDarkAlpha = 0.5;
const QColor kDarkEdge(255, 255, 255, 28);

// Share of pixel [i, i + 1) covered by the span [a, b) after a gaussian blur.
qreal blurredSpan(int i, qreal a, qreal b, qreal sigma)
{
    const qreal c = i + 0.5;
    const qreal k = 1.0 / (sigma * std::sqrt(2.0));
    return 0.5 * (std::erf((b - c) * k) - std::erf((a - c) * k));
}
} // namespace

QList<CanvasShadow::Slice> CanvasShadow::slices(const QRect &canvas, int margin)
{
    const int m = margin;
    const int far = 3 * m + 1;   // first tile row/column past the canvas
    const int mid = 2 * m;       // the tile's middle row/column
    const int l = qMin(m, canvas.width() / 2);
    const int r = qMin(m, canvas.width() - l);
    const int t = qMin(m, canvas.height() / 2);
    const int b = qMin(m, canvas.height() - t);
    const int x0 = canvas.x(), y0 = canvas.y();
    const int x1 = x0 + canvas.width(), y1 = y0 + canvas.height();

    QList<Slice> out{
        {QRect(x0 - m, y0 - m, m + l, m + t), QRect(0, 0, m + l, m + t)},
        {QRect(x1 - r, y0 - m, r + m, m + t), QRect(far - r, 0, r + m, m + t)},
        {QRect(x0 - m, y1 - b, m + l, b + m), QRect(0, far - b, m + l, b + m)},
        {QRect(x1 - r, y1 - b, r + m, b + m), QRect(far - r, far - b, r + m, b + m)},
    };
    const int w = canvas.width() - l - r;
    const int h = canvas.height() - t - b;
    if (w > 0) {
        out.append({QRect(x0 + l, y0 - m, w, m), QRect(mid, 0, 1, m)});
        out.append({QRect(x0 + l, y1, w, m), QRect(mid, far, 1, m)});
    }
    if (h > 0) {
        out.append({QRect(x0 - m, y0 + t, m, h), QRect(0, mid, m, 1)});
        out.append({QRect(x1, y0 + t, m, h), QRect(far, mid, m, 1)});
    }
    return out;
}

void CanvasShadow::rebuild(qreal dpr)
{
    const qreal sigma = kSigma * dpr;
    const qreal dy = kOffsetY * dpr;
    m_margin = int(std::ceil(3 * sigma + dy));
    const int side = 4 * m_margin + 1;
    const int lo = m_margin, hi = side - m_margin;   // the canvas, [lo, hi) on both axes

    // A blurred rectangle is separable: coverage(x, y) = gx(x) * gy(y).
    QVector<qreal> gx(side), gy(side);
    for (int i = 0; i < side; ++i) {
        gx[i] = blurredSpan(i, lo, hi, sigma);
        gy[i] = blurredSpan(i, lo + dy, hi + dy, sigma);
    }
    const qreal alpha = 255 * (m_dark ? kDarkAlpha : kLightAlpha);
    QImage img(side, side, QImage::Format_ARGB32_Premultiplied);
    for (int y = 0; y < side; ++y) {
        auto *line = reinterpret_cast<QRgb *>(img.scanLine(y));
        const bool rowInside = y >= lo && y < hi;
        for (int x = 0; x < side; ++x) {
            const bool inside = rowInside && x >= lo && x < hi;
            line[x] = inside ? 0 : qRgba(0, 0, 0, qRound(alpha * gx[x] * gy[y]));
        }
    }

    if (m_dark) {
        // Black barely shows on the dark background, so add a faint light edge.
        QPainter p(&img);
        const int e = qMax(1, qRound(dpr));
        p.fillRect(QRect(lo - e, lo - e, hi - lo + 2 * e, e), kDarkEdge);
        p.fillRect(QRect(lo - e, hi, hi - lo + 2 * e, e), kDarkEdge);
        p.fillRect(QRect(lo - e, lo, e, hi - lo), kDarkEdge);
        p.fillRect(QRect(hi, lo, e, hi - lo), kDarkEdge);
    }

    m_tile = QPixmap::fromImage(img);
    m_tileDpr = dpr;
    m_tileDark = m_dark;
}

void CanvasShadow::paint(QPainter *painter, const QRectF &canvas, const QRectF &exposed)
{
    // Work in device pixels so the tile lands 1:1, whatever the zoom.
    const qreal dpr = painter->device()->devicePixelRatio();
    const QTransform toDevice = painter->worldTransform() * QTransform::fromScale(dpr, dpr);
    const QRectF c = toDevice.mapRect(canvas);
    const QRect target(QPoint(qRound(c.left()), qRound(c.top())),
                       QPoint(qRound(c.right()) - 1, qRound(c.bottom()) - 1));
    if (target.isEmpty())
        return;
    if (m_tile.isNull() || m_tileDpr != dpr || m_tileDark != m_dark)
        rebuild(dpr);
    const QRect visible = toDevice.mapRect(exposed).toAlignedRect();

    painter->save();
    painter->setWorldTransform(QTransform::fromScale(1 / dpr, 1 / dpr));
    painter->setRenderHint(QPainter::Antialiasing, false);
    painter->setRenderHint(QPainter::SmoothPixmapTransform, false);
    for (const Slice &s : slices(target, m_margin))
        if (s.target.intersects(visible))
            painter->drawPixmap(s.target, m_tile, s.source);
    painter->restore();
}

} // namespace Editor
