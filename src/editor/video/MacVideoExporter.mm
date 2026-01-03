#include "editor/video/MacVideoExporter.h"

#import <AVFoundation/AVFoundation.h>
#import <CoreMedia/CoreMedia.h>
#import <Foundation/Foundation.h>

#include <QCoreApplication>
#include <QPointer>
#include <QString>

namespace Editor::Video {

// AVFoundation state. ObjC pointer members are ARC-managed (this .mm builds with ARC).
struct MacVideoExporter::Impl {
    AVAssetExportSession *session = nil;   // kept alive for the export's duration
};

MacVideoExporter::MacVideoExporter(QObject *parent)
    : VideoExporter(parent), d(std::make_unique<Impl>())
{
}

MacVideoExporter::~MacVideoExporter()
{
    // A pending completion handler holds only a QPointer to us; after destruction
    // it just no-ops. Cancel so the session doesn't keep exporting pointlessly.
    cancel();
}

void MacVideoExporter::completeExport(bool ok, const QString &path, const QString &error)
{
    d->session = nil;
    if (ok)
        emit finished(path);
    else
        emit failed(error);
}

void MacVideoExporter::trim(const QString &input, const QString &output,
                            qint64 inMs, qint64 outMs)
{
    if (d->session) {
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

void MacVideoExporter::cancel()
{
    if (d->session)
        [d->session cancelExport];   // completion handler still fires (Cancelled)
}

} // namespace Editor::Video
