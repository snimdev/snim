cmake_minimum_required(VERSION 3.31)

# Must run before project(): it defines the version project(VERSION) is given.
include("${CMAKE_CURRENT_LIST_DIR}/cmake/SnimVersion.cmake")

project(Snim VERSION ${SNIM_VERSION_NUMERIC})

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

if(WIN32)
    add_compile_definitions(NOMINMAX WIN32_LEAN_AND_MEAN)
endif()
if(MSVC)
    add_compile_options(/utf-8)
endif()

# 6.5 minimum: the permission API (QCameraPermission and friends) starts there.
find_package(Qt6 6.5 COMPONENTS
        Core
        Gui
        Widgets
        Concurrent
        Svg
        Multimedia
        MultimediaWidgets
        Network
        REQUIRED)

# Only the Linux portal and KWin code talks D-Bus; Windows has no session bus.
if(NOT WIN32)
    find_package(Qt6 COMPONENTS DBus REQUIRED)
endif()

# Windows takes its libraries from vcpkg packages; a pkg-config on PATH there
# (Strawberry Perl) would hand out MinGW builds.
if(NOT WIN32)
    find_package(PkgConfig)
endif()

# Optional Tesseract OCR support for text sniping feature
set(TESSERACT_AVAILABLE FALSE)

if(VCPKG_TOOLCHAIN)
    find_package(Tesseract CONFIG)
    if(Tesseract_FOUND)
        set(TESSERACT_AVAILABLE TRUE)
        set(TESSERACT_LIBRARIES Tesseract::libtesseract)
        message(STATUS "Tesseract OCR found via vcpkg - enabling text snipping feature")
        message(STATUS "  Version: ${Tesseract_VERSION}")
    endif()
endif()

if(PkgConfig_FOUND AND NOT TESSERACT_AVAILABLE)
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
if(NOT TESSERACT_AVAILABLE AND NOT WIN32)
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

# vcpkg's curl[ssl] uses Schannel on Windows.
if(VCPKG_TOOLCHAIN)
    find_package(CURL CONFIG)
    if(CURL_FOUND)
        set(LIBCURL_AVAILABLE TRUE)
        set(LIBCURL_LIBRARIES CURL::libcurl)
        message(STATUS "libcurl found via vcpkg - enabling FTP/FTPS uploads")
        message(STATUS "  Version: ${CURL_VERSION}")
    endif()
endif()

if(PkgConfig_FOUND AND NOT LIBCURL_AVAILABLE)
    pkg_check_modules(LIBCURL libcurl)
    if(LIBCURL_FOUND)
        set(LIBCURL_AVAILABLE TRUE)
        message(STATUS "libcurl found via pkg-config - enabling FTP/FTPS uploads")
        message(STATUS "  Version: ${LIBCURL_VERSION}")
        message(STATUS "  Libraries: ${LIBCURL_LIBRARIES}")
    endif()
endif()

# The macOS SDK ships libcurl without a .pc file, so fall back to CMake's finder.
if(NOT LIBCURL_AVAILABLE AND NOT WIN32)
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

# vcpkg's libssh2[openssl]: WinCNG has no ed25519 host or user keys.
if(VCPKG_TOOLCHAIN)
    find_package(libssh2 CONFIG)
    if(libssh2_FOUND)
        set(LIBSSH2_AVAILABLE TRUE)
        set(LIBSSH2_LIBRARIES libssh2::libssh2)
        message(STATUS "libssh2 found via vcpkg - enabling SFTP uploads")
        message(STATUS "  Version: ${libssh2_VERSION}")
    endif()
endif()

if(PkgConfig_FOUND AND NOT LIBSSH2_AVAILABLE)
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

# The vcpkg toolchain (Windows) provides CMake packages for libwebp and libimagequant.
if(VCPKG_TOOLCHAIN)
    find_package(WebP CONFIG)
    if(WebP_FOUND)
        set(LIBWEBP_AVAILABLE TRUE)
        set(LIBWEBP_LIBRARIES WebP::webp)
        set(LIBWEBPMUX_LIBRARIES WebP::libwebpmux)
        message(STATUS "libwebp found via vcpkg - animated WebP export")
        message(STATUS "  Version: ${WebP_VERSION}")
    endif()
endif()

if(PkgConfig_FOUND AND NOT LIBWEBP_AVAILABLE)
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

# Required giflib + libimagequant - animated GIF export in the trim editor: giflib writes
# the file, libimagequant picks each frame's palette and dithers it. Required like
# libwebp: every build ships GIF export. 5.1 is the first giflib with the current API.
find_package(GIF 5.1)
if(NOT GIF_FOUND)
    message(FATAL_ERROR
            "giflib 5.1 or newer is required but was not found.\n"
            "  Install with: sudo apt install libgif-dev / sudo dnf install giflib-devel / brew install giflib")
endif()
message(STATUS "giflib found - animated GIF export")
message(STATUS "  Version: ${GIF_VERSION}")
message(STATUS "  Include: ${GIF_INCLUDE_DIRS}")
message(STATUS "  Libraries: ${GIF_LIBRARIES}")

set(IMAGEQUANT_AVAILABLE FALSE)

if(VCPKG_TOOLCHAIN)
    find_package(imagequant CONFIG)
    if(imagequant_FOUND)
        set(IMAGEQUANT_AVAILABLE TRUE)
        set(IMAGEQUANT_LIBRARIES imagequant::imagequant)
        message(STATUS "libimagequant found via vcpkg - GIF palettes")
        message(STATUS "  Version: ${imagequant_VERSION}")
    endif()
endif()

if(PkgConfig_FOUND AND NOT IMAGEQUANT_AVAILABLE)
    pkg_check_modules(IMAGEQUANT imagequant)
    if(IMAGEQUANT_FOUND)
        set(IMAGEQUANT_AVAILABLE TRUE)
        message(STATUS "libimagequant found via pkg-config - GIF palettes")
        message(STATUS "  Version: ${IMAGEQUANT_VERSION}")
        message(STATUS "  Include: ${IMAGEQUANT_INCLUDE_DIRS}")
        message(STATUS "  Libraries: ${IMAGEQUANT_LIBRARIES}")
    endif()
endif()

if(NOT IMAGEQUANT_AVAILABLE)
    find_path(Imagequant_INCLUDE_DIR libimagequant.h
        PATHS /usr/include /usr/local/include /opt/homebrew/include)
    find_library(Imagequant_LIBRARY imagequant
        PATHS /usr/lib64 /usr/lib /usr/local/lib /opt/homebrew/lib)

    if(Imagequant_INCLUDE_DIR AND Imagequant_LIBRARY)
        set(IMAGEQUANT_AVAILABLE TRUE)
        set(IMAGEQUANT_INCLUDE_DIRS ${Imagequant_INCLUDE_DIR})
        set(IMAGEQUANT_LIBRARIES ${Imagequant_LIBRARY})
        message(STATUS "libimagequant found (manual detection) - GIF palettes")
        message(STATUS "  Include: ${IMAGEQUANT_INCLUDE_DIRS}")
        message(STATUS "  Libraries: ${IMAGEQUANT_LIBRARIES}")
    endif()
endif()

if(NOT IMAGEQUANT_AVAILABLE)
    message(FATAL_ERROR
            "libimagequant is required but was not found.\n"
            "  Install with: sudo apt install libimagequant-dev / sudo dnf install libimagequant-devel / brew install libimagequant")
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

# Optional XCB - the X11 window picker and window recording. Qt's xcb platform plugin
# needs libxcb at runtime anyway; without its headers both fall back to whole screens.
set(XCB_AVAILABLE FALSE)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND PkgConfig_FOUND)
    pkg_check_modules(XCB IMPORTED_TARGET xcb)
    if(XCB_FOUND)
        set(XCB_AVAILABLE TRUE)
        message(STATUS "XCB found - enabling the X11 window picker and window recording")
        message(STATUS "  Version: ${XCB_VERSION}")
    else()
        message(STATUS "XCB not found - X11 window picking and recording fall back to screens")
        message(STATUS "  Install with: sudo apt install libxcb1-dev / sudo dnf install libxcb-devel")
    endif()
endif()

# Optional native Wayland screencopy (ext-image-copy-capture, wlr-screencopy): instant,
# prompt-free captures on wlroots-family compositors. The protocol XMLs are vendored, so
# only libwayland-client and wayland-scanner are needed.
set(SCREENCOPY_AVAILABLE FALSE)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND PkgConfig_FOUND)
    pkg_check_modules(WAYLAND_CLIENT IMPORTED_TARGET wayland-client>=1.20)
    pkg_get_variable(_wayland_scanner_pc wayland-scanner wayland_scanner)
    find_program(WAYLAND_SCANNER_EXECUTABLE NAMES wayland-scanner HINTS "${_wayland_scanner_pc}")
    if(WAYLAND_CLIENT_FOUND AND WAYLAND_SCANNER_EXECUTABLE)
        set(SCREENCOPY_AVAILABLE TRUE)
        message(STATUS "wayland-client found - enabling native Wayland screencopy")
        message(STATUS "  Version: ${WAYLAND_CLIENT_VERSION}")
        message(STATUS "  Scanner: ${WAYLAND_SCANNER_EXECUTABLE}")
    endif()
endif()

if(NOT SCREENCOPY_AVAILABLE AND CMAKE_SYSTEM_NAME STREQUAL "Linux")
    message(STATUS "wayland-client or wayland-scanner not found - native Wayland screencopy disabled")
    message(STATUS "  Install with: sudo apt install libwayland-dev / sudo dnf install wayland-devel")
endif()

# All cross-platform sources live in a static library so both the executable and
# the test suite can link them. main.cpp + the .app bundle stay on the executable.
add_library(snim_lib STATIC
        # Application: tray, workflows, settings UI
        src/app/Application.cpp
        src/app/Application.h
        src/app/CaptureWorkflow.cpp
        src/app/CaptureWorkflow.h
        src/app/Notifier.h
        src/app/RecordingWorkflow.cpp
        src/app/RecordingWorkflow.h
        src/app/SettingsDialog.cpp
        src/app/SettingsDialog.h
        src/app/TextSnipWorkflow.cpp
        src/app/TextSnipWorkflow.h
        src/app/TrayMenu.cpp
        src/app/TrayMenu.h
        src/app/UploadWorkflow.cpp
        src/app/UploadWorkflow.h

        # Core application files
        src/core/Settings.cpp
        src/core/Settings.h
        src/core/IconUtil.cpp
        src/core/IconUtil.h
        src/core/Perf.cpp
        src/core/Perf.h
        src/core/FileNames.h
        src/core/PostToGui.h
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
        # Flatpak detection, so sandbox-managed features step aside.
        src/core/Sandbox.cpp
        src/core/Sandbox.h
        # Finds the Tesseract language packs beside a relocated binary.
        src/core/BundledPaths.cpp
        src/core/BundledPaths.h
        # Finds and dlopens the optional Linux modules (recorder, video exporter).
        src/core/DynamicModule.cpp
        src/core/DynamicModule.h
        # `snim --self-test`: plugin and library checks for a deployed bundle.
        src/core/SelfTest.cpp
        src/core/SelfTest.h

        # Upload: S3-compatible Strategy + Factory (Qt Network + hand-rolled SigV4). The
        # keychain backings are added per platform below. The SFTP/FTP backends are
        # conditional on libssh2/libcurl; everything here is not.
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
        src/upload/SocketShim.cpp
        src/upload/SocketShim.h
        src/upload/SigV4.cpp
        src/upload/SigV4.h
        src/upload/strategies/BlockingUploader.cpp
        src/upload/strategies/BlockingUploader.h
        src/upload/strategies/S3Uploader.cpp
        src/upload/strategies/S3Uploader.h
        src/upload/strategies/StubUploader.cpp
        src/upload/strategies/StubUploader.h

        # Screen: selection overlay, window enumeration, native overlay tweaks
        src/screen/AreaSelector.cpp
        src/screen/AreaSelector.h
        src/screen/AreaSelectorInput.cpp
        src/screen/AreaSelectorInput.h
        src/screen/OverlayStyle.h
        src/screen/SelectionLayer.h
        src/screen/SelectorGroup.cpp
        src/screen/SelectorGroup.h
        src/screen/ScreenMap.h
        src/screen/WinScreenMap.h
        src/screen/X11ScreenMap.h
        src/screen/WinWindowFilter.h
        # Frozen backdrop for the selection overlays (portal-based on Wayland).
        src/screen/FrozenFrameGrabber.cpp
        src/screen/FrozenFrameGrabber.h
        src/screen/WindowEnumerator.h
        # Facade over native overlay tweaks; the per-platform file is added below.
        src/screen/OverlayWindows.h
        # Wayland layer-shell helper for the overlays (no-op without LayerShellQt).
        src/screen/LayerShellSupport.h
        src/screen/DesktopStitch.h
        src/screen/ScreencopyGeometry.h
        # Full-desktop frame sources, shared by capture and the frozen frame.
        src/screen/sources/DesktopFrameSource.h
        src/screen/sources/FrameSourceChain.cpp
        src/screen/sources/FrameSourceChain.h
        src/screen/sources/FrameSourceFactory.cpp
        src/screen/sources/FrameSourceFactory.h
        src/screen/sources/StrategySelection.h
        src/screen/sources/QtScreensFrameSource.cpp
        src/screen/sources/QtScreensFrameSource.h

        # Capture functionality
        src/capture/CaptureFactory.cpp
        src/capture/CaptureFactory.h
        src/capture/CaptureGeometry.h
        src/capture/OverlayAnnotations.cpp
        src/capture/OverlayAnnotations.h
        src/capture/strategies/CaptureStrategy.cpp
        src/capture/strategies/CaptureStrategy.h
        src/capture/strategies/FrameSourceCaptureStrategy.cpp
        src/capture/strategies/FrameSourceCaptureStrategy.h
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
        src/editor/annotations/tools/ToolItem.cpp
        src/editor/annotations/tools/ToolItem.h
        src/editor/annotations/CanvasShadow.cpp
        src/editor/annotations/CanvasShadow.h
        src/editor/annotations/tools/ArrowTool.cpp
        src/editor/annotations/tools/ArrowTool.h
        src/editor/annotations/tools/ShapeTool.cpp
        src/editor/annotations/tools/ShapeTool.h
        src/editor/annotations/tools/HandleItem.cpp
        src/editor/annotations/tools/HandleItem.h
        src/editor/annotations/tools/RectangleTool.cpp
        src/editor/annotations/tools/RectangleTool.h
        src/editor/annotations/tools/EllipseTool.cpp
        src/editor/annotations/tools/EllipseTool.h
        src/editor/annotations/tools/PathTool.cpp
        src/editor/annotations/tools/PathTool.h
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
        src/editor/annotations/interactions/DrawingInteractions.cpp
        src/editor/annotations/interactions/DrawingInteractions.h

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
        src/editor/video/AnimationEncoder.cpp
        src/editor/video/AnimationEncoder.h
        src/editor/video/GifEncoder.cpp
        src/editor/video/GifEncoder.h
        src/editor/video/WebpEncoder.cpp
        src/editor/video/WebpEncoder.h
        src/editor/video/AnimationExporter.cpp
        src/editor/video/AnimationExporter.h

        # Recording: platform-agnostic Strategy + Factory (the macOS ScreenCaptureKit
        # backend is added in the APPLE block below). The stub is the universal fallback.
        src/recording/RecordTarget.h
        src/recording/RecordingGeometry.h
        src/recording/PcmMixBuffer.h
        src/recording/PauseAwareClock.h
        src/recording/RecordingStrategy.cpp
        src/recording/RecordingStrategy.h
        src/recording/RecordingFactory.cpp
        src/recording/RecordingFactory.h
        src/recording/RecordingController.cpp
        src/recording/RecordingController.h
        src/recording/RecordingJournal.cpp
        src/recording/RecordingJournal.h
        src/recording/RecordingControls.cpp
        src/recording/RecordingControls.h
        src/recording/RecordingFrameOverlay.cpp
        src/recording/RecordingFrameOverlay.h
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
add_executable(Snim src/main.cpp resources/resources.qrc)
target_link_libraries(Snim PRIVATE snim_lib)

# Lowercase binary everywhere but macOS, where the bundle must stay Snim.app.
if(NOT APPLE)
    set_target_properties(Snim PROPERTIES OUTPUT_NAME "snim")
endif()

# A GUI-subsystem exe (no console window) with icon, version resource and manifest.
# Tests stay console apps: only this target gets WIN32_EXECUTABLE.
if(WIN32)
    set(SNIM_WIN_ICON "${CMAKE_CURRENT_SOURCE_DIR}/resources/snim.ico")
    string(REPLACE "." "," SNIM_VERSION_RC "${SNIM_VERSION_NUMERIC},0")
    configure_file("${CMAKE_CURRENT_SOURCE_DIR}/cmake/snim.rc.in"
            "${CMAKE_BINARY_DIR}/generated/snim.rc" @ONLY)
    target_sources(Snim PRIVATE
            "${CMAKE_BINARY_DIR}/generated/snim.rc"
            "${CMAKE_CURRENT_SOURCE_DIR}/cmake/snim.manifest")
    set_target_properties(Snim PROPERTIES WIN32_EXECUTABLE TRUE)
endif()

# Platform-specific window enumeration for the interactive window picker.
# macOS uses CGWindowList (Objective-C++), Windows EnumWindows; Linux gets a stub
# that returns no windows, so window-pick falls back to the screen under the cursor.
if(APPLE)
    enable_language(OBJCXX)
    target_sources(snim_lib PRIVATE
            src/screen/WindowEnumerator_mac.mm
            src/screen/OverlayWindows_mac.mm
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
    # Activates the Mac branch of RecordingFactory and the Carbon branch of
    # HotkeyBackendFactory.
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
elseif(WIN32)
    target_sources(snim_lib PRIVATE
            # EnumWindows + DWM frame bounds for the window picker
            src/screen/WindowEnumerator_win.cpp
            src/screen/OverlayWindows_win.cpp
            # Credential Manager backing for upload secrets (advapi32, linked by default)
            src/core/KeychainStore_win.cpp
            src/core/SingleInstance.h
            src/core/SingleInstance_win.cpp)
elseif(XCB_AVAILABLE)
    target_sources(snim_lib PRIVATE
            # The window manager's client list over Qt's own XCB connection
            src/screen/WindowEnumerator_x11.cpp
            src/screen/X11WindowFilter.h
            src/screen/X11Windows.cpp
            src/screen/X11Windows.h
            src/screen/OverlayWindows_stub.cpp)
    target_link_libraries(snim_lib PRIVATE PkgConfig::XCB)
else()
    target_sources(snim_lib PRIVATE
            src/screen/WindowEnumerator_stub.cpp
            src/screen/OverlayWindows_stub.cpp)
endif()

# Upload secrets: Linux keeps them in the freedesktop Secret Service over Qt DBus; any
# platform without a keychain backing above gets the no-op stub.
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_sources(snim_lib PRIVATE
            src/core/KeychainStore_linux.cpp
            src/core/SecretService.cpp
            src/core/SecretService.h)
elseif(NOT APPLE AND NOT WIN32)
    target_sources(snim_lib PRIVATE src/core/KeychainStore_stub.cpp)
endif()

# Linux-only capture sources. These rely on Linux desktop APIs that don't exist
# elsewhere: KWin uses the org.kde.KWin.ScreenShot2 D-Bus interface plus pipe2() for
# pixel streaming, and the portal source the xdg-desktop-portal D-Bus interface. On
# other platforms (e.g. macOS) the cross-platform Qt-based NativeCaptureStrategy is used.
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_sources(snim_lib PRIVATE
            # Request paths, Response subscriptions and probes every portal client shares
            src/core/Portal.cpp
            src/core/Portal.h
            src/capture/strategies/KWinCaptureStrategy.cpp
            src/capture/strategies/KWinCaptureStrategy.h
            src/screen/sources/KWinFrameSource.cpp
            src/screen/sources/KWinFrameSource.h
            src/screen/sources/PortalFrameSource.cpp
            src/screen/sources/PortalFrameSource.h
            # Global hotkeys via the xdg-desktop-portal GlobalShortcuts session
            src/hotkeys/backends/PortalHotkeyBackend.cpp
            src/hotkeys/backends/PortalHotkeyBackend.h
    )
endif()

# Native Wayland screencopy: client code generated from the vendored protocols, kept in
# its own library so the manual probe can link it without the rest of the app.
if(SCREENCOPY_AVAILABLE)
    set(_snim_protocol_dir "${CMAKE_CURRENT_SOURCE_DIR}/third_party/wayland-protocols")
    set(_snim_protocol_out "${CMAKE_CURRENT_BINARY_DIR}/wayland-protocols")
    file(MAKE_DIRECTORY "${_snim_protocol_out}")
    set(_snim_protocol_sources)
    # ext-image-capture-source references ext-foreign-toplevel-list, so its code links too.
    foreach(_protocol ext-image-copy-capture-v1 ext-image-capture-source-v1
                      ext-foreign-toplevel-list-v1 wlr-screencopy-unstable-v1)
        set(_xml "${_snim_protocol_dir}/${_protocol}.xml")
        set(_header "${_snim_protocol_out}/${_protocol}-client-protocol.h")
        set(_code "${_snim_protocol_out}/${_protocol}-protocol.c")
        add_custom_command(OUTPUT "${_header}"
                COMMAND "${WAYLAND_SCANNER_EXECUTABLE}" client-header "${_xml}" "${_header}"
                DEPENDS "${_xml}" VERBATIM)
        add_custom_command(OUTPUT "${_code}"
                COMMAND "${WAYLAND_SCANNER_EXECUTABLE}" private-code "${_xml}" "${_code}"
                DEPENDS "${_xml}" VERBATIM)
        list(APPEND _snim_protocol_sources "${_header}" "${_code}")
    endforeach()

    add_library(snim_screencopy STATIC
            src/screen/ScreencopyClient.cpp
            src/screen/ScreencopyClient.h
            ${_snim_protocol_sources})
    set_target_properties(snim_screencopy PROPERTIES AUTOMOC OFF)
    target_include_directories(snim_screencopy
            PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src
            PRIVATE "${_snim_protocol_out}")
    target_link_libraries(snim_screencopy PUBLIC Qt::Gui PRIVATE PkgConfig::WAYLAND_CLIENT)
endif()

if(SCREENCOPY_AVAILABLE)
    target_sources(snim_lib PRIVATE
            src/screen/sources/ScreencopyFrameSource.cpp
            src/screen/sources/ScreencopyFrameSource.h)
    target_link_libraries(snim_lib PUBLIC snim_screencopy)
    # Activates the screencopy source in FrameSourceFactory. PUBLIC because the tests check it.
    target_compile_definitions(snim_lib PUBLIC SNIM_HAVE_SCREENCOPY)
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
            src/recording/strategies/LinuxRecorderModule.h
            # Screenshots from a ScreenCast session; the module grabs the frames.
            src/screen/sources/ScreencastFrameSource.cpp
            src/screen/sources/ScreencastFrameSource.h
            src/screen/PipeWireFrames.cpp
            src/screen/PipeWireFrames.h
            src/screen/ScreenCastPortalSession.cpp
            src/screen/ScreenCastPortalSession.h)
    # Activates the Linux branch of RecordingFactory. PUBLIC because the tests check
    # the macro.
    target_compile_definitions(snim_lib PUBLIC SNIM_HAVE_LINUX_RECORDER)

    add_library(snim_recorder_linux MODULE
            src/recording/strategies/LinuxRecordingStrategy.cpp
            src/recording/strategies/LinuxRecordingStrategy.h
            src/recording/strategies/LinuxPipeline.h
            # The portal handshake is an implementation detail of the recorder, so the
            # module carries its own copy: a static library hands an object out only
            # when something else already pulled it into the binary doing the loading.
            src/screen/ScreenCastPortalSession.cpp
            src/screen/ScreenCastPortalSession.h
            src/core/Portal.cpp
            src/core/Portal.h
            # One-frame grabs for the ScreenCast frame source.
            src/recording/strategies/PipeWireFrameGrabber.cpp
            src/media/gst/GstSupport.cpp
            src/media/gst/GstSupport.h)
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
    target_link_directories(snim_recorder_linux PRIVATE ${GSTREAMER_LIBRARY_DIRS})
    target_link_libraries(snim_recorder_linux PRIVATE
            Qt::Core
            Qt::Gui
            Qt::DBus
            ${GSTREAMER_LIBRARIES})
    if(XCB_AVAILABLE)
        # Window recording reads the picked window by id, through the picker's own queries.
        target_sources(snim_recorder_linux PRIVATE
                src/screen/X11WindowFilter.h
                src/screen/X11Windows.cpp
                src/screen/X11Windows.h)
        target_link_libraries(snim_recorder_linux PRIVATE PkgConfig::XCB)
        target_compile_definitions(snim_recorder_linux PRIVATE SNIM_HAVE_XCB)
    endif()
    # RecordingStrategy stays undefined in the module and binds to the loading binary at
    # dlopen time, which needs that binary's own symbols in its dynamic table.
    set_target_properties(Snim PROPERTIES ENABLE_EXPORTS ON)
    add_dependencies(Snim snim_recorder_linux)

    # The trim exporter is a second dlopened module, so it borrows only VideoExporter
    # from the host and fails independently of the recorder.
    target_sources(snim_lib PRIVATE
            src/editor/video/LinuxVideoModule.cpp
            src/editor/video/LinuxVideoModule.h)
    target_compile_definitions(snim_lib PUBLIC SNIM_HAVE_LINUX_VIDEO_EXPORTER)

    add_library(snim_video_linux MODULE
            src/editor/video/GstVideoExporter.cpp
            src/editor/video/GstVideoExporter.h
            src/media/gst/GstSupport.cpp
            src/media/gst/GstSupport.h)
    set_target_properties(snim_video_linux PROPERTIES
            OUTPUT_NAME "snim-video-linux"
            LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
            INSTALL_RPATH "$ORIGIN")
    target_include_directories(snim_video_linux PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}
            ${CMAKE_CURRENT_SOURCE_DIR}/src
            ${GSTREAMER_INCLUDE_DIRS})
    target_link_directories(snim_video_linux PRIVATE ${GSTREAMER_LIBRARY_DIRS})
    target_link_libraries(snim_video_linux PRIVATE
            Qt::Core
            ${GSTREAMER_LIBRARIES})
    add_dependencies(Snim snim_video_linux)
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

# The FFmpeg media layer: encode and trim, Windows only for now. vcpkg's x64-windows-snim
# triplet builds FFmpeg static, so it cannot clash with Qt Multimedia's FFmpeg DLLs.
if(WIN32)
    # vcpkg's module, not Qt's FindFFmpeg.cmake: only it lists the static deps (x264, VPL).
    list(PREPEND CMAKE_MODULE_PATH "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/share/ffmpeg")
    find_package(FFMPEG REQUIRED)
    list(POP_FRONT CMAKE_MODULE_PATH)
    add_library(snim_media_ffmpeg STATIC
            src/media/ffmpeg/FfmpegSupport.cpp
            src/media/ffmpeg/FfmpegSupport.h
            src/media/ffmpeg/H264EncoderChain.cpp
            src/media/ffmpeg/H264EncoderChain.h
            src/media/ffmpeg/FfmpegEncoder.cpp
            src/media/ffmpeg/FfmpegEncoder.h
            src/media/ffmpeg/FfmpegTranscoder.cpp
            src/media/ffmpeg/FfmpegTranscoder.h
            src/media/ffmpeg/FfmpegTrimTranscoder.cpp
            src/media/ffmpeg/FfmpegTrimTranscoder.h)
    target_include_directories(snim_media_ffmpeg PUBLIC
            ${CMAKE_CURRENT_SOURCE_DIR}/src
            ${FFMPEG_INCLUDE_DIRS})
    target_link_directories(snim_media_ffmpeg PUBLIC ${FFMPEG_LIBRARY_DIRS})
    target_link_libraries(snim_media_ffmpeg PUBLIC Qt::Core ${FFMPEG_LIBRARIES})

    # Trims through FFmpeg. PUBLIC because the tests check the macro.
    target_sources(snim_lib PRIVATE
            src/editor/video/FfmpegVideoExporter.cpp
            src/editor/video/FfmpegVideoExporter.h)
    target_link_libraries(snim_lib PUBLIC snim_media_ffmpeg)
    target_compile_definitions(snim_lib PUBLIC SNIM_HAVE_FFMPEG_VIDEO_EXPORTER)
endif()

# Windows screen recording: Graphics Capture through C++/WinRT and Direct3D 11, WASAPI audio.
if(WIN32)
    target_sources(snim_lib PRIVATE
            src/recording/strategies/windows/WgcFrameSource.cpp
            src/recording/strategies/windows/WgcFrameSource.h
            src/recording/strategies/windows/WasapiAudioSource.cpp
            src/recording/strategies/windows/WasapiAudioSource.h
            src/recording/strategies/windows/WindowsRecordingStrategy.cpp
            src/recording/strategies/windows/WindowsRecordingStrategy.h)
    target_link_libraries(snim_lib PUBLIC windowsapp d3d11 dxgi)
    # Activates the Windows branch of RecordingFactory.
    target_compile_definitions(snim_lib PUBLIC SNIM_HAVE_WIN_RECORDER)
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
if(NOT WIN32)
    target_link_libraries(snim_lib PUBLIC Qt::DBus)
endif()
# Winsock, behind upload/SocketShim; DWM for window bounds and cloaking.
if(WIN32)
    target_link_libraries(snim_lib PUBLIC ws2_32 dwmapi)
endif()

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
    target_link_directories(snim_lib PUBLIC ${LIBCURL_LIBRARY_DIRS})
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
    target_link_directories(snim_lib PUBLIC ${LIBSSH2_LIBRARY_DIRS})
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
target_link_directories(snim_lib PUBLIC ${LIBWEBP_LIBRARY_DIRS})
target_link_libraries(snim_lib PUBLIC ${LIBWEBPMUX_LIBRARIES} ${LIBWEBP_LIBRARIES})

# Animated GIF export, shared like WebP. PUBLIC link so the tests resolve the same symbols.
target_link_libraries(snim_lib PUBLIC GIF::GIF)
target_include_directories(snim_lib PRIVATE ${IMAGEQUANT_INCLUDE_DIRS})
# pkg-config reports plain library names; add its dirs so a non-default prefix
# (Homebrew) still links. Empty for the manual-detection fallback (absolute paths).
target_link_directories(snim_lib PUBLIC ${IMAGEQUANT_LIBRARY_DIRS})
target_link_libraries(snim_lib PUBLIC ${IMAGEQUANT_LIBRARIES})

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

    set(SNIM_APP_ICON "${CMAKE_CURRENT_SOURCE_DIR}/resources/AppIcon.icns")
    target_sources(Snim PRIVATE "${SNIM_APP_ICON}")
    set_source_files_properties("${SNIM_APP_ICON}" PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")
    set_target_properties(Snim PROPERTIES MACOSX_BUNDLE_ICON_FILE "AppIcon.icns")

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
# and the hicolor icons every launcher indexes. The release job installs into the
# bundled tree that the packages put at /opt/snim and the portable tarball carries,
# with the same rules, so the layouts can never drift.
if(UNIX AND NOT APPLE)
    include(GNUInstallDirs)

    install(TARGETS Snim RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

    # The GStreamer modules are dlopened, not linked, so no deployment tool finds them by
    # walking the binary: they need an install rule of their own for the packages and the
    # portable tarball to carry them (the loader looks in ../lib next to the binary).
    if(GSTREAMER_AVAILABLE)
        install(TARGETS snim_recorder_linux snim_video_linux
                LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
    endif()

    # KWin authorizes ScreenShot2 only when Exec names the real binary, hence the absolute
    # default. linuxdeploy and Flatpak want a bare command name and pass one instead.
    set(SNIM_DESKTOP_EXEC "" CACHE STRING "Exec of the installed desktop entry (default: the installed binary)")
    if(NOT SNIM_DESKTOP_EXEC)
        set(SNIM_DESKTOP_EXEC "${CMAKE_INSTALL_FULL_BINDIR}/snim")
    endif()
    configure_file("${CMAKE_CURRENT_SOURCE_DIR}/deploy/dev.snim.Snim.desktop.in"
                   "${CMAKE_CURRENT_BINARY_DIR}/dev.snim.Snim.desktop" @ONLY)
    install(FILES "${CMAKE_CURRENT_BINARY_DIR}/dev.snim.Snim.desktop"
            DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)

    install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/deploy/dev.snim.Snim.metainfo.xml"
            DESTINATION ${CMAKE_INSTALL_DATADIR}/metainfo)

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
# Windows install rules
# ---------------------------------------------------------------------------
# `cmake --install` stages a self-contained folder: snim.exe at the root, the vcpkg DLLs,
# the MSVC runtime, then everything windeployqt adds. The installer and the portable zip
# both package this tree.
if(WIN32)
    install(TARGETS Snim RUNTIME DESTINATION .)

    # Applocal deployment already copied the DLLs snim.exe needs beside it: mirror that set.
    install(CODE "
        file(GLOB _snim_vcpkg_dlls \"${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/bin/*.dll\")
        foreach(_snim_dll IN LISTS _snim_vcpkg_dlls)
            get_filename_component(_snim_dll_name \"\${_snim_dll}\" NAME)
            if(EXISTS \"$<TARGET_FILE_DIR:Snim>/\${_snim_dll_name}\")
                file(INSTALL \"\${_snim_dll}\" DESTINATION \"\${CMAKE_INSTALL_PREFIX}\")
            endif()
        endforeach()")

    # vcpkg's tesseract ships no language data. This is tessdata 4.1.0, the model Debian's
    # tesseract-ocr-eng carries; it lands in <build>/tessdata, where snim.exe also finds it.
    set(SNIM_ENG_TRAINEDDATA "" CACHE FILEPATH "Local eng.traineddata to bundle (offline builds)")
    set(_snim_traineddata_sha256 daa0c97d651c19fba3b25e81317cd697e9908c8208090c94c3905381c23fc047)
    set(_snim_traineddata "${CMAKE_BINARY_DIR}/tessdata/eng.traineddata")
    if(SNIM_ENG_TRAINEDDATA)
        file(SHA256 "${SNIM_ENG_TRAINEDDATA}" _snim_traineddata_actual)
        if(NOT _snim_traineddata_actual STREQUAL _snim_traineddata_sha256)
            message(FATAL_ERROR "SNIM_ENG_TRAINEDDATA does not match tessdata 4.1.0 eng.traineddata")
        endif()
        file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/tessdata")
        file(COPY_FILE "${SNIM_ENG_TRAINEDDATA}" "${_snim_traineddata}" ONLY_IF_DIFFERENT)
    else()
        # Skipped when the cached file already has the expected hash.
        file(DOWNLOAD
                "https://raw.githubusercontent.com/tesseract-ocr/tessdata/4767ea922bcc460e70b87b1d303ebdfed0897da8/eng.traineddata"
                "${_snim_traineddata}"
                EXPECTED_HASH SHA256=${_snim_traineddata_sha256}
                TLS_VERIFY ON)
    endif()
    install(FILES "${_snim_traineddata}" DESTINATION tessdata)

    set(CMAKE_INSTALL_SYSTEM_RUNTIME_DESTINATION .)
    include(InstallRequiredSystemLibraries)

    get_target_property(_qmake_executable Qt6::qmake IMPORTED_LOCATION)
    get_filename_component(_qt_bin_dir "${_qmake_executable}" DIRECTORY)
    find_program(WINDEPLOYQT_EXECUTABLE windeployqt HINTS "${_qt_bin_dir}" REQUIRED)

    # The CRT is installed above, so windeployqt must not drop vc_redist.x64.exe in too.
    install(CODE "
        execute_process(
                COMMAND \"${WINDEPLOYQT_EXECUTABLE}\" --release --no-translations
                        --no-compiler-runtime \"\${CMAKE_INSTALL_PREFIX}/snim.exe\"
                COMMAND_ERROR_IS_FATAL ANY)")
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
