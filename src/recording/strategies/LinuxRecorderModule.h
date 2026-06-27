#ifndef RECORDING_LINUXRECORDERMODULE_H
#define RECORDING_LINUXRECORDERMODULE_H

#include "recording/RecordingStrategy.h"
#include "screen/PipeWireFrames.h"

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

// The module also exports snimGrabPipeWireFrames, declared in screen/PipeWireFrames.h.

// What the recorder lacks on this host, named with the packages to install. Empty when
// it can record.
extern "C" Q_DECL_EXPORT void snimLinuxRecorderMissingPieces(QStringList *pieces);

namespace Recording::LinuxRecorderModule {

inline constexpr char kEntryPoint[] = "snimCreateLinuxRecorder";
inline constexpr const char *kBaseName = Screen::PipeWireFrames::kModuleBaseName;
inline constexpr char kMissingEntryPoint[] = "snimLinuxRecorderMissingPieces";

// Core::DynamicModule's search order; SNIM_RECORDER_MODULE replaces the whole list.
[[nodiscard]] QStringList candidatePaths(const QString &binDir);

// Loads the module and calls its entry point. Returns nullptr on ANY failure: no module
// on disk, no GStreamer on this host, a symbol that will not resolve.
[[nodiscard]] RecordingStrategy *create(QObject *parent = nullptr);

// Whether the module loads AND its strategy finds the portal, the plugins and an encoder.
[[nodiscard]] bool isAvailable();

// What keeps recording from working here, for the tray and the log. A module that will
// not load is itself the missing piece.
[[nodiscard]] QStringList missingPieces();

} // namespace Recording::LinuxRecorderModule

#endif // RECORDING_LINUXRECORDERMODULE_H
