#include "record/RecordingController.h"

#include "record/RecordingFactory.h"
#include "record/RecordingJournal.h"
#include "record/RecordingStrategy.h"
#include "record/CameraBubble.h"
#include "record/RecordingFrameOverlay.h"
#include "record/RecordingOptionsBar.h"
#include "screen/AreaSelector.h"
#include "screen/SelectorGroup.h"
#include "screen/WindowEnumerator.h"
#include "core/FileNames.h"
#include "core/Settings.h"
#include "screen/OverlayWindows.h"

#include <QDir>
#include <QDateTime>
#include <QStandardPaths>
#include <QScreen>
#include <QGuiApplication>
#include <QPermissions>
#include <QPixmap>
#include <QPointer>
#include <QList>

#include <utility>

namespace Record {

RecordingController::RecordingController(QObject *parent)
    : RecordingController(RecordingFactory::createStrategy(), parent)
{
}

RecordingController::RecordingController(std::unique_ptr<RecordingStrategy> strategy, QObject *parent)
    : QObject(parent), m_strategy(std::move(strategy))
{
    wireStrategy();
}

RecordingController::~RecordingController()
{
    delete m_cameraBubble;   // top-level windows, not parented to the controller
    delete m_frameOverlay;
}

void RecordingController::ensureCameraBubble(const QRect &regionVirtual)
{
    if (!Core::Settings::cameraEnabled()) {
        destroyCameraBubble();
        return;
    }
    if (!m_cameraBubble)
        m_cameraBubble = new CameraBubble();
    m_cameraBubble->setCameraDevice(Core::Settings::cameraDeviceId());
    // Park before mapping: a Wayland layer surface binds its output at creation, so
    // the recorded region has to pick the screen before the bubble is shown.
    if (!regionVirtual.isEmpty())
        m_cameraBubble->moveToRegionCorner(regionVirtual);
    m_cameraBubble->show();
    m_cameraBubble->raise();
    m_cameraBubble->startCamera();
}

void RecordingController::destroyCameraBubble()
{
    if (m_cameraBubble) {
        m_cameraBubble->stopCamera();
        m_cameraBubble->hide();
        m_cameraBubble->deleteLater();
        m_cameraBubble = nullptr;
    }
}

// Recording frame: dim + border marking the recorded region for the whole recording.
// macOS's capture filter leaves it out of the video; elsewhere it stays outside the region.
void RecordingController::showFrameOverlay()
{
    if (m_activeRegion.isEmpty() || !Core::Settings::recordingFrameEnabled())
        return;
    if (!m_frameOverlay)
        m_frameOverlay = new RecordingFrameOverlay();
    m_frameOverlay->showForRegion(m_activeRegion);
    if (m_cameraBubble)
        m_cameraBubble->raise();   // same window level: keep the bubble above the dim
}

void RecordingController::destroyFrameOverlay()
{
    if (m_frameOverlay) {
        m_frameOverlay->hide();
        m_frameOverlay->deleteLater();
        m_frameOverlay = nullptr;
    }
}

quint64 RecordingController::cameraBubbleWindowId() const
{
    if (m_cameraBubble)
        return Screen::nativeWindowId(m_cameraBubble);
    return 0;
}

bool RecordingController::isAvailable() const
{
    return m_strategy && m_strategy->isAvailable();
}

void RecordingController::startRecording(const RecordTarget &target)
{
    if (m_state != State::Idle)
        return;                       // already recording or starting
    if (!target.isValid())
        return;
    if (!isAvailable()) {
        emit recordingFailed(tr("Screen recording is unavailable on this system."));
        return;
    }
    m_state = State::Starting;        // becomes Recording once the backend signals started()
    m_outputPath = makeOutputPath();
    RecordingJournal::add(m_outputPath);
    m_strategy->start(target, m_outputPath);
}

void RecordingController::stop()
{
    if (m_state == State::Idle)
        return;
    m_strategy->stop();
}

void RecordingController::togglePause()
{
    if (m_state != State::Recording || !m_strategy)
        return;
    if (m_strategy->isPaused())
        m_strategy->resume();
    else
        m_strategy->pause();
}

void RecordingController::recordArea()
{
    if (m_state != State::Idle)
        return;
    if (!isAvailable()) {
        emit recordingFailed(tr("Screen recording is unavailable on this system."));
        return;
    }
    presentSelection(/*windowPick=*/false);
}

void RecordingController::recordWindow()
{
    if (m_state != State::Idle)
        return;
    if (!isAvailable()) {
        emit recordingFailed(tr("Screen recording is unavailable on this system."));
        return;
    }
    if (m_strategy->windowCapture() == RecordingStrategy::WindowCapture::SystemPicked) {
        beginRecordingForSelection(QRect(), /*windowId=*/0, /*isWindow=*/true);
        return;
    }
    presentSelection(/*windowPick=*/true);
}

bool RecordingController::resolveInputPermissions(const std::function<void()> &done)
{
    // The selection overlays sit at the macOS shielding window level (ABOVE system
    // dialogs), so a TCC permission prompt fired while they are up opens underneath
    // them where it can never be answered. This gate therefore runs only at the two
    // overlay-free moments: before the overlays appear, and after teardown at commit
    // (inputs may have been switched on via the options bar mid-selection). The
    // m_asked* flags cap each request at one per gate run, so a backend that answers
    // without changing the status (e.g. a missing usage description) cannot loop us.
    if (Core::Settings::cameraEnabled() && !m_askedCameraPermission) {
        QCameraPermission cameraPermission;
        switch (qApp->checkPermission(cameraPermission)) {
        case Qt::PermissionStatus::Undetermined:
            m_askedCameraPermission = true;
            qApp->requestPermission(cameraPermission, this,
                                    [done](const QPermission &) { done(); });
            return true;
        case Qt::PermissionStatus::Denied:
            // Record anyway: the bubble shows its own "no access" state.
            emit recordingWarning(tr("Camera access is denied. Grant Snim camera access "
                                     "in your system privacy settings."));
            break;
        case Qt::PermissionStatus::Granted:
            break;
        }
    }
    if (Core::Settings::micEnabled() && !m_askedMicPermission) {
        QMicrophonePermission micPermission;
        switch (qApp->checkPermission(micPermission)) {
        case Qt::PermissionStatus::Undetermined:
            // Asking now also keeps the system prompt out of the recording itself
            // (ScreenCaptureKit would otherwise trigger it right as capture starts).
            m_askedMicPermission = true;
            qApp->requestPermission(micPermission, this,
                                    [done](const QPermission &) { done(); });
            return true;
        case Qt::PermissionStatus::Denied:
            emit recordingWarning(tr("Microphone access is denied, so your narration "
                                     "won't be recorded. Grant Snim microphone access "
                                     "in your system privacy settings."));
            break;
        case Qt::PermissionStatus::Granted:
            break;
        }
    }
    return false;
}

void RecordingController::presentSelection(bool windowPick)
{
    m_state = State::Selecting;   // swallow re-triggers while a prompt/overlay is up
    if (resolveInputPermissions([this, windowPick] {
            m_state = State::Idle;
            presentSelection(windowPick);
        }))
        return;
    // Past the gate: this attempt is done asking. Let the commit gate ask afresh
    // for anything the user enables on the options bar during selection.
    m_askedCameraPermission = false;
    m_askedMicPermission = false;

    // The grabber is a member and dies with the controller, so a plain `this` is safe.
    m_frameGrabber.grab([this, windowPick](const QPixmap &frozen, const QRect &virtualGeometry) {
        if (frozen.isNull()) {
            m_state = State::Idle;
            if (m_frameGrabber.wasCancelled()) {   // the user dismissed the system's dialog
                emit recordingCancelled();
                return;
            }
            const QString why = m_frameGrabber.lastError();
            emit recordingFailed(why.isEmpty() ? tr("Could not capture the screen for selection.")
                                               : why);
            return;
        }

        // Show the webcam bubble now (if enabled) so the user sees it while selecting;
        // it is re-ensured at commit (the options bar can toggle it mid-selection).
        ensureCameraBubble();

        Screen::SelectorGroup::Options options;
        if (windowPick) {
            options.mode = Screen::AreaSelector::Mode::WindowPick;
            // Each candidate window's rect + id, for true window capture.
            options.windows = Screen::enumerateWindowInfos();
        }
        // Recording has no Edit/Copy/Save toolbar; the overlay layer covers the panels too.
        options.layerSurface = true;
        auto *group = new Screen::SelectorGroup(frozen, virtualGeometry, options, this);
        const QList<Screen::AreaSelector *> selectors = group->selectors();

        // The inline options bar: camera/mic/audio/fps/scale write Settings directly
        // (the commit handler re-reads them); record and cancel drive the selectors.
        // Off macOS it is a CHILD of the overlay under the cursor, so the compositor
        // cannot stack the overlay above it and move() works on Wayland.
#ifdef Q_OS_MACOS
        auto *optionsBar = new RecordingOptionsBar();
#else
        // QCursor::pos() is (0,0) on Wayland, so start on the primary screen and let the
        // first hover reparent the bar to the overlay the user is actually working on.
        Screen::AreaSelector *barParent = selectors.value(0);
        const QList<QScreen *> screens = QGuiApplication::screens();
        for (int i = 0; i < screens.size() && i < selectors.size(); ++i)
            if (screens.at(i) == QGuiApplication::primaryScreen())
                barParent = selectors.at(i);
        auto *optionsBar = new RecordingOptionsBar(barParent);
#endif
        optionsBar->setRecordVisible(!windowPick);   // in window-pick the click commits
        connect(optionsBar, &RecordingOptionsBar::cameraToggled, this, [this](bool on) {
            // Never fire a TCC prompt while the shielding overlays are up (it would
            // open underneath them): only touch the bubble live when access is already
            // granted; otherwise the commit-time permission gate handles it.
            if (on) {
                QCameraPermission cameraPermission;
                if (qApp->checkPermission(cameraPermission) != Qt::PermissionStatus::Granted)
                    return;
            }
            ensureCameraBubble();   // honors the just-written setting: creates or destroys
        });
        connect(optionsBar, &RecordingOptionsBar::recordRequested, group, [selectors] {
            // All overlays share the synced selection; the first one in Adjusting
            // commits, which closes them all.
            for (auto *sel : selectors)
                if (sel->commitCurrentSelection())
                    break;
        });
        connect(optionsBar, &RecordingOptionsBar::cancelRequested, group, [selectors] {
            if (!selectors.isEmpty())
                selectors.first()->cancelSelection();   // mirrors Esc
        });
        optionsBar->show();
        optionsBar->raise();
        // AFTER show() returns: Qt re-applies its own window level while making the
        // window visible, which would bury the bar beneath the shielding-level overlay
        // if we only configured from showEvent (same reason configureOverlayWindow is
        // applied to the selectors post-show above).
        Screen::configureSelectionHud(optionsBar);

        const auto dropBar = [bar = QPointer<RecordingOptionsBar>(optionsBar)] {
            if (bar) {
                bar->hide();
                bar->deleteLater();
            }
        };
        connect(group, &Screen::SelectorGroup::areaSelected, this,
                [this, dropBar, windowPick](const QRect &area) {
                    dropBar();
                    m_state = State::Idle;
                    if (area.isEmpty()) {           // cancelled (Esc / ✕ / empty)
                        destroyCameraBubble();
                        return;
                    }
                    if (windowPick)                 // window success handled in windowPicked
                        return;
                    beginRecordingForSelection(area, /*windowId=*/0, /*isWindow=*/false);
                });
        connect(group, &Screen::SelectorGroup::windowPicked, this,
                [this, dropBar](const QRect &area, quint64 windowId) {
                    dropBar();
                    m_state = State::Idle;
                    if (area.isEmpty() && windowId == 0) {   // nothing under the cursor
                        destroyCameraBubble();
                        return;
                    }
                    beginRecordingForSelection(area, windowId, /*isWindow=*/true);
                });
        // The bar follows the pointer's overlay; the bubble previews its corner (2 == Adjusting).
        connect(group, &Screen::SelectorGroup::liveStateChanged, this,
                [this, bar = QPointer<RecordingOptionsBar>(optionsBar)](
                    Screen::AreaSelector *selector, const QRect &sel, int phase) {
#ifndef Q_OS_MACOS
                    if (bar)
                        bar->attachToOverlay(selector);   // a no-op once it is the parent
#else
                    Q_UNUSED(selector);
                    Q_UNUSED(bar);
#endif
                    if (m_cameraBubble && phase == 2 && !sel.isEmpty())
                        m_cameraBubble->moveToRegionCorner(sel);
                });
    });
}

void RecordingController::beginRecordingForSelection(const QRect &area, quint64 windowId,
                                                     bool isWindow)
{
    // Overlays are gone: safe to resolve permissions for inputs the user enabled on
    // the options bar mid-selection (the prompt is visible now). Re-enters here.
    if (resolveInputPermissions([this, area, windowId, isWindow] {
            beginRecordingForSelection(area, windowId, isWindow);
        }))
        return;
    m_askedCameraPermission = false;
    m_askedMicPermission = false;

    const RecordingStrategy::WindowCapture windowCapture = m_strategy->windowCapture();
    const bool systemPicker = isWindow
                              && windowCapture == RecordingStrategy::WindowCapture::SystemPicked;
    if (isWindow && windowCapture != RecordingStrategy::WindowCapture::WithOverlays) {
        destroyCameraBubble();   // the recording holds the window alone
    } else {
        // Re-evaluate the camera setting (bar toggles), get a fresh window id for the
        // capture filter, and park the bubble where the recording will actually be.
        ensureCameraBubble(area);
        if (m_cameraBubble)
            m_cameraBubble->moveToRegionCorner(area);
    }

    RecordTarget target;
    target.kind = isWindow ? RecordTarget::Kind::Window : RecordTarget::Kind::Region;
    target.windowId = windowId;
    target.systemPicker = systemPicker;
    target.regionVirtual = area;       // window: fallback rect for older macOS
    target.fps = Core::Settings::recordingFps();
    target.captureCursor = Core::Settings::recordingCaptureCursor();
    target.retinaCapture = Core::Settings::recordingRetina();
    target.cameraWindowId = cameraBubbleWindowId();
    target.captureSystemAudio = Core::Settings::systemAudioEnabled();
    target.captureMic = Core::Settings::micEnabled();
    target.micDeviceId = Core::Settings::micDeviceId();
    // The frame overlay marks the region once capture starts (windows move, so no
    // frame for window recordings).
    m_activeRegion = isWindow ? QRect() : area;
    startRecording(target);
}

void RecordingController::wireStrategy()
{
    if (!m_strategy)
        return;

    connect(m_strategy.get(), &RecordingStrategy::started, this, [this] {
        m_state = State::Recording;
        showFrameOverlay();   // capture is live: mark the recorded region on screen
        emit recordingStateChanged(true);
    });
    connect(m_strategy.get(), &RecordingStrategy::finished, this, [this](const QString &path) {
        m_state = State::Idle;
        m_outputPath.clear();
        destroyCameraBubble();
        destroyFrameOverlay();
        emit recordingStateChanged(false);
        emit recordingFinished(path);
    });
    connect(m_strategy.get(), &RecordingStrategy::failed, this, [this](const QString &error) {
        m_state = State::Idle;
        const QString partial = std::exchange(m_outputPath, QString());
        destroyCameraBubble();
        destroyFrameOverlay();
        emit recordingStateChanged(false);
        if (!partial.isEmpty() && RecordingJournal::isRecoverable(partial))
            emit partialRecordingKept(partial);
        emit recordingFailed(error);
    });
    connect(m_strategy.get(), &RecordingStrategy::cancelled, this, [this] {
        m_state = State::Idle;
        // Nothing was captured, so there is nothing for the journal to offer later.
        if (const QString path = std::exchange(m_outputPath, QString()); !path.isEmpty())
            RecordingJournal::discard(path);
        destroyCameraBubble();
        destroyFrameOverlay();
        emit recordingStateChanged(false);
        emit recordingCancelled();
    });
    connect(m_strategy.get(), &RecordingStrategy::durationChanged,
            this, &RecordingController::recordingDuration);
    connect(m_strategy.get(), &RecordingStrategy::pausedChanged,
            this, &RecordingController::recordingPausedChanged);
}

QString RecordingController::makeOutputPath() const
{
    // Record into a temp file; the app prompts for the final destination on stop and
    // moves it there. recordingFolder() is then just the save dialog's default dir.
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    return dir + "/" + Core::recordingFileName(QDateTime::currentDateTime(),
                                               Core::Settings::recordingFormat());
}

} // namespace Record
