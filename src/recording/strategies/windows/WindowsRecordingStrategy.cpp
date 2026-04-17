#include "recording/strategies/windows/WindowsRecordingStrategy.h"

#include "media/ffmpeg/FfmpegEncoder.h"
#include "recording/PauseAwareClock.h"
#include "recording/PcmMixBuffer.h"
#include "recording/RecordingGeometry.h"
#include "recording/strategies/windows/WasapiAudioSource.h"
#include "recording/strategies/windows/WgcFrameSource.h"

#include <QDebug>
#include <QFile>
#include <QGuiApplication>
#include <QScreen>
#include <QTimer>
#include <QtGui/qscreen_platform.h>

#include <windows.h>
#include <objbase.h>

#include <atomic>
#include <condition_variable>
#include <cstring>
#include <deque>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>

namespace Recording {

namespace {

using Media::Ffmpeg::FfmpegEncoder;
using Media::Ffmpeg::FfmpegEncoderSettings;

constexpr int kMixRate = WasapiAudioSource::kSampleRate;
constexpr int kMixChannels = WasapiAudioSource::kChannels;
// Same mixing windows as the macOS recorder: 150 ms holdback, 50 ms resync.
constexpr std::int64_t kMixHoldbackFrames = 7200;
constexpr std::int64_t kMixResyncFrames = 2400;
// Frames waiting for the encoder; past this the capture side drops new ones.
constexpr int kMaxPendingFrames = 3;
constexpr int kDurationIntervalMs = 200;

// Runs tasks one after another on its own thread, draining the queue before it quits.
class SerialQueue
{
public:
    SerialQueue() : m_thread([this] { run(); }) {}

    ~SerialQueue()
    {
        {
            std::lock_guard lock(m_mutex);
            m_quit = true;
        }
        m_wake.notify_one();
        m_thread.join();
    }

    void post(std::function<void()> task)
    {
        {
            std::lock_guard lock(m_mutex);
            m_tasks.push_back(std::move(task));
        }
        m_wake.notify_one();
    }

private:
    void run()
    {
        // Media Foundation encoders expect COM on the thread that drives them.
        const HRESULT com = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        for (;;) {
            std::function<void()> task;
            {
                std::unique_lock lock(m_mutex);
                m_wake.wait(lock, [this] { return m_quit || !m_tasks.empty(); });
                if (m_tasks.empty())
                    break;
                task = std::move(m_tasks.front());
                m_tasks.pop_front();
            }
            task();
        }
        if (SUCCEEDED(com))
            CoUninitialize();
    }

    std::mutex m_mutex;
    std::condition_variable m_wake;
    std::deque<std::function<void()>> m_tasks;
    bool m_quit = false;
    std::thread m_thread;
};

struct VideoFrame {
    std::vector<uchar> pixels;   // tightly packed BGRA
    int width = 0;
    int height = 0;
    qint64 timestampUs = 0;
};

struct Setup {
    quint64 generation = 0;
    QString path;
    WgcFrameSource::Target video;
    QSize outputPx;
    int fps = 30;
    bool systemAudio = false;
    bool mic = false;
    QByteArray micDeviceId;
};

} // namespace

struct WindowsRecordingStrategy::Engine {
    explicit Engine(WindowsRecordingStrategy *owner) : owner(owner) {}

    // Encoder thread.
    void begin(const Setup &setup);
    void stopSources();
    void finishEncoding(bool report);
    void encodeVideo(const std::shared_ptr<const VideoFrame> &frame);
    void encodeAudio(int source, const std::vector<float> &samples, qint64 timestampUs);
    bool addFrame(const VideoFrame &frame, qint64 outputUs);
    bool flushMix(bool drain);
    void failWith(const QString &error);

    // Capture threads.
    void onFrame(const WgcFrameSource::Frame &frame);
    void onAudio(int source, const float *samples, int frames, qint64 timestampUs);

    // Any thread.
    [[nodiscard]] qint64 elapsedMs();

    WindowsRecordingStrategy *owner;

    WgcFrameSource video;
    WasapiAudioSource systemAudio{WasapiAudioSource::Kind::Loopback};
    WasapiAudioSource microphone{WasapiAudioSource::Kind::Microphone};
    std::atomic<int> pendingFrames{0};

    std::mutex clockMutex;
    PauseAwareClock clock;

    // Encoder thread only.
    quint64 generation = 0;
    QString path;
    std::unique_ptr<FfmpegEncoder> encoder;
    std::unique_ptr<PcmMixBuffer> mixer;
    MixSourceClock mixClocks[2]{MixSourceClock(kMixResyncFrames), MixSourceClock(kMixResyncFrames)};
    std::shared_ptr<const VideoFrame> lastFrame;
    qint64 lastVideoUs = -1;
    qint64 frameIntervalUs = 1000000 / 30;
    qint64 stopAtUs = 0;

    // Last: its thread starts once everything above exists and stops before it goes.
    SerialQueue queue;
};

void WindowsRecordingStrategy::Engine::begin(const Setup &setup)
{
    generation = setup.generation;
    path = setup.path;
    lastFrame.reset();
    lastVideoUs = -1;
    frameIntervalUs = 1000000 / qMax(setup.fps, 1);
    {
        std::lock_guard lock(clockMutex);
        clock.reset();
    }

    // Audio that cannot start leaves the recording silent rather than failing it.
    int audioSources = 0;
    QString error;
    if (setup.systemAudio) {
        if (systemAudio.start({}, [this](const float *s, int n, qint64 t) { onAudio(0, s, n, t); },
                              &error))
            ++audioSources;
        else
            qWarning() << "System audio is not recorded:" << error;
    }
    if (setup.mic) {
        if (microphone.start(setup.micDeviceId,
                             [this](const float *s, int n, qint64 t) { onAudio(1, s, n, t); },
                             &error))
            ++audioSources;
        else
            qWarning() << "The microphone is not recorded:" << error;
    }
    mixer = audioSources > 0
        ? std::make_unique<PcmMixBuffer>(kMixChannels, audioSources > 1 ? kMixHoldbackFrames : 0)
        : nullptr;
    for (MixSourceClock &mixClock : mixClocks)
        mixClock.reset();

    FfmpegEncoderSettings settings;
    settings.path = path;
    settings.video.width = setup.outputPx.width();
    settings.video.height = setup.outputPx.height();
    settings.video.frameRate = AVRational{qMax(setup.fps, 1), 1};
    settings.sampleRate = kMixRate;
    settings.channels = audioSources > 0 ? kMixChannels : 0;
    encoder = std::make_unique<FfmpegEncoder>();
    if (!encoder->open(settings)) {
        failWith(encoder->errorString());
        return;
    }

    const quint64 current = generation;
    WindowsRecordingStrategy *strategy = owner;
    error.clear();
    if (!video.start(setup.video, [this](const WgcFrameSource::Frame &f) { onFrame(f); },
                     [strategy, current] {
                         QMetaObject::invokeMethod(strategy, [strategy, current] {
                             strategy->reportClosed(current);
                         }, Qt::QueuedConnection);
                     },
                     &error)) {
        failWith(error.isEmpty() ? QStringLiteral("Could not start screen capture.") : error);
    }
}

void WindowsRecordingStrategy::Engine::stopSources()
{
    video.stop();
    systemAudio.stop();
    microphone.stop();
}

void WindowsRecordingStrategy::Engine::failWith(const QString &error)
{
    stopSources();
    encoder.reset();
    mixer.reset();
    lastFrame.reset();
    const quint64 current = generation;
    WindowsRecordingStrategy *strategy = owner;
    QMetaObject::invokeMethod(strategy, [strategy, current, error] {
        strategy->reportFailed(current, error);
    }, Qt::QueuedConnection);
}

void WindowsRecordingStrategy::Engine::onFrame(const WgcFrameSource::Frame &frame)
{
    if (pendingFrames.load() >= kMaxPendingFrames)
        return;   // the encoder is behind: skip this frame, never queue unboundedly
    {
        std::lock_guard lock(clockMutex);
        if (clock.isStarted() && clock.toOutput(frame.timestampUs) < 0)
            return;   // paused
    }
    auto copy = std::make_shared<VideoFrame>();
    copy->width = frame.width;
    copy->height = frame.height;
    copy->timestampUs = frame.timestampUs;
    const size_t row = size_t(frame.width) * 4;
    copy->pixels.resize(row * size_t(frame.height));
    for (int y = 0; y < frame.height; ++y)
        std::memcpy(copy->pixels.data() + row * size_t(y), frame.bgra + size_t(frame.stride) * size_t(y), row);
    ++pendingFrames;
    queue.post([this, copy] {
        encodeVideo(copy);
        --pendingFrames;
    });
}

void WindowsRecordingStrategy::Engine::onAudio(int source, const float *samples, int frames,
                                               qint64 timestampUs)
{
    std::vector<float> copy(samples, samples + size_t(frames) * kMixChannels);
    queue.post([this, source, copy = std::move(copy), timestampUs] {
        encodeAudio(source, copy, timestampUs);
    });
}

bool WindowsRecordingStrategy::Engine::addFrame(const VideoFrame &frame, qint64 outputUs)
{
    Media::Ffmpeg::FramePtr av = Media::Ffmpeg::makeFrame();
    if (!av)
        return false;
    // Borrowed pixels: the encoder converts them before it returns.
    av->format = AV_PIX_FMT_BGRA;
    av->width = frame.width;
    av->height = frame.height;
    av->data[0] = const_cast<uchar *>(frame.pixels.data());
    av->linesize[0] = frame.width * 4;
    av->color_range = AVCOL_RANGE_JPEG;
    return encoder->addVideoFrame(av.get(), outputUs);
}

void WindowsRecordingStrategy::Engine::encodeVideo(const std::shared_ptr<const VideoFrame> &frame)
{
    if (!encoder)
        return;
    bool first = false;
    qint64 outputUs = -1;
    {
        std::lock_guard lock(clockMutex);
        if (!clock.isStarted()) {
            clock.start(frame->timestampUs);   // the first frame anchors the timeline
            first = true;
        }
        outputUs = clock.toOutput(frame->timestampUs);
    }
    if (outputUs < 0)
        return;
    if (!addFrame(*frame, outputUs)) {
        failWith(encoder->errorString());
        return;
    }
    lastFrame = frame;
    lastVideoUs = outputUs;
    if (first) {
        const quint64 current = generation;
        WindowsRecordingStrategy *strategy = owner;
        QMetaObject::invokeMethod(strategy, [strategy, current] {
            strategy->reportStarted(current);
        }, Qt::QueuedConnection);
    }
}

void WindowsRecordingStrategy::Engine::encodeAudio(int source, const std::vector<float> &samples,
                                                   qint64 timestampUs)
{
    if (!encoder || !mixer)
        return;
    qint64 outputUs = -1;
    {
        std::lock_guard lock(clockMutex);
        if (clock.isStarted())
            outputUs = clock.toOutput(timestampUs);
    }
    if (outputUs < 0)
        return;   // before the first frame, or paused
    const std::int64_t frames = std::int64_t(samples.size() / kMixChannels);
    const std::int64_t at = mixClocks[source].place(outputUs * kMixRate / 1000000, frames);
    mixer->mix(at, samples.data(), frames);
    if (!flushMix(false))
        failWith(encoder->errorString());
}

bool WindowsRecordingStrategy::Engine::flushMix(bool drain)
{
    bool ok = true;
    mixer->flush([this, &ok](std::int64_t start, const float *data, std::int64_t frames) {
        ok = encoder->addAudio(data, int(frames), start * 1000000 / kMixRate);
    }, drain);
    return ok;
}

void WindowsRecordingStrategy::Engine::finishEncoding(bool report)
{
    if (!encoder)
        return;   // failed earlier and already reported
    const quint64 current = generation;
    WindowsRecordingStrategy *strategy = owner;
    auto finishWith = [&](bool ok, const QString &error) {
        encoder.reset();
        mixer.reset();
        lastFrame.reset();
        if (!ok)
            QFile::remove(path);
        if (!report)
            return;
        const QString file = path;
        QMetaObject::invokeMethod(strategy, [strategy, current, ok, error, file] {
            if (ok)
                strategy->reportFinished(current, file);
            else
                strategy->reportFailed(current, error);
        }, Qt::QueuedConnection);
    };

    if (lastVideoUs < 0) {
        finishWith(false, QStringLiteral("Nothing was recorded."));
        return;
    }
    if (mixer && !flushMix(true)) {
        finishWith(false, encoder->errorString());
        return;
    }
    // A still screen sends no frames: hold the last one until the moment Stop was pressed.
    qint64 endUs = 0;
    {
        std::lock_guard lock(clockMutex);
        endUs = clock.elapsed(stopAtUs);
    }
    if (lastFrame && endUs - frameIntervalUs > lastVideoUs && !addFrame(*lastFrame, endUs - frameIntervalUs)) {
        finishWith(false, encoder->errorString());
        return;
    }
    const bool ok = encoder->finish();
    finishWith(ok, ok ? QString() : encoder->errorString());
}

qint64 WindowsRecordingStrategy::Engine::elapsedMs()
{
    std::lock_guard lock(clockMutex);
    return clock.elapsed(PauseAwareClock::nowUs()) / 1000;
}

WindowsRecordingStrategy::WindowsRecordingStrategy(QObject *parent)
    : RecordingStrategy(parent), m_engine(std::make_unique<Engine>(this))
{
    m_durationTimer = new QTimer(this);
    m_durationTimer->setInterval(kDurationIntervalMs);
    connect(m_durationTimer, &QTimer::timeout, this, [this] {
        emit durationChanged(m_engine->elapsedMs());
    });
}

WindowsRecordingStrategy::~WindowsRecordingStrategy()
{
    if (m_active && !m_stopping)
        requestFinish(/*report=*/false);
    m_engine.reset();   // drains the encoder thread
}

bool WindowsRecordingStrategy::isAvailable() const
{
    return WgcFrameSource::isSupported();
}

void WindowsRecordingStrategy::start(const RecordTarget &target, const QString &outputPath)
{
    if (m_active)
        return;
    ++m_generation;
    const quint64 generation = m_generation;
    m_stopping = false;
    m_paused = false;
    auto failSoon = [this, generation](const QString &error) {
        m_active = true;
        QMetaObject::invokeMethod(this, [this, generation, error] {
            reportFailed(generation, error);
        }, Qt::QueuedConnection);
    };

    const QRect region = target.regionVirtual;
    QScreen *screen = QGuiApplication::screenAt(region.center());
    if (!screen)
        screen = QGuiApplication::primaryScreen();
    auto *native = screen ? screen->nativeInterface<QNativeInterface::QWindowsScreen>() : nullptr;
    MONITORINFO monitor{};
    monitor.cbSize = sizeof(monitor);
    if (!native || !GetMonitorInfoW(native->handle(), &monitor)) {
        failSoon(QStringLiteral("No display available to record."));
        return;
    }
    const QSize monitorPx(monitor.rcMonitor.right - monitor.rcMonitor.left,
                          monitor.rcMonitor.bottom - monitor.rcMonitor.top);
    const StreamCrop crop = portalStreamCrop(region, screen->geometry(), monitorPx,
                                             /*retinaCapture=*/true);
    if (!crop.valid) {
        failSoon(QStringLiteral("The selected region is outside the display."));
        return;
    }

    Setup setup;
    setup.generation = generation;
    setup.path = outputPath;
    setup.fps = target.fps > 0 ? target.fps : 30;
    setup.video.monitor = quintptr(native->handle());
    setup.video.cropPx = crop.cropPx;
    setup.video.captureCursor = target.captureCursor;
    setup.video.maxFps = setup.fps;
    setup.outputPx = crop.outputPx;
    setup.systemAudio = target.captureSystemAudio;
    setup.mic = target.captureMic;
    setup.micDeviceId = target.micDeviceId;

    m_active = true;
    Engine *engine = m_engine.get();
    engine->queue.post([engine, setup] { engine->begin(setup); });
}

void WindowsRecordingStrategy::requestFinish(bool report)
{
    m_stopping = true;
    m_durationTimer->stop();
    Engine *engine = m_engine.get();
    const qint64 at = PauseAwareClock::nowUs();
    engine->queue.post([engine, at, report] {
        engine->stopSources();
        engine->stopAtUs = at;
        // Behind whatever the sources queued before they stopped.
        engine->queue.post([engine, report] { engine->finishEncoding(report); });
    });
}

void WindowsRecordingStrategy::stop()
{
    if (!m_active || m_stopping)
        return;
    requestFinish(/*report=*/true);
}

void WindowsRecordingStrategy::pause()
{
    if (!m_active || m_stopping || m_paused)
        return;
    {
        std::lock_guard lock(m_engine->clockMutex);
        m_engine->clock.pause(PauseAwareClock::nowUs());
    }
    m_paused = true;
    emit pausedChanged(true);
}

void WindowsRecordingStrategy::resume()
{
    if (!m_active || m_stopping || !m_paused)
        return;
    {
        std::lock_guard lock(m_engine->clockMutex);
        m_engine->clock.resume(PauseAwareClock::nowUs());
    }
    m_paused = false;
    emit pausedChanged(false);
}

void WindowsRecordingStrategy::reportStarted(quint64 generation)
{
    if (generation != m_generation || !m_active || m_stopping)
        return;
    m_durationTimer->start();
    emit started();
}

void WindowsRecordingStrategy::reportFinished(quint64 generation, const QString &path)
{
    if (generation != m_generation || !m_active)
        return;
    m_active = false;
    m_durationTimer->stop();
    emit finished(path);
}

void WindowsRecordingStrategy::reportFailed(quint64 generation, const QString &error)
{
    if (generation != m_generation || !m_active)
        return;
    m_active = false;
    m_durationTimer->stop();
    emit failed(error);
}

void WindowsRecordingStrategy::reportClosed(quint64 generation)
{
    // The window or display went away: keep what was recorded.
    if (generation == m_generation)
        stop();
}

} // namespace Recording
