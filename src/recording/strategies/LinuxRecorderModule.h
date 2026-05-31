#ifndef RECORDING_LINUXRECORDERMODULE_H
#define RECORDING_LINUXRECORDERMODULE_H

#include "recording/RecordingStrategy.h"

#include <QImage>
#include <QList>
#include <QString>
#include <QStringList>

/**
 * The GStreamer half of the Linux recorder ships as its own module,
 * libsnim-recorder-linux.so, which RecordingFactory dlopens instead of linking.
 * GStreamer has to come from the distribution: a pipewiresrc frozen at build time
 * cannot track an evolving PipeWire daemon (a 1.20 build fails on a 1.6 daemon with
 * "error alloc buffers"). Behind dlopen, nothing the app links needs GStreamer at all,
 * so a host without it, or with one too different, still runs: recording falls back to
 * the stub exactly as on an unsupported platform.
 *
 * The module borrows one thing from whoever loads it, the RecordingStrategy base class,
 * so the strategy it returns is the very type the rest of the app already knows.
 * Everything else it carries itself.
 */

// The module's single entry point. Returns a strategy owned by the caller (or by
// `parent`); declared here so both sides of the dlopen agree on the signature.
extern "C" Q_DECL_EXPORT Recording::RecordingStrategy *snimCreateLinuxRecorder(QObject *parent);

// A second entry point for screenshots: one frame from each PipeWire node behind
// pipewireFd (which stays the caller's), into frames[0..count). Blocking.
extern "C" Q_DECL_EXPORT bool snimGrabPipeWireFrames(int pipewireFd, const quint32 *nodeIds,
                                                     int count, int timeoutMs, QImage *frames,
                                                     QString *error);

namespace Recording::LinuxRecorderModule {

inline constexpr char kEntryPoint[] = "snimCreateLinuxRecorder";
inline constexpr char kBaseName[] = "snim-recorder-linux";
inline constexpr char kGrabEntryPoint[] = "snimGrabPipeWireFrames";

// Core::DynamicModule's search order; SNIM_RECORDER_MODULE replaces the whole list.
[[nodiscard]] QStringList candidatePaths(const QString &binDir);

// Loads the module and calls its entry point. Returns nullptr on ANY failure: no module
// on disk, no GStreamer on this host, a symbol that will not resolve.
[[nodiscard]] RecordingStrategy *create(QObject *parent = nullptr);

// Whether the module loads AND its strategy finds the portal, the plugins and an encoder.
[[nodiscard]] bool isAvailable();

// Whether the module loads and exports the frame grabber.
[[nodiscard]] bool canGrabFrames();

// One frame per node, in nodeIds order. Blocking, so call it off the GUI thread.
[[nodiscard]] bool grabFrames(int pipewireFd, const QList<quint32> &nodeIds, int timeoutMs,
                              QList<QImage> *frames, QString *error);

} // namespace Recording::LinuxRecorderModule

#endif // RECORDING_LINUXRECORDERMODULE_H
