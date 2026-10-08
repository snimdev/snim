#include "record/strategies/MacRecordingStrategy.h"
#include "record/RecordingGeometry.h"
#include "record/PcmMixBuffer.h"

#import <ScreenCaptureKit/ScreenCaptureKit.h>
#import <AVFoundation/AVFoundation.h>
#import <CoreMedia/CoreMedia.h>
#import <CoreVideo/CoreVideo.h>
#import <CoreGraphics/CoreGraphics.h>

#include <QString>
#include <QRect>
#include <QObject>
#include <QtGlobal>
#include <atomic>
#include <cmath>
#include <memory>
#include <unistd.h>   // getpid (own-app exclusion)

// Microphone capture needs ScreenCaptureKit's captureMicrophone (SDK 15+). Guarded so
// the file still compiles against older SDKs (CI); @available gates it at runtime.
#if defined(MAC_OS_VERSION_15_0) && MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_VERSION_15_0
#define SNIM_SDK_HAS_SCK_MIC 1
#else
#define SNIM_SDK_HAS_SCK_MIC 0
#endif

using Record::MacRecordingStrategy;

namespace {
// Canonical mix format: every audio source is converted to this before summing, and
// the mixed result is what the AAC writer input transcodes.
constexpr double kMixSampleRate = 48000.0;
constexpr int    kMixChannels   = 2;
// Emit a mixed region only once it is this far behind the newest audio seen, so the
// other (slightly later-delivered) source can still land in it.
constexpr int64_t kMixHoldbackFrames = 7200;   // 150 ms @ 48 kHz
// A source whose PTS drifts further than this from its running position re-anchors
// (real discontinuity); smaller jitter keeps buffers contiguous instead.
constexpr int64_t kMixResyncFrames = 2400;     // 50 ms @ 48 kHz
} // namespace

namespace Record {

// ScreenCaptureKit object graph + recording state. Defined here so the header
// stays pure C++. ObjC pointer members are ARC-managed (this .mm builds with ARC).
struct MacRecordingStrategy::Impl {
    MacRecordingStrategy *owner = nullptr;
    SCStream *stream = nil;
    NSObject *output = nil;                 // the SCStreamOutput delegate (kept alive)
    AVAssetWriter *writer = nil;
    AVAssetWriterInput *videoInput = nil;
    AVAssetWriterInputPixelBufferAdaptor *adaptor = nil;
    AVAssetWriterInput *audioInput = nil;   // one AAC track for all audio (nil = video only)
    dispatch_queue_t queue = nil;
    QString outputPath;
    std::atomic_bool recording{false};
    std::atomic_bool paused{false};
    std::atomic_bool needResumeAdjust{false};
    bool sessionStarted = false;
    CMTime firstPts = kCMTimeInvalid;
    CMTime pauseStartPts = kCMTimeInvalid;   // pts when the current pause began
    CMTime pausedDuration = kCMTimeZero;     // total paused time, excluded from output
    qint64 lastDurationMs = -1000;

    // Audio mixing (only while the mic is captured): every source is converted to the
    // canonical mix format, summed on a shared timeline, and the mix feeds audioInput.
    // With no mic, system audio keeps the direct passthrough path in the handler.
    struct AudioSource {
        AVAudioConverter *converter = nil;   // source format -> canonical mix format
        AVAudioFormat *inFormat = nil;       // cached to detect mid-recording format changes
        MixSourceClock clock{kMixResyncFrames};
    };
    AVAudioFormat *mixFormat = nil;                  // 48 kHz stereo float32 interleaved
    CMAudioFormatDescriptionRef mixDesc = nullptr;   // format of the emitted mixed buffers
    std::unique_ptr<PcmMixBuffer> mixer;             // null = no mixing (mic off)
    AudioSource audioSources[2];                     // [0] system audio, [1] microphone

    ~Impl() { if (mixDesc) CFRelease(mixDesc); }
};

} // namespace Record

// ---- Manual audio mix (mic + system audio -> one AAC track) -------------------------
// All three run on the strategy's single serial sample queue, so the mixer needs no
// locking. CF objects created here are not ARC-managed and are released manually.

// Append one settled chunk of mixed interleaved float PCM to the AAC writer input,
// timestamped on the output timeline (frame index / 48 kHz).
static void appendMixedChunk(MacRecordingStrategy::Impl *impl, int64_t startFrame,
                             const float *data, int64_t frames)
{
    const size_t byteLen = size_t(frames) * kMixChannels * sizeof(float);
    CMBlockBufferRef block = nullptr;
    if (CMBlockBufferCreateWithMemoryBlock(kCFAllocatorDefault, nullptr, byteLen,
                                           kCFAllocatorDefault, nullptr, 0, byteLen,
                                           kCMBlockBufferAssureMemoryNowFlag,
                                           &block) != kCMBlockBufferNoErr || !block)
        return;
    CMBlockBufferReplaceDataBytes(data, block, 0, byteLen);
    CMSampleBufferRef sbuf = nullptr;
    if (CMAudioSampleBufferCreateReadyWithPacketDescriptions(
            kCFAllocatorDefault, block, impl->mixDesc, (CMItemCount) frames,
            CMTimeMake(startFrame, (int32_t) kMixSampleRate), nullptr, &sbuf) == noErr && sbuf) {
        [impl->audioInput appendSampleBuffer:sbuf];
        CFRelease(sbuf);
    }
    CFRelease(block);
}

// Emit every settled mix region (drain = everything left, on stop). Deferred while
// the writer input is busy — the audio just stays in the mix buffer for the next call.
static void flushMixer(MacRecordingStrategy::Impl *impl, bool drain)
{
    if (!impl->mixer || !impl->audioInput)
        return;
    if (!drain && !impl->audioInput.isReadyForMoreMediaData)
        return;
    impl->mixer->flush([impl](int64_t startFrame, const float *data, int64_t frames) {
        appendMixedChunk(impl, startFrame, data, frames);
    }, drain);
}

// Convert one SCK PCM sample buffer (system audio or mic — any rate / channel count /
// interleaving) to the canonical mix format with the per-source AVAudioConverter,
// place it on the shared timeline, and sum it into the mix buffer.
static void mixAudioSample(MacRecordingStrategy::Impl *impl, CMSampleBufferRef sampleBuffer,
                           CMTime outPts, int sourceIndex)
{
    CMAudioFormatDescriptionRef fdesc =
        (CMAudioFormatDescriptionRef) CMSampleBufferGetFormatDescription(sampleBuffer);
    const AudioStreamBasicDescription *asbd =
        fdesc ? CMAudioFormatDescriptionGetStreamBasicDescription(fdesc) : nullptr;
    const CMItemCount frames = CMSampleBufferGetNumSamples(sampleBuffer);
    if (!asbd || asbd->mSampleRate <= 0 || frames <= 0)
        return;

    // Wrap the incoming PCM in an AVAudioPCMBuffer for the converter.
    AVAudioFormat *inFormat = [[AVAudioFormat alloc] initWithStreamDescription:asbd];
    if (!inFormat)
        return;
    AVAudioPCMBuffer *inBuf = [[AVAudioPCMBuffer alloc] initWithPCMFormat:inFormat
                                                            frameCapacity:(AVAudioFrameCount) frames];
    if (!inBuf)
        return;
    inBuf.frameLength = (AVAudioFrameCount) frames;
    if (CMSampleBufferCopyPCMDataIntoAudioBufferList(sampleBuffer, 0, (int32_t) frames,
                                                     inBuf.mutableAudioBufferList) != noErr)
        return;

    // Lazy per-source converter, rebuilt if the device/format changes mid-recording.
    auto &source = impl->audioSources[sourceIndex];
    if (!source.converter || ![source.inFormat isEqual:inFormat]) {
        source.converter = [[AVAudioConverter alloc] initFromFormat:inFormat
                                                           toFormat:impl->mixFormat];
        source.inFormat = inFormat;
        source.clock.reset();
        if (!source.converter)
            return;
    }

    const double ratio = kMixSampleRate / asbd->mSampleRate;
    const auto capacity = (AVAudioFrameCount) llround(frames * ratio) + 64;
    AVAudioPCMBuffer *outBuf = [[AVAudioPCMBuffer alloc] initWithPCMFormat:impl->mixFormat
                                                             frameCapacity:capacity];
    if (!outBuf)
        return;
    __block BOOL fed = NO;
    NSError *cerr = nil;
    const AVAudioConverterOutputStatus status =
        [source.converter convertToBuffer:outBuf error:&cerr
                       withInputFromBlock:^AVAudioBuffer *(AVAudioPacketCount,
                                                           AVAudioConverterInputStatus *st) {
            if (fed) { *st = AVAudioConverterInputStatus_NoDataNow; return nil; }
            fed = YES;
            *st = AVAudioConverterInputStatus_HaveData;
            return inBuf;
        }];
    if (status == AVAudioConverterOutputStatus_Error || outBuf.frameLength == 0)
        return;

    // The PTS suggests the position; the source clock keeps successive buffers
    // contiguous so rounding/resampler jitter can't punch clicks into the mix.
    const int64_t ptsFrame = llround(CMTimeGetSeconds(outPts) * kMixSampleRate);
    const int64_t at = source.clock.place(ptsFrame, outBuf.frameLength);
    impl->mixer->mix(at, (const float *) outBuf.audioBufferList->mBuffers[0].mData,
                     outBuf.frameLength);
    flushMixer(impl, false);
}

// Receives ScreenCaptureKit sample buffers (on a serial queue) and stream-stop
// callbacks, and feeds frames to the AVAssetWriter. All Qt signal emission hops
// back to the owner's thread via QMetaObject::invokeMethod.
@interface SnimStreamOutput : NSObject <SCStreamOutput, SCStreamDelegate>
@property (nonatomic, assign) MacRecordingStrategy::Impl *impl;
@end

@implementation SnimStreamOutput

- (void)stream:(SCStream *)stream
    didOutputSampleBuffer:(CMSampleBufferRef)sampleBuffer
                   ofType:(SCStreamOutputType)type
{
    MacRecordingStrategy::Impl *impl = self.impl;
    if (!impl || !impl->recording.load())
        return;
    if (!CMSampleBufferIsValid(sampleBuffer) || CMSampleBufferGetNumSamples(sampleBuffer) == 0)
        return;

    if (type == SCStreamOutputTypeScreen) {
        [self handleVideoSample:sampleBuffer impl:impl];
        return;
    }
#if SNIM_SDK_HAS_SCK_MIC
    if (@available(macOS 15.0, *)) {
        if (type == SCStreamOutputTypeMicrophone) {
            [self handleAudioSample:sampleBuffer impl:impl source:1];
            return;
        }
    }
#endif
    if (@available(macOS 13.0, *)) {
        if (type == SCStreamOutputTypeAudio)
            [self handleAudioSample:sampleBuffer impl:impl source:0];
    }
}

// Screen frames: feed the pixel-buffer adaptor, retimed by the pause-aware clock.
- (void)handleVideoSample:(CMSampleBufferRef)sampleBuffer impl:(MacRecordingStrategy::Impl *)impl
{
    // Append only "complete" frames; skip idle/blank/suspended ones.
    CFArrayRef attachmentsCF = CMSampleBufferGetSampleAttachmentsArray(sampleBuffer, false);
    if (attachmentsCF && CFArrayGetCount(attachmentsCF) > 0) {
        NSDictionary *attach = (__bridge NSDictionary *)CFArrayGetValueAtIndex(attachmentsCF, 0);
        NSNumber *status = attach[SCStreamFrameInfoStatus];
        if (status && status.intValue != SCFrameStatusComplete)
            return;
    }
    if (!impl->writer || !impl->videoInput)
        return;

    const CMTime pts = CMSampleBufferGetPresentationTimeStamp(sampleBuffer);
    MacRecordingStrategy *owner = impl->owner;

    if (!impl->sessionStarted) {
        impl->firstPts = pts;
        [impl->writer startSessionAtSourceTime:kCMTimeZero];   // we feed relative times
        impl->sessionStarted = true;
        QMetaObject::invokeMethod(owner, [owner] { owner->reportStarted(); }, Qt::QueuedConnection);
    }

    // While paused: remember where the pause began and drop frames.
    if (impl->paused.load()) {
        if (CMTIME_IS_INVALID(impl->pauseStartPts))
            impl->pauseStartPts = pts;
        return;
    }
    // First frame after a resume: fold the paused gap into pausedDuration so the
    // output timeline skips it (rather than showing a frozen segment).
    if (impl->needResumeAdjust.exchange(false)) {
        if (CMTIME_IS_VALID(impl->pauseStartPts))
            impl->pausedDuration = CMTimeAdd(impl->pausedDuration,
                                             CMTimeSubtract(pts, impl->pauseStartPts));
        impl->pauseStartPts = kCMTimeInvalid;
    }

    if (!impl->videoInput.isReadyForMoreMediaData)
        return;
    CVImageBufferRef image = CMSampleBufferGetImageBuffer(sampleBuffer);
    if (!image)
        return;

    // Output time = elapsed since the first frame, minus all paused time.
    const CMTime outPts = CMTimeSubtract(CMTimeSubtract(pts, impl->firstPts), impl->pausedDuration);
    [impl->adaptor appendPixelBuffer:image withPresentationTime:outPts];

    const qint64 ms = (qint64) std::llround(CMTimeGetSeconds(outPts) * 1000.0);
    if (ms - impl->lastDurationMs >= 200) {   // throttle the elapsed-time updates
        impl->lastDurationMs = ms;
        QMetaObject::invokeMethod(owner, [owner, ms] { owner->reportDuration(ms); },
                                  Qt::QueuedConnection);
    }
}

// Audio (source 0 = system, 1 = microphone): retime each PCM buffer with the SAME
// pause-aware clock as video, then either sum it into the manual mix (mic active) or
// hand it straight to the AAC writer input (the writer transcodes PCM -> AAC). Runs on
// the same serial queue as the video handler, so the bookkeeping needs no locking.
- (void)handleAudioSample:(CMSampleBufferRef)sampleBuffer impl:(MacRecordingStrategy::Impl *)impl
                   source:(int)sourceIndex
{
    if (!impl->writer || !impl->audioInput || !impl->sessionStarted)
        return;   // video anchors the timeline; drop audio until the first frame

    const CMTime pts = CMSampleBufferGetPresentationTimeStamp(sampleBuffer);

    if (impl->paused.load()) {
        if (CMTIME_IS_INVALID(impl->pauseStartPts))
            impl->pauseStartPts = pts;
        return;
    }
    // Whichever handler (audio or video) sees the first sample after a resume folds
    // the paused gap into pausedDuration — both run on this one queue, so the other
    // stream picks up the adjusted clock and neither timeline jumps backwards.
    if (impl->needResumeAdjust.exchange(false)) {
        if (CMTIME_IS_VALID(impl->pauseStartPts))
            impl->pausedDuration = CMTimeAdd(impl->pausedDuration,
                                             CMTimeSubtract(pts, impl->pauseStartPts));
        impl->pauseStartPts = kCMTimeInvalid;
    }

    const CMTime outPts = CMTimeSubtract(CMTimeSubtract(pts, impl->firstPts), impl->pausedDuration);
    if (CMTimeCompare(outPts, kCMTimeZero) < 0)
        return;   // straddles the session start; skip

    // Mic capture active: every audio source goes through the manual mix.
    if (impl->mixer) {
        mixAudioSample(impl, sampleBuffer, outPts, sourceIndex);
        return;
    }
    if (sourceIndex != 0)
        return;   // mic without a mixer (setup failed): never feed raw mic PCM through
    if (!impl->audioInput.isReadyForMoreMediaData)
        return;

    CMSampleTimingInfo timing = {};
    if (CMSampleBufferGetSampleTimingInfo(sampleBuffer, 0, &timing) != noErr)
        return;
    timing.presentationTimeStamp = outPts;
    timing.decodeTimeStamp = kCMTimeInvalid;
    CMSampleBufferRef retimed = NULL;   // CF object: not ARC-managed, release manually
    if (CMSampleBufferCreateCopyWithNewTiming(kCFAllocatorDefault, sampleBuffer, 1, &timing,
                                              &retimed) != noErr || !retimed)
        return;
    [impl->audioInput appendSampleBuffer:retimed];
    CFRelease(retimed);
}

- (void)stream:(SCStream *)stream didStopWithError:(NSError *)error
{
    MacRecordingStrategy::Impl *impl = self.impl;
    if (!impl)
        return;
    if (!impl->recording.exchange(false))
        return;   // an explicit stop() is already finalizing
    MacRecordingStrategy *owner = impl->owner;
    QString msg = error ? QString::fromNSString(error.localizedDescription)
                        : QStringLiteral("Recording stopped unexpectedly.");
    QMetaObject::invokeMethod(owner, [owner, msg] { owner->reportFailed(msg); },
                              Qt::QueuedConnection);
}

@end

namespace Record {

MacRecordingStrategy::MacRecordingStrategy(QObject *parent)
    : RecordingStrategy(parent), d(std::make_unique<Impl>())
{
    d->owner = this;
    d->queue = dispatch_queue_create("dev.snim.recording", DISPATCH_QUEUE_SERIAL);
}

MacRecordingStrategy::~MacRecordingStrategy()
{
    if (d->stream)
        [d->stream stopCaptureWithCompletionHandler:^(NSError *){}];
}

bool MacRecordingStrategy::isRecording() const { return d->recording.load(); }

bool MacRecordingStrategy::isAvailable() const
{
    if (@available(macOS 12.3, *))
        return true;
    return false;
}

void MacRecordingStrategy::reportStarted() { emit started(); }
void MacRecordingStrategy::reportFinished(const QString &path) { d->recording.store(false); emit finished(path); }
void MacRecordingStrategy::reportFailed(const QString &error) { d->recording.store(false); emit failed(error); }
void MacRecordingStrategy::reportDuration(qint64 ms) { emit durationChanged(ms); }

void MacRecordingStrategy::start(const RecordTarget &target, const QString &outputPath)
{
    if (d->recording.load())
        return;

    if (@available(macOS 12.3, *)) {
        d->outputPath = outputPath;
        d->sessionStarted = false;
        d->firstPts = kCMTimeInvalid;
        d->lastDurationMs = -1000;

        const QRect region = target.regionVirtual;
        const bool captureCursor = target.captureCursor;
        const bool retinaCapture = target.retinaCapture;
        const int fps = target.fps > 0 ? target.fps : 30;
        const bool isWindow = (target.kind == RecordTarget::Kind::Window);
        const quint64 windowId = target.windowId;
        const quint64 cameraWindowId = target.cameraWindowId;
        const bool captureSystemAudio = target.captureSystemAudio;
        const bool captureMic = target.captureMic;
        const QByteArray micDeviceId = target.micDeviceId;
        MacRecordingStrategy *self = this;
        Impl *impl = d.get();

        // Enumerate shareable content (async; the first call triggers the TCC
        // Screen Recording prompt). A null result / error means permission denied.
        [SCShareableContent getShareableContentWithCompletionHandler:^(SCShareableContent *content, NSError *error) {
            if (error || !content) {
                QString msg = error ? QString::fromNSString(error.localizedDescription)
                    : QStringLiteral("Screen Recording permission is required. Enable Snim in "
                                     "System Settings > Privacy & Security > Screen Recording.");
                QMetaObject::invokeMethod(self, [self, msg] { self->reportFailed(msg); }, Qt::QueuedConnection);
                return;
            }

            // Helpers shared by both paths.
            auto evenPx = [](double v) { long n = std::lround(v); if (n < 2) n = 2; return (int) (n % 2 == 0 ? n : n + 1); };
            auto displayScale = [](CGDirectDisplayID did) {
                double s = 1.0;
                if (CGDisplayModeRef m = CGDisplayCopyDisplayMode(did)) {
                    const double pxW = CGDisplayModeGetPixelWidth(m), ptW = CGDisplayModeGetWidth(m);
                    if (ptW > 0) s = pxW / ptW;
                    CGDisplayModeRelease(m);
                }
                return s;
            };

            SCContentFilter *filter = nil;
            CGRect sourceRect = CGRectNull;   // set for the region + window-fallback paths
            int pxW = 0, pxH = 0;

            if (isWindow) {
                // True window capture: match the picked CGWindowID to its SCWindow so
                // the recording follows the window and excludes overlapping windows.
                SCWindow *scWindow = nil;
                for (SCWindow *w in content.windows) {
                    if ((quint64) w.windowID == windowId) { scWindow = w; break; }
                }
                if (!scWindow) {
                    QMetaObject::invokeMethod(self, [self] { self->reportFailed(QStringLiteral("That window is no longer available.")); }, Qt::QueuedConnection);
                    return;
                }
                const CGRect wf = scWindow.frame;
                SCDisplay *wd = nil;
                const CGPoint wc = CGPointMake(CGRectGetMidX(wf), CGRectGetMidY(wf));
                for (SCDisplay *cand in content.displays)
                    if (CGRectContainsPoint(cand.frame, wc)) { wd = cand; break; }
                if (!wd) wd = content.displays.firstObject;
                const double scale = (retinaCapture && wd) ? displayScale(wd.displayID) : 1.0;
                pxW = evenPx(wf.size.width * scale);
                pxH = evenPx(wf.size.height * scale);

                if (@available(macOS 14.0, *)) {
                    filter = [[SCContentFilter alloc] initWithDesktopIndependentWindow:scWindow];
                } else if (wd) {
                    filter = [[SCContentFilter alloc] initWithDisplay:wd includingWindows:@[scWindow]];
                    const QRect localW = Record::displayLocalRect(
                        QRect((int) wf.origin.x, (int) wf.origin.y, (int) wf.size.width, (int) wf.size.height),
                        QRect((int) wd.frame.origin.x, (int) wd.frame.origin.y,
                              (int) wd.frame.size.width, (int) wd.frame.size.height));
                    sourceRect = CGRectMake(localW.x(), localW.y(), localW.width(), localW.height());
                }
            } else {
                // Region capture: a display cropped to the selection, EXCLUDING all of
                // our own windows so the Stop control etc. never appear in the file.
                SCDisplay *display = nil;
                const QPoint c = region.center();
                for (SCDisplay *cand in content.displays)
                    if (CGRectContainsPoint(cand.frame, CGPointMake(c.x(), c.y()))) { display = cand; break; }
                if (!display) display = content.displays.firstObject;
                if (!display) {
                    QMetaObject::invokeMethod(self, [self] { self->reportFailed(QStringLiteral("No display available to record.")); }, Qt::QueuedConnection);
                    return;
                }
                const CGRect f = display.frame;
                const QRect screenGeo((int) f.origin.x, (int) f.origin.y, (int) f.size.width, (int) f.size.height);
                const QRect local = Record::displayLocalRect(region, screenGeo)
                                        .intersected(QRect(0, 0, screenGeo.width(), screenGeo.height()));
                if (local.isEmpty()) {
                    QMetaObject::invokeMethod(self, [self] { self->reportFailed(QStringLiteral("The selected region is outside the display.")); }, Qt::QueuedConnection);
                    return;
                }
                // 1x ("standard") capture: record at logical pixels instead of native scale.
                const double scale = retinaCapture ? displayScale(display.displayID) : 1.0;
                pxW = evenPx(local.width() * scale);
                pxH = evenPx(local.height() * scale);
                sourceRect = CGRectMake(local.x(), local.y(), local.width(), local.height());

                // Exclude our whole app (current AND future windows) so the floating
                // Stop control is never captured, whatever order it appears in — but
                // KEEP the webcam bubble (cameraWindowId) so it IS recorded.
                const pid_t myPid = getpid();
                SCRunningApplication *me = nil;
                for (SCRunningApplication *app in content.applications)
                    if (app.processID == myPid) { me = app; break; }
                SCWindow *cameraWin = nil;
                if (cameraWindowId != 0)
                    for (SCWindow *w in content.windows)
                        if ((quint64) w.windowID == cameraWindowId) { cameraWin = w; break; }
                if (@available(macOS 13.0, *)) {
                    filter = [[SCContentFilter alloc] initWithDisplay:display
                                                excludingApplications:(me ? @[me] : @[])
                                                      exceptingWindows:(cameraWin ? @[cameraWin] : @[])];
                } else {
                    NSMutableArray<SCWindow *> *mine = [NSMutableArray array];
                    for (SCWindow *w in content.windows)
                        if (w.owningApplication && w.owningApplication.processID == myPid && w != cameraWin)
                            [mine addObject:w];
                    filter = [[SCContentFilter alloc] initWithDisplay:display excludingWindows:mine];
                }
            }

            if (!filter) {
                QMetaObject::invokeMethod(self, [self] { self->reportFailed(QStringLiteral("Could not set up screen capture for the target.")); }, Qt::QueuedConnection);
                return;
            }

            SCStreamConfiguration *config = [[SCStreamConfiguration alloc] init];
            if (!CGRectIsNull(sourceRect))
                config.sourceRect = sourceRect;
            config.width = pxW;
            config.height = pxH;
            config.minimumFrameInterval = CMTimeMake(1, fps);
            config.showsCursor = captureCursor;
            config.pixelFormat = kCVPixelFormatType_32BGRA;
            config.queueDepth = 6;

            // System audio (macOS 13+): SCK captures it alongside the screen; the
            // handler retimes it on the same pause-aware clock, so Pause excludes
            // audio too. Our own process audio stays out (no feedback loops).
            bool wantSystemAudio = false;
            if (captureSystemAudio) {
                if (@available(macOS 13.0, *)) {
                    config.capturesAudio = YES;
                    config.sampleRate = 48000;
                    config.channelCount = 2;
                    config.excludesCurrentProcessAudio = YES;
                    wantSystemAudio = true;
                }
            }

            // Microphone (macOS 15+): SCK captures the chosen input device as a third
            // sample-buffer stream; on older systems the recording simply proceeds
            // without the mic. The first capture triggers the TCC microphone prompt.
            bool wantMic = false;
#if SNIM_SDK_HAS_SCK_MIC
            if (captureMic) {
                if (@available(macOS 15.0, *)) {
                    config.captureMicrophone = YES;
                    // Settings stores QAudioDevice::id() — the Core Audio device UID,
                    // which is exactly AVCaptureDevice.uniqueID. Validate it so a saved
                    // but unplugged device falls back to the system default instead of
                    // failing the whole stream.
                    if (!micDeviceId.isEmpty()) {
                        NSString *uid = QString::fromUtf8(micDeviceId).toNSString();
                        if ([AVCaptureDevice deviceWithUniqueID:uid])
                            config.microphoneCaptureDeviceID = uid;
                    }
                    wantMic = true;
                }
            }
#else
            Q_UNUSED(captureMic);
            Q_UNUSED(micDeviceId);
#endif
            const bool wantAudio = wantSystemAudio || wantMic;

            // AVAssetWriter: H.264 into an MP4 container. Read the path from the Impl,
            // NOT the start() parameter: this handler runs asynchronously, after that
            // reference argument has gone out of scope (capturing it dangled -> nil URL).
            NSString *nsPath = impl->outputPath.toNSString();
            if (nsPath.length == 0) {
                QMetaObject::invokeMethod(self, [self] { self->reportFailed(QStringLiteral("No output path for the recording.")); }, Qt::QueuedConnection);
                return;
            }
            NSURL *url = [NSURL fileURLWithPath:nsPath];
            [[NSFileManager defaultManager] removeItemAtURL:url error:nil];
            NSError *werr = nil;
            AVAssetWriter *writer = [[AVAssetWriter alloc] initWithURL:url fileType:AVFileTypeMPEG4 error:&werr];
            if (!writer) {
                QString msg = werr ? QString::fromNSString(werr.localizedDescription)
                                   : QStringLiteral("Could not create the output file.");
                QMetaObject::invokeMethod(self, [self, msg] { self->reportFailed(msg); }, Qt::QueuedConnection);
                return;
            }
            NSDictionary *settings = @{ AVVideoCodecKey : AVVideoCodecTypeH264,
                                        AVVideoWidthKey : @(pxW),
                                        AVVideoHeightKey : @(pxH) };
            AVAssetWriterInput *input = [[AVAssetWriterInput alloc] initWithMediaType:AVMediaTypeVideo
                                                                       outputSettings:settings];
            input.expectsMediaDataInRealTime = YES;
            AVAssetWriterInputPixelBufferAdaptor *adaptor =
                [[AVAssetWriterInputPixelBufferAdaptor alloc] initWithAssetWriterInput:input
                                                           sourcePixelBufferAttributes:nil];
            if ([writer canAddInput:input])
                [writer addInput:input];
            // One AAC track carries all audio. The writer transcodes the PCM it is fed:
            // SCK's buffers directly for system-audio-only, or the manually mixed
            // system+mic PCM when the mic is captured.
            AVAssetWriterInput *audioInput = nil;
            if (wantAudio) {
                AudioChannelLayout layout = {};
                layout.mChannelLayoutTag = kAudioChannelLayoutTag_Stereo;
                NSDictionary *audioSettings = @{
                    AVFormatIDKey : @(kAudioFormatMPEG4AAC),
                    AVSampleRateKey : @48000,
                    AVNumberOfChannelsKey : @2,
                    AVEncoderBitRateKey : @192000,
                    AVChannelLayoutKey : [NSData dataWithBytes:&layout length:sizeof(layout)] };
                audioInput = [[AVAssetWriterInput alloc] initWithMediaType:AVMediaTypeAudio
                                                            outputSettings:audioSettings];
                audioInput.expectsMediaDataInRealTime = YES;
                if ([writer canAddInput:audioInput])
                    [writer addInput:audioInput];
            }
            // Fragments keep a killed recording readable; a writer that refuses them raises.
            @try {
                writer.movieFragmentInterval = CMTimeMakeWithSeconds(1, 600);
            } @catch (NSException *exception) {
                qWarning("Recording without movie fragments: %s",
                         qPrintable(QString::fromNSString(exception.reason)));
            }
            [writer startWriting];
            impl->writer = writer;
            impl->videoInput = input;
            impl->adaptor = adaptor;
            impl->audioInput = audioInput;

            // Mic active -> manual mix: convert every source to one canonical PCM
            // format, sum on a shared timeline, feed the single AAC track. The holdback
            // only matters when TWO sources must line up; a lone mic flushes instantly.
            impl->mixer.reset();
            impl->audioSources[0] = {};
            impl->audioSources[1] = {};
            if (impl->mixDesc) { CFRelease(impl->mixDesc); impl->mixDesc = nullptr; }
            impl->mixFormat = nil;
            if (wantMic && audioInput) {
                AVAudioFormat *mixFormat =
                    [[AVAudioFormat alloc] initWithCommonFormat:AVAudioPCMFormatFloat32
                                                     sampleRate:kMixSampleRate
                                                       channels:kMixChannels
                                                    interleaved:YES];
                CMAudioFormatDescriptionRef mixDesc = nullptr;
                if (mixFormat &&
                    CMAudioFormatDescriptionCreate(kCFAllocatorDefault, mixFormat.streamDescription,
                                                   0, nullptr, 0, nullptr, nullptr,
                                                   &mixDesc) == noErr) {
                    impl->mixFormat = mixFormat;
                    impl->mixDesc = mixDesc;
                    impl->mixer = std::make_unique<Record::PcmMixBuffer>(
                        kMixChannels, wantSystemAudio ? kMixHoldbackFrames : 0);
                }
            }
            impl->paused.store(false);
            impl->needResumeAdjust.store(false);
            impl->pauseStartPts = kCMTimeInvalid;
            impl->pausedDuration = kCMTimeZero;

            SnimStreamOutput *out = [[SnimStreamOutput alloc] init];
            out.impl = impl;
            impl->output = out;

            SCStream *stream = [[SCStream alloc] initWithFilter:filter configuration:config delegate:out];
            NSError *addErr = nil;
            if (![stream addStreamOutput:out type:SCStreamOutputTypeScreen sampleHandlerQueue:impl->queue error:&addErr]) {
                QString msg = addErr ? QString::fromNSString(addErr.localizedDescription)
                                     : QStringLiteral("Could not attach the capture output.");
                QMetaObject::invokeMethod(self, [self, msg] { self->reportFailed(msg); }, Qt::QueuedConnection);
                return;
            }
            if (wantSystemAudio) {
                if (@available(macOS 13.0, *)) {
                    // Same serial queue as video: keeps the shared pause/timeline
                    // bookkeeping race-free between the two handlers.
                    NSError *audioAddErr = nil;
                    if (![stream addStreamOutput:out type:SCStreamOutputTypeAudio
                               sampleHandlerQueue:impl->queue error:&audioAddErr]) {
                        QString msg = audioAddErr ? QString::fromNSString(audioAddErr.localizedDescription)
                                                  : QStringLiteral("Could not attach the audio output.");
                        QMetaObject::invokeMethod(self, [self, msg] { self->reportFailed(msg); }, Qt::QueuedConnection);
                        return;
                    }
                }
            }
#if SNIM_SDK_HAS_SCK_MIC
            if (wantMic) {
                if (@available(macOS 15.0, *)) {
                    // Also on the one serial queue: mic, system audio and video share
                    // the pause clock and the mix buffer without locks.
                    NSError *micAddErr = nil;
                    if (![stream addStreamOutput:out type:SCStreamOutputTypeMicrophone
                               sampleHandlerQueue:impl->queue error:&micAddErr]) {
                        QString msg = micAddErr ? QString::fromNSString(micAddErr.localizedDescription)
                                                : QStringLiteral("Could not attach the microphone output.");
                        QMetaObject::invokeMethod(self, [self, msg] { self->reportFailed(msg); }, Qt::QueuedConnection);
                        return;
                    }
                }
            }
#endif
            impl->stream = stream;
            impl->recording.store(true);

            [stream startCaptureWithCompletionHandler:^(NSError *startErr) {
                if (startErr) {
                    impl->recording.store(false);
                    QString msg = QString::fromNSString(startErr.localizedDescription);
                    QMetaObject::invokeMethod(self, [self, msg] {
                        self->reportFailed(msg.isEmpty() ? QStringLiteral("Could not start screen capture.") : msg);
                    }, Qt::QueuedConnection);
                }
                // On success, reportStarted() fires from the first delivered frame.
            }];
        }];
    } else {
        reportFailed(QStringLiteral("Screen recording requires macOS 12.3 or later."));
    }
}

void MacRecordingStrategy::stop()
{
    SCStream *stream = d->stream;
    if (!stream) {
        d->recording.store(false);
        return;
    }
    d->recording.store(false);
    MacRecordingStrategy *self = this;
    Impl *impl = d.get();

    [stream stopCaptureWithCompletionHandler:^(NSError *) {
      // Finalize on the sample queue: any in-flight buffer handlers run first, then
      // the mixer drains its held-back tail into the AAC input — race-free, no locks.
      dispatch_async(impl->queue, ^{
        if (impl->sessionStarted)
            flushMixer(impl, true);

        AVAssetWriterInput *input = impl->videoInput;
        AVAssetWriterInput *audio = impl->audioInput;
        AVAssetWriter *writer = impl->writer;
        if (input)
            [input markAsFinished];
        if (audio)
            [audio markAsFinished];

        if (writer && writer.status == AVAssetWriterStatusWriting) {
            const QString path = impl->outputPath;
            [writer finishWritingWithCompletionHandler:^{
                if (writer.status == AVAssetWriterStatusCompleted) {
                    QMetaObject::invokeMethod(self, [self, path] { self->reportFinished(path); }, Qt::QueuedConnection);
                } else {
                    QString err = writer.error ? QString::fromNSString(writer.error.localizedDescription)
                                               : QStringLiteral("Failed to finalize the recording.");
                    QMetaObject::invokeMethod(self, [self, err] { self->reportFailed(err); }, Qt::QueuedConnection);
                }
            }];
        } else {
            QMetaObject::invokeMethod(self, [self] { self->reportFailed(QStringLiteral("Nothing was recorded.")); }, Qt::QueuedConnection);
        }

        impl->stream = nil;
        impl->output = nil;
        impl->writer = nil;
        impl->videoInput = nil;
        impl->adaptor = nil;
        impl->audioInput = nil;
        impl->mixer.reset();
        impl->audioSources[0] = {};
        impl->audioSources[1] = {};
        impl->mixFormat = nil;
        if (impl->mixDesc) { CFRelease(impl->mixDesc); impl->mixDesc = nullptr; }
      });
    }];
}

void MacRecordingStrategy::pause()
{
    if (!d->recording.load() || d->paused.load())
        return;
    d->paused.store(true);               // the sample handler drops frames while set
    emit pausedChanged(true);
}

void MacRecordingStrategy::resume()
{
    if (!d->recording.load() || !d->paused.load())
        return;
    d->needResumeAdjust.store(true);     // next frame folds the gap into pausedDuration
    d->paused.store(false);
    emit pausedChanged(false);
}

bool MacRecordingStrategy::isPaused() const { return d->paused.load(); }

} // namespace Record
