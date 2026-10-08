#ifndef SCREEN_PIPEWIREFRAMES_H
#define SCREEN_PIPEWIREFRAMES_H

#include <QImage>
#include <QList>
#include <QString>
#include <QtGlobal>

/**
 * One-frame PipeWire grabs for the ScreenCast frame source. The grabber needs GStreamer,
 * so it lives in the dlopened recorder module (libsnim-recorder-linux) and is reached
 * here by name, never linked; see Record::LinuxRecorderModule for why.
 */

// Exported by the recorder module: one frame from each PipeWire node behind pipewireFd
// (which stays the caller's), into frames[0..count). Blocking.
extern "C" Q_DECL_EXPORT bool snimGrabPipeWireFrames(int pipewireFd, const quint32 *nodeIds,
                                                     int count, int timeoutMs, QImage *frames,
                                                     QString *error);

// Exported by the recorder module: whether GStreamer has every element the grab needs;
// missing names the absent ones otherwise.
extern "C" Q_DECL_EXPORT bool snimCanGrabPipeWireFrames(QString *missing);

namespace Screen::PipeWireFrames {

inline constexpr char kModuleBaseName[] = "snim-recorder-linux";
inline constexpr char kModuleOverride[] = "SNIM_RECORDER_MODULE";
inline constexpr char kGrabEntryPoint[] = "snimGrabPipeWireFrames";
inline constexpr char kCanGrabEntryPoint[] = "snimCanGrabPipeWireFrames";

// Whether the module loads, exports the frame grabber and finds its GStreamer elements;
// missing says what is not there otherwise.
[[nodiscard]] bool canGrab(QString *missing = nullptr);

// One frame per node, in nodeIds order. Blocking, so call it off the GUI thread.
[[nodiscard]] bool grab(int pipewireFd, const QList<quint32> &nodeIds, int timeoutMs,
                        QList<QImage> *frames, QString *error);

} // namespace Screen::PipeWireFrames

#endif // SCREEN_PIPEWIREFRAMES_H
