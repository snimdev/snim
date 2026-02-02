#ifndef TESTS_VIDEOBACKENDPROBE_H
#define TESTS_VIDEOBACKENDPROBE_H

/**
 * Asks the platform whether Qt Multimedia actually decodes anything here, so the tests
 * that need a real decode can skip instead of hanging. The offscreen platform on macOS
 * delivers no frames at all, and ctest runs every test offscreen.
 */

#include <QImage>
#include <QSignalSpy>
#include <QString>
#include <QtTest>

#include "editor/video/VideoFrameGrabber.h"

namespace TestSupport {

// One short real decode, cached for the rest of the process.
inline bool videoFramesAvailable()
{
    static const bool available = [] {
        const QString clip = QFINDTESTDATA("data/clip.mp4");
        if (clip.isEmpty())
            return false;

        auto grabber = Editor::Video::VideoFrameGrabber::create();
        QSignalSpy frames(grabber.get(), &Editor::Video::VideoFrameGrabber::frameReady);
        grabber->start(clip, 0, 300, 10, 64);
        // Short on purpose: on a platform that delivers nothing, the probe must not hang.
        frames.wait(5000);
        grabber->cancel();
        return !frames.isEmpty();
    }();
    return available;
}

} // namespace TestSupport

#endif // TESTS_VIDEOBACKENDPROBE_H
