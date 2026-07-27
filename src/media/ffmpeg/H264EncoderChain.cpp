#include "media/ffmpeg/H264EncoderChain.h"

extern "C" {
#include <libavutil/dict.h>
#include <libavutil/log.h>
}

#include <QDebug>
#include <QtGlobal>

#include <algorithm>
#include <cstdarg>
#include <mutex>
#include <utility>

namespace Media::Ffmpeg {

namespace {

// Per thread, since av_log_set_level is global and another thread may be encoding for real.
thread_local int t_quietDepth = 0;

void filteredLogCallback(void *avcl, int level, const char *format, va_list args)
{
    if (t_quietDepth == 0)
        av_log_default_callback(avcl, level, format, args);
}

// Silences FFmpeg's log on this thread while a probe runs; the chain logs its own verdict.
class QuietFfmpegLog
{
public:
    QuietFfmpegLog()
    {
        static std::once_flag installed;
        std::call_once(installed, [] { av_log_set_callback(filteredLogCallback); });
        ++t_quietDepth;
    }
    ~QuietFfmpegLog() { --t_quietDepth; }
    QuietFfmpegLog(const QuietFfmpegLog &) = delete;
    QuietFfmpegLog &operator=(const QuietFfmpegLog &) = delete;
};

// YUV420P is what H.264 decoders hand a transcode; Quick Sync only takes NV12.
AVPixelFormat pickPixelFormat(const AVCodec *codec)
{
    const void *config = nullptr;
    int count = 0;
    if (avcodec_get_supported_config(nullptr, codec, AV_CODEC_CONFIG_PIX_FORMAT, 0, &config,
                                     &count) < 0 || !config) {
        return AV_PIX_FMT_YUV420P;
    }
    const auto *formats = static_cast<const AVPixelFormat *>(config);
    for (const AVPixelFormat wanted : {AV_PIX_FMT_YUV420P, AV_PIX_FMT_NV12}) {
        if (std::find(formats, formats + count, wanted) != formats + count)
            return wanted;
    }
    return AV_PIX_FMT_NONE;
}

qint64 derivedBitRate(const H264EncoderSettings &settings)
{
    const double fps = settings.frameRate.num > 0 && settings.frameRate.den > 0
                       ? av_q2d(settings.frameRate) : 30.0;
    // About 0.1 bit per pixel: 1080p30 lands near 6 Mbit/s.
    return std::max<qint64>(1'000'000, qint64(double(settings.width) * settings.height * fps * 0.1));
}

void setEncoderOptions(const QString &name, const H264EncoderSettings &settings,
                       AVDictionary **options)
{
    const bool live = settings.tuning == H264EncoderSettings::Live;
    if (name == QLatin1String("libx264")) {
        av_dict_set(options, "preset", live ? "veryfast" : "medium", 0);
        av_dict_set(options, "crf", "20", 0);
    } else if (name == QLatin1String("h264_nvenc")) {
        av_dict_set(options, "preset", live ? "p3" : "p5", 0);
        av_dict_set(options, "rc", "vbr", 0);
    } else if (name == QLatin1String("h264_qsv")) {
        av_dict_set(options, "preset", live ? "veryfast" : "medium", 0);
    } else if (name == QLatin1String("h264_amf")) {
        av_dict_set(options, "quality", live ? "speed" : "quality", 0);
        av_dict_set(options, "rc", "vbr_peak", 0);
    } else if (name == QLatin1String("h264_mf")) {
        // Without this Media Foundation opens its software MFT, which libx264 beats.
        av_dict_set(options, "hw_encoding", "1", 0);
    }
}

} // namespace

class H264EncoderChain::Link
{
public:
    Link(QString name, std::unique_ptr<Link> next)
        : m_name(std::move(name)), m_next(std::move(next))
    {
    }

    [[nodiscard]] const QString &name() const { return m_name; }
    [[nodiscard]] const Link *next() const { return m_next.get(); }

    [[nodiscard]] OpenedH264Encoder handle(const H264EncoderSettings &settings) const
    {
        if (CodecContextPtr context = tryOpen(settings))
            return {std::move(context), m_name};
        return m_next ? m_next->handle(settings) : OpenedH264Encoder{};
    }

private:
    [[nodiscard]] CodecContextPtr tryOpen(const H264EncoderSettings &settings) const
    {
        const QByteArray name = m_name.toLatin1();
        const AVCodec *codec = avcodec_find_encoder_by_name(name.constData());
        if (!codec) {
            qDebug() << "H.264 encoder not built in:" << m_name;
            return nullptr;
        }
        const AVPixelFormat format = pickPixelFormat(codec);
        if (format == AV_PIX_FMT_NONE) {
            qDebug() << "H.264 encoder takes no YUV420P or NV12 input:" << m_name;
            return nullptr;
        }

        CodecContextPtr context(avcodec_alloc_context3(codec));
        if (!context)
            return nullptr;
        context->width = settings.width;
        context->height = settings.height;
        context->time_base = settings.timeBase;
        context->framerate = settings.frameRate;
        context->pix_fmt = format;
        // A keyframe a second lets a recording close a fragment every second.
        const double gopSeconds = settings.tuning == H264EncoderSettings::Live ? 1.0 : 2.0;
        context->gop_size = std::max(1, int(av_q2d(settings.frameRate) * gopSeconds));
        context->bit_rate = derivedBitRate(settings);
        context->color_range = settings.colorRange;
        context->color_primaries = settings.colorPrimaries;
        context->color_trc = settings.colorTrc;
        context->colorspace = settings.colorSpace;
        // MP4 wants the SPS/PPS out of band.
        context->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
        // Live frames come at uneven times; B-frame reordering skews the track's start and length.
        if (settings.tuning == H264EncoderSettings::Live)
            context->max_b_frames = 0;

        AVDictionary *options = nullptr;
        setEncoderOptions(m_name, settings, &options);
        int result = 0;
        {
            const QuietFfmpegLog quiet;
            result = avcodec_open2(context.get(), codec, &options);
        }
        av_dict_free(&options);
        if (result < 0) {
            qDebug() << "H.264 encoder did not open:" << m_name << errorString(result);
            return nullptr;
        }
        qDebug() << "H.264 encoder opened:" << m_name;
        return context;
    }

    QString m_name;
    std::unique_ptr<Link> m_next;
};

QStringList H264EncoderChain::defaultOrder()
{
    return {QStringLiteral("h264_nvenc"), QStringLiteral("h264_qsv"), QStringLiteral("h264_amf"),
            QStringLiteral("h264_mf"), QStringLiteral("libx264")};
}

H264EncoderChain H264EncoderChain::fromEnvironment()
{
    const QString wanted = qEnvironmentVariable(kOverrideVariable).trimmed();
    if (wanted.isEmpty())
        return H264EncoderChain(defaultOrder());
    if (defaultOrder().contains(wanted))
        return H264EncoderChain({wanted});
    qWarning() << kOverrideVariable << "names no known H.264 encoder:" << wanted
               << "; expected one of" << defaultOrder();
    return H264EncoderChain(defaultOrder());
}

H264EncoderChain::H264EncoderChain(const QStringList &order)
{
    for (auto it = order.crbegin(); it != order.crend(); ++it)
        m_head = std::make_unique<Link>(*it, std::move(m_head));
}

H264EncoderChain::H264EncoderChain(H264EncoderChain &&) noexcept = default;
H264EncoderChain &H264EncoderChain::operator=(H264EncoderChain &&) noexcept = default;
H264EncoderChain::~H264EncoderChain() = default;

QStringList H264EncoderChain::names() const
{
    QStringList names;
    for (const Link *link = m_head.get(); link; link = link->next())
        names.append(link->name());
    return names;
}

OpenedH264Encoder H264EncoderChain::open(const H264EncoderSettings &settings) const
{
    OpenedH264Encoder opened = m_head ? m_head->handle(settings) : OpenedH264Encoder{};
    if (opened)
        qInfo() << "H.264 encoder:" << opened.name;
    else
        qWarning() << "No H.264 encoder opened from" << names();
    return opened;
}

} // namespace Media::Ffmpeg
