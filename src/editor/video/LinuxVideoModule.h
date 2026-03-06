#ifndef EDITOR_VIDEO_LINUXVIDEOMODULE_H
#define EDITOR_VIDEO_LINUXVIDEOMODULE_H

#include "editor/video/VideoExporter.h"

#include <QString>
#include <QStringList>

/**
 * The GStreamer trim exporter ships as its own module, libsnim-video-linux.so, which
 * VideoExporter::create dlopens instead of linking, for the same reasons as the
 * recorder module: GStreamer has to be the distribution's own, and a host without it
 * (or without the plugins) must still run, with trimming falling back to the stub.
 * Separate from the recorder module so each borrows only its own base class from the
 * host and the two fail independently.
 */

// The module's single entry point. Returns an exporter owned by the caller (or by
// `parent`); declared here so both sides of the dlopen agree on the signature.
extern "C" Q_DECL_EXPORT Editor::Video::VideoExporter *snimCreateLinuxVideoExporter(QObject *parent);

namespace Editor::Video::LinuxVideoModule {

inline constexpr char kEntryPoint[] = "snimCreateLinuxVideoExporter";
inline constexpr char kBaseName[] = "snim-video-linux";

// Core::DynamicModule's search order; SNIM_VIDEO_MODULE replaces the whole list.
[[nodiscard]] QStringList candidatePaths(const QString &binDir);

// Loads the module and calls its entry point. Returns nullptr on ANY failure.
[[nodiscard]] VideoExporter *create(QObject *parent = nullptr);

} // namespace Editor::Video::LinuxVideoModule

#endif // EDITOR_VIDEO_LINUXVIDEOMODULE_H
