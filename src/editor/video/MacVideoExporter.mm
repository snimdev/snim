#include "editor/video/MacVideoExporter.h"

#import <AVFoundation/AVFoundation.h>
#import <CoreMedia/CoreMedia.h>
#import <Foundation/Foundation.h>
#import <ImageIO/ImageIO.h>

#include <QCoreApplication>
#include <QPointer>
#include <QString>
#include <atomic>
#include <memory>

namespace Editor::Video {

// AVFoundation state. ObjC pointer members are ARC-managed (this .mm builds with ARC).
struct MacVideoExporter::Impl {
    AVAssetExportSession *session = nil;       // trim: kept alive for the export
    AVAssetImageGenerator *generator = nil;    // gif: kept alive for the generation
    std::atomic_bool running{false};           // one export at a time (trim OR gif)
    // The gif generator callback runs on its own queue and may outlive this object;
    // it must NOT deref `this`/`d`. A shared cancel flag is held by both the block
    // (keeping it alive) and cancel(), so cancellation crosses threads UAF-free.
    std::shared_ptr<std::atomic_bool> gifCancel;
};

MacVideoExporter::MacVideoExporter(QObject *parent)
    : VideoExporter(parent), d(std::make_unique<Impl>())
{
}

MacVideoExporter::~MacVideoExporter()
{
    // A pending completion handler holds only a QPointer to us; after destruction
    // it just no-ops. Cancel so the backend doesn't keep working pointlessly.
    cancel();
}

void MacVideoExporter::completeExport(bool ok, const QString &path, const QString &error)
{
    // Runs on the Qt thread after the terminal backend callback, so clearing these
    // here races nothing: the session/generator are done by now.
    d->session = nil;
    d->generator = nil;
    d->gifCancel.reset();
    d->running.store(false);
    if (ok)
        emit finished(path);
    else
        emit failed(error);
}

void MacVideoExporter::reportProgress(int done, int total) { emit progress(done, total); }

void MacVideoExporter::trim(const QString &input, const QString &output,
                            qint64 inMs, qint64 outMs)
{
    bool expected = false;
    if (!d->running.compare_exchange_strong(expected, true)) {
        emit failed(QStringLiteral("An export is already running."));
        return;
    }

    NSURL *inputURL = [NSURL fileURLWithPath:input.toNSString()];
    AVURLAsset *asset = [AVURLAsset URLAssetWithURL:inputURL options:nil];

    // Passthrough = remux only: fast and lossless, the cut snapping to safe sample
    // boundaries. Fall back to a re-encode preset when the asset can't pass through.
    AVAssetExportSession *session =
        [[AVAssetExportSession alloc] initWithAsset:asset
                                         presetName:AVAssetExportPresetPassthrough];
    if (!session)
        session = [[AVAssetExportSession alloc] initWithAsset:asset
                                                   presetName:AVAssetExportPresetHighestQuality];
    if (!session) {
        d->running.store(false);
        emit failed(QStringLiteral("Could not start the trim export."));
        return;
    }

    [[NSFileManager defaultManager] removeItemAtPath:output.toNSString() error:nil];
    session.outputURL = [NSURL fileURLWithPath:output.toNSString()];
    session.outputFileType = AVFileTypeMPEG4;   // matches the recorder's container
    session.shouldOptimizeForNetworkUse = YES;
    session.timeRange = CMTimeRangeMake(CMTimeMake(inMs, 1000),
                                        CMTimeMake(outMs - inMs, 1000));

    d->session = session;

    // The handler runs on an AVFoundation queue, possibly after this exporter is
    // gone — capture a weak guard, do all object access on the Qt thread.
    QPointer<MacVideoExporter> weak(this);
    const QString outPath = output;

    [session exportAsynchronouslyWithCompletionHandler:^{
        const AVAssetExportSessionStatus status = session.status;
        const bool ok = (status == AVAssetExportSessionStatusCompleted);
        QString err;
        if (!ok) {
            err = (status == AVAssetExportSessionStatusCancelled)
                      ? QStringLiteral("Export cancelled.")
                      : (session.error ? QString::fromNSString(session.error.localizedDescription)
                                       : QStringLiteral("The trim export failed."));
            // Centralised cleanup: never leave a partial output behind.
            [[NSFileManager defaultManager] removeItemAtURL:session.outputURL error:nil];
        }
        QMetaObject::invokeMethod(QCoreApplication::instance(), [weak, ok, outPath, err] {
            if (weak)
                weak->completeExport(ok, outPath, err);
        }, Qt::QueuedConnection);
    }];
}

void MacVideoExporter::toGif(const QString &input, const QString &output,
                             qint64 inMs, qint64 outMs, const GifParams &paramsIn)
{
    bool expected = false;
    if (!d->running.compare_exchange_strong(expected, true)) {
        emit failed(QStringLiteral("An export is already running."));
        return;
    }

    const GifParams params = paramsIn.clamped();
    const QVector<qint64> frameTimesMs = planGifFrames(inMs, outMs, params.fps);
    const int total = int(frameTimesMs.size());
    if (total <= 0) {
        d->running.store(false);
        emit failed(QStringLiteral("Nothing to export."));
        return;
    }

    NSURL *inputURL = [NSURL fileURLWithPath:input.toNSString()];
    AVURLAsset *asset = [AVURLAsset URLAssetWithURL:inputURL options:nil];

    AVAssetImageGenerator *gen = [[AVAssetImageGenerator alloc] initWithAsset:asset];
    gen.appliesPreferredTrackTransform = YES;          // honor portrait/rotation
    gen.maximumSize = CGSizeMake(params.maxWidth, 0);  // 0 height = preserve aspect
    // Let the generator snap to real sample times near each request (dedupe + speed)
    // rather than re-decoding for frame accuracy a GIF can't show anyway: half a
    // frame interval = 1/(2*fps) seconds of tolerance either side.
    const CMTime tol = CMTimeMake(1, 2 * params.fps);
    gen.requestedTimeToleranceBefore = tol;
    gen.requestedTimeToleranceAfter = tol;
    d->generator = gen;
    auto cancelFlag = std::make_shared<std::atomic_bool>(false);
    d->gifCancel = cancelFlag;

    NSMutableArray<NSValue *> *times = [NSMutableArray arrayWithCapacity:total];
    for (qint64 ms : frameTimesMs)
        [times addObject:[NSValue valueWithCMTime:CMTimeMake(ms, 1000)]];

    // Prepare the GIF destination — a LOCAL owned solely by the callback block, so
    // cancel() (on the Qt thread) never races a CFRelease/AddImage here.
    NSURL *outputURL = [NSURL fileURLWithPath:output.toNSString()];
    [[NSFileManager defaultManager] removeItemAtURL:outputURL error:nil];
    // Literal UTI avoids the UniformTypeIdentifiers framework / deployment-target dance.
    CGImageDestinationRef dest = CGImageDestinationCreateWithURL(
        (__bridge CFURLRef)outputURL, CFSTR("com.compuserve.gif"), (size_t)total, NULL);
    if (!dest) {
        d->generator = nil;
        d->running.store(false);
        emit failed(QStringLiteral("Could not create the GIF file."));
        return;
    }
    NSDictionary *fileProps = @{ (id)kCGImagePropertyGIFDictionary :
                                     @{ (id)kCGImagePropertyGIFLoopCount : @(params.loopCount) } };
    CGImageDestinationSetProperties(dest, (__bridge CFDictionaryRef)fileProps);
    NSDictionary *frameProps = @{ (id)kCGImagePropertyGIFDictionary :
                                      @{ (id)kCGImagePropertyGIFUnclampedDelayTime :
                                             @(gifFrameDelaySec(params.fps)) } };

    QPointer<MacVideoExporter> weak(this);
    const QString outPath = output;
    // __block state mutated only on the (serial) generator callback queue.
    __block int seen = 0;
    __block int added = 0;

    [gen generateCGImagesAsynchronouslyForTimes:times
        completionHandler:^(CMTime, CGImageRef image, CMTime,
                            AVAssetImageGeneratorResult result, NSError *) {
            // NOTE: this block runs on the generator's own queue and may outlive the
            // exporter — it touches only block-local / captured state, never `this`/`d`.
            // Count EVERY callback (succeeded/failed/cancelled) so the terminal
            // condition is reached even when the generator drops or aborts frames —
            // otherwise the editor would hang in its "busy" state forever.
            ++seen;
            if (result == AVAssetImageGeneratorSucceeded && image && !cancelFlag->load()) {
                CGImageDestinationAddImage(dest, image, (__bridge CFDictionaryRef)frameProps);
                ++added;
                const int done = seen;   // plain copy: a __block var can't enter a lambda
                QMetaObject::invokeMethod(QCoreApplication::instance(), [weak, done, total] {
                    if (weak) weak->reportProgress(done, total);
                }, Qt::QueuedConnection);
            }

            if (seen < total)
                return;   // not the last callback yet

            // Terminal callback: finalize (unless cancelled / nothing decoded).
            const bool cancelled = cancelFlag->load();
            bool ok = false;
            if (!cancelled && added > 0)
                ok = CGImageDestinationFinalize(dest);
            CFRelease(dest);
            QString err;
            if (!ok) {
                [[NSFileManager defaultManager] removeItemAtURL:outputURL error:nil];
                err = cancelled ? QStringLiteral("Export cancelled.")
                                : QStringLiteral("The GIF export failed.");
            }
            QMetaObject::invokeMethod(QCoreApplication::instance(), [weak, ok, outPath, err] {
                if (weak)
                    weak->completeExport(ok, outPath, err);
            }, Qt::QueuedConnection);
        }];
}

void MacVideoExporter::cancel()
{
    if (d->gifCancel)
        d->gifCancel->store(true);                  // gif block stops appending + finalizing
    if (d->session)
        [d->session cancelExport];                 // completion handler fires (Cancelled)
    if (d->generator)
        [d->generator cancelAllCGImageGeneration];  // remaining frames -> Cancelled callbacks
}

} // namespace Editor::Video
