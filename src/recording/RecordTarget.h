#ifndef RECORDING_RECORDTARGET_H
#define RECORDING_RECORDTARGET_H

#include <QByteArray>
#include <QRect>
#include <QtGlobal>

namespace Recording {

/**
 * Describes what a recording should capture. Built by RecordingController from the
 * AreaSelector result and handed to a RecordingStrategy. It is a plain struct so it
 * can cross the C++/Objective-C++ boundary on macOS without dragging in Qt widgets.
 */
struct RecordTarget {
    enum class Kind { Region, Window };

    Kind kind = Kind::Region;

    // Selected geometry in virtual-desktop logical coordinates (the AreaSelector
    // contract). For a window this is its on-screen rect, used to pick the display
    // and as a fallback when true window capture is unavailable.
    QRect regionVirtual;

    // Platform window handle for true window capture (macOS CGWindowID). 0 = none.
    quint64 windowId = 0;

    // Window only: the system's picker chooses the window once recording starts (the
    // ScreenCast portal's), so neither the id nor the rect is known up front.
    bool systemPicker = false;

    // The webcam-bubble window to KEEP in the capture (CGWindowID) while the rest of
    // our app is excluded. 0 = no camera overlay.
    quint64 cameraWindowId = 0;

    bool captureCursor = true;
    int  fps = 30;

    // Capture pixel scale, a contract EVERY platform strategy must honor: true =
    // native display scale (Retina/HiDPI), false = logical pixels (1x — smaller
    // files, "standard" resolution). Set from Settings via the recording options bar.
    bool retinaCapture = true;

    // Audio options, filled from Settings by RecordingController. System audio is
    // captured via ScreenCaptureKit (macOS 13+); the mic (macOS 15+) is captured by
    // SCK too and manually mixed with system audio into the same single AAC track.
    bool captureSystemAudio = false;
    bool captureMic = false;
    QByteArray micDeviceId;   // QAudioDevice::id() = Core Audio device UID; empty = default input

    [[nodiscard]] bool isValid() const {
        return kind == Kind::Window ? (systemPicker || windowId != 0 || !regionVirtual.isEmpty())
                                    : !regionVirtual.isEmpty();
    }
};

} // namespace Recording

#endif // RECORDING_RECORDTARGET_H
