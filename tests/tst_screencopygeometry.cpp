#include <QtTest>
#include <QPixmap>
#include <cstring>

#include "capture/CaptureGeometry.h"
#include "capture/ScreencopyGeometry.h"

using namespace Capture;
using namespace Capture::Screencopy;

namespace {

// A w x h frame of one colour with a marker pixel at (mx, my).
QImage markedImage(int w, int h, int mx, int my, QColor fill = Qt::white)
{
    QImage image(w, h, QImage::Format_RGB32);
    image.fill(fill);
    image.setPixelColor(mx, my, Qt::red);
    return image;
}

QPoint markerPosition(const QImage &image)
{
    for (int y = 0; y < image.height(); ++y)
        for (int x = 0; x < image.width(); ++x)
            if (image.pixelColor(x, y) == QColor(Qt::red))
                return {x, y};
    return {-1, -1};
}

QImage solid(int w, int h, QColor colour)
{
    QImage image(w, h, QImage::Format_RGB32);
    image.fill(colour);
    return image;
}

} // namespace

class tst_ScreencopyGeometry : public QObject
{
    Q_OBJECT

private slots:
    void protocolChoice_data()
    {
        QTest::addColumn<bool>("hasExt");
        QTest::addColumn<bool>("hasWlr");
        QTest::addColumn<Protocol>("forced");
        QTest::addColumn<Protocol>("expected");

        QTest::newRow("ext wins") << true << true << Protocol::None << Protocol::ExtImageCopyCapture;
        QTest::newRow("wlr only") << false << true << Protocol::None << Protocol::WlrScreencopy;
        QTest::newRow("ext only") << true << false << Protocol::None << Protocol::ExtImageCopyCapture;
        QTest::newRow("neither") << false << false << Protocol::None << Protocol::None;
        QTest::newRow("forced wlr") << true << true << Protocol::WlrScreencopy << Protocol::WlrScreencopy;
        QTest::newRow("forced ext missing") << false << true << Protocol::ExtImageCopyCapture << Protocol::None;
    }

    void protocolChoice()
    {
        QFETCH(bool, hasExt);
        QFETCH(bool, hasWlr);
        QFETCH(Protocol, forced);
        QFETCH(Protocol, expected);
        QCOMPARE(pickProtocol(hasExt, hasWlr, forced), expected);
    }

    void desktopGate_data()
    {
        QTest::addColumn<QString>("desktop");
        QTest::addColumn<bool>("preferred");

        QTest::newRow("sway") << "sway" << true;
        QTest::newRow("hyprland") << "Hyprland" << true;
        QTest::newRow("niri") << "niri" << true;
        QTest::newRow("river") << "river" << true;
        QTest::newRow("wayfire") << "wayfire:wlroots" << true;
        QTest::newRow("cosmic") << "COSMIC" << true;
        QTest::newRow("unset") << "" << true;
        QTest::newRow("kde") << "KDE" << false;
        QTest::newRow("gnome") << "GNOME" << false;
        QTest::newRow("ubuntu gnome") << "ubuntu:GNOME" << false;
        QTest::newRow("lowercase kde") << "kde" << false;
    }

    void desktopGate()
    {
        QFETCH(QString, desktop);
        QFETCH(bool, preferred);
        QCOMPARE(preferredOnDesktop(desktop), preferred);
    }

    void fourccMatchesDrm()
    {
        QCOMPARE(ShmFormat::XBGR8888, 0x34324258u);
        QCOMPARE(ShmFormat::XRGB2101010, 0x30335258u);
        QCOMPARE(ShmFormat::RGB565, 0x36314752u);
    }

    // Each format's bytes for pure red, little-endian as wl_shm defines them.
    void formatsDecodeRed_data()
    {
        QTest::addColumn<quint32>("format");
        QTest::addColumn<QByteArray>("bytes");
        QTest::addColumn<int>("bpp");

        QTest::newRow("XRGB8888") << ShmFormat::XRGB8888 << QByteArray("\x00\x00\xff\x00", 4) << 4;
        QTest::newRow("ARGB8888") << ShmFormat::ARGB8888 << QByteArray("\x00\x00\xff\x00", 4) << 4;
        QTest::newRow("XBGR8888") << ShmFormat::XBGR8888 << QByteArray("\xff\x00\x00\x00", 4) << 4;
        QTest::newRow("ABGR8888") << ShmFormat::ABGR8888 << QByteArray("\xff\x00\x00\x00", 4) << 4;
        QTest::newRow("RGB888") << ShmFormat::RGB888 << QByteArray("\x00\x00\xff", 3) << 3;
        QTest::newRow("BGR888") << ShmFormat::BGR888 << QByteArray("\xff\x00\x00", 3) << 3;
        QTest::newRow("RGB565") << ShmFormat::RGB565 << QByteArray("\x00\xf8", 2) << 2;
        QTest::newRow("XRGB2101010") << ShmFormat::XRGB2101010 << QByteArray("\x00\x00\xf0\x3f", 4) << 4;
        QTest::newRow("XBGR2101010") << ShmFormat::XBGR2101010 << QByteArray("\xff\x03\x00\x00", 4) << 4;
    }

    void formatsDecodeRed()
    {
        QFETCH(quint32, format);
        QFETCH(QByteArray, bytes);
        QFETCH(int, bpp);

        QCOMPARE(bytesPerPixel(format), bpp);
        const QImage::Format imageFormat = imageFormatFor(format);
        QVERIFY(imageFormat != QImage::Format_Invalid);

        const int stride = strideFor(format, 1);
        QByteArray row(stride, '\0');
        std::memcpy(row.data(), bytes.constData(), bytes.size());
        const QImage image(reinterpret_cast<const uchar *>(row.constData()), 1, 1, stride, imageFormat);
        const QColor pixel = image.pixelColor(0, 0);
        QCOMPARE(pixel.red(), 255);
        QCOMPARE(pixel.green(), 0);
        QCOMPARE(pixel.blue(), 0);
        QCOMPARE(pixel.alpha(), 255);
    }

    void unknownFormatIsRejected()
    {
        QCOMPARE(imageFormatFor(fourcc('N', 'V', '1', '2')), QImage::Format_Invalid);
        QCOMPARE(bytesPerPixel(fourcc('N', 'V', '1', '2')), 0);
        QCOMPARE(strideFor(fourcc('N', 'V', '1', '2'), 100), 0);
    }

    void strideIsPaddedToFourBytes()
    {
        QCOMPARE(strideFor(ShmFormat::XRGB8888, 1920), 7680);
        QCOMPARE(strideFor(ShmFormat::BGR888, 5), 16);
        QCOMPARE(strideFor(ShmFormat::RGB565, 3), 8);
    }

    void formatPreference()
    {
        QCOMPARE(pickShmFormat({ShmFormat::ABGR8888, ShmFormat::XRGB8888}), ShmFormat::XRGB8888);
        QCOMPARE(pickShmFormat({ShmFormat::XRGB2101010, ShmFormat::XBGR8888}), ShmFormat::XBGR8888);
        QCOMPARE(pickShmFormat({ShmFormat::RGB565}), ShmFormat::RGB565);
        QVERIFY(!pickShmFormat({fourcc('N', 'V', '1', '2')}).has_value());
        QVERIFY(!pickShmFormat({}).has_value());
    }

    // A 3x2 buffer with the marker at its top-left corner, undone per wl_output.transform.
    void transformsAreUndone_data()
    {
        QTest::addColumn<quint32>("transform");
        QTest::addColumn<bool>("yInvert");
        QTest::addColumn<QSize>("size");
        QTest::addColumn<QPoint>("marker");

        QTest::newRow("normal") << 0u << false << QSize(3, 2) << QPoint(0, 0);
        QTest::newRow("90") << 1u << false << QSize(2, 3) << QPoint(1, 0);
        QTest::newRow("180") << 2u << false << QSize(3, 2) << QPoint(2, 1);
        QTest::newRow("270") << 3u << false << QSize(2, 3) << QPoint(0, 2);
        QTest::newRow("flipped") << 4u << false << QSize(3, 2) << QPoint(2, 0);
        QTest::newRow("flipped-90") << 5u << false << QSize(2, 3) << QPoint(0, 0);
        QTest::newRow("flipped-180") << 6u << false << QSize(3, 2) << QPoint(0, 1);
        QTest::newRow("flipped-270") << 7u << false << QSize(2, 3) << QPoint(1, 2);
        QTest::newRow("y-invert") << 0u << true << QSize(3, 2) << QPoint(0, 1);
        QTest::newRow("y-invert 90") << 1u << true << QSize(2, 3) << QPoint(0, 0);
    }

    void transformsAreUndone()
    {
        QFETCH(quint32, transform);
        QFETCH(bool, yInvert);
        QFETCH(QSize, size);
        QFETCH(QPoint, marker);

        const QImage upright = uprightFrame(markedImage(3, 2, 0, 0), transform, yInvert);
        QCOMPARE(upright.size(), size);
        QCOMPARE(markerPosition(upright), marker);
    }

    void framesMatchByNameThenPosition()
    {
        const QList<ScreenSlot> screens{
            {QStringLiteral("DP-1"), QRect(0, 0, 1920, 1080)},
            {QStringLiteral("HDMI-A-1"), QRect(1920, 0, 1280, 1024)},
            {QStringLiteral("eDP-1"), QRect(-1280, 0, 1280, 800)},
        };
        const QList<OutputFrame> frames{
            {QStringLiteral("HDMI-A-1"), QPoint(0, 0), {}},       // name beats a stale position
            {QString(), QPoint(-1280, 0), {}},                    // no name: layout position
            {QStringLiteral("DP-1"), QPoint(1920, 0), {}},
            {QStringLiteral("DP-9"), QPoint(5000, 0), {}},        // nothing shows it
        };
        QCOMPARE(matchFramesToScreens(frames, screens), (QList<int>{1, 2, 0, -1}));
    }

    void screenIsNeverMatchedTwice()
    {
        const QList<ScreenSlot> screens{{QStringLiteral("A"), QRect(0, 0, 10, 10)}};
        const QList<OutputFrame> frames{
            {QStringLiteral("A"), QPoint(0, 0), {}},
            {QString(), QPoint(0, 0), {}},
        };
        QCOMPARE(matchFramesToScreens(frames, screens), (QList<int>{0, -1}));
    }

    void stitchAtSharedScale()
    {
        // Two scale-2 outputs side by side: copied pixel for pixel at DPR 2.
        const QList<ScreenSlot> screens{
            {QStringLiteral("A"), QRect(0, 0, 100, 50)},
            {QStringLiteral("B"), QRect(100, 0, 80, 60)},
        };
        const QList<OutputFrame> frames{
            {QStringLiteral("A"), QPoint(0, 0), solid(200, 100, Qt::red)},
            {QStringLiteral("B"), QPoint(100, 0), solid(160, 120, Qt::blue)},
        };
        const QRect virtualGeometry(0, 0, 180, 60);

        const QImage stitched = stitchFrames(frames, screens, virtualGeometry);
        QCOMPARE(stitched.size(), QSize(360, 120));
        QCOMPARE(stitched.devicePixelRatio(), 2.0);
        QCOMPARE(stitched.pixelColor(199, 99), QColor(Qt::red));
        QCOMPARE(stitched.pixelColor(200, 0), QColor(Qt::blue));
        QCOMPARE(stitched.pixelColor(359, 119), QColor(Qt::blue));
        QCOMPARE(stitched.pixelColor(10, 110), QColor(Qt::black));   // below A: no output
    }

    void stitchMixedAndFractionalScale()
    {
        // 1920x1080 at scale 1 beside 1600x1200 at 1.5: sway truncates 1066.67 to 1066.
        const QList<ScreenSlot> screens{
            {QStringLiteral("HEADLESS-1"), QRect(0, 0, 1920, 1080)},
            {QStringLiteral("HEADLESS-2"), QRect(1920, 0, 1066, 800)},
        };
        const QList<OutputFrame> frames{
            {QStringLiteral("HEADLESS-1"), QPoint(0, 0), solid(1920, 1080, Qt::red)},
            {QStringLiteral("HEADLESS-2"), QPoint(1920, 0), solid(1600, 1200, Qt::blue)},
        };
        const QRect virtualGeometry(0, 0, 2986, 1080);

        const QImage stitched = stitchFrames(frames, screens, virtualGeometry);
        QCOMPARE(stitched.devicePixelRatio(), 1.5);
        QCOMPARE(stitched.size(), QSize(4480, 1620));   // the dense output's last column survives
        QCOMPARE(stitched.pixelColor(2870, 1610), QColor(Qt::red));   // scale-1 output, scaled up
        QCOMPARE(stitched.pixelColor(2880, 10), QColor(Qt::blue));
        QCOMPARE(stitched.pixelColor(4479, 1199), QColor(Qt::blue));
        QCOMPARE(stitched.pixelColor(3500, 1300), QColor(Qt::black));

        // The selector's crop math lands on the right output.
        QPixmap shot = QPixmap::fromImage(stitched);
        shot.setDevicePixelRatio(stitched.devicePixelRatio());
        const QPixmap crop = cropVirtualArea(shot, virtualGeometry, QRect(2000, 100, 200, 100));
        QCOMPARE(crop.size(), QSize(300, 150));
        QCOMPARE(crop.toImage().pixelColor(150, 75), QColor(Qt::blue));
    }

    void stitchHonoursNegativeOrigin()
    {
        const QList<ScreenSlot> screens{
            {QStringLiteral("L"), QRect(-50, 0, 50, 40)},
            {QStringLiteral("R"), QRect(0, 0, 60, 40)},
        };
        const QList<OutputFrame> frames{
            {QStringLiteral("L"), QPoint(-50, 0), solid(50, 40, Qt::green)},
            {QStringLiteral("R"), QPoint(0, 0), solid(60, 40, Qt::blue)},
        };
        const QImage stitched = stitchFrames(frames, screens, QRect(-50, 0, 110, 40));
        QCOMPARE(stitched.size(), QSize(110, 40));
        QCOMPARE(stitched.pixelColor(49, 0), QColor(Qt::green));
        QCOMPARE(stitched.pixelColor(50, 0), QColor(Qt::blue));
    }

    void stitchWithoutMatchIsNull()
    {
        const QList<ScreenSlot> screens{{QStringLiteral("A"), QRect(0, 0, 10, 10)}};
        const QList<OutputFrame> frames{{QStringLiteral("B"), QPoint(5, 5), solid(10, 10, Qt::red)}};
        QVERIFY(stitchFrames(frames, screens, QRect(0, 0, 10, 10)).isNull());
        QVERIFY(stitchFrames({}, screens, QRect(0, 0, 10, 10)).isNull());
    }
};

QTEST_MAIN(tst_ScreencopyGeometry)
#include "tst_screencopygeometry.moc"
