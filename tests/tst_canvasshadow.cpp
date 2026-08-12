#include <QtTest>
#include <QImage>
#include <QPainter>
#include <QVector>

#include "editor/annotations/CanvasShadow.h"

using Editor::CanvasShadow;

// The editor's canvas shadow: nine-slice geometry and a fixed on-screen size.
class tst_CanvasShadow : public QObject
{
    Q_OBJECT

    // Signed distance outside [lo, hi), 0 within it.
    static int offset(int v, int lo, int hi)
    {
        return v < lo ? v - lo : (v >= hi ? v - hi + 1 : 0);
    }

    // Every pixel of the band around the canvas is covered once, and each covered pixel
    // takes the tile pixel at the same offset from the tile's canvas.
    static void verifyLayout(const QRect &canvas, int m)
    {
        const QRect band = canvas.adjusted(-m, -m, m, m);
        const int tileCanvasLo = m, tileCanvasHi = 3 * m + 1;
        QVector<int> hits(band.width() * band.height(), 0);
        for (const CanvasShadow::Slice &s : CanvasShadow::slices(canvas, m)) {
            QVERIFY(band.contains(s.target));
            QVERIFY(QRect(0, 0, 4 * m + 1, 4 * m + 1).contains(s.source));
            for (int y = s.target.top(); y <= s.target.bottom(); ++y) {
                for (int x = s.target.left(); x <= s.target.right(); ++x) {
                    const int sx = s.source.x() + (x - s.target.x()) * s.source.width() / s.target.width();
                    const int sy = s.source.y() + (y - s.target.y()) * s.source.height() / s.target.height();
                    QCOMPARE(offset(sx, tileCanvasLo, tileCanvasHi),
                             offset(x, canvas.left(), canvas.right() + 1));
                    QCOMPARE(offset(sy, tileCanvasLo, tileCanvasHi),
                             offset(y, canvas.top(), canvas.bottom() + 1));
                    ++hits[(y - band.y()) * band.width() + (x - band.x())];
                }
            }
        }
        for (int y = band.top(); y <= band.bottom(); ++y)
            for (int x = band.left(); x <= band.right(); ++x)
                if (!canvas.contains(x, y))
                    QCOMPARE(hits[(y - band.y()) * band.width() + (x - band.x())], 1);
    }

    static QImage paintedAt(qreal dpr, qreal zoom, bool dark)
    {
        QImage img(QSize(200, 160) * dpr, QImage::Format_ARGB32_Premultiplied);
        img.setDevicePixelRatio(dpr);
        img.fill(QColor(dark ? "#202124" : "#ececec"));
        CanvasShadow shadow;
        shadow.setDark(dark);
        QPainter p(&img);
        p.scale(zoom, zoom);   // the view's zoom; the canvas lands at (50, 40, 100, 60)
        shadow.paint(&p, QRectF(QPointF(50, 40) / zoom, QSizeF(100, 60) / zoom),
                     QRectF(0, 0, 200 / zoom, 160 / zoom));
        return img;
    }

private slots:
    void slicesCoverTheBandAroundALargeCanvas()
    {
        const QRect canvas(30, 20, 200, 120);
        QCOMPARE(CanvasShadow::slices(canvas, 10).size(), 8);
        verifyLayout(canvas, 10);
    }

    void cornersMeetHalfwayOnASmallCanvas()
    {
        QCOMPARE(CanvasShadow::slices(QRect(0, 0, 7, 5), 10).size(), 4);
        verifyLayout(QRect(0, 0, 7, 5), 10);
        verifyLayout(QRect(-3, 4, 1, 1), 10);
        verifyLayout(QRect(5, 5, 20, 400), 10);   // edges on one axis only
    }

    void paintsOutsideTheCanvasOnly()
    {
        const QImage img = paintedAt(1, 1, false);
        const QRgb bg = QColor("#ececec").rgb();
        QCOMPARE(img.pixel(100, 70), bg);   // inside the canvas: untouched
        QCOMPARE(img.pixel(5, 5), bg);      // beyond the shadow's reach
        QVERIFY(qGray(img.pixel(100, 101)) < qGray(img.pixel(100, 38)));   // dropped downwards
        QCOMPARE(img.pixel(48, 70), img.pixel(151, 70));                    // left/right symmetric
    }

    void darkThemeAddsALightEdge()
    {
        const QImage img = paintedAt(1, 1, true);
        QVERIFY(qGray(img.pixel(49, 70)) > qGray(QColor("#202124").rgb()));
        QVERIFY(qGray(img.pixel(150, 70)) > qGray(QColor("#202124").rgb()));
    }

    // The same size on screen at any zoom, and twice the device pixels at 2x.
    void sizeIsFixedInScreenPixels()
    {
        const QImage ref = paintedAt(1, 1, false);
        QCOMPARE(paintedAt(1, 4, false), ref);
        QCOMPARE(paintedAt(1, 0.25, false), ref);

        const QImage hi = paintedAt(2, 1, false);
        for (const int d : {1, 4, 8, 16})
            QVERIFY(qAbs(qGray(hi.pixel(200, 2 * (100 + d))) - qGray(ref.pixel(100, 100 + d))) <= 2);
    }
};

QTEST_MAIN(tst_CanvasShadow)
#include "tst_canvasshadow.moc"
