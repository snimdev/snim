#include "recording/strategies/windows/WasapiAudioSource.h"

#include "media/ffmpeg/FfmpegSupport.h"
#include "recording/PauseAwareClock.h"

extern "C" {
#include <libavutil/channel_layout.h>
}

#include <windows.h>
#include <audioclient.h>
#include <mmdeviceapi.h>
#include <wrl/client.h>

#include <future>
#include <string>
#include <thread>
#include <vector>

namespace Recording {

namespace {

using Microsoft::WRL::ComPtr;
using Media::Ffmpeg::SwrContextPtr;

constexpr REFERENCE_TIME kBufferHns = 1000000;   // 100 ms
constexpr DWORD kPollMs = 10;
// Loopback fills silence once it has been quiet this long, lagging this far behind now.
constexpr qint64 kSilenceAfterUs = 30000;
constexpr qint64 kSilenceLagUs = 10000;
constexpr qint64 kMaxSilenceFrames = WasapiAudioSource::kSampleRate / 10;

qint64 framesToUs(qint64 frames)
{
    return frames * 1000000 / WasapiAudioSource::kSampleRate;
}

QString hresultText(const char *context, HRESULT hr)
{
    return QStringLiteral("%1 (0x%2)").arg(QLatin1String(context))
        .arg(quint32(hr), 8, 16, QLatin1Char('0'));
}

struct ComScope {
    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    ~ComScope()
    {
        if (SUCCEEDED(hr))
            CoUninitialize();
    }
};

ComPtr<IMMDevice> openDevice(WasapiAudioSource::Kind kind, const QByteArray &deviceId)
{
    ComPtr<IMMDeviceEnumerator> enumerator;
    if (FAILED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                                IID_PPV_ARGS(&enumerator))))
        return nullptr;
    ComPtr<IMMDevice> device;
    if (kind == WasapiAudioSource::Kind::Microphone && !deviceId.isEmpty()) {
        const std::wstring id = QString::fromUtf8(deviceId).toStdWString();
        DWORD state = 0;
        if (SUCCEEDED(enumerator->GetDevice(id.c_str(), &device))
            && SUCCEEDED(device->GetState(&state)) && state == DEVICE_STATE_ACTIVE)
            return device;
        device.Reset();
    }
    const EDataFlow flow = kind == WasapiAudioSource::Kind::Loopback ? eRender : eCapture;
    if (FAILED(enumerator->GetDefaultAudioEndpoint(flow, eConsole, &device)))
        return nullptr;
    return device;
}

AVSampleFormat sampleFormatOf(const WAVEFORMATEX *format)
{
    WORD tag = format->wFormatTag;
    if (tag == WAVE_FORMAT_EXTENSIBLE)   // the subformat GUID starts with the plain tag
        tag = WORD(reinterpret_cast<const WAVEFORMATEXTENSIBLE *>(format)->SubFormat.Data1);
    if (tag == WAVE_FORMAT_IEEE_FLOAT && format->wBitsPerSample == 32)
        return AV_SAMPLE_FMT_FLT;
    if (tag == WAVE_FORMAT_PCM && format->wBitsPerSample == 16)
        return AV_SAMPLE_FMT_S16;
    if (tag == WAVE_FORMAT_PCM && format->wBitsPerSample == 32)
        return AV_SAMPLE_FMT_S32;
    return AV_SAMPLE_FMT_NONE;
}

// One opened WASAPI stream, driven from the source's capture thread.
class CaptureStream
{
public:
    bool open(WasapiAudioSource::Kind kind, const QByteArray &deviceId, QString *error);
    void run(HANDLE stopEvent, const WasapiAudioSource::Handler &handler);

private:
    bool initialize(const WAVEFORMATEX *format, DWORD flags, HRESULT *hr);
    void deliver(const BYTE *data, UINT32 frames, bool silent, qint64 timestampUs,
                 const WasapiAudioSource::Handler &handler);
    void fillSilence(const WasapiAudioSource::Handler &handler);

    WasapiAudioSource::Kind m_kind = WasapiAudioSource::Kind::Loopback;
    ComPtr<IMMDevice> m_device;
    ComPtr<IAudioClient> m_client;
    ComPtr<IAudioCaptureClient> m_capture;
    SwrContextPtr m_swr;           // null when WASAPI already converts to the mix format
    int m_inRate = WasapiAudioSource::kSampleRate;
    UINT32 m_blockAlign = 0;
    std::vector<BYTE> m_zeros;
    std::vector<float> m_out;
    qint64 m_nextUs = 0;           // where the last delivered buffer ended
};

bool CaptureStream::initialize(const WAVEFORMATEX *format, DWORD flags, HRESULT *hr)
{
    m_client.Reset();
    *hr = m_device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, &m_client);
    if (FAILED(*hr))
        return false;
    if (m_kind == WasapiAudioSource::Kind::Loopback)
        flags |= AUDCLNT_STREAMFLAGS_LOOPBACK;
    *hr = m_client->Initialize(AUDCLNT_SHAREMODE_SHARED, flags, kBufferHns, 0, format, nullptr);
    return SUCCEEDED(*hr);
}

bool CaptureStream::open(WasapiAudioSource::Kind kind, const QByteArray &deviceId, QString *error)
{
    m_kind = kind;
    m_device = openDevice(kind, deviceId);
    if (!m_device) {
        *error = kind == WasapiAudioSource::Kind::Loopback
            ? QStringLiteral("No audio output device to record from")
            : QStringLiteral("No microphone found");
        return false;
    }

    // Ask WASAPI to convert to the mix format; older drivers refuse, so fall back
    // to the device format and convert with swresample.
    WAVEFORMATEX wanted{};
    wanted.wFormatTag = WAVE_FORMAT_IEEE_FLOAT;
    wanted.nChannels = WasapiAudioSource::kChannels;
    wanted.nSamplesPerSec = WasapiAudioSource::kSampleRate;
    wanted.wBitsPerSample = 32;
    wanted.nBlockAlign = wanted.nChannels * wanted.wBitsPerSample / 8;
    wanted.nAvgBytesPerSec = wanted.nSamplesPerSec * wanted.nBlockAlign;
    HRESULT hr = S_OK;
    if (initialize(&wanted, AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM
                                | AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY, &hr)) {
        m_blockAlign = wanted.nBlockAlign;
    } else {
        WAVEFORMATEX *mix = nullptr;
        if (!m_client || FAILED(hr = m_client->GetMixFormat(&mix)) || !mix) {
            *error = hresultText("Cannot read the audio device format", hr);
            return false;
        }
        const AVSampleFormat format = sampleFormatOf(mix);
        m_inRate = int(mix->nSamplesPerSec);
        m_blockAlign = mix->nBlockAlign;
        const int channels = mix->nChannels;
        const bool opened = format != AV_SAMPLE_FMT_NONE && initialize(mix, 0, &hr);
        CoTaskMemFree(mix);
        if (format == AV_SAMPLE_FMT_NONE) {
            *error = QStringLiteral("Unsupported audio device format");
            return false;
        }
        if (!opened) {
            *error = hresultText("Cannot open the audio device", hr);
            return false;
        }
        AVChannelLayout in{};
        AVChannelLayout out{};
        av_channel_layout_default(&in, channels);
        av_channel_layout_default(&out, WasapiAudioSource::kChannels);
        SwrContext *swr = nullptr;
        const int result = swr_alloc_set_opts2(&swr, &out, AV_SAMPLE_FMT_FLT,
                                               WasapiAudioSource::kSampleRate, &in, format,
                                               m_inRate, 0, nullptr);
        m_swr.reset(swr);
        av_channel_layout_uninit(&in);
        av_channel_layout_uninit(&out);
        if (result < 0 || swr_init(m_swr.get()) < 0) {
            *error = QStringLiteral("Cannot convert the audio device format");
            return false;
        }
    }

    hr = m_client->GetService(IID_PPV_ARGS(&m_capture));
    if (FAILED(hr)) {
        *error = hresultText("Cannot capture from the audio device", hr);
        return false;
    }
    return true;
}

void CaptureStream::deliver(const BYTE *data, UINT32 frames, bool silent, qint64 timestampUs,
                            const WasapiAudioSource::Handler &handler)
{
    if (frames == 0)
        return;
    if (silent || !data) {
        m_zeros.assign(size_t(frames) * m_blockAlign, 0);
        data = m_zeros.data();
    }
    const float *samples = reinterpret_cast<const float *>(data);
    int outFrames = int(frames);
    if (m_swr) {
        const int capacity = swr_get_out_samples(m_swr.get(), int(frames));
        m_out.resize(size_t(qMax(capacity, 0)) * WasapiAudioSource::kChannels);
        uint8_t *out = reinterpret_cast<uint8_t *>(m_out.data());
        const uint8_t *in = data;
        outFrames = swr_convert(m_swr.get(), &out, capacity, &in, int(frames));
        samples = m_out.data();
    }
    if (outFrames <= 0)
        return;
    handler(samples, outFrames, timestampUs);
    m_nextUs = timestampUs + framesToUs(outFrames);
}

void CaptureStream::fillSilence(const WasapiAudioSource::Handler &handler)
{
    const qint64 now = PauseAwareClock::nowUs();
    if (now - m_nextUs < kSilenceAfterUs)
        return;
    qint64 frames = (now - kSilenceLagUs - m_nextUs) * WasapiAudioSource::kSampleRate / 1000000;
    while (frames > 0) {
        const int chunk = int(qMin(frames, kMaxSilenceFrames));
        m_out.assign(size_t(chunk) * WasapiAudioSource::kChannels, 0.0f);
        handler(m_out.data(), chunk, m_nextUs);
        m_nextUs += framesToUs(chunk);
        frames -= chunk;
    }
}

void CaptureStream::run(HANDLE stopEvent, const WasapiAudioSource::Handler &handler)
{
    m_nextUs = PauseAwareClock::nowUs();
    bool alive = SUCCEEDED(m_client->Start());
    while (WaitForSingleObject(stopEvent, kPollMs) == WAIT_TIMEOUT) {
        bool delivered = false;
        HRESULT hr = S_OK;
        UINT32 packet = 0;
        while (alive && SUCCEEDED(hr = m_capture->GetNextPacketSize(&packet)) && packet > 0) {
            BYTE *data = nullptr;
            UINT32 frames = 0;
            DWORD flags = 0;
            UINT64 qpc100ns = 0;
            hr = m_capture->GetBuffer(&data, &frames, &flags, nullptr, &qpc100ns);
            if (FAILED(hr))
                break;
            const qint64 timestampUs = (flags & AUDCLNT_BUFFERFLAGS_TIMESTAMP_ERROR)
                ? PauseAwareClock::nowUs() - qint64(frames) * 1000000 / m_inRate
                : qint64(qpc100ns / 10);
            deliver(data, frames, flags & AUDCLNT_BUFFERFLAGS_SILENT, timestampUs, handler);
            m_capture->ReleaseBuffer(frames);
            delivered = true;
        }
        if (FAILED(hr))
            alive = false;   // the device went away: loopback carries on in silence
        if (!delivered && m_kind == WasapiAudioSource::Kind::Loopback)
            fillSilence(handler);
    }
    m_client->Stop();
}

} // namespace

struct WasapiAudioSource::Impl {
    Kind kind;
    std::thread thread;
    HANDLE stopEvent = nullptr;
};

WasapiAudioSource::WasapiAudioSource(Kind kind) : d(std::make_unique<Impl>())
{
    d->kind = kind;
}

WasapiAudioSource::~WasapiAudioSource()
{
    stop();
}

bool WasapiAudioSource::start(const QByteArray &deviceId, Handler handler, QString *error)
{
    stop();
    d->stopEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!d->stopEvent) {
        if (error)
            *error = QStringLiteral("Cannot start audio capture");
        return false;
    }

    std::promise<QString> opened;
    std::future<QString> result = opened.get_future();
    d->thread = std::thread([kind = d->kind, deviceId, stopEvent = d->stopEvent,
                             handler = std::move(handler), opened = std::move(opened)]() mutable {
        ComScope com;
        CaptureStream stream;
        QString openError;
        if (!stream.open(kind, deviceId, &openError)) {
            opened.set_value(openError);
            return;
        }
        opened.set_value(QString());
        stream.run(stopEvent, handler);
    });

    const QString openError = result.get();
    if (!openError.isEmpty()) {
        stop();
        if (error)
            *error = openError;
        return false;
    }
    return true;
}

void WasapiAudioSource::stop()
{
    if (d->stopEvent)
        SetEvent(d->stopEvent);
    if (d->thread.joinable())
        d->thread.join();
    if (d->stopEvent) {
        CloseHandle(d->stopEvent);
        d->stopEvent = nullptr;
    }
}

} // namespace Recording
