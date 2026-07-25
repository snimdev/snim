#ifndef RECORDING_LINUXPIPELINE_H
#define RECORDING_LINUXPIPELINE_H

#include <QByteArray>
#include <QList>
#include <QRect>
#include <QString>
#include <QStringView>

#include <algorithm>

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

// What the one-frame PipeWire grab behind ScreenCast screenshots needs.
[[nodiscard]] inline QList<const char *> frameGrabElements()
{
    return {"pipewiresrc", "videoconvert", "appsink"};
}

// What a session recording from `source` needs besides an encoder: the recorder's
// elements, plus the frame grab's outside X11, where ScreenCast screenshots never run.
[[nodiscard]] inline QList<const char *> sessionElements(VideoSource source)
{
    QList<const char *> elements = requiredElements(source);
    if (source == VideoSource::X11)
        return elements;
    for (const char *element : frameGrabElements()) {
        const bool listed = std::any_of(elements.cbegin(), elements.cend(),
                                        [element](const char *have) {
                                            return qstrcmp(have, element) == 0;
                                        });
        if (!listed)
            elements << element;
    }
    return elements;
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

// One window by its X id, wherever it moves: ximagesrc reads it window-relative and
// renegotiates as it resizes, and the pinned output caps scale every size to the first.
[[nodiscard]] inline QString x11WindowSource(quint64 xid, bool showPointer, int fps)
{
    return QStringLiteral("ximagesrc name=src xid=%1 use-damage=false show-pointer=%2 "
                          "! video/x-raw,framerate=%3/1")
        .arg(xid)
        .arg(showPointer ? QStringLiteral("true") : QStringLiteral("false"))
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
