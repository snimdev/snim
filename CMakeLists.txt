cmake_minimum_required(VERSION 3.31)
project(Niceshot)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 COMPONENTS
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

# All cross-platform sources live in a static library so both the executable and
# the test suite can link them. main.cpp + the .app bundle stay on the executable.
add_library(niceshot_lib STATIC
        # Core application files
        src/core/ScreenshotApp.cpp
        src/core/ScreenshotApp.h
        src/core/Settings.cpp
        src/core/Settings.h
        src/core/IconUtil.cpp
        src/core/IconUtil.h
        src/core/ScreenshotDialog.cpp
        src/core/ScreenshotDialog.h
        src/core/SettingsDialog.cpp
        src/core/SettingsDialog.h
        src/core/TextSnipCapture.cpp
        src/core/TextSnipCapture.h
        src/core/KeychainStore.h

        # Upload: S3-compatible Strategy + Factory (Qt Network + hand-rolled SigV4). The
        # macOS keychain backing is added in the APPLE block; a stub elsewhere. The
        # SFTP/FTP backends are conditional on libssh2/libcurl; everything here is not.
        src/upload/Uploader.h
        src/upload/UploaderFactory.cpp
        src/upload/UploaderFactory.h
        src/upload/UploadConfig.h
        src/upload/UploadProfiles.cpp
        src/upload/UploadProfiles.h
        src/upload/UploadUtil.cpp
        src/upload/UploadUtil.h
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
        src/capture/CaptureFactory.cpp
        src/capture/CaptureFactory.h
        src/capture/WindowEnumerator.h
        src/capture/strategies/CaptureStrategy.cpp
        src/capture/strategies/CaptureStrategy.h
        src/capture/strategies/NativeCaptureStrategy.cpp
        src/capture/strategies/NativeCaptureStrategy.h

        # Editor: shared canvas, layers, tools, interactions, commands
        src/editor/EditorChrome.h
        src/editor/ToolRegistry.cpp
        src/editor/ToolRegistry.h
        src/editor/commands/EditorCommands.cpp
        src/editor/commands/EditorCommands.h
        src/editor/DrawingGraphicsView.cpp
        src/editor/DrawingGraphicsView.h
        src/editor/tools/ArrowTool.cpp
        src/editor/tools/ArrowTool.h
        src/editor/tools/ArrowHandleTool.cpp
        src/editor/tools/ArrowHandleTool.h
        src/editor/tools/ShapeTool.cpp
        src/editor/tools/ShapeTool.h
        src/editor/tools/ShapeHandleTool.cpp
        src/editor/tools/ShapeHandleTool.h
        src/editor/tools/RectangleTool.cpp
        src/editor/tools/RectangleTool.h
        src/editor/tools/EllipseTool.cpp
        src/editor/tools/EllipseTool.h
        src/editor/tools/FreehandTool.cpp
        src/editor/tools/FreehandTool.h
        src/editor/tools/HighlightTool.cpp
        src/editor/tools/HighlightTool.h
        src/editor/tools/BlurTool.cpp
        src/editor/tools/BlurTool.h
        src/editor/tools/TextTool.cpp
        src/editor/tools/TextTool.h
        src/editor/Layer.cpp
        src/editor/Layer.h
        src/editor/LayerManager.cpp
        src/editor/LayerManager.h
        src/editor/LayerProperties.cpp
        src/editor/LayerProperties.h
        # Drawing Interactions
        src/editor/interactions/IDrawingInteraction.h
        src/editor/interactions/BaseDrawingInteraction.cpp
        src/editor/interactions/BaseDrawingInteraction.h
        src/editor/interactions/PointerToolInteraction.cpp
        src/editor/interactions/PointerToolInteraction.h
        src/editor/interactions/ArrowDrawingInteraction.cpp
        src/editor/interactions/ArrowDrawingInteraction.h
        src/editor/interactions/TextDrawingInteraction.cpp
        src/editor/interactions/TextDrawingInteraction.h
        src/editor/interactions/RectangleDrawingInteraction.cpp
        src/editor/interactions/RectangleDrawingInteraction.h
        src/editor/interactions/EllipseDrawingInteraction.cpp
        src/editor/interactions/EllipseDrawingInteraction.h
        src/editor/interactions/FreehandDrawingInteraction.cpp
        src/editor/interactions/FreehandDrawingInteraction.h
        src/editor/interactions/HighlightDrawingInteraction.cpp
        src/editor/interactions/HighlightDrawingInteraction.h
        src/editor/interactions/BlurDrawingInteraction.cpp
        src/editor/interactions/BlurDrawingInteraction.h

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
        src/editor/video/GifParams.h
        src/editor/video/StubVideoExporter.cpp
        src/editor/video/StubVideoExporter.h

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
add_executable(Niceshot main.cpp resources/resources.qrc)
target_link_libraries(Niceshot PRIVATE niceshot_lib)

# Platform-specific window enumeration for the interactive window picker.
# macOS uses CGWindowList (Objective-C++); other platforms get a stub that
# returns no windows, so window-pick falls back to the screen under the cursor.
if(APPLE)
    enable_language(OBJCXX)
    target_sources(niceshot_lib PRIVATE
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
    target_compile_definitions(niceshot_lib PUBLIC
            NICESHOT_HAVE_MAC_RECORDER
            NICESHOT_HAVE_MAC_HOTKEYS)
    target_link_libraries(niceshot_lib PUBLIC
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
    qt_import_plugins(Niceshot INCLUDE
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
    set_target_properties(Niceshot PROPERTIES
            _qt_has_QDarwinCameraPermissionPlugin_usage_description TRUE
            _qt_has_QDarwinMicrophonePermissionPlugin_usage_description TRUE)
else()
    target_sources(niceshot_lib PRIVATE
            src/capture/WindowEnumerator_stub.cpp
            src/core/KeychainStore_stub.cpp)
endif()

# Linux-only capture strategies. These rely on Linux desktop APIs that don't
# exist elsewhere: KWin uses the org.kde.KWin.ScreenShot2 D-Bus interface plus
# pipe2() for pixel streaming, and Wayland uses the xdg-desktop-portal D-Bus
# interface. On other platforms (e.g. macOS) the cross-platform Qt-based
# NativeCaptureStrategy is used instead.
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_sources(niceshot_lib PRIVATE
            src/capture/strategies/WaylandCaptureStrategy.cpp
            src/capture/strategies/WaylandCaptureStrategy.h
            src/capture/strategies/KWinCaptureStrategy.cpp
            src/capture/strategies/KWinCaptureStrategy.h
            # Global hotkeys via the xdg-desktop-portal GlobalShortcuts session
            src/hotkeys/backends/PortalHotkeyBackend.cpp
            src/hotkeys/backends/PortalHotkeyBackend.h
            # Screen recording handshake via the xdg-desktop-portal ScreenCast session.
            # Needs only QtDBus, so it is not gated on the GStreamer recorder.
            src/recording/strategies/ScreenCastPortalSession.cpp
            src/recording/strategies/ScreenCastPortalSession.h
    )
endif()

# Linux screen-recording backend - compiled only where GStreamer exists, like the
# optional upload transports. Without it RecordingFactory keeps returning the stub.
if(GSTREAMER_AVAILABLE)
    target_sources(niceshot_lib PRIVATE
            src/recording/strategies/LinuxRecordingStrategy.cpp
            src/recording/strategies/LinuxRecordingStrategy.h)
    target_include_directories(niceshot_lib PRIVATE ${GSTREAMER_INCLUDE_DIRS})
    # pkg-config reports plain library names; add its dirs so a non-default prefix links.
    if(GSTREAMER_LIBRARY_DIRS)
        target_link_directories(niceshot_lib PUBLIC ${GSTREAMER_LIBRARY_DIRS})
    endif()
    target_link_libraries(niceshot_lib PUBLIC ${GSTREAMER_LIBRARIES})
    # Activates the Linux branch of RecordingFactory + makes Auto pick the portal
    # recorder. PUBLIC because the tests check the macro.
    target_compile_definitions(niceshot_lib PUBLIC NICESHOT_HAVE_LINUX_RECORDER)
endif()

# Windows global hotkeys: RegisterHotKey + a WM_HOTKEY native event filter. The define
# activates the Windows branch of HotkeyBackendFactory (user32 needs no extra linking).
if(WIN32)
    target_sources(niceshot_lib PRIVATE
            src/hotkeys/backends/WindowsHotkeyBackend.cpp
            src/hotkeys/backends/WindowsHotkeyBackend.h
    )
    target_compile_definitions(niceshot_lib PUBLIC NICESHOT_HAVE_WIN_HOTKEYS)
endif()

# Set include directories for the organized structure. PUBLIC so the executable
# and the tests inherit the `src/`-rooted include path every #include relies on.
target_include_directories(niceshot_lib PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(niceshot_lib PUBLIC
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
    target_include_directories(niceshot_lib PRIVATE ${TESSERACT_INCLUDE_DIRS})
    # pkg-config reports library names (-ltesseract ...) without their location;
    # add the reported library dirs so the linker can find them (needed on
    # macOS/Homebrew where they live under a non-default prefix). Empty for the
    # manual-detection fallback, which already uses absolute library paths.
    target_link_directories(niceshot_lib PUBLIC ${TESSERACT_LIBRARY_DIRS})
    target_link_libraries(niceshot_lib PUBLIC ${TESSERACT_LIBRARIES})
endif()

# FTP/FTPS upload backend - compiled only where libcurl exists (like the Linux-only
# capture strategies). Without it UploaderFactory returns a Stub that says so, and the
# rest of the upload feature is unaffected. PUBLIC link: HAVE_LIBCURL is a global define,
# so consumers (the exe and the tests) must resolve FtpUploader's curl symbols too.
if(LIBCURL_AVAILABLE)
    target_sources(niceshot_lib PRIVATE
            src/upload/strategies/FtpUploader.cpp
            src/upload/strategies/FtpUploader.h)
    target_include_directories(niceshot_lib PRIVATE ${LIBCURL_INCLUDE_DIRS})
    # pkg-config reports plain library names; add its dirs so a non-default prefix
    # (Homebrew) still links. Empty for the find_package fallback (absolute paths).
    if(LIBCURL_LIBRARY_DIRS)
        target_link_directories(niceshot_lib PUBLIC ${LIBCURL_LIBRARY_DIRS})
    endif()
    target_link_libraries(niceshot_lib PUBLIC ${LIBCURL_LIBRARIES})
endif()

# SFTP upload backend - compiled only where libssh2 exists, exactly like the FTP one
# above. Without it UploaderFactory returns a Stub that says so. PUBLIC link:
# HAVE_LIBSSH2 is a global define, so consumers (the exe and the tests) must resolve
# SftpUploader's libssh2 symbols too.
if(LIBSSH2_AVAILABLE)
    target_sources(niceshot_lib PRIVATE
            src/upload/strategies/SftpUploader.cpp
            src/upload/strategies/SftpUploader.h)
    target_include_directories(niceshot_lib PRIVATE ${LIBSSH2_INCLUDE_DIRS})
    # pkg-config reports plain library names; add its dirs so a non-default prefix
    # (Homebrew) still links. Empty for the manual-detection fallback (absolute paths).
    if(LIBSSH2_LIBRARY_DIRS)
        target_link_directories(niceshot_lib PUBLIC ${LIBSSH2_LIBRARY_DIRS})
    endif()
    target_link_libraries(niceshot_lib PUBLIC ${LIBSSH2_LIBRARIES})
endif()

# ---------------------------------------------------------------------------
# macOS application bundle + deployment
# ---------------------------------------------------------------------------
# Building as a .app bundle (rather than a bare executable) gives the app a
# stable bundle identifier. The `deploy` target then runs macdeployqt to copy
# the Qt frameworks (and other linked dylibs) into the bundle and ad-hoc
# code-signs it, producing a self-contained, distributable Niceshot.app.
if(APPLE)
    set_target_properties(Niceshot PROPERTIES
            MACOSX_BUNDLE TRUE
            MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/cmake/Info.plist.in"
            MACOSX_BUNDLE_BUNDLE_NAME "Niceshot"
            MACOSX_BUNDLE_GUI_IDENTIFIER "com.darkog.niceshot"
            MACOSX_BUNDLE_BUNDLE_VERSION "1.0"
            MACOSX_BUNDLE_SHORT_VERSION_STRING "1.0"
            MACOSX_BUNDLE_COPYRIGHT "Copyright © 2026 Darko Gjorgjijoski"
    )

    # Optional custom app icon: drop an AppIcon.icns into resources/ and it gets
    # bundled automatically. Without it the bundle uses the default icon.
    set(NICESHOT_APP_ICON "${CMAKE_CURRENT_SOURCE_DIR}/resources/AppIcon.icns")
    if(EXISTS "${NICESHOT_APP_ICON}")
        target_sources(Niceshot PRIVATE "${NICESHOT_APP_ICON}")
        set_source_files_properties("${NICESHOT_APP_ICON}" PROPERTIES
                MACOSX_PACKAGE_LOCATION "Resources")
        set_target_properties(Niceshot PROPERTIES MACOSX_BUNDLE_ICON_FILE "AppIcon.icns")
    endif()

    # Dev code signing. Without this the linker ad-hoc signs the binary, whose
    # identity is the binary's own hash - it changes on EVERY rebuild, so modern
    # macOS treats each build as a new app and re-prompts for (or silently breaks)
    # the Screen Recording / Camera / Microphone TCC grants, even though System
    # Settings still shows Niceshot as enabled. Signing with a stable identity
    # (a local self-signed "Niceshot Dev Signing" certificate) keeps the
    # designated requirement identical across rebuilds, so grants persist.
    # Set to "" (or build on a machine without the cert) to fall back to ad-hoc.
    set(NICESHOT_CODESIGN_IDENTITY "Niceshot Dev Signing" CACHE STRING
            "Code-signing identity for the dev build (empty = linker ad-hoc signing)")
    if(NICESHOT_CODESIGN_IDENTITY)
        # Only sign when the identity actually exists in the keychain, so fresh
        # clones / CI runners without the cert still build out of the box.
        execute_process(COMMAND security find-identity -v -p codesigning
                OUTPUT_VARIABLE _niceshot_identities ERROR_QUIET)
        string(FIND "${_niceshot_identities}" "${NICESHOT_CODESIGN_IDENTITY}" _niceshot_identity_found)
        if(NOT _niceshot_identity_found EQUAL -1)
            add_custom_command(TARGET Niceshot POST_BUILD
                    COMMAND codesign --force --sign "${NICESHOT_CODESIGN_IDENTITY}"
                            --timestamp=none "$<TARGET_BUNDLE_DIR:Niceshot>"
                    COMMENT "Code-signing Niceshot.app with '${NICESHOT_CODESIGN_IDENTITY}' (stable TCC identity)")
        else()
            message(STATUS "Code-signing identity '${NICESHOT_CODESIGN_IDENTITY}' not in keychain - leaving ad-hoc signature")
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
        # the self-contained build lives in dist/Niceshot.app.
        set(NICESHOT_DIST_APP "${CMAKE_BINARY_DIR}/dist/Niceshot.app")
        # macos_deploy.sh runs macdeployqt, strips the build-time Homebrew rpaths,
        # and ad-hoc signs. For real distribution, replace the ad-hoc `-` identity
        # in the script with a Developer ID and notarize the result.
        add_custom_target(deploy
                COMMAND "${CMAKE_COMMAND}" -E rm -rf "${NICESHOT_DIST_APP}"
                COMMAND "${CMAKE_COMMAND}" -E make_directory "${CMAKE_BINARY_DIR}/dist"
                COMMAND "${CMAKE_COMMAND}" -E copy_directory
                        "$<TARGET_BUNDLE_DIR:Niceshot>" "${NICESHOT_DIST_APP}"
                COMMAND bash "${CMAKE_CURRENT_SOURCE_DIR}/cmake/macos_deploy.sh"
                        "${NICESHOT_DIST_APP}" "${MACDEPLOYQT_EXECUTABLE}"
                DEPENDS Niceshot
                COMMENT "Creating self-contained dist/Niceshot.app (Qt bundled, rpaths fixed, signed)"
                VERBATIM
        )
    else()
        message(WARNING "macdeployqt not found - the `deploy` target will be unavailable")
    endif()
endif()

# ---------------------------------------------------------------------------
# Tests (Qt Test + CTest). Configure with -DNICESHOT_BUILD_TESTS=OFF to skip.
# Run with: ctest --test-dir <build> --output-on-failure
# ---------------------------------------------------------------------------
option(NICESHOT_BUILD_TESTS "Build the Qt Test suite" ON)
if(NICESHOT_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
