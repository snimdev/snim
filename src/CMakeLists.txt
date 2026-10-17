# One section per module: its sources, then its platform files and the helper targets it owns.

# ---- app ----
target_sources(snim_lib PRIVATE
        app/Application.cpp
        app/Application.h
        app/CaptureWorkflow.cpp
        app/CaptureWorkflow.h
        app/Notifier.h
        app/ProviderForm.cpp
        app/ProviderForm.h
        app/RecordingWorkflow.cpp
        app/RecordingWorkflow.h
        app/SettingsDialog.cpp
        app/SettingsDialog.h
        app/TextSnipWorkflow.cpp
        app/TextSnipWorkflow.h
        app/TrayMenu.cpp
        app/TrayMenu.h
        app/UploadWorkflow.cpp
        app/UploadWorkflow.h)

# ---- core ----
target_sources(snim_lib PRIVATE
        core/Settings.cpp
        core/Settings.h
        core/IconUtil.cpp
        core/IconUtil.h
        core/Perf.cpp
        core/Perf.h
        core/FileNames.h
        core/Desktop.h
        core/PostToGui.h
        core/KeychainStore.h
        core/UpdateCheck.cpp
        core/UpdateCheck.h
        core/DesktopIntegration.cpp
        core/DesktopIntegration.h
        core/AppScope.cpp
        core/AppScope.h
        core/Sandbox.cpp
        core/Sandbox.h
        core/BundledPaths.cpp
        core/BundledPaths.h
        core/DynamicModule.cpp
        core/DynamicModule.h
        core/SelfTest.cpp
        core/SelfTest.h)

# Upload secrets go to the platform keychain; the stub covers platforms without one.
if(APPLE)
    target_sources(snim_lib PRIVATE
            core/KeychainStore_mac.mm
            core/MacTrayWorkaround.h
            core/MacTrayWorkaround.mm)
    snim_objc_arc(core/KeychainStore_mac.mm)
elseif(WIN32)
    target_sources(snim_lib PRIVATE
            core/KeychainStore_win.cpp
            core/SingleInstance.h
            core/SingleInstance_win.cpp)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_sources(snim_lib PRIVATE
            core/KeychainStore_linux.cpp
            core/SecretService.cpp
            core/SecretService.h
            core/Portal.cpp
            core/Portal.h)
else()
    target_sources(snim_lib PRIVATE core/KeychainStore_stub.cpp)
endif()

# ---- media ----
# The FFmpeg layer is Windows only for now; gst/ is compiled into the Linux modules.
if(WIN32)
    add_library(snim_media_ffmpeg STATIC
            media/ffmpeg/FfmpegSupport.cpp
            media/ffmpeg/FfmpegSupport.h
            media/ffmpeg/H264EncoderChain.cpp
            media/ffmpeg/H264EncoderChain.h
            media/ffmpeg/FfmpegEncoder.cpp
            media/ffmpeg/FfmpegEncoder.h
            media/ffmpeg/FfmpegTranscoder.cpp
            media/ffmpeg/FfmpegTranscoder.h
            media/ffmpeg/FfmpegTrimTranscoder.cpp
            media/ffmpeg/FfmpegTrimTranscoder.h)
    target_include_directories(snim_media_ffmpeg PUBLIC
            ${PROJECT_SOURCE_DIR}/src
            ${FFMPEG_INCLUDE_DIRS})
    target_link_directories(snim_media_ffmpeg PUBLIC ${FFMPEG_LIBRARY_DIRS})
    target_link_libraries(snim_media_ffmpeg PUBLIC Qt::Core ${FFMPEG_LIBRARIES})
endif()

# ---- upload ----
target_sources(snim_lib PRIVATE
        upload/ProviderRegistry.cpp
        upload/ProviderRegistry.h
        upload/Uploader.h
        upload/UploaderFactory.cpp
        upload/UploaderFactory.h
        upload/UploadConfig.h
        upload/UploadProfiles.cpp
        upload/UploadProfiles.h
        upload/Util.cpp
        upload/Util.h
        upload/KnownHosts.cpp
        upload/KnownHosts.h
        upload/SocketShim.cpp
        upload/SocketShim.h
        upload/SigV4.cpp
        upload/SigV4.h
        upload/strategies/BlockingUploader.cpp
        upload/strategies/BlockingUploader.h
        upload/strategies/S3Uploader.cpp
        upload/strategies/S3Uploader.h
        upload/strategies/StubUploader.cpp
        upload/strategies/StubUploader.h)

# Without libcurl or libssh2, UploaderFactory returns a Stub that says so.
if(LIBCURL_AVAILABLE)
    target_sources(snim_lib PRIVATE
            upload/strategies/FtpUploader.cpp
            upload/strategies/FtpUploader.h)
endif()
if(LIBSSH2_AVAILABLE)
    target_sources(snim_lib PRIVATE
            upload/strategies/SftpUploader.cpp
            upload/strategies/SftpUploader.h)
endif()

# ---- screen ----
target_sources(snim_lib PRIVATE
        screen/AreaSelector.cpp
        screen/AreaSelector.h
        screen/AreaSelectorInput.cpp
        screen/AreaSelectorInput.h
        screen/OverlayStyle.h
        screen/SelectionLayer.h
        screen/SelectorGroup.cpp
        screen/SelectorGroup.h
        screen/ScreenMap.h
        screen/WinScreenMap.h
        screen/X11ScreenMap.h
        screen/WinWindowFilter.h
        screen/FrozenFrameGrabber.cpp
        screen/FrozenFrameGrabber.h
        screen/WindowEnumerator.h
        screen/OverlayWindows.h
        screen/LayerShellSupport.h
        screen/DesktopStitch.h
        screen/ScreencopyGeometry.h
        screen/sources/DesktopFrameSource.h
        screen/sources/FrameSourceChain.cpp
        screen/sources/FrameSourceChain.h
        screen/sources/FrameSourceFactory.cpp
        screen/sources/FrameSourceFactory.h
        screen/sources/StrategySelection.h
        screen/sources/QtScreensFrameSource.cpp
        screen/sources/QtScreensFrameSource.h)

# Without a native window list, window-pick falls back to the screen under the cursor.
if(APPLE)
    target_sources(snim_lib PRIVATE
            screen/WindowEnumerator_mac.mm
            screen/OverlayWindows_mac.mm)
elseif(WIN32)
    target_sources(snim_lib PRIVATE
            screen/WindowEnumerator_win.cpp
            screen/OverlayWindows_win.cpp)
elseif(XCB_AVAILABLE)
    target_sources(snim_lib PRIVATE
            screen/WindowEnumerator_x11.cpp
            screen/X11WindowFilter.h
            screen/X11Windows.cpp
            screen/X11Windows.h
            screen/OverlayWindows_stub.cpp)
    target_link_libraries(snim_lib PRIVATE PkgConfig::XCB)
else()
    target_sources(snim_lib PRIVATE
            screen/WindowEnumerator_stub.cpp
            screen/OverlayWindows_stub.cpp)
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_sources(snim_lib PRIVATE
            screen/sources/KWinFrameSource.cpp
            screen/sources/KWinFrameSource.h
            screen/sources/PortalFrameSource.cpp
            screen/sources/PortalFrameSource.h)
endif()

# Its own library, so the manual probe can link it without the rest of the app.
if(SCREENCOPY_AVAILABLE)
    set(_snim_protocol_dir "${PROJECT_SOURCE_DIR}/third_party/wayland-protocols")
    set(_snim_protocol_out "${PROJECT_BINARY_DIR}/wayland-protocols")
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
            screen/ScreencopyClient.cpp
            screen/ScreencopyClient.h
            ${_snim_protocol_sources})
    set_target_properties(snim_screencopy PROPERTIES AUTOMOC OFF)
    target_include_directories(snim_screencopy
            PUBLIC ${PROJECT_SOURCE_DIR}/src
            PRIVATE "${_snim_protocol_out}")
    target_link_libraries(snim_screencopy PUBLIC Qt::Gui PRIVATE PkgConfig::WAYLAND_CLIENT)

    target_sources(snim_lib PRIVATE
            screen/sources/ScreencopyFrameSource.cpp
            screen/sources/ScreencopyFrameSource.h)
    target_link_libraries(snim_lib PUBLIC snim_screencopy)
    target_compile_definitions(snim_lib PUBLIC SNIM_HAVE_SCREENCOPY)
endif()

# ScreenCast screenshots; the recorder module grabs the frames.
if(GSTREAMER_AVAILABLE)
    target_sources(snim_lib PRIVATE
            screen/sources/ScreencastFrameSource.cpp
            screen/sources/ScreencastFrameSource.h
            screen/PipeWireFrames.cpp
            screen/PipeWireFrames.h
            screen/ScreenCastPortalSession.cpp
            screen/ScreenCastPortalSession.h)
endif()

# Attaching LayerShellQt::Window per window is enough on Qt 6.5+, no global registration.
if(LAYER_SHELL_AVAILABLE)
    target_link_libraries(snim_lib PUBLIC LayerShellQt::Interface)
    target_compile_definitions(snim_lib PUBLIC SNIM_HAVE_LAYER_SHELL)
endif()

# ---- capture ----
target_sources(snim_lib PRIVATE
        capture/CaptureFactory.cpp
        capture/CaptureFactory.h
        capture/CaptureGeometry.h
        capture/OverlayAnnotations.cpp
        capture/OverlayAnnotations.h
        capture/strategies/CaptureStrategy.cpp
        capture/strategies/CaptureStrategy.h
        capture/strategies/FrameSourceCaptureStrategy.cpp
        capture/strategies/FrameSourceCaptureStrategy.h
        capture/strategies/NativeCaptureStrategy.cpp
        capture/strategies/NativeCaptureStrategy.h)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_sources(snim_lib PRIVATE
            capture/strategies/KWinCaptureStrategy.cpp
            capture/strategies/KWinCaptureStrategy.h)
endif()

# ---- editor ----
target_sources(snim_lib PRIVATE
        editor/EditorChrome.cpp
        editor/EditorChrome.h
        editor/annotations/ToolRegistry.cpp
        editor/annotations/ToolRegistry.h
        editor/annotations/AnnotationBuilder.cpp
        editor/annotations/AnnotationBuilder.h
        editor/annotations/AnnotationSet.cpp
        editor/annotations/AnnotationSet.h
        editor/annotations/IAnnotationSink.h
        editor/annotations/StepNumbering.cpp
        editor/annotations/StepNumbering.h
        editor/annotations/commands/EditorCommands.cpp
        editor/annotations/commands/EditorCommands.h
        editor/annotations/DrawingGraphicsView.cpp
        editor/annotations/DrawingGraphicsView.h
        editor/annotations/tools/ToolItem.cpp
        editor/annotations/tools/ToolItem.h
        editor/annotations/CanvasShadow.cpp
        editor/annotations/CanvasShadow.h
        editor/annotations/tools/ArrowTool.cpp
        editor/annotations/tools/ArrowTool.h
        editor/annotations/tools/ShapeTool.cpp
        editor/annotations/tools/ShapeTool.h
        editor/annotations/tools/HandleItem.cpp
        editor/annotations/tools/HandleItem.h
        editor/annotations/tools/RectangleTool.cpp
        editor/annotations/tools/RectangleTool.h
        editor/annotations/tools/EllipseTool.cpp
        editor/annotations/tools/EllipseTool.h
        editor/annotations/tools/PathTool.cpp
        editor/annotations/tools/PathTool.h
        editor/annotations/tools/FreehandTool.cpp
        editor/annotations/tools/FreehandTool.h
        editor/annotations/tools/HighlightTool.cpp
        editor/annotations/tools/HighlightTool.h
        editor/annotations/tools/BlurTool.cpp
        editor/annotations/tools/BlurTool.h
        editor/annotations/tools/TextTool.cpp
        editor/annotations/tools/TextTool.h
        editor/annotations/tools/StepTool.cpp
        editor/annotations/tools/StepTool.h
        editor/annotations/Layer.cpp
        editor/annotations/Layer.h
        editor/annotations/LayerManager.cpp
        editor/annotations/LayerManager.h
        editor/annotations/LayerProperties.cpp
        editor/annotations/LayerProperties.h
        editor/annotations/interactions/IDrawingInteraction.h
        editor/annotations/interactions/DrawingInteractions.cpp
        editor/annotations/interactions/DrawingInteractions.h
        editor/image/ImageEditor.cpp
        editor/image/ImageEditor.h
        editor/image/BackdropItem.cpp
        editor/image/BackdropItem.h
        editor/image/BackdropMemento.h
        editor/image/BackdropPresets.cpp
        editor/image/BackdropPresets.h
        # The stub exporter hosts the factory and is the fallback for the platform ones below.
        editor/video/TrimState.cpp
        editor/video/TrimState.h
        editor/video/TrimTimeline.cpp
        editor/video/TrimTimeline.h
        editor/video/VideoEditor.cpp
        editor/video/VideoEditor.h
        editor/video/VideoExporter.h
        editor/video/VideoExporter.cpp
        editor/video/AnimationParams.h
        editor/video/AnimationOptionsDialog.cpp
        editor/video/AnimationOptionsDialog.h
        editor/video/Timecode.h
        editor/video/VideoFrameGrabber.h
        editor/video/MediaFrameGrabber.cpp
        editor/video/MediaFrameGrabber.h
        editor/video/StubVideoExporter.cpp
        editor/video/StubVideoExporter.h
        editor/video/AnimationEncoder.cpp
        editor/video/AnimationEncoder.h
        editor/video/GifEncoder.cpp
        editor/video/GifEncoder.h
        editor/video/WebpEncoder.cpp
        editor/video/WebpEncoder.h
        editor/video/AnimationExporter.cpp
        editor/video/AnimationExporter.h)

if(APPLE)
    target_sources(snim_lib PRIVATE
            editor/video/MacVideoExporter.h
            editor/video/MacVideoExporter.mm)
    snim_objc_arc(editor/video/MacVideoExporter.mm)
endif()

# A second dlopened module: it borrows only VideoExporter and fails apart from the recorder.
if(GSTREAMER_AVAILABLE)
    target_sources(snim_lib PRIVATE
            editor/video/LinuxVideoModule.cpp
            editor/video/LinuxVideoModule.h)
    target_compile_definitions(snim_lib PUBLIC SNIM_HAVE_LINUX_VIDEO_EXPORTER)

    add_library(snim_video_linux MODULE
            editor/video/GstVideoExporter.cpp
            editor/video/GstVideoExporter.h
            media/gst/GstSupport.cpp
            media/gst/GstSupport.h)
    set_target_properties(snim_video_linux PROPERTIES
            OUTPUT_NAME "snim-video-linux"
            LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
            INSTALL_RPATH "$ORIGIN")
    target_include_directories(snim_video_linux PRIVATE
            ${PROJECT_SOURCE_DIR}
            ${PROJECT_SOURCE_DIR}/src
            ${GSTREAMER_INCLUDE_DIRS})
    target_link_directories(snim_video_linux PRIVATE ${GSTREAMER_LIBRARY_DIRS})
    target_link_libraries(snim_video_linux PRIVATE
            Qt::Core
            ${GSTREAMER_LIBRARIES})
endif()

if(WIN32)
    target_sources(snim_lib PRIVATE
            editor/video/FfmpegVideoExporter.cpp
            editor/video/FfmpegVideoExporter.h)
    target_link_libraries(snim_lib PUBLIC snim_media_ffmpeg)
    target_compile_definitions(snim_lib PUBLIC SNIM_HAVE_FFMPEG_VIDEO_EXPORTER)
endif()

# ---- ocr ----
target_sources(snim_lib PRIVATE
        ocr/OCRService.cpp
        ocr/OCRService.h
        ocr/OCRResult.h)

# ---- record ----
target_sources(snim_lib PRIVATE
        record/RecordTarget.h
        record/RecordingGeometry.h
        record/PcmMixBuffer.h
        record/PauseAwareClock.h
        record/RecordingStrategy.cpp
        record/RecordingStrategy.h
        record/RecordingFactory.cpp
        record/RecordingFactory.h
        record/RecordingController.cpp
        record/RecordingController.h
        record/RecordingJournal.cpp
        record/RecordingJournal.h
        record/RecordingControls.cpp
        record/RecordingControls.h
        record/RecordingFrameOverlay.cpp
        record/RecordingFrameOverlay.h
        record/RecordingOptionsBar.cpp
        record/RecordingOptionsBar.h
        record/CameraBubble.cpp
        record/CameraBubble.h
        record/strategies/StubRecordingStrategy.cpp
        record/strategies/StubRecordingStrategy.h)

if(APPLE)
    target_sources(snim_lib PRIVATE
            record/strategies/MacRecordingStrategy.h
            record/strategies/MacRecordingStrategy.mm)
    snim_objc_arc(record/strategies/MacRecordingStrategy.mm)
    target_compile_definitions(snim_lib PUBLIC SNIM_HAVE_MAC_RECORDER)
endif()

# GStreamer is dlopened, never linked: a pipewiresrc frozen at build time cannot track the
# host's PipeWire. snim_lib gets the loader, the module everything that touches gst_*.
if(GSTREAMER_AVAILABLE)
    target_sources(snim_lib PRIVATE
            record/strategies/LinuxRecorderModule.cpp
            record/strategies/LinuxRecorderModule.h)
    target_compile_definitions(snim_lib PUBLIC SNIM_HAVE_LINUX_RECORDER)

    add_library(snim_recorder_linux MODULE
            record/strategies/LinuxRecordingStrategy.cpp
            record/strategies/LinuxRecordingStrategy.h
            record/strategies/LinuxPipeline.h
            # Its own copy: a static library hands out an object only if the loader pulled it in.
            screen/ScreenCastPortalSession.cpp
            screen/ScreenCastPortalSession.h
            core/Portal.cpp
            core/Portal.h
            record/strategies/PipeWireFrameGrabber.cpp
            media/gst/GstSupport.cpp
            media/gst/GstSupport.h)
    set_target_properties(snim_recorder_linux PROPERTIES
            OUTPUT_NAME "snim-recorder-linux"
            # Beside the built binary, where the loader looks first.
            LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
            # Beside the bundled Qt; the portable tarball has no AppRun to set LD_LIBRARY_PATH.
            INSTALL_RPATH "$ORIGIN")
    target_include_directories(snim_recorder_linux PRIVATE
            ${PROJECT_SOURCE_DIR}
            ${PROJECT_SOURCE_DIR}/src
            ${GSTREAMER_INCLUDE_DIRS})
    target_link_directories(snim_recorder_linux PRIVATE ${GSTREAMER_LIBRARY_DIRS})
    target_link_libraries(snim_recorder_linux PRIVATE
            Qt::Core
            Qt::Gui
            Qt::DBus
            ${GSTREAMER_LIBRARIES})
    # Window recording reads the picked window through the picker's own queries.
    if(XCB_AVAILABLE)
        target_sources(snim_recorder_linux PRIVATE
                screen/X11WindowFilter.h
                screen/X11Windows.cpp
                screen/X11Windows.h)
        target_link_libraries(snim_recorder_linux PRIVATE PkgConfig::XCB)
        target_compile_definitions(snim_recorder_linux PRIVATE SNIM_HAVE_XCB)
    endif()
endif()

if(WIN32)
    target_sources(snim_lib PRIVATE
            record/strategies/windows/WgcFrameSource.cpp
            record/strategies/windows/WgcFrameSource.h
            record/strategies/windows/WasapiAudioSource.cpp
            record/strategies/windows/WasapiAudioSource.h
            record/strategies/windows/WindowsRecordingStrategy.cpp
            record/strategies/windows/WindowsRecordingStrategy.h)
    target_link_libraries(snim_lib PUBLIC windowsapp d3d11 dxgi)
    target_compile_definitions(snim_lib PUBLIC SNIM_HAVE_WIN_RECORDER)
endif()

# ---- hotkeys ----
# The core and the key mappers use no platform headers, so they build and test anywhere.
target_sources(snim_lib PRIVATE
        hotkeys/HotkeyAction.cpp
        hotkeys/HotkeyAction.h
        hotkeys/HotkeyBindings.cpp
        hotkeys/HotkeyBindings.h
        hotkeys/HotkeyBackend.cpp
        hotkeys/HotkeyBackend.h
        hotkeys/HotkeyBackendFactory.cpp
        hotkeys/HotkeyBackendFactory.h
        hotkeys/GlobalHotkeyManager.cpp
        hotkeys/GlobalHotkeyManager.h
        hotkeys/MacKeyMapping.cpp
        hotkeys/MacKeyMapping.h
        hotkeys/WinKeyMapping.cpp
        hotkeys/WinKeyMapping.h
        hotkeys/PortalKeyMapping.cpp
        hotkeys/PortalKeyMapping.h
        hotkeys/backends/StubHotkeyBackend.h
        hotkeys/ScreenshotKey.cpp
        hotkeys/ScreenshotKey.h
        hotkeys/ScreenshotKeyFactory.cpp
        hotkeys/ScreenshotKeyFactory.h
        hotkeys/ScreenshotKeySwap.cpp
        hotkeys/ScreenshotKeySwap.h
        hotkeys/MacSymbolicHotkeys.cpp
        hotkeys/MacSymbolicHotkeys.h
        hotkeys/screenshotkey/UnsupportedScreenshotKey.h
        hotkeys/screenshotkey/GnomeScreenshotKey.cpp
        hotkeys/screenshotkey/GnomeScreenshotKey.h
        hotkeys/screenshotkey/MacScreenshotKey.cpp
        hotkeys/screenshotkey/MacScreenshotKey.h)

if(APPLE)
    target_sources(snim_lib PRIVATE
            # Listed so AUTOMOC sees its Q_OBJECT, like the other .mm-backed headers.
            hotkeys/backends/MacHotkeyBackend.h
            hotkeys/backends/MacHotkeyBackend.mm
            hotkeys/screenshotkey/MacSymbolicPrefs.mm)
    snim_objc_arc(hotkeys/screenshotkey/MacSymbolicPrefs.mm)
    target_compile_definitions(snim_lib PUBLIC SNIM_HAVE_MAC_HOTKEYS)
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_sources(snim_lib PRIVATE
            hotkeys/backends/PortalHotkeyBackend.cpp
            hotkeys/backends/PortalHotkeyBackend.h
            hotkeys/screenshotkey/KdeScreenshotKey.cpp
            hotkeys/screenshotkey/KdeScreenshotKey.h)
endif()

if(WIN32)
    target_sources(snim_lib PRIVATE
            hotkeys/backends/WindowsHotkeyBackend.cpp
            hotkeys/backends/WindowsHotkeyBackend.h
            hotkeys/WinPrintScreen.cpp
            hotkeys/WinPrintScreen.h
            hotkeys/screenshotkey/WindowsScreenshotKey.cpp
            hotkeys/screenshotkey/WindowsScreenshotKey.h)
    target_compile_definitions(snim_lib PUBLIC SNIM_HAVE_WIN_HOTKEYS)
endif()
