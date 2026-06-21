find_package(Qt6 COMPONENTS Test REQUIRED)

# Each entry is a single-file Qt Test (QTEST_MAIN). They link the app's static
# library + Qt6::Test and run headless via the offscreen platform (set per-test
# below; CI also exports QT_QPA_PLATFORM=offscreen as a belt-and-suspenders).
# The list grows tier by tier as tests are added.
set(SNIM_TESTS
        tst_settings
        tst_filenames
        tst_layer
        tst_toolregistry
        tst_tools
        tst_annotationbuilder
        tst_annotationset
        tst_stepnumbering
        tst_editorcommands
        tst_backdroppresets
        tst_backdrop_memento
        tst_layermanager
        tst_areaselector
        tst_selectionlayer
        tst_overlayannotations
        tst_annotationhandoff
        tst_capturefactory
        tst_capturegeometry
        tst_screencaststitch
        tst_screencopygeometry
        tst_winscreenmap
        tst_winwindowfilter
        tst_mixeddpicapture
        tst_frozenframegrabber
        tst_ocrservice
        tst_integration_scene
        tst_recordingfactory
        tst_recordtarget
        tst_recordinggeometry
        tst_recordingcontroller
        tst_recordingjournal
        tst_recordingworkflow
        tst_recordingoptionsbar
        tst_pcmmixbuffer
        tst_pauseawareclock
        tst_streamtimestamp
        tst_trimstate
        tst_timecode
        tst_animationparams
        tst_animationoptionsdialog
        tst_webpencoder
        tst_gifencoder
        tst_animationexporter
        tst_videoframegrabber
        tst_videoexportercontract
        tst_sigv4
        tst_uploaderfactory
        tst_uploadprofiles
        tst_uploadconfig
        tst_uploadworkflow
        tst_uploadutil
        tst_socketshim
        tst_hotkeyaction
        tst_hotkeybindings
        tst_hotkeymapping
        tst_hotkeymanager
        tst_desktopintegration
        tst_screenshotapp
        tst_updatecheck
        tst_appscope
        tst_sandbox
        tst_bundledpaths
)

# The dlopened Linux modules only exist where they are built.
if(GSTREAMER_AVAILABLE)
    list(APPEND SNIM_TESTS tst_linuxrecordermodule tst_gstvideoexporter tst_recordingrecovery
            tst_screencasttoken tst_gstsupport)
endif()

# Round-trips the real Credential Manager, so it only exists on Windows.
if(WIN32)
    list(APPEND SNIM_TESTS tst_keychainstore)
endif()

# The FFmpeg media layer is built on Windows only.
if(WIN32)
    list(APPEND SNIM_TESTS tst_h264encoderchain tst_ffmpegencoder tst_windowsrecorder)
endif()

foreach(test_name ${SNIM_TESTS})
    add_executable(${test_name} ${test_name}.cpp)
    target_link_libraries(${test_name} PRIVATE snim_lib Qt6::Test)
    add_test(NAME ${test_name} COMMAND ${test_name})
    set_tests_properties(${test_name} PROPERTIES
            ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endforeach()

# The Linux recorder is dlopened, so the tests that reach it need the module built, a
# path to it in the build tree, and (like the app) their own symbols exported: that is
# what the module's RecordingStrategy references bind to at dlopen time.
if(GSTREAMER_AVAILABLE)
    add_dependencies(tst_linuxrecordermodule snim_recorder_linux)
    target_compile_definitions(tst_linuxrecordermodule PRIVATE
            SNIM_RECORDER_MODULE_PATH="$<TARGET_FILE:snim_recorder_linux>")
    set_target_properties(tst_linuxrecordermodule PROPERTIES ENABLE_EXPORTS ON)

    # Keeps tst_recordingfactory reaching the real backend, as it did when the strategy
    # was linked in: without the override it would only ever see the stub.
    add_dependencies(tst_recordingfactory snim_recorder_linux)
    set_target_properties(tst_recordingfactory PROPERTIES ENABLE_EXPORTS ON)
    set_tests_properties(tst_recordingfactory PROPERTIES ENVIRONMENT
            "QT_QPA_PLATFORM=offscreen;SNIM_RECORDER_MODULE=$<TARGET_FILE:snim_recorder_linux>")

    # Same seam for the trim exporter module; the test also drives GStreamer itself to
    # make a fixture with audio.
    add_dependencies(tst_gstvideoexporter snim_video_linux)
    target_compile_definitions(tst_gstvideoexporter PRIVATE
            SNIM_VIDEO_MODULE_PATH="$<TARGET_FILE:snim_video_linux>")
    set_target_properties(tst_gstvideoexporter PROPERTIES ENABLE_EXPORTS ON)
    target_include_directories(tst_gstvideoexporter PRIVATE ${GSTREAMER_INCLUDE_DIRS})
    if(GSTREAMER_LIBRARY_DIRS)
        target_link_directories(tst_gstvideoexporter PRIVATE ${GSTREAMER_LIBRARY_DIRS})
    endif()
    target_link_libraries(tst_gstvideoexporter PRIVATE ${GSTREAMER_LIBRARIES})
    set_tests_properties(tst_gstvideoexporter PROPERTIES ENVIRONMENT
            "QT_QPA_PLATFORM=offscreen;SNIM_VIDEO_MODULE=$<TARGET_FILE:snim_video_linux>")

    # Kills a recording child mid-write with the recorder's own muxer setup, so it
    # compiles GstSupport in; the trim module then reads back what survived.
    target_sources(tst_recordingrecovery PRIVATE ${CMAKE_SOURCE_DIR}/src/media/gst/GstSupport.cpp)
    add_dependencies(tst_recordingrecovery snim_video_linux)
    target_compile_definitions(tst_recordingrecovery PRIVATE
            SNIM_VIDEO_MODULE_PATH="$<TARGET_FILE:snim_video_linux>")
    set_target_properties(tst_recordingrecovery PROPERTIES ENABLE_EXPORTS ON)
    target_include_directories(tst_recordingrecovery PRIVATE ${GSTREAMER_INCLUDE_DIRS})
    if(GSTREAMER_LIBRARY_DIRS)
        target_link_directories(tst_recordingrecovery PRIVATE ${GSTREAMER_LIBRARY_DIRS})
    endif()
    target_link_libraries(tst_recordingrecovery PRIVATE ${GSTREAMER_LIBRARIES})
    set_tests_properties(tst_recordingrecovery PROPERTIES ENVIRONMENT
            "QT_QPA_PLATFORM=offscreen;SNIM_VIDEO_MODULE=$<TARGET_FILE:snim_video_linux>")

    # The encoder and parser choice, plus real encodes through each chain.
    target_sources(tst_gstsupport PRIVATE ${CMAKE_SOURCE_DIR}/src/media/gst/GstSupport.cpp)
    target_include_directories(tst_gstsupport PRIVATE ${GSTREAMER_INCLUDE_DIRS})
    if(GSTREAMER_LIBRARY_DIRS)
        target_link_directories(tst_gstsupport PRIVATE ${GSTREAMER_LIBRARY_DIRS})
    endif()
    target_link_libraries(tst_gstsupport PRIVATE ${GSTREAMER_LIBRARIES})

    # The exporter contract runs against the same module.
    add_dependencies(tst_videoexportercontract snim_video_linux)
    target_compile_definitions(tst_videoexportercontract PRIVATE
            SNIM_VIDEO_MODULE_PATH="$<TARGET_FILE:snim_video_linux>")
    set_target_properties(tst_videoexportercontract PROPERTIES ENABLE_EXPORTS ON)
    set_tests_properties(tst_videoexportercontract PROPERTIES ENVIRONMENT
            "QT_QPA_PLATFORM=offscreen;SNIM_VIDEO_MODULE=$<TARGET_FILE:snim_video_linux>")
endif()

if(WIN32)
    target_link_libraries(tst_h264encoderchain PRIVATE snim_media_ffmpeg)
    target_link_libraries(tst_ffmpegencoder PRIVATE snim_media_ffmpeg)
    # Graphics Capture needs real screens; the test skips where there is no desktop.
    set_tests_properties(tst_windowsrecorder PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=windows")
endif()

# FFmpeg must stay static in snim.exe, or it would clash with Qt Multimedia's DLLs.
if(WIN32)
    get_filename_component(_snim_linker_dir "${CMAKE_LINKER}" DIRECTORY)
    find_program(SNIM_DUMPBIN dumpbin HINTS "${_snim_linker_dir}" REQUIRED)
    add_test(NAME snim_no_shared_ffmpeg
            COMMAND "${CMAKE_COMMAND}" -DDUMPBIN=${SNIM_DUMPBIN} -DEXE=$<TARGET_FILE:Snim>
                    -P "${CMAKE_SOURCE_DIR}/cmake/CheckNoSharedFfmpeg.cmake")
endif()

# install() copies the app icon out of the qrc, so this one needs the resources too.
target_sources(tst_desktopintegration PRIVATE ${CMAKE_SOURCE_DIR}/resources/resources.qrc)

# The options bar themes :/icons/icons/*.svg at construction, so it needs the qrc too.
target_sources(tst_recordingoptionsbar PRIVATE ${CMAKE_SOURCE_DIR}/resources/resources.qrc)

# The handoff test opens an ImageEditor, which loads its qss and icons from the qrc.
target_sources(tst_annotationhandoff PRIVATE ${CMAKE_SOURCE_DIR}/resources/resources.qrc)

# The ImageEditor smoke test also needs the qrc (it loads :/styles + themed
# :/icons at construction), so it gets resources.qrc compiled into it directly.
add_executable(tst_imageeditor_smoke tst_imageeditor_smoke.cpp
        ${CMAKE_SOURCE_DIR}/resources/resources.qrc)
target_link_libraries(tst_imageeditor_smoke PRIVATE snim_lib Qt6::Test)
add_test(NAME tst_imageeditor_smoke COMMAND tst_imageeditor_smoke)
set_tests_properties(tst_imageeditor_smoke PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

# Same deal for the VideoEditor smoke test (themed toolbar icons at construction).
add_executable(tst_videoeditor_smoke tst_videoeditor_smoke.cpp
        ${CMAKE_SOURCE_DIR}/resources/resources.qrc)
target_link_libraries(tst_videoeditor_smoke PRIVATE snim_lib Qt6::Test)
add_test(NAME tst_videoeditor_smoke COMMAND tst_videoeditor_smoke)
set_tests_properties(tst_videoeditor_smoke PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

# Convenience aggregate: build every test, then run the whole suite.
#   CLI:    cmake --build <build-dir> --target check
#   CLion:  pick the "check" target in the configuration switcher and Build/Run it.
# (`test` is reserved by CTest and runs ctest without building the tests first, so
# this `check` target depends on the test executables and then runs ctest.)
add_custom_target(check
        COMMAND "${CMAKE_CTEST_COMMAND}" --output-on-failure
        WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
        USES_TERMINAL
        COMMENT "Running all Snim tests")
add_dependencies(check ${SNIM_TESTS} tst_imageeditor_smoke tst_videoeditor_smoke)

# Manual ScreenCast capture and recording checks against the real portal; never run by ctest.
if(GSTREAMER_AVAILABLE)
    add_executable(snim_screencast_probe manual/screencast_probe.cpp)
    target_link_libraries(snim_screencast_probe PRIVATE snim_lib)
    add_dependencies(snim_screencast_probe snim_recorder_linux)
    target_compile_definitions(snim_screencast_probe PRIVATE
            SNIM_RECORDER_MODULE_PATH="$<TARGET_FILE:snim_recorder_linux>")
    set_target_properties(snim_screencast_probe PROPERTIES ENABLE_EXPORTS ON)

    add_executable(snim_recording_probe manual/recording_probe.cpp)
    target_link_libraries(snim_recording_probe PRIVATE snim_lib)
    add_dependencies(snim_recording_probe snim_recorder_linux)
    target_compile_definitions(snim_recording_probe PRIVATE
            SNIM_RECORDER_MODULE_PATH="$<TARGET_FILE:snim_recorder_linux>")
    set_target_properties(snim_recording_probe PROPERTIES ENABLE_EXPORTS ON)
endif()

# Manual frozen frame check for the recording selector on a live Linux desktop.
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    add_executable(snim_frozenframe_probe manual/frozenframe_probe.cpp)
    target_link_libraries(snim_frozenframe_probe PRIVATE snim_lib)
    set_target_properties(snim_frozenframe_probe PROPERTIES ENABLE_EXPORTS ON)
    if(GSTREAMER_AVAILABLE)
        add_dependencies(snim_frozenframe_probe snim_recorder_linux)
        target_compile_definitions(snim_frozenframe_probe PRIVATE
                SNIM_RECORDER_MODULE_PATH="$<TARGET_FILE:snim_recorder_linux>")
    endif()
endif()

# Manual probe for a live wlroots-family compositor; built, never registered with ctest.
if(SCREENCOPY_AVAILABLE)
    add_executable(snim_screencopy_probe manual/screencopy_probe.cpp)
    target_link_libraries(snim_screencopy_probe PRIVATE snim_screencopy)
endif()
