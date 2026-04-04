#include "editor/video/FfmpegVideoExporter.h"

#include "media/ffmpeg/FfmpegTrimTranscoder.h"

#include <QDebug>
#include <QFile>
#include <QtConcurrent/QtConcurrentRun>

#include <atomic>

namespace Editor::Video {

namespace {

// Progress is posted to the GUI thread no more often than this.
constexpr qint64 kProgressStepUs = 100'000;

} // namespace

struct FfmpegVideoExporter::Job {
    Job(qint64 inUs, qint64 outUs) : transcoder(inUs, outUs) {}

    Media::Ffmpeg::FfmpegTrimTranscoder transcoder;
    QString output;
    qint64 lastProgressUs = -kProgressStepUs;   // touched only by the worker
};

FfmpegVideoExporter::FfmpegVideoExporter(QObject *parent) : VideoExporter(parent)
{
}

FfmpegVideoExporter::~FfmpegVideoExporter()
{
    cancel();
}

bool FfmpegVideoExporter::isAvailable() const
{
    return avcodec_find_decoder(AV_CODEC_ID_H264) && avcodec_find_encoder(AV_CODEC_ID_AAC)
           && avcodec_find_encoder_by_name("libx264") && av_guess_format("mp4", nullptr, nullptr)
           && av_guess_format("mov", nullptr, nullptr);
}

void FfmpegVideoExporter::trim(const QString &input, const QString &output,
                               qint64 inMs, qint64 outMs)
{
    if (m_job) {
        failLater(tr("An export is already running."));
        return;
    }
    if (outMs <= inMs) {
        failLater(tr("The trim range is empty."));
        return;
    }

    const quint64 generation = ++m_generation;
    auto job = std::make_shared<Job>(inMs * 1000, outMs * 1000);
    job->output = output;
    job->transcoder.setProgressCallback([this, job = job.get(), generation](qint64 doneUs,
                                                                             qint64 totalUs) {
        if (doneUs - job->lastProgressUs < kProgressStepUs && doneUs < totalUs)
            return;
        job->lastProgressUs = doneUs;
        QMetaObject::invokeMethod(this, [this, generation, doneUs, totalUs] {
            if (generation == m_generation)
                emit progress(int(doneUs / 1000), int(totalUs / 1000));
        }, Qt::QueuedConnection);
    });
    m_job = job;

    // The destructor waits for the worker, so `this` outlives every call made from it.
    m_worker = QtConcurrent::run([this, job, input, output, generation] {
        const bool ok = job->transcoder.run(input, output);
        if (job->transcoder.wasCancelled())
            return;
        const QString error = job->transcoder.errorString();
        QMetaObject::invokeMethod(this, [this, generation, ok, error] {
            complete(generation, ok, error);
        }, Qt::QueuedConnection);
    });
}

void FfmpegVideoExporter::cancel()
{
    if (!m_job)
        return;
    ++m_generation;   // drops a completion already queued
    m_job->transcoder.cancel();
    m_worker.waitForFinished();
    // A run that finished just before the cancel still leaves nothing behind.
    QFile::remove(m_job->output);
    m_job.reset();
}

void FfmpegVideoExporter::failLater(const QString &error)
{
    // Deferred, so callers connect before the signal fires.
    QMetaObject::invokeMethod(this, [this, error] { emit failed(error); }, Qt::QueuedConnection);
}

void FfmpegVideoExporter::complete(quint64 generation, bool ok, const QString &error)
{
    if (generation != m_generation || !m_job)
        return;
    const QString output = m_job->output;
    m_worker.waitForFinished();
    m_job.reset();
    if (ok) {
        emit finished(output);
        return;
    }
    qWarning() << "FFmpeg trim failed:" << error;
    emit failed(tr("The trim export failed: %1").arg(error));
}

} // namespace Editor::Video
