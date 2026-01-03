#include <QtTest>
#include <QRegion>

#include "recording/RecordTarget.h"
#include "recording/RecordingGeometry.h"

using namespace Recording;

class tst_RecordTarget : public QObject
{
    Q_OBJECT

private slots:
    void defaults()
    {
        RecordTarget t;
        QCOMPARE(t.kind, RecordTarget::Kind::Region);
        QCOMPARE(t.windowId, quint64(0));
        QVERIFY(t.captureCursor);
        QCOMPARE(t.fps, 30);
        QVERIFY(t.retinaCapture);                   // native pixel scale by default
        QVERIFY(!t.captureSystemAudio);             // audio is opt-in via Settings
        QVERIFY(!t.captureMic);
        QVERIFY(t.micDeviceId.isEmpty());           // empty = system default input
        QVERIFY(!t.isValid());                      // empty region is not valid
    }

    void regionValidity()
    {
        RecordTarget t;
        t.kind = RecordTarget::Kind::Region;
        QVERIFY(!t.isValid());
        t.regionVirtual = QRect(0, 0, 100, 80);
        QVERIFY(t.isValid());
    }

    void windowValidity()
    {
        RecordTarget t;
        t.kind = RecordTarget::Kind::Window;
        QVERIFY(!t.isValid());                      // no id and no rect
        t.windowId = 42;
        QVERIFY(t.isValid());                       // a window id is enough

        RecordTarget byRect;
        byRect.kind = RecordTarget::Kind::Window;
        byRect.regionVirtual = QRect(0, 0, 10, 10);
        QVERIFY(byRect.isValid());                  // rect fallback (window id unknown)
    }

    void displayLocalMapping()
    {
        // A region on a secondary display maps to that display's local origin.
        const QRect region(1920 + 120, 80, 200, 150);
        const QRect screen(1920, 0, 1920, 1080);
        QCOMPARE(displayLocalRect(region, screen), QRect(120, 80, 200, 150));
    }

    void surroundingRectsTileExactly()
    {
        // Hole strictly inside: four strips that tile outer minus hole exactly.
        const QRect outer(0, 0, 100, 80);
        const QRect hole(20, 10, 40, 30);
        const QVector<QRect> strips = surroundingRects(outer, hole);
        QCOMPARE(strips.size(), 4);

        QRegion covered;
        qsizetype area = 0;
        for (const QRect &s : strips) {
            QVERIFY(outer.contains(s));
            QVERIFY(!s.intersects(hole));
            QVERIFY(!covered.intersects(s));            // no overlap between strips
            covered += s;
            area += qsizetype(s.width()) * s.height();
        }
        QCOMPARE(area, qsizetype(100) * 80 - qsizetype(40) * 30);   // union == outer - hole
    }

    void surroundingRectsEdgeCases()
    {
        const QRect outer(0, 0, 100, 80);

        // Hole flush with the top-left corner: only bottom + right strips remain.
        const QVector<QRect> corner = surroundingRects(outer, QRect(0, 0, 50, 40));
        QCOMPARE(corner.size(), 2);

        // Hole covering everything: nothing left to dim.
        QVERIFY(surroundingRects(outer, outer).isEmpty());

        // Hole outside (or empty): the whole outer rect is dimmed.
        QCOMPARE(surroundingRects(outer, QRect(500, 500, 10, 10)), QVector<QRect>{outer});
        QCOMPARE(surroundingRects(outer, QRect()), QVector<QRect>{outer});

        // Hole overlapping an edge is clamped to outer first.
        const QVector<QRect> clamped = surroundingRects(outer, QRect(-10, 20, 30, 20));
        for (const QRect &s : clamped)
            QVERIFY(!s.intersects(QRect(0, 20, 20, 20)));
    }
};

QTEST_MAIN(tst_RecordTarget)
#include "tst_recordtarget.moc"
