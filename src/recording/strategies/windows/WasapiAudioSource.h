#ifndef RECORDING_WASAPIAUDIOSOURCE_H
#define RECORDING_WASAPIAUDIOSOURCE_H

#include <QByteArray>
#include <QString>
#include <QtGlobal>

#include <functional>
#include <memory>

namespace Recording {

/**
 * Live audio from WASAPI in the recorder's mix format (48 kHz interleaved float
 * stereo): either loopback of the default playback device (what you hear) or a
 * microphone. Buffers are stamped in microseconds on the QueryPerformanceCounter
 * clock, like the Graphics Capture frames. Loopback delivers nothing while nothing
 * plays, so that source fills the silence itself to keep the track continuous.
 */
class WasapiAudioSource
{
public:
    enum class Kind { Loopback, Microphone };

    static constexpr int kSampleRate = 48000;
    static constexpr int kChannels = 2;

    // Runs on the capture thread; frames counts samples per channel.
    using Handler = std::function<void(const float *interleaved, int frames, qint64 timestampUs)>;

    explicit WasapiAudioSource(Kind kind);
    ~WasapiAudioSource();

    WasapiAudioSource(const WasapiAudioSource &) = delete;
    WasapiAudioSource &operator=(const WasapiAudioSource &) = delete;

    // Whether a default playback (Loopback) or recording (Microphone) device exists.
    [[nodiscard]] static bool hasEndpoint(Kind kind);

    // deviceId is a microphone's QAudioDevice::id() (the endpoint id); empty or
    // unplugged falls back to the default device. Ignored for loopback.
    [[nodiscard]] bool start(const QByteArray &deviceId, Handler handler, QString *error);
    void stop();

private:
    struct Impl;
    std::unique_ptr<Impl> d;
};

} // namespace Recording

#endif // RECORDING_WASAPIAUDIOSOURCE_H
