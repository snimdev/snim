#include <QtTest>

#include "media/ffmpeg/H264EncoderChain.h"

using namespace Media::Ffmpeg;

namespace {

H264EncoderSettings smallSettings()
{
    H264EncoderSettings settings;
    settings.width = 320;
    settings.height = 240;
    settings.globalHeader = true;
    return settings;
}

} // namespace

// The chain itself, against the statically linked FFmpeg. Hardware links may or may not
// open on a given machine, so only libx264 is asserted.
class tst_H264EncoderChain : public QObject
{
    Q_OBJECT

private slots:
    void init() { qunsetenv(H264EncoderChain::kOverrideVariable); }
    void cleanup() { qunsetenv(H264EncoderChain::kOverrideVariable); }

    void defaultOrderEndsWithLibx264()
    {
        const QStringList order = H264EncoderChain::defaultOrder();
        QCOMPARE(order, QStringList({QStringLiteral("h264_nvenc"), QStringLiteral("h264_qsv"),
                                     QStringLiteral("h264_amf"), QStringLiteral("h264_mf"),
                                     QStringLiteral("libx264")}));
    }

    void libx264AlwaysOpens()
    {
        const OpenedH264Encoder opened = H264EncoderChain({QStringLiteral("libx264")})
                                             .open(smallSettings());
        QVERIFY(opened);
        QCOMPARE(opened.name, QStringLiteral("libx264"));
        QCOMPARE(opened.context->width, 320);
    }

    void liveEncodingHasNoBFrames()
    {
        H264EncoderSettings settings = smallSettings();
        settings.tuning = H264EncoderSettings::Live;
        for (const QString &name : H264EncoderChain::defaultOrder()) {
            const OpenedH264Encoder opened = H264EncoderChain({name}).open(settings);
            if (!opened)
                continue;
            QCOMPARE(opened.context->max_b_frames, 0);
            QCOMPARE(opened.context->has_b_frames, 0);
        }
        settings.tuning = H264EncoderSettings::Offline;
        const OpenedH264Encoder offline = H264EncoderChain({QStringLiteral("libx264")}).open(settings);
        QVERIFY(offline);
        QVERIFY(offline.context->has_b_frames > 0);
    }
    void theDefaultChainAlwaysOpensSomething()
    {
        const OpenedH264Encoder opened = H264EncoderChain::fromEnvironment().open(smallSettings());
        QVERIFY(opened);
        QVERIFY(H264EncoderChain::defaultOrder().contains(opened.name));
        qInfo() << "Default chain picked" << opened.name;
    }

    void reportsEveryEncoderThatOpensHere()
    {
        for (const QString &name : H264EncoderChain::defaultOrder()) {
            const OpenedH264Encoder opened = H264EncoderChain({name}).open(smallSettings());
            qInfo() << name << (opened ? "opens" : "does not open");
        }
    }

    void anEncoderThatFailsPassesTheRequestOn()
    {
        const OpenedH264Encoder opened =
            H264EncoderChain({QStringLiteral("no_such_encoder"), QStringLiteral("libx264")})
                .open(smallSettings());
        QVERIFY(opened);
        QCOMPARE(opened.name, QStringLiteral("libx264"));
    }

    void theOverrideLeavesOneLink()
    {
        qputenv(H264EncoderChain::kOverrideVariable, "libx264");
        const H264EncoderChain chain = H264EncoderChain::fromEnvironment();
        QCOMPARE(chain.names(), QStringList{QStringLiteral("libx264")});
        const OpenedH264Encoder opened = chain.open(smallSettings());
        QVERIFY(opened);
        QCOMPARE(opened.name, QStringLiteral("libx264"));
    }

    void anUnknownOverrideKeepsTheDefaultChain()
    {
        qputenv(H264EncoderChain::kOverrideVariable, "h264_bogus");
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression("names no known H.264 encoder"));
        QCOMPARE(H264EncoderChain::fromEnvironment().names(), H264EncoderChain::defaultOrder());
    }

    void anEmptyChainOpensNothing()
    {
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression("No H.264 encoder opened"));
        QVERIFY(!H264EncoderChain(QStringList{}).open(smallSettings()));
    }
};

QTEST_GUILESS_MAIN(tst_H264EncoderChain)
#include "tst_h264encoderchain.moc"
