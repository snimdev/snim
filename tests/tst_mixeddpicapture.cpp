#include <QtTest>
#include <QApplication>
#include <QScreen>
#include <QTemporaryDir>

#include "capture/CaptureGeometry.h"
#include "screen/FrozenFrameGrabber.h"

using namespace Capture;
using namespace Screen;

// The capture geometry on a mixed-DPI desktop, laid out by the offscreen platform
// the way Qt lays out Windows under per-monitor DPI awareness: a 100% screen, a
// 150% screen to its right and a 150% screen to its left (negative origin). Each
// screen keeps its native origin and only its extent is scaled, so the logical
// desktop has a gap left of the 100% screen. Frame contents are not checked: offscreen
// grabs are only placeholders, the geometry is what is under test.
class tst_MixedDpiCapture : public QObject
{
    Q_OBJECT

    QPixmap m_frame;
    QRect m_virtual;

    static QRect crop(const QRect &area, const QPixmap &frame, const QRect &virt)
    {
        return physicalCropRect(area, virt, frame.devicePixelRatio(), frame.size());
    }

private slots:
    void initTestCase()
    {
        FrozenFrameGrabber grabber;
        grabber.grab([this](QPixmap frozen, QRect virtualGeometry) {
            m_frame = frozen;
            m_virtual = virtualGeometry;
        });
        QVERIFY(!m_frame.isNull());
    }

    void layoutIsMixedDpi_data()
    {
        QTest::addColumn<QString>("name");
        QTest::addColumn<QRect>("geometry");
        QTest::addColumn<qreal>("dpr");
        QTest::newRow("primary") << "primary" << QRect(0, 0, 800, 600) << 1.0;
        QTest::newRow("right") << "right" << QRect(800, 0, 800, 600) << 1.5;
        QTest::newRow("left") << "left" << QRect(-1200, 0, 800, 600) << 1.5;
    }

    void layoutIsMixedDpi()
    {
        QFETCH(QString, name);
        QFETCH(QRect, geometry);
        QFETCH(qreal, dpr);
        QCOMPARE(QGuiApplication::screens().size(), 3);
        QScreen *screen = nullptr;
        for (QScreen *s : QGuiApplication::screens())
            if (s->name() == name)
                screen = s;
        QVERIFY(screen);
        QCOMPARE(screen->geometry(), geometry);
        QCOMPARE(screen->devicePixelRatio(), dpr);
    }

    void frozenFrameCoversEveryScreenAtTheHighestDpr()
    {
        QCOMPARE(m_virtual, QRect(-1200, 0, 2800, 600));
        QCOMPARE(m_frame.devicePixelRatio(), 1.5);
        QCOMPARE(m_frame.size(), QSize(4200, 900));
    }

    void cropsAnAreaOnTheOneHundredPercentScreen()
    {
        // The 100% screen is composited at 1.5x, so its pixels come out upscaled.
        const QRect area(100, 100, 200, 100);
        QCOMPARE(crop(area, m_frame, m_virtual), QRect(1950, 150, 300, 150));

        const QPixmap out = cropVirtualArea(m_frame, m_virtual, area);
        QCOMPARE(out.size(), QSize(300, 150));
        QCOMPARE(out.devicePixelRatio(), 1.5);
        QCOMPARE(out.deviceIndependentSize(), QSizeF(area.size()));
    }

    void cropsAnAreaOnEachOneHundredFiftyPercentScreen()
    {
        const QRect right(900, 50, 300, 200);
        QCOMPARE(crop(right, m_frame, m_virtual), QRect(3150, 75, 450, 300));
        QCOMPARE(cropVirtualArea(m_frame, m_virtual, right).deviceIndependentSize(),
                 QSizeF(right.size()));

        const QRect left(-1100, 500, 100, 50);
        QCOMPARE(crop(left, m_frame, m_virtual), QRect(150, 750, 150, 75));
        QCOMPARE(cropVirtualArea(m_frame, m_virtual, left).deviceIndependentSize(),
                 QSizeF(left.size()));
    }

    void cropsAnAreaSpanningTwoDprs()
    {
        // Half on the 100% primary, half on the 150% screen: one crop, one DPR.
        const QRect area(700, 200, 200, 100);
        QCOMPARE(crop(area, m_frame, m_virtual), QRect(2850, 300, 300, 150));

        const QPixmap out = cropVirtualArea(m_frame, m_virtual, area);
        QCOMPARE(out.devicePixelRatio(), 1.5);
        QCOMPARE(out.deviceIndependentSize(), QSizeF(area.size()));
    }

    void cropsAnAreaSpanningTheGap()
    {
        // The logical gap left of the primary is part of the frame (black), not an error.
        const QRect area(-500, 0, 600, 100);
        QCOMPARE(crop(area, m_frame, m_virtual), QRect(1050, 0, 900, 150));
        QCOMPARE(cropVirtualArea(m_frame, m_virtual, area).size(), QSize(900, 150));
    }
};

int main(int argc, char **argv)
{
    // Screens in native pixels: logicalDpi / logicalBaseDpi is the scale factor.
    QTemporaryDir dir;
    QFile config(dir.filePath(QStringLiteral("screens.json")));
    if (!config.open(QIODevice::WriteOnly))
        return 1;
    config.write(R"({
        "windowFrameMargins": false,
        "screens": [
            { "name": "primary", "x": 0, "y": 0, "width": 800, "height": 600,
              "logicalDpi": 96, "logicalBaseDpi": 96 },
            { "name": "right", "x": 800, "y": 0, "width": 1200, "height": 900,
              "logicalDpi": 144, "logicalBaseDpi": 96 },
            { "name": "left", "x": -1200, "y": 0, "width": 1200, "height": 900,
              "logicalDpi": 144, "logicalBaseDpi": 96 }
        ]
    })");
    config.close();

    // The platform argument splits on ':', so a Windows drive letter cannot be in it.
    const QString cwd = QDir::currentPath();
    QDir::setCurrent(dir.path());
    qputenv("QT_QPA_PLATFORM", "offscreen:configfile=screens.json");
    QApplication app(argc, argv);
    QDir::setCurrent(cwd);

    tst_MixedDpiCapture test;
    return QTest::qExec(&test, argc, argv);
}

#include "tst_mixeddpicapture.moc"
