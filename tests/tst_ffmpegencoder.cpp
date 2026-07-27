#include <QtTest>
#include <QTemporaryDir>

#include <cmath>
#include <vector>

#include "media/ffmpeg/FfmpegEncoder.h"
#include "Mp4Boxes.h"

using namespace Media::Ffmpeg;
using namespace TestSupport;

namespace {

constexpr int kFps = 30;
constexpr int kSampleRate = 48000;
constexpr int kChannels = 2;
// One frame of video either way; AAC frames are 21.3 ms at 48 kHz.
constexpr qint64 kVideoToleranceMs = 34;
constexpr qint64 kAudioToleranceMs = 50;

FfmpegEncoderSettings settingsFor(const QString &path, int width = 160, int height = 120,
                                  int channels = kChannels)
{
    FfmpegEncoderSettings settings;
    settings.path = path;
    settings.video.width = width;
    settings.video.height = height;
    settings.video.frameRate = AVRational{kFps, 1};
    settings.sampleRate = kSampleRate;
    settings.channels = channels;
    return settings;
}

// A moving gradient, so the encoder has something to spend bits on.
std::vector<uchar> frameAt(int index, int width, int height)
{
    std::vector<uchar> pixels(size_t(width) * height * 4);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            uchar *pixel = &pixels[(size_t(y) * width + x) * 4];
            pixel[0] = uchar(x * 2 + index * 4);
            pixel[1] = uchar(y * 2);
            pixel[2] = uchar(index * 8);
            pixel[3] = 255;
        }
    }
    return pixels;
}

// Feeds one second of 30 fps video and a 440 Hz tone in 10 ms chunks, like a recorder.
bool encodeOneSecond(FfmpegEncoder &encoder, int width, int height, int channels)
{
    int sample = 0;
    constexpr int chunk = kSampleRate / 100;
    std::vector<float> pcm(size_t(chunk) * std::max(channels, 1));
    for (int frame = 0; frame < kFps; ++frame) {
        const qint64 frameUs = qint64(frame) * 1000000 / kFps;
        const std::vector<uchar> pixels = frameAt(frame, width, height);
        if (!encoder.addVideoFrame(pixels.data(), width * 4, frameUs))
            return false;
        while (channels > 0 && qint64(sample) * 1000000 / kSampleRate < frameUs + 1000000 / kFps) {
            for (int i = 0; i < chunk; ++i) {
                const float value = 0.25f * std::sin(2.0f * 3.14159265f * 440.0f
                                                     * float(sample + i) / kSampleRate);
                for (int c = 0; c < channels; ++c)
                    pcm[size_t(i) * channels + c] = value;
            }
            if (!encoder.addAudio(pcm.data(), chunk, qint64(sample) * 1000000 / kSampleRate))
                return false;
            sample += chunk;
        }
    }
    return encoder.finish();
}

} // namespace

// Encodes synthetic clips and reads the boxes back. libx264 is forced, so CI machines
// without a GPU encoder take the same path as everyone else.
class tst_FfmpegEncoder : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        qputenv("SNIM_H264_ENCODER", "libx264");
        QVERIFY(m_dir.isValid());
    }

    void writesFaststartMp4WithVideoAndAudio()
    {
        const QString path = m_dir.filePath(QStringLiteral("clip.mp4"));
        FfmpegEncoder encoder;
        QVERIFY2(encoder.open(settingsFor(path)), qPrintable(encoder.errorString()));
        QVERIFY2(encodeOneSecond(encoder, 160, 120, kChannels), qPrintable(encoder.errorString()));

        const Mp4Info info = readMp4(path);
        QCOMPARE(info.brand, QByteArray("isom"));
        const qsizetype moov = info.topLevel.indexOf("moov");
        const qsizetype mdat = info.topLevel.indexOf("mdat");
        QVERIFY2(moov >= 0 && mdat >= 0 && moov < mdat,
                 qPrintable(QString::fromLatin1(info.topLevel.join(' '))));

        QCOMPARE(info.tracks.size(), 2);
        const Mp4Track *video = trackOf(info, "vide");
        const Mp4Track *audio = trackOf(info, "soun");
        QVERIFY(video);
        QVERIFY(audio);
        QVERIFY2(qAbs(video->durationMs - 1000) <= kVideoToleranceMs,
                 qPrintable(QString::number(video->durationMs)));
        QVERIFY2(qAbs(audio->durationMs - 1000) <= kAudioToleranceMs,
                 qPrintable(QString::number(audio->durationMs)));
        QVERIFY2(qAbs(info.durationMs - 1000) <= kAudioToleranceMs,
                 qPrintable(QString::number(info.durationMs)));
    }

    void writesQuickTimeForMov()
    {
        const QString path = m_dir.filePath(QStringLiteral("clip.mov"));
        FfmpegEncoder encoder;
        QVERIFY2(encoder.open(settingsFor(path)), qPrintable(encoder.errorString()));
        QVERIFY2(encodeOneSecond(encoder, 160, 120, kChannels), qPrintable(encoder.errorString()));

        const Mp4Info info = readMp4(path);
        QCOMPARE(info.brand, QByteArray("qt  "));
        QVERIFY(info.topLevel.indexOf("moov") < info.topLevel.indexOf("mdat"));
        QCOMPARE(info.tracks.size(), 2);
    }

    void writesFragmentsForARecording()
    {
        const QString path = m_dir.filePath(QStringLiteral("fragmented.mp4"));
        FfmpegEncoderSettings settings = settingsFor(path);
        settings.fragmented = true;
        FfmpegEncoder encoder;
        QVERIFY2(encoder.open(settings), qPrintable(encoder.errorString()));
        QVERIFY2(encodeOneSecond(encoder, 160, 120, kChannels), qPrintable(encoder.errorString()));

        const Mp4Info info = readMp4(path);
        const qsizetype moov = info.topLevel.indexOf("moov");
        const qsizetype moof = info.topLevel.indexOf("moof");
        QVERIFY2(moov >= 0 && moof > moov,
                 qPrintable(QString::fromLatin1(info.topLevel.join(' '))));
        QCOMPARE(info.tracks.size(), 2);
    }

    void writesVideoOnlyWithoutChannels()
    {
        const QString path = m_dir.filePath(QStringLiteral("silent.mp4"));
        FfmpegEncoder encoder;
        QVERIFY2(encoder.open(settingsFor(path, 160, 120, 0)), qPrintable(encoder.errorString()));
        QVERIFY(encoder.addAudio(nullptr, 0, 0));   // ignored without a track
        QVERIFY2(encodeOneSecond(encoder, 160, 120, 0), qPrintable(encoder.errorString()));

        const Mp4Info info = readMp4(path);
        QCOMPARE(info.tracks.size(), 1);
        QVERIFY(trackOf(info, "vide"));
    }

    void cropsOddSizesToEven()
    {
        const QString path = m_dir.filePath(QStringLiteral("odd.mp4"));
        FfmpegEncoder encoder;
        QVERIFY2(encoder.open(settingsFor(path, 161, 121, 0)), qPrintable(encoder.errorString()));
        QVERIFY2(encodeOneSecond(encoder, 161, 121, 0), qPrintable(encoder.errorString()));
        QVERIFY(trackOf(readMp4(path), "vide"));
    }

    void keepsVideoTimestampsIncreasing()
    {
        const QString path = m_dir.filePath(QStringLiteral("repeat.mp4"));
        FfmpegEncoder encoder;
        QVERIFY2(encoder.open(settingsFor(path, 64, 48, 0)), qPrintable(encoder.errorString()));
        const std::vector<uchar> pixels = frameAt(0, 64, 48);
        // A clock that stalls or steps back must not make the muxer reject a packet.
        for (const qint64 us : {0, 0, 33000, 20000, 66000, 66000, 100000})
            QVERIFY2(encoder.addVideoFrame(pixels.data(), 64 * 4, us), qPrintable(encoder.errorString()));
        QVERIFY2(encoder.finish(), qPrintable(encoder.errorString()));
        QVERIFY(trackOf(readMp4(path), "vide"));
    }

    void failsOnAnUnwritablePath()
    {
        FfmpegEncoder encoder;
        QVERIFY(!encoder.open(settingsFor(m_dir.filePath(QStringLiteral("missing/dir/clip.mp4")))));
        QVERIFY(!encoder.errorString().isEmpty());
        QVERIFY(!encoder.finish());
    }

private:
    QTemporaryDir m_dir;
};

QTEST_GUILESS_MAIN(tst_FfmpegEncoder)
#include "tst_ffmpegencoder.moc"
