#include "editor/video/WebpExporter.h"

#include "editor/video/WebpEncoder.h"

#include <QFile>
#include <QMetaObject>
#include <QThread>
#include <atomic>

namespace Editor::Video {

namespace {
// Frames wait in the worker's queue at full size (a 600x400 RGBA frame is about 1 MB), so
// the grabber is held back instead of letting a long clip queue hundreds of megabytes.
constexpr int kHighWaterFrames = 8;
constexpr int kLowWaterFrames = 2;
} // namespace

/**
 * Lives on the worker thread and is the only thing that touches WebpEncoder: a lossy
 * encode at 10 fps is hundreds of compressions, which the GUI thread cannot afford.
 *
 * Every call arrives as a queued post from the exporter, so the frames are encoded in
 * the order they were grabbed.
 */
class WebpEncodeWorker : public QObject
{
    Q_OBJECT

public:
    void begin(const QString &path, const QSize &size, const AnimationParams &params)
    {
        if (stopped())
            return;
        QString error;
        m_path = path;
        if (!m_encoder.begin(path, size, params, &error))
            breakWith(error);
    }

    void addFrame(const QImage &frame, qint64 sourceMs)
    {
        if (stopped())
            return;
        QString error;
        if (!m_encoder.addFrame(frame, sourceMs, &error)) {
            breakWith(error);
            return;
        }
        emit frameEncoded();
    }

    void finish()
    {
        if (stopped())
            return;
        QString error;
        if (!m_encoder.finish(&error)) {
            breakWith(error);
            return;
        }
        // A cancel can land after the check above, by which point the file is committed,
        // so the finished export is thrown away rather than left behind.
        if (m_aborted.load(std::memory_order_relaxed)) {
            QFile::remove(m_path);
            return;
        }
        emit wrote(m_path);
    }

    // Called from the GUI thread while the worker may still be encoding, so the frames
    // already queued behind it are dropped instead of encoded for nobody.
    void abort() { m_aborted.store(true, std::memory_order_relaxed); }

signals:
    void frameEncoded();
    void wrote(const QString &outputPath);
    void failed(const QString &error);

private:
    // One failure ends the run: without this a bad frame would fail once per frame left.
    void breakWith(const QString &error)
    {
        m_broken = true;
        m_encoder.cancel();   // no partial file survives a failure
        emit failed(error);
    }

    [[nodiscard]] bool stopped() const
    {
        return m_broken || m_aborted.load(std::memory_order_relaxed);
    }

    WebpEncoder m_encoder;
    QString m_path;
    bool m_broken = false;                 // worker thread only
    std::atomic<bool> m_aborted{false};    // written by the GUI thread
};

WebpExporter::WebpExporter(QObject *parent) : QObject(parent) {}

WebpExporter::WebpExporter(std::unique_ptr<VideoFrameGrabber> grabber, QObject *parent)
    : QObject(parent)
{
    adopt(std::move(grabber));
}

WebpExporter::~WebpExporter()
{
    reset();   // never leave the worker thread running past this object
}

void WebpExporter::adopt(std::unique_ptr<VideoFrameGrabber> grabber)
{
    m_grabber = std::move(grabber);
    connect(m_grabber.get(), &VideoFrameGrabber::frameReady, this, &WebpExporter::onFrame);
    connect(m_grabber.get(), &VideoFrameGrabber::finished, this,
            &WebpExporter::onGrabberFinished);
    connect(m_grabber.get(), &VideoFrameGrabber::failed, this, &WebpExporter::fail);
}

VideoFrameGrabber *WebpExporter::grabber()
{
    // Created on first use: the real grabber builds a QMediaPlayer, which the editor must
    // not pay for just by opening a window.
    if (!m_grabber) {
        // No QObject parent: the unique_ptr is the only owner, so teardown order is never
        // in question.
        adopt(VideoFrameGrabber::create(nullptr));
    }
    return m_grabber.get();
}

void WebpExporter::startWorker()
{
    m_thread = new QThread;
    m_thread->setObjectName(QStringLiteral("webp-encode"));
    m_worker = new WebpEncodeWorker;   // no parent: stopWorker() is the only owner
    m_worker->moveToThread(m_thread);
    // Queued because the worker is on another thread; every handler re-checks m_running,
    // since a disconnect cannot recall a signal that is already in this thread's queue.
    connect(m_worker, &WebpEncodeWorker::frameEncoded, this, &WebpExporter::onFrameEncoded);
    connect(m_worker, &WebpEncodeWorker::wrote, this, &WebpExporter::onWrote);
    connect(m_worker, &WebpEncodeWorker::failed, this, &WebpExporter::fail);
    m_thread->start();
}

void WebpExporter::stopWorker()
{
    if (!m_worker)
        return;
    m_worker->abort();          // whatever is still queued is dropped, not encoded
    m_worker->disconnect(this);
    m_thread->quit();
    m_thread->wait();           // blocks for at most the frame being encoded right now
    delete m_worker;            // and with it the encoder, so no partial file is left
    delete m_thread;
    m_worker = nullptr;
    m_thread = nullptr;
}

void WebpExporter::start(const QString &input, const QString &output,
                         qint64 inMs, qint64 outMs, const AnimationParams &params)
{
    cancel();   // one export at a time: a previous one is abandoned, not queued

    if (input.isEmpty() || output.isEmpty()) {
        fail(QStringLiteral("Nothing to export."));
        return;
    }

    m_params = params.clamped();
    m_output = output;
    m_total = planAnimationFrames(inMs, outMs, m_params.fps).size();
    m_done = 0;
    m_inFlight = 0;
    m_paused = false;
    m_started = false;
    startWorker();
    m_running = true;

    grabber()->start(input, inMs, outMs, m_params.fps, m_params.maxWidth);
}

void WebpExporter::onFrame(const QImage &frame, qint64 sourceMs)
{
    if (!m_running || !m_worker)
        return;

    auto *worker = m_worker;
    if (!m_started) {
        // The first frame is what fixes the canvas, so the encoder cannot start earlier.
        const QSize size = frame.size();
        const AnimationParams params = m_params;
        const QString path = m_output;
        QMetaObject::invokeMethod(worker, [worker, path, size, params] {
            worker->begin(path, size, params);
        }, Qt::QueuedConnection);
        m_started = true;
    }
    QMetaObject::invokeMethod(worker, [worker, frame, sourceMs] {
        worker->addFrame(frame, sourceMs);
    }, Qt::QueuedConnection);
    ++m_inFlight;
    throttle();
}

void WebpExporter::onGrabberFinished()
{
    if (!m_running || !m_worker)
        return;
    if (!m_started) {
        fail(QStringLiteral("No frames could be read from this recording."));
        return;
    }

    // A queued post is FIFO, so this necessarily runs after every frame already posted.
    auto *worker = m_worker;
    QMetaObject::invokeMethod(worker, [worker] { worker->finish(); }, Qt::QueuedConnection);
}

void WebpExporter::onFrameEncoded()
{
    if (!m_running)
        return;
    --m_inFlight;
    ++m_done;
    throttle();
    emit progress(m_done, m_total);   // encoded frames, not grabbed ones
}

void WebpExporter::throttle()
{
    if (!m_grabber)
        return;
    if (!m_paused && m_inFlight >= kHighWaterFrames) {
        m_paused = true;
        m_grabber->pause();
    } else if (m_paused && m_inFlight <= kLowWaterFrames) {
        m_paused = false;
        m_grabber->resume();
    }
}

void WebpExporter::onWrote(const QString &outputPath)
{
    if (!m_running)
        return;
    reset();
    emit finished(outputPath);
}

void WebpExporter::fail(const QString &error)
{
    reset();
    // Deferred like the stub's failures, so a caller that connects after calling start()
    // still sees it.
    QMetaObject::invokeMethod(this, [this, error] { emit failed(error); }, Qt::QueuedConnection);
}

void WebpExporter::cancel()
{
    reset();
}

void WebpExporter::reset()
{
    // Cleared first: it is what makes a signal already queued from the worker a no-op.
    m_running = false;
    if (m_grabber)
        m_grabber->cancel();
    stopWorker();
    m_output.clear();
    m_total = 0;
    m_done = 0;
    m_inFlight = 0;
    m_paused = false;
    m_started = false;
}

} // namespace Editor::Video

#include "WebpExporter.moc"
