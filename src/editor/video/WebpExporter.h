#ifndef EDITOR_VIDEO_WEBPEXPORTER_H
#define EDITOR_VIDEO_WEBPEXPORTER_H

#include "editor/video/AnimationParams.h"
#include "editor/video/VideoFrameGrabber.h"

#include <QImage>
#include <QObject>
#include <QString>
#include <memory>

class QThread;

namespace Editor::Video {

class WebpEncodeWorker;

/**
 * The animated-WebP export path: a frame grabber decodes the trimmed range and
 * libwebp writes it one frame at a time. Shared code rather than a platform seam,
 * so every platform gets it from this one implementation (trimming and GIF stay on
 * VideoExporter, which is platform-specific).
 *
 * The grabber stays on this object's thread (QMediaPlayer and QVideoSink are not
 * movable), and the encode, which is the expensive half, runs on a worker thread.
 *
 * The signals match VideoExporter's exactly, so the editor can wire both objects to
 * the same handlers.
 */
class WebpExporter : public QObject
{
    Q_OBJECT

public:
    explicit WebpExporter(QObject *parent = nullptr);

    // Test seam: drives the given grabber instead of decoding a real recording.
    WebpExporter(std::unique_ptr<VideoFrameGrabber> grabber, QObject *parent = nullptr);
    ~WebpExporter() override;

    // Asynchronously encode [inMs, outMs] of input as an animated WebP at output
    // (overwriting it), per `params`. Exactly one of finished()/failed() fires later,
    // on this object's thread; progress() fires per frame.
    void start(const QString &input, const QString &output,
               qint64 inMs, qint64 outMs, const AnimationParams &params);

    // Stops whatever is running. Silent, so no finished()/failed() follows.
    void cancel();

signals:
    void finished(const QString &outputPath);
    void failed(const QString &error);
    void progress(int done, int total);           // per frame

private:
    void onFrame(const QImage &frame, qint64 sourceMs);
    void onGrabberFinished();
    void onFrameEncoded();
    void onWrote(const QString &outputPath);
    void fail(const QString &error);              // deferred, like the stub's failures
    void reset();                                 // stop and drop the state, silently
    void adopt(std::unique_ptr<VideoFrameGrabber> grabber);   // takes ownership + wires
    [[nodiscard]] VideoFrameGrabber *grabber();   // lazily created + wired
    void throttle();                              // backpressure onto the grabber
    void startWorker();
    void stopWorker();                            // joins the thread, then destroys both

    std::unique_ptr<VideoFrameGrabber> m_grabber;
    QThread *m_thread = nullptr;                  // only alive during an export
    WebpEncodeWorker *m_worker = nullptr;         // lives on m_thread, owned here
    AnimationParams m_params;
    QString m_output;
    int m_total = 0;
    int m_done = 0;                               // frames the worker has encoded
    int m_inFlight = 0;                           // handed to the worker, not yet encoded
    bool m_running = false;
    bool m_paused = false;                        // the grabber is held back, not stopped
    bool m_started = false;                       // the canvas comes from the first frame
};

} // namespace Editor::Video

#endif // EDITOR_VIDEO_WEBPEXPORTER_H
