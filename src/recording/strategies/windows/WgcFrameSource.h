#ifndef RECORDING_WGCFRAMESOURCE_H
#define RECORDING_WGCFRAMESOURCE_H

#include <QRect>
#include <QSize>
#include <QString>
#include <QtGlobal>

#include <functional>
#include <memory>

namespace Recording {

/**
 * Screen frames from Windows Graphics Capture: one monitor (optionally cropped to a
 * pixel rect) or one window, read back as BGRA through a staging texture. Frames
 * arrive on a capture thread pool, throttled to maxFps, stamped in microseconds on
 * the QueryPerformanceCounter clock (the one PauseAwareClock::nowUs() reads). The
 * header stays free of Windows and C++/WinRT headers; they live in the .cpp.
 */
class WgcFrameSource
{
public:
    struct Target {
        quintptr monitor = 0;     // HMONITOR, used when no window is set
        quintptr window = 0;      // HWND
        QRect cropPx;             // in the monitor's pixels; empty takes all of it
        bool captureCursor = true;
        int maxFps = 30;
    };

    // Valid only for the duration of the handler call.
    struct Frame {
        const uchar *bgra = nullptr;
        int stride = 0;
        int width = 0;
        int height = 0;
        qint64 timestampUs = 0;
    };

    using FrameHandler = std::function<void(const Frame &frame)>;
    using ClosedHandler = std::function<void()>;

    WgcFrameSource();
    ~WgcFrameSource();

    WgcFrameSource(const WgcFrameSource &) = delete;
    WgcFrameSource &operator=(const WgcFrameSource &) = delete;

    // Graphics Capture with cursor control (Windows 10 2004 or later).
    [[nodiscard]] static bool isSupported();

    // Handlers run on capture threads, never after stop() returns. onClosed fires
    // when the captured window or monitor goes away.
    [[nodiscard]] bool start(const Target &target, FrameHandler onFrame,
                             ClosedHandler onClosed, QString *error);
    void stop();

    // The captured monitor's or window's pixel size when capture started.
    [[nodiscard]] QSize itemSize() const;

private:
    struct Impl;
    std::unique_ptr<Impl> d;
};

} // namespace Recording

#endif // RECORDING_WGCFRAMESOURCE_H
