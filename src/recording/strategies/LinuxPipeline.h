#ifndef RECORDING_LINUXPIPELINE_H
#define RECORDING_LINUXPIPELINE_H

#include <QList>
#include <QRect>
#include <QString>
#include <QStringView>

/**
 * The GStreamer launch strings of the Linux recorder. Only the video source differs
 * between an X11 session (ximagesrc reads the root window directly) and everything
 * else (a ScreenCast portal stream through pipewiresrc); the chain after it is shared.
 * Pure string building, so it's unit-tested without GStreamer.
 */
namespace Recording::LinuxPipeline {

enum class VideoSource { Portal, X11 };

// An xcb client inside a Wayland session (XWayland) sees only other X clients. Without
// ximagesrc (the Flatpak runtime lacks it) an X11 session still records through a portal.
[[nodiscard]] inline VideoSource videoSourceFor(QStringView platformName, bool waylandSession,
                                                bool hasXimagesrc, bool hasPortal)
{
    if (platformName != u"xcb" || waylandSession)
        return VideoSource::Portal;
    return hasXimagesrc || !hasPortal ? VideoSource::X11 : VideoSource::Portal;
}

// What the pipeline needs besides an H.264 encoder.
[[nodiscard]] inline QList<const char *> requiredElements(VideoSource source)
{
    return {source == VideoSource::X11 ? "ximagesrc" : "pipewiresrc",
            "videorate", "videocrop", "videoscale", "videoconvert", "capsfilter", "valve",
            "mp4mux"};
}

[[nodiscard]] inline QString portalSource()
{
    return QStringLiteral("pipewiresrc name=src keepalive-time=1000 resend-last=true "
                          "provide-clock=false");
}

// ximagesrc's endx/endy are inclusive, as QRect::right()/bottom() are.
[[nodiscard]] inline QString x11Source(const QRect &rootPx, bool showPointer, int fps)
{
    return QStringLiteral("ximagesrc name=src use-damage=false show-pointer=%1 "
                          "startx=%2 starty=%3 endx=%4 endy=%5 "
                          "! video/x-raw,framerate=%6/1")
        .arg(showPointer ? QStringLiteral("true") : QStringLiteral("false"))
        .arg(rootPx.left())
        .arg(rootPx.top())
        .arg(rootPx.right())
        .arg(rootPx.bottom())
        .arg(fps);
}

// The source, then everything shared: pause valve, rate cap, crop and scale, encoder, muxer.
[[nodiscard]] inline QString videoChain(const QString &source, int fps, const QString &encoder,
                                        const QString &mux)
{
    return source
           + QStringLiteral(" ! valve name=videovalve drop=false "
                            "! videorate drop-only=true max-rate=%1 skip-to-first=true "
                            "! videocrop name=crop ! videoscale ! videoconvert "
                            "! capsfilter name=outcaps caps=video/x-raw,pixel-aspect-ratio=1/1 "
                            "! queue ! %2 ! queue ! %3 name=mux ! filesink name=sink")
                 .arg(QString::number(fps), encoder, mux);
}

} // namespace Recording::LinuxPipeline

#endif // RECORDING_LINUXPIPELINE_H
