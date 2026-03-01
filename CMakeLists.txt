cmake_minimum_required(VERSION 3.31)

# Must run before project(): it defines the version project(VERSION) is given.
include("${CMAKE_CURRENT_LIST_DIR}/cmake/SnimVersion.cmake")

project(Snim VERSION ${SNIM_VERSION_NUMERIC})

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# 6.5 minimum: the permission API (QCameraPermission and friends) starts there.
find_package(Qt6 6.5 COMPONENTS
        Core
        Gui
        Widgets
        DBus
        Concurrent
        Svg
        Multimedia
        MultimediaWidgets
        Network
        REQUIRED)

# Optional Tesseract OCR support for text sniping feature
find_package(PkgConfig)
set(TESSERACT_AVAILABLE FALSE)

if(PkgConfig_FOUND)
    pkg_check_modules(TESSERACT tesseract)
    if(TESSERACT_FOUND)
        set(TESSERACT_AVAILABLE TRUE)
        message(STATUS "Tesseract OCR found via pkg-config - enabling text snipping feature")
        message(STATUS "  Version: ${TESSERACT_VERSION}")
        message(STATUS "  Include: ${TESSERACT_INCLUDE_DIRS}")
        message(STATUS "  Libraries: ${TESSERACT_LIBRARIES}")
    endif()
endif()

# If pkg-config didn't find it, try manual detection
if(NOT TESSERACT_AVAILABLE)
    find_path(Tesseract_INCLUDE_DIR tesseract/baseapi.h
        PATHS /usr/include /usr/local/include)
    find_library(Tesseract_LIBRARY tesseract
        PATHS /usr/lib64 /usr/lib /usr/local/lib)
    find_library(Leptonica_LIBRARY lept
        PATHS /usr/lib64 /usr/lib /usr/local/lib)

    if(Tesseract_INCLUDE_DIR AND Tesseract_LIBRARY AND Leptonica_LIBRARY)
        set(TESSERACT_AVAILABLE TRUE)
        set(TESSERACT_INCLUDE_DIRS ${Tesseract_INCLUDE_DIR})
        set(TESSERACT_LIBRARIES ${Tesseract_LIBRARY} ${Leptonica_LIBRARY})
        message(STATUS "Tesseract OCR found (manual detection) - enabling text snipping feature")
        message(STATUS "  Include: ${TESSERACT_INCLUDE_DIRS}")
        message(STATUS "  Libraries: ${TESSERACT_LIBRARIES}")
    endif()
endif()

if(TESSERACT_AVAILABLE)
    # Some Leptonica builds expose pkg-config include as <prefix>/include/leptonica
    # while OCRService includes <leptonica/allheaders.h>; also add the parent of each
    # reported include dir so both tesseract/* and leptonica/* resolve either way.
    set(_tess_inc ${TESSERACT_INCLUDE_DIRS})
    foreach(_d ${TESSERACT_INCLUDE_DIRS})
        get_filename_component(_p "${_d}" DIRECTORY)
        if(_p)
            list(APPEND _tess_inc "${_p}")
        endif()
    endforeach()
    list(REMOVE_DUPLICATES _tess_inc)
    set(TESSERACT_INCLUDE_DIRS ${_tess_inc})

    add_compile_definitions(HAVE_TESSERACT)
else()
    message(STATUS "Tesseract OCR not found - text snipping feature will be disabled")
    message(STATUS "  Install with: sudo dnf install tesseract-devel")
endif()

# Optional libcurl - FTP/FTPS upload transport (Qt6 dropped FTP from QNetworkAccessManager)
set(LIBCURL_AVAILABLE FALSE)

if(PkgConfig_FOUND)
    pkg_check_modules(LIBCURL libcurl)
    if(LIBCURL_FOUND)
        set(LIBCURL_AVAILABLE TRUE)
        message(STATUS "libcurl found via pkg-config - enabling FTP/FTPS uploads")
        message(STATUS "  Version: ${LIBCURL_VERSION}")
        message(STATUS "  Libraries: ${LIBCURL_LIBRARIES}")
    endif()
endif()

# The macOS SDK ships libcurl without a .pc file, so fall back to CMake's finder.
if(NOT LIBCURL_AVAILABLE)
    find_package(CURL QUIET)
    if(CURL_FOUND)
        set(LIBCURL_AVAILABLE TRUE)
        set(LIBCURL_INCLUDE_DIRS ${CURL_INCLUDE_DIRS})
        set(LIBCURL_LIBRARIES ${CURL_LIBRARIES})
        message(STATUS "libcurl found (find_package CURL) - enabling FTP/FTPS uploads")
        message(STATUS "  Version: ${CURL_VERSION_STRING}")
        message(STATUS "  Libraries: ${LIBCURL_LIBRARIES}")
    endif()
endif()

if(LIBCURL_AVAILABLE)
    add_compile_definitions(HAVE_LIBCURL)
else()
    message(STATUS "libcurl not found - FTP/FTPS upload destinations will be disabled")
    message(STATUS "  Install with: sudo apt install libcurl4-openssl-dev (macOS ships one)")
endif()

# Optional libssh2 - SFTP upload transport (Qt has no SFTP at all)
set(LIBSSH2_AVAILABLE FALSE)

if(PkgConfig_FOUND)
    pkg_check_modules(LIBSSH2 libssh2)
    if(LIBSSH2_FOUND)
        set(LIBSSH2_AVAILABLE TRUE)
        message(STATUS "libssh2 found via pkg-config - enabling SFTP uploads")
        message(STATUS "  Version: ${LIBSSH2_VERSION}")
        message(STATUS "  Include: ${LIBSSH2_INCLUDE_DIRS}")
        message(STATUS "  Libraries: ${LIBSSH2_LIBRARIES}")
    endif()
endif()

# If pkg-config didn't find it, try manual detection (Homebrew keeps it under /opt).
if(NOT LIBSSH2_AVAILABLE)
    find_path(Libssh2_INCLUDE_DIR libssh2.h
        PATHS /usr/include /usr/local/include /opt/homebrew/include)
    find_library(Libssh2_LIBRARY ssh2
        PATHS /usr/lib64 /usr/lib /usr/local/lib /opt/homebrew/lib)

    if(Libssh2_INCLUDE_DIR AND Libssh2_LIBRARY)
        set(LIBSSH2_AVAILABLE TRUE)
        set(LIBSSH2_INCLUDE_DIRS ${Libssh2_INCLUDE_DIR})
        set(LIBSSH2_LIBRARIES ${Libssh2_LIBRARY})
        message(STATUS "libssh2 found (manual detection) - enabling SFTP uploads")
        message(STATUS "  Include: ${LIBSSH2_INCLUDE_DIRS}")
        message(STATUS "  Libraries: ${LIBSSH2_LIBRARIES}")
    endif()
endif()

if(LIBSSH2_AVAILABLE AND WIN32)
    # SftpUploader is written against POSIX sockets; a Windows port needs Winsock
    # plumbing first, so a found libssh2 must not switch it on there.
    set(LIBSSH2_AVAILABLE FALSE)
    message(STATUS "libssh2 found but SFTP upload is not ported to Windows - disabled")
endif()

if(LIBSSH2_AVAILABLE)
    add_compile_definitions(HAVE_LIBSSH2)
else()
    message(STATUS "libssh2 not found - SFTP upload destinations will be disabled")
    message(STATUS "  Install with: brew install libssh2 / sudo apt install libssh2-1-dev")
endif()

# Required libwebp - animated WebP export in the trim editor (a silent short loop at a
# fraction of the GIF's size). Unlike the upload transports this is not optional: every
# build ships the feature, so a missing dev package fails the configure step below.
set(LIBWEBP_AVAILABLE FALSE)

if(PkgConfig_FOUND)
    pkg_check_modules(LIBWEBP libwebp)
    pkg_check_modules(LIBWEBPMUX libwebpmux)
    if(LIBWEBP_FOUND AND LIBWEBPMUX_FOUND)
        set(LIBWEBP_AVAILABLE TRUE)
        message(STATUS "libwebp found via pkg-config - animated WebP export")
        message(STATUS "  Version: ${LIBWEBP_VERSION}")
        message(STATUS "  Include: ${LIBWEBP_INCLUDE_DIRS}")
        message(STATUS "  Libraries: ${LIBWEBPMUX_LIBRARIES} ${LIBWEBP_LIBRARIES}")
    endif()
endif()

# The animated encoder needs both libraries, so a partial find is not usable.
if(NOT LIBWEBP_AVAILABLE)
    find_path(WebP_INCLUDE_DIR webp/encode.h
        PATHS /usr/include /usr/local/include /opt/homebrew/include)
    find_library(WebP_LIBRARY webp
        PATHS /usr/lib64 /usr/lib /usr/local/lib /opt/homebrew/lib)
    find_library(WebPMux_LIBRARY webpmux
        PATHS /usr/lib64 /usr/lib /usr/local/lib /opt/homebrew/lib)

    if(WebP_INCLUDE_DIR AND WebP_LIBRARY AND WebPMux_LIBRARY)
        set(LIBWEBP_AVAILABLE TRUE)
        set(LIBWEBP_INCLUDE_DIRS ${WebP_INCLUDE_DIR})
        set(LIBWEBP_LIBRARIES ${WebPMux_LIBRARY} ${WebP_LIBRARY})
        message(STATUS "libwebp found (manual detection) - animated WebP export")
        message(STATUS "  Include: ${LIBWEBP_INCLUDE_DIRS}")
        message(STATUS "  Libraries: ${LIBWEBP_LIBRARIES}")
    endif()
endif()

if(NOT LIBWEBP_AVAILABLE)
    message(FATAL_ERROR
            "libwebp is required but was not found.\n"
            "  Install with: sudo apt install libwebp-dev / sudo dnf install libwebp-devel / brew install webp")
endif()

# Optional GStreamer - Linux screen recording (portal ScreenCast stream -> H.264 file)
set(GSTREAMER_AVAILABLE FALSE)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND PkgConfig_FOUND)
    pkg_check_modules(GSTREAMER gstreamer-1.0>=1.18)
    if(GSTREAMER_FOUND)
        set(GSTREAMER_AVAILABLE TRUE)
        message(STATUS "GStreamer found via pkg-config - enabling Linux screen recording")
        message(STATUS "  Version: ${GSTREAMER_VERSION}")
        message(STATUS "  Include: ${GSTREAMER_INCLUDE_DIRS}")
        message(STATUS "  Libraries: ${GSTREAMER_LIBRARIES}")
    endif()
endif()

if(NOT GSTREAMER_AVAILABLE AND CMAKE_SYSTEM_NAME STREQUAL "Linux")
    message(STATUS "GStreamer not found - Linux screen recording will be disabled")
    message(STATUS "  Install with: sudo dnf install gstreamer1-devel")
endif()

# Optional LayerShellQt - lets the recording frame overlay become a Wayland layer
# surface. Without it the overlay stays a plain toplevel, which the compositor sinks
# below the windows the user is working in (it is never focused, by design).
set(LAYER_SHELL_AVAILABLE FALSE)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    find_package(LayerShellQt CONFIG QUIET)
    if(LayerShellQt_FOUND)
        set(LAYER_SHELL_AVAILABLE TRUE)
        message(STATUS "LayerShellQt found - the recording frame overlay will use a Wayland layer surface")
        message(STATUS "  Version: ${LayerShellQt_VERSION}")
    else()
        message(STATUS "LayerShellQt not found - the recording frame overlay stays a plain Wayland toplevel")
        message(STATUS "  Install with: sudo dnf install layer-shell-qt-devel")
    endif()
endif()

# All cross-platform sources live in a static library so both the executable and
# the test suite can link them. main.cpp + the .app bundle stay on the executable.
add_library(snim_lib STATIC
        # Core application files
        src/core/ScreenshotApp.cpp
        src/core/ScreenshotApp.h
        src/core/Settings.cpp
        src/core/Settings.h
        src/core/IconUtil.cpp
        src/core/IconUtil.h
        src/core/Perf.cpp
        src/core/Perf.h
        src/core/ScreenshotDialog.cpp
        src/core/ScreenshotDialog.h
        src/core/SettingsDialog.cpp
        src/core/SettingsDialog.h
        src/core/KeychainStore.h
        # GitHub releases lookup behind the tray's "Check for updates" action.
        src/core/UpdateCheck.cpp
        src/core/UpdateCheck.h
        # KDE desktop-entry repair for KWin's ScreenShot2 authorization (no-op elsewhere).
        src/core/DesktopIntegration.cpp
        src/core/DesktopIntegration.h
        # Transient systemd scope so the portals can read an app id off our cgroup (Linux).
        src/core/AppScope.cpp
        src/core/AppScope.h
        # Points GStreamer and Tesseract at the resources beside a relocated binary.
        src/core/BundledPaths.cpp
        src/core/BundledPaths.h

        # Upload: S3-compatible Strategy + Factory (Qt Network + hand-rolled SigV4). The
        # macOS keychain backing is added in the APPLE block; a stub elsewhere. The
        # SFTP/FTP backends are conditional on libssh2/libcurl; everything here is not.
        src/upload/Uploader.h
        src/upload/UploaderFactory.cpp
        src/upload/UploaderFactory.h
        src/upload/UploadConfig.h
        src/upload/UploadProfiles.cpp
        src/upload/UploadProfiles.h
        src/upload/Util.cpp
        src/upload/Util.h
        src/upload/KnownHosts.cpp
        src/upload/KnownHosts.h
        src/upload/SigV4.cpp
        src/upload/SigV4.h
        src/upload/strategies/S3Uploader.cpp
        src/upload/strategies/S3Uploader.h
        src/upload/strategies/StubUploader.cpp
        src/upload/strategies/StubUploader.h

        # Capture functionality
        src/capture/AreaSelector.cpp
        src/capture/AreaSelector.h
        src/capture/AreaSelectorInput.cpp
        src/capture/AreaSelectorInput.h
        src/capture/CaptureFactory.cpp
        src/capture/CaptureFactory.h
        src/capture/CaptureGeometry.h
        src/capture/OverlayAnnotations.cpp
        src/capture/OverlayAnnotations.h
        # Frozen backdrop for the selection overlays (portal-based on Wayland).
        src/capture/FrozenFrameGrabber.cpp
        src/capture/FrozenFrameGrabber.h
        src/capture/WindowEnumerator.h
        src/capture/strategies/CaptureStrategy.cpp
        src/capture/strategies/CaptureStrategy.h
        src/capture/strategies/NativeCaptureStrategy.cpp
        src/capture/strategies/NativeCaptureStrategy.h

        # Editor: shared canvas, layers, tools, interactions, commands
        src/editor/EditorChrome.h
        src/editor/annotations/ToolRegistry.cpp
        src/editor/annotations/ToolRegistry.h
        src/editor/annotations/AnnotationBuilder.cpp
        src/editor/annotations/AnnotationBuilder.h
        src/editor/annotations/AnnotationSet.cpp
        src/editor/annotations/AnnotationSet.h
        src/editor/annotations/IAnnotationSink.h
        src/editor/annotations/StepNumbering.cpp
        src/editor/annotations/StepNumbering.h
        src/editor/annotations/commands/EditorCommands.cpp
        src/editor/annotations/commands/EditorCommands.h
        src/editor/annotations/DrawingGraphicsView.cpp
        src/editor/annotations/DrawingGraphicsView.h
        src/editor/annotations/tools/ArrowTool.cpp
        src/editor/annotations/tools/ArrowTool.h
        src/editor/annotations/tools/ArrowHandleTool.cpp
        src/editor/annotations/tools/ArrowHandleTool.h
        src/editor/annotations/tools/ShapeTool.cpp
        src/editor/annotations/tools/ShapeTool.h
        src/editor/annotations/tools/ShapeHandleTool.cpp
        src/editor/annotations/tools/ShapeHandleTool.h
        src/editor/annotations/tools/RectangleTool.cpp
        src/editor/annotations/tools/RectangleTool.h
        src/editor/annotations/tools/EllipseTool.cpp
        src/editor/annotations/tools/EllipseTool.h
        src/editor/annotations/tools/FreehandTool.cpp
        src/editor/annotations/tools/FreehandTool.h
        src/editor/annotations/tools/HighlightTool.cpp
        src/editor/annotations/tools/HighlightTool.h
        src/editor/annotations/tools/BlurTool.cpp
        src/editor/annotations/tools/BlurTool.h
        src/editor/annotations/tools/TextTool.cpp
        src/editor/annotations/tools/TextTool.h
        src/editor/annotations/tools/StepTool.cpp
        src/editor/annotations/tools/StepTool.h
        src/editor/annotations/Layer.cpp
        src/editor/annotations/Layer.h
        src/editor/annotations/LayerManager.cpp
        src/editor/annotations/LayerManager.h
        src/editor/annotations/LayerProperties.cpp
        src/editor/annotations/LayerProperties.h
        # Drawing Interactions
        src/editor/annotations/interactions/IDrawingInteraction.h
        src/editor/annotations/interactions/BaseDrawingInteraction.cpp
        src/editor/annotations/interactions/BaseDrawingInteraction.h
        src/editor/annotations/interactions/PointerToolInteraction.cpp
        src/editor/annotations/interactions/PointerToolInteraction.h
        src/editor/annotations/interactions/ArrowDrawingInteraction.cpp
        src/editor/annotations/interactions/ArrowDrawingInteraction.h
        src/editor/annotations/interactions/TextDrawingInteraction.cpp
        src/editor/annotations/interactions/TextDrawingInteraction.h
        src/editor/annotations/interactions/RectangleDrawingInteraction.cpp
        src/editor/annotations/interactions/RectangleDrawingInteraction.h
        src/editor/annotations/interactions/EllipseDrawingInteraction.cpp
        src/editor/annotations/interactions/EllipseDrawingInteraction.h
        src/editor/annotations/interactions/FreehandDrawingInteraction.cpp
        src/editor/annotations/interactions/FreehandDrawingInteraction.h
        src/editor/annotations/interactions/HighlightDrawingInteraction.cpp
        src/editor/annotations/interactions/HighlightDrawingInteraction.h
        src/editor/annotations/interactions/BlurDrawingInteraction.cpp
        src/editor/annotations/interactions/BlurDrawingInteraction.h
        src/editor/annotations/interactions/StepDrawingInteraction.cpp
        src/editor/annotations/interactions/StepDrawingInteraction.h

        # Editor::Image: screenshot-specific (image editor window + beautify backdrop)
        src/editor/image/ImageEditor.cpp
        src/editor/image/ImageEditor.h
        src/editor/image/BackdropItem.cpp
        src/editor/image/BackdropItem.h
        src/editor/image/BackdropMemento.h
        src/editor/image/BackdropPresets.cpp
        src/editor/image/BackdropPresets.h

        # OCR functionality
        src/ocr/OCRService.cpp
        src/ocr/OCRService.h
        src/ocr/OCRResult.h
        src/ocr/TextSnipCapture.cpp
        src/ocr/TextSnipCapture.h

        # Editor::Video - post-recording trim editor (preview + cut + save). The
        # macOS AVAssetExportSession exporter is added in the APPLE block below;
        # the stub (which also hosts the factory) is the universal fallback.
        src/editor/video/TrimState.cpp
        src/editor/video/TrimState.h
        src/editor/video/TrimTimeline.cpp
        src/editor/video/TrimTimeline.h
        src/editor/video/VideoEditor.cpp
        src/editor/video/VideoEditor.h
        src/editor/video/VideoExporter.h
        src/editor/video/VideoExporter.cpp
        src/editor/video/AnimationParams.h
        src/editor/video/AnimationOptionsDialog.cpp
        src/editor/video/AnimationOptionsDialog.h
        src/editor/video/Timecode.h
        src/editor/video/VideoFrameGrabber.h
        src/editor/video/MediaFrameGrabber.cpp
        src/editor/video/MediaFrameGrabber.h
        src/editor/video/StubVideoExporter.cpp
        src/editor/video/StubVideoExporter.h
        src/editor/video/WebpEncoder.cpp
        src/editor/video/WebpEncoder.h
        src/editor/video/WebpExporter.cpp
        src/editor/video/WebpExporter.h

        # Recording: platform-agnostic Strategy + Factory (the macOS ScreenCaptureKit
        # backend is added in the APPLE block below). The stub is the universal fallback.
        src/recording/RecordTarget.h
        src/recording/RecordingGeometry.h
        src/recording/PcmMixBuffer.h
        src/recording/RecordingStrategy.cpp
        src/recording/RecordingStrategy.h
        src/recording/RecordingFactory.cpp
        src/recording/RecordingFactory.h
        src/recording/RecordingController.cpp
        src/recording/RecordingController.h
        src/recording/RecordingControls.cpp
        src/recording/RecordingControls.h
        src/recording/RecordingFrameOverlay.cpp
        src/recording/RecordingFrameOverlay.h
        src/recording/LayerShellSupport.h
        src/recording/RecordingOptionsBar.cpp
        src/recording/RecordingOptionsBar.h
        src/recording/CameraBubble.cpp
        src/recording/CameraBubble.h
        src/recording/strategies/StubRecordingStrategy.cpp
        src/recording/strategies/StubRecordingStrategy.h

        # Hotkeys: platform-agnostic core + pure key mappers (no platform headers, so
        # testable on any host). The real backends live in the platform blocks below.
        src/hotkeys/HotkeyAction.cpp
        src/hotkeys/HotkeyAction.h
        src/hotkeys/HotkeyBindings.cpp
        src/hotkeys/HotkeyBindings.h
        src/hotkeys/HotkeyBackend.cpp
        src/hotkeys/HotkeyBackend.h
        src/hotkeys/HotkeyBackendFactory.cpp
        src/hotkeys/HotkeyBackendFactory.h
        src/hotkeys/GlobalHotkeyManager.cpp
        src/hotkeys/GlobalHotkeyManager.h
        src/hotkeys/MacKeyMapping.cpp
        src/hotkeys/MacKeyMapping.h
        src/hotkeys/WinKeyMapping.cpp
        src/hotkeys/WinKeyMapping.h
        src/hotkeys/PortalKeyMapping.cpp
        src/hotkeys/PortalKeyMapping.h
        src/hotkeys/backends/StubHotkeyBackend.cpp
        src/hotkeys/backends/StubHotkeyBackend.h

)

# The executable: entry point + resources only. The qrc is kept on the exe (and
# on the few tests that need it), NOT in the static lib, so AUTORCC's resource
# initializer isn't stripped out of the archive.
add_executable(Snim main.cpp resources/resources.qrc)
target_link_libraries(Snim PRIVATE snim_lib)

# Lowercase binary everywhere but macOS, where the bundle must stay Snim.app.
if(NOT APPLE)
    set_target_properties(Snim PROPERTIES OUTPUT_NAME "snim")
endif()

# Platform-specific window enumeration for the interactive window picker.
# macOS uses CGWindowList (Objective-C++); other platforms get a stub that
# returns no windows, so window-pick falls back to the screen under the cursor.
if(APPLE)
    enable_language(OBJCXX)
    target_sources(snim_lib PRIVATE
            src/capture/WindowEnumerator_mac.mm
            src/capture/MacOverlay.h
            src/capture/MacOverlay.mm
            # macOS screen-recording backend (ScreenCaptureKit + AVAssetWriter)
            src/recording/strategies/MacRecordingStrategy.h
            src/recording/strategies/MacRecordingStrategy.mm
            # macOS trim exporter for the video editor (AVAssetExportSession remux)
            src/editor/video/MacVideoExporter.h
            src/editor/video/MacVideoExporter.mm
            # macOS keychain backing for the S3 secret access key (Security.framework)
            src/core/KeychainStore_mac.mm
            # macOS tray crash workaround: neutralises Qt's menu-tracking callback
            src/core/MacTrayWorkaround.h
            src/core/MacTrayWorkaround.mm
            # macOS global hotkeys (Carbon RegisterEventHotKey); the header is listed
            # so AUTOMOC sees its Q_OBJECT, as with the .mm-backed classes above
            src/hotkeys/backends/MacHotkeyBackend.h
            src/hotkeys/backends/MacHotkeyBackend.mm)
    # These .mm files hold ScreenCaptureKit/AVFoundation/Foundation objects across calls,
    # so compile them with ARC to manage those refs automatically.
    set_source_files_properties(
            src/recording/strategies/MacRecordingStrategy.mm
            src/editor/video/MacVideoExporter.mm
            src/core/KeychainStore_mac.mm
            PROPERTIES COMPILE_OPTIONS "-fobjc-arc")
    # Activates the Mac branch of RecordingFactory + makes Auto pick ScreenCaptureKit,
    # and the Carbon branch of HotkeyBackendFactory.
    target_compile_definitions(snim_lib PUBLIC
            SNIM_HAVE_MAC_RECORDER
            SNIM_HAVE_MAC_HOTKEYS)
    target_link_libraries(snim_lib PUBLIC
            "-framework CoreGraphics"
            "-framework CoreFoundation"
            "-framework AppKit"
            "-framework ScreenCaptureKit"
            "-framework AVFoundation"
            "-framework CoreMedia"
            "-framework CoreVideo"
            "-framework ImageIO"     # animated-GIF encode in the trim editor
            "-framework Security"    # keychain (S3 secret access key)
            "-framework Carbon")     # RegisterEventHotKey (global hotkeys)
    # Qt ships the macOS permission backends as STATIC plugins (libqdarwin*permission.a),
    # so they must be imported into the executable - otherwise the QPermission API has no
    # backend at runtime and QCamera reports "Could not find permission plugin for
    # QCameraPermission" even with the usage strings present in Info.plist.
    qt_import_plugins(Snim INCLUDE
            Qt6::QDarwinCameraPermissionPlugin
            Qt6::QDarwinMicrophonePermissionPlugin)
    # Each plugin archive keeps its requestPermission() implementation in a SEPARATE
    # object, dragged in only by a `-Wl,-u,_QDarwin*PermissionRequest` link flag that
    # Qt's exported targets emit when the consumer has the property below. Normally
    # qt_add_executable's finalizer sets it after finding the NS*UsageDescription key
    # in the Info.plist; this project uses plain add_executable, so no finalizer runs
    # and without the property checkPermission() works but requestPermission() falls
    # back to a stub that logs "Could not request QCameraPermission..." and instantly
    # denies - the TCC prompt can never appear. The usage strings are guaranteed by
    # cmake/Info.plist.in, so assert the properties ourselves.
    set_target_properties(Snim PROPERTIES
            _qt_has_QDarwinCameraPermissionPlugin_usage_description TRUE
            _qt_has_QDarwinMicrophonePermissionPlugin_usage_description TRUE)
else()
    target_sources(snim_lib PRIVATE
            src/capture/WindowEnumerator_stub.cpp
            src/core/KeychainStore_stub.cpp)
endif()

# Linux-only capture strategies. These rely on Linux desktop APIs that don't
# exist elsewhere: KWin uses the org.kde.KWin.ScreenShot2 D-Bus interface plus
# pipe2() for pixel streaming, and Wayland uses the xdg-desktop-portal D-Bus
# interface. On other platforms (e.g. macOS) the cross-platform Qt-based
# NativeCaptureStrategy is used instead.
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_sources(snim_lib PRIVATE
            src/capture/strategies/WaylandCaptureStrategy.cpp
            src/capture/strategies/WaylandCaptureStrategy.h
            src/capture/strategies/KWinCaptureStrategy.cpp
            src/capture/strategies/KWinCaptureStrategy.h
            # Global hotkeys via the xdg-desktop-portal GlobalShortcuts session
            src/hotkeys/backends/PortalHotkeyBackend.cpp
            src/hotkeys/backends/PortalHotkeyBackend.h
    )
endif()

# Linux screen recording - compiled only where GStreamer exists, like the optional
# upload transports. The backend is dlopened, never linked: GStreamer must be the
# distribution's own (a pipewiresrc frozen at build time cannot track an evolving
# PipeWire daemon), so neither snim_lib nor the executable may link it. snim_lib gets
# the loader, the module gets everything that touches gst_*. Without the module, or
# without GStreamer on the host, RecordingFactory keeps returning the stub.
if(GSTREAMER_AVAILABLE)
    target_sources(snim_lib PRIVATE
            src/recording/strategies/LinuxRecorderModule.cpp
            src/recording/strategies/LinuxRecorderModule.h)
    # Activates the Linux branch of RecordingFactory + makes Auto pick the portal
    # recorder. PUBLIC because the tests check the macro.
    target_compile_definitions(snim_lib PUBLIC SNIM_HAVE_LINUX_RECORDER)

    add_library(snim_recorder_linux MODULE
            src/recording/strategies/LinuxRecordingStrategy.cpp
            src/recording/strategies/LinuxRecordingStrategy.h
            # The portal handshake and the self-location helper are implementation
            # details of the recorder, so the module carries its own copies: a static
            # library hands an object out only when something else already pulled it
            # into the binary doing the loading.
            src/recording/strategies/ScreenCastPortalSession.cpp
            src/recording/strategies/ScreenCastPortalSession.h
            src/core/BundledPaths.cpp
            src/core/BundledPaths.h)
    set_target_properties(snim_recorder_linux PROPERTIES
            OUTPUT_NAME "snim-recorder-linux"
            # Beside the built binary, the first place the loader looks, so ./build/snim
            # records without an install step.
            LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
            # Installed into <prefix>/lib beside the bundled Qt, and the portable tarball
            # has no AppRun to set LD_LIBRARY_PATH for it.
            INSTALL_RPATH "$ORIGIN")
    target_include_directories(snim_recorder_linux PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}
            ${CMAKE_CURRENT_SOURCE_DIR}/src
            ${GSTREAMER_INCLUDE_DIRS})
    # pkg-config reports plain library names; add its dirs so a non-default prefix links.
    if(GSTREAMER_LIBRARY_DIRS)
        target_link_directories(snim_recorder_linux PRIVATE ${GSTREAMER_LIBRARY_DIRS})
    endif()
    target_link_libraries(snim_recorder_linux PRIVATE
            Qt::Core
            Qt::Gui
            Qt::DBus
            ${GSTREAMER_LIBRARIES})
    # RecordingStrategy stays undefined in the module and binds to the loading binary at
    # dlopen time, which needs that binary's own symbols in its dynamic table.
    set_target_properties(Snim PROPERTIES ENABLE_EXPORTS ON)
    add_dependencies(Snim snim_recorder_linux)
endif()

# Wayland layer-shell path for the recording frame overlay. Attaching
# LayerShellQt::Window per window is enough on Qt 6.5+, no global registration.
if(LAYER_SHELL_AVAILABLE)
    target_link_libraries(snim_lib PUBLIC LayerShellQt::Interface)
    target_compile_definitions(snim_lib PUBLIC SNIM_HAVE_LAYER_SHELL)
endif()

# Windows global hotkeys: RegisterHotKey + a WM_HOTKEY native event filter. The define
# activates the Windows branch of HotkeyBackendFactory (user32 needs no extra linking).
if(WIN32)
    target_sources(snim_lib PRIVATE
            src/hotkeys/backends/WindowsHotkeyBackend.cpp
            src/hotkeys/backends/WindowsHotkeyBackend.h
    )
    target_compile_definitions(snim_lib PUBLIC SNIM_HAVE_WIN_HOTKEYS)
endif()

# The version header is generated, not checked in, so `core/Version.h` resolves out
# of the build tree for the library, the executable and the tests alike.
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/cmake/Version.h.in"
        "${CMAKE_BINARY_DIR}/generated/core/Version.h" @ONLY)

# Set include directories for the organized structure. PUBLIC so the executable
# and the tests inherit the `src/`-rooted include path every #include relies on.
target_include_directories(snim_lib PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_BINARY_DIR}/generated
)

target_link_libraries(snim_lib PUBLIC
        Qt::Core
        Qt::Gui
        Qt::Widgets
        Qt::DBus
        Qt::Concurrent
        # Needed to render the SVG tray/toolbar icons. Linking it explicitly also
        # ensures macdeployqt bundles QtSvg + the svg image/icon-engine plugins.
        Qt::Svg
        # Camera/mic device enumeration + preview, and video playback in the trim editor.
        # Linking these makes macdeployqt bundle the multimedia frameworks + media plugins.
        Qt::Multimedia
        Qt::MultimediaWidgets
        # S3-compatible upload via QNetworkAccessManager + hand-rolled SigV4.
        Qt::Network
)

# Link Tesseract if available (PUBLIC link: OCRService.o references its symbols and
# HAVE_TESSERACT is a global define, so consumers - exe and tests - must link it).
if(TESSERACT_AVAILABLE)
    target_include_directories(snim_lib PRIVATE ${TESSERACT_INCLUDE_DIRS})
    # pkg-config reports library names (-ltesseract ...) without their location;
    # add the reported library dirs so the linker can find them (needed on
    # macOS/Homebrew where they live under a non-default prefix). Empty for the
    # manual-detection fallback, which already uses absolute library paths.
    target_link_directories(snim_lib PUBLIC ${TESSERACT_LIBRARY_DIRS})
    target_link_libraries(snim_lib PUBLIC ${TESSERACT_LIBRARIES})
endif()

# FTP/FTPS upload backend - compiled only where libcurl exists (like the Linux-only
# capture strategies). Without it UploaderFactory returns a Stub that says so, and the
# rest of the upload feature is unaffected. PUBLIC link: HAVE_LIBCURL is a global define,
# so consumers (the exe and the tests) must resolve FtpUploader's curl symbols too.
if(LIBCURL_AVAILABLE)
    target_sources(snim_lib PRIVATE
            src/upload/strategies/FtpUploader.cpp
            src/upload/strategies/FtpUploader.h)
    target_include_directories(snim_lib PRIVATE ${LIBCURL_INCLUDE_DIRS})
    # pkg-config reports plain library names; add its dirs so a non-default prefix
    # (Homebrew) still links. Empty for the find_package fallback (absolute paths).
    if(LIBCURL_LIBRARY_DIRS)
        target_link_directories(snim_lib PUBLIC ${LIBCURL_LIBRARY_DIRS})
    endif()
    target_link_libraries(snim_lib PUBLIC ${LIBCURL_LIBRARIES})
endif()

# SFTP upload backend - compiled only where libssh2 exists, exactly like the FTP one
# above. Without it UploaderFactory returns a Stub that says so. PUBLIC link:
# HAVE_LIBSSH2 is a global define, so consumers (the exe and the tests) must resolve
# SftpUploader's libssh2 symbols too.
if(LIBSSH2_AVAILABLE)
    target_sources(snim_lib PRIVATE
            src/upload/strategies/SftpUploader.cpp
            src/upload/strategies/SftpUploader.h)
    target_include_directories(snim_lib PRIVATE ${LIBSSH2_INCLUDE_DIRS})
    # pkg-config reports plain library names; add its dirs so a non-default prefix
    # (Homebrew) still links. Empty for the manual-detection fallback (absolute paths).
    if(LIBSSH2_LIBRARY_DIRS)
        target_link_directories(snim_lib PUBLIC ${LIBSSH2_LIBRARY_DIRS})
    endif()
    target_link_libraries(snim_lib PUBLIC ${LIBSSH2_LIBRARIES})
endif()

# Animated WebP export (shared, not macOS-only). PUBLIC link so the tests resolve the
# same symbols.
# libwebp's pkg-config reports <prefix>/include/webp, so add the parent of each
# reported dir to let the canonical <webp/encode.h> resolve too (as the Tesseract
# block does for leptonica).
set(_webp_inc ${LIBWEBP_INCLUDE_DIRS})
foreach(_d ${LIBWEBP_INCLUDE_DIRS})
    get_filename_component(_p "${_d}" DIRECTORY)
    if(_p)
        list(APPEND _webp_inc "${_p}")
    endif()
endforeach()
list(REMOVE_DUPLICATES _webp_inc)
target_include_directories(snim_lib PRIVATE ${_webp_inc})
# pkg-config reports plain library names; add its dirs so a non-default prefix
# (Homebrew) still links. Empty for the manual-detection fallback (absolute paths).
if(LIBWEBP_LIBRARY_DIRS)
    target_link_directories(snim_lib PUBLIC ${LIBWEBP_LIBRARY_DIRS})
endif()
target_link_libraries(snim_lib PUBLIC ${LIBWEBPMUX_LIBRARIES} ${LIBWEBP_LIBRARIES})

# ---------------------------------------------------------------------------
# macOS application bundle + deployment
# ---------------------------------------------------------------------------
# Building as a .app bundle (rather than a bare executable) gives the app a
# stable bundle identifier. The `deploy` target then runs macdeployqt to copy
# the Qt frameworks (and other linked dylibs) into the bundle and ad-hoc
# code-signs it, producing a self-contained, distributable Snim.app.
if(APPLE)
    set_target_properties(Snim PROPERTIES
            MACOSX_BUNDLE TRUE
            MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/cmake/Info.plist.in"
            MACOSX_BUNDLE_BUNDLE_NAME "Snim"
            MACOSX_BUNDLE_GUI_IDENTIFIER "dev.snim.Snim"
            MACOSX_BUNDLE_BUNDLE_VERSION "${SNIM_VERSION_NUMERIC}"
            MACOSX_BUNDLE_SHORT_VERSION_STRING "${SNIM_VERSION_NUMERIC}"
            MACOSX_BUNDLE_COPYRIGHT "Copyright © 2026 Darko Gjorgjijoski"
    )

    # Optional custom app icon: drop an AppIcon.icns into resources/ and it gets
    # bundled automatically. Without it the bundle uses the default icon.
    set(SNIM_APP_ICON "${CMAKE_CURRENT_SOURCE_DIR}/resources/AppIcon.icns")
    if(EXISTS "${SNIM_APP_ICON}")
        target_sources(Snim PRIVATE "${SNIM_APP_ICON}")
        set_source_files_properties("${SNIM_APP_ICON}" PROPERTIES
                MACOSX_PACKAGE_LOCATION "Resources")
        set_target_properties(Snim PROPERTIES MACOSX_BUNDLE_ICON_FILE "AppIcon.icns")
    endif()

    # Dev code signing. Without this the linker ad-hoc signs the binary, whose
    # identity is the binary's own hash - it changes on EVERY rebuild, so modern
    # macOS treats each build as a new app and re-prompts for (or silently breaks)
    # the Screen Recording / Camera / Microphone TCC grants, even though System
    # Settings still shows Snim as enabled. Signing with a stable identity
    # (a local self-signed "Snim Dev Signing" certificate) keeps the
    # designated requirement identical across rebuilds, so grants persist.
    # Set to "" (or build on a machine without the cert) to fall back to ad-hoc.
    set(SNIM_CODESIGN_IDENTITY "Snim Dev Signing" CACHE STRING
            "Code-signing identity for the dev build (empty = linker ad-hoc signing)")
    if(SNIM_CODESIGN_IDENTITY)
        # Only sign when the identity actually exists in the keychain, so fresh
        # clones / CI runners without the cert still build out of the box.
        execute_process(COMMAND security find-identity -v -p codesigning
                OUTPUT_VARIABLE _snim_identities ERROR_QUIET)
        string(FIND "${_snim_identities}" "${SNIM_CODESIGN_IDENTITY}" _snim_identity_found)
        if(NOT _snim_identity_found EQUAL -1)
            add_custom_command(TARGET Snim POST_BUILD
                    COMMAND codesign --force --sign "${SNIM_CODESIGN_IDENTITY}"
                            --timestamp=none "$<TARGET_BUNDLE_DIR:Snim>"
                    COMMENT "Code-signing Snim.app with '${SNIM_CODESIGN_IDENTITY}' (stable TCC identity)")
        else()
            message(STATUS "Code-signing identity '${SNIM_CODESIGN_IDENTITY}' not in keychain - leaving ad-hoc signature")
        endif()
    endif()

    # Locate macdeployqt next to the Qt tools (qmake lives in the same bin dir).
    get_target_property(_qmake_executable Qt6::qmake IMPORTED_LOCATION)
    get_filename_component(_qt_bin_dir "${_qmake_executable}" DIRECTORY)
    find_program(MACDEPLOYQT_EXECUTABLE macdeployqt HINTS "${_qt_bin_dir}")

    if(MACDEPLOYQT_EXECUTABLE)
        # IMPORTANT: deploy to a SEPARATE copy under <build>/dist, never in place.
        # macdeployqt adds a qt.conf + bundled Qt frameworks/plugins to the .app.
        # If that were done to the bundle CLion builds and runs, the next plain
        # build relinks the executable against Homebrew Qt while the leftover
        # qt.conf still loads the bundled Qt plugins -> two copies of Qt in one
        # process -> "Class ... implemented in both" and a SIGABRT on launch.
        # Keeping the dev bundle pristine (Homebrew Qt via rpath) avoids this;
        # the self-contained build lives in dist/Snim.app.
        set(SNIM_DIST_APP "${CMAKE_BINARY_DIR}/dist/Snim.app")
        # macos_deploy.sh runs macdeployqt, strips the build-time Homebrew rpaths,
        # and ad-hoc signs. For real distribution, replace the ad-hoc `-` identity
        # in the script with a Developer ID and notarize the result.
        add_custom_target(deploy
                COMMAND "${CMAKE_COMMAND}" -E rm -rf "${SNIM_DIST_APP}"
                COMMAND "${CMAKE_COMMAND}" -E make_directory "${CMAKE_BINARY_DIR}/dist"
                COMMAND "${CMAKE_COMMAND}" -E copy_directory
                        "$<TARGET_BUNDLE_DIR:Snim>" "${SNIM_DIST_APP}"
                COMMAND bash "${CMAKE_CURRENT_SOURCE_DIR}/cmake/macos_deploy.sh"
                        "${SNIM_DIST_APP}" "${MACDEPLOYQT_EXECUTABLE}"
                DEPENDS Snim
                COMMENT "Creating self-contained dist/Snim.app (Qt bundled, rpaths fixed, signed)"
                VERBATIM
        )
    else()
        message(WARNING "macdeployqt not found - the `deploy` target will be unavailable")
    endif()
endif()

# ---------------------------------------------------------------------------
# Linux install rules
# ---------------------------------------------------------------------------
# `cmake --install` produces a normal FHS tree: the binary plus the desktop entry
# and the hicolor icons every launcher indexes. The AppImage job installs into an
# AppDir with the same rules, so the two layouts can never drift.
if(UNIX AND NOT APPLE)
    include(GNUInstallDirs)

    install(TARGETS Snim RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

    # The recorder module is dlopened, not linked, so no deployment tool finds it by
    # walking the binary: it needs an install rule of its own for the AppImage and the
    # portable tarball to carry it (the loader looks in ../lib next to the binary).
    if(TARGET snim_recorder_linux)
        install(TARGETS snim_recorder_linux LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
    endif()

    install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/deploy/dev.snim.Snim.desktop"
            DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)

    install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/resources/icons/app-icon.svg"
            DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps
            RENAME dev.snim.Snim.svg)

    foreach(_snim_icon_size 16 32 48 64 128 256 512)
        install(FILES
                "${CMAKE_CURRENT_SOURCE_DIR}/resources/icons/hicolor/${_snim_icon_size}x${_snim_icon_size}/apps/dev.snim.Snim.png"
                DESTINATION
                "${CMAKE_INSTALL_DATADIR}/icons/hicolor/${_snim_icon_size}x${_snim_icon_size}/apps")
    endforeach()
endif()

# ---------------------------------------------------------------------------
# Tests (Qt Test + CTest). Configure with -DSNIM_BUILD_TESTS=OFF to skip.
# Run with: ctest --test-dir <build> --output-on-failure
# ---------------------------------------------------------------------------
option(SNIM_BUILD_TESTS "Build the Qt Test suite" ON)
if(SNIM_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
