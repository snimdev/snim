find_package(Qt6 COMPONENTS Test REQUIRED)

# Each entry is a single-file Qt Test (QTEST_MAIN). They link the app's static
# library + Qt6::Test and run headless via the offscreen platform (set per-test
# below; CI also exports QT_QPA_PLATFORM=offscreen as a belt-and-suspenders).
# The list grows tier by tier as tests are added.
set(SNIM_TESTS
        tst_settings
        tst_filenames
        tst_layer
        tst_canvasshadow
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
        tst_x11screenmap
        tst_winwindowfilter
        tst_x11windowfilter
        tst_mixeddpicapture
        tst_frozenframegrabber
        tst_framesourcechain
        tst_ocrservice
        tst_integration_scene
        tst_recordtarget
        tst_recordinggeometry
        tst_linuxpipeline
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
        tst_imageeditor_smoke
        tst_videoeditor_smoke
)

# The dlopened Linux modules only exist where they are built.
if(GSTREAMER_AVAILABLE)
    list(APPEND SNIM_TESTS tst_linuxrecordermodule tst_gstvideoexporter tst_recordingrecovery
            tst_screencasttoken tst_screencastrequest tst_gstsupport)
endif()

# Windows only: the real Credential Manager round trip and the FFmpeg media layer.
if(WIN32)
    list(APPEND SNIM_TESTS tst_keychainstore tst_h264encoderchain tst_ffmpegencoder
            tst_windowsrecorder)
endif()

# The Secret Service client behind the Linux keychain.
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    list(APPEND SNIM_TESTS tst_secretservice)
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

    # Same seam for the trim exporter module.
    foreach(test_name tst_gstvideoexporter tst_recordingrecovery tst_videoexportercontract)
        add_dependencies(${test_name} snim_video_linux)
        target_compile_definitions(${test_name} PRIVATE
                SNIM_VIDEO_MODULE_PATH="$<TARGET_FILE:snim_video_linux>")
        set_target_properties(${test_name} PROPERTIES ENABLE_EXPORTS ON)
        set_tests_properties(${test_name} PROPERTIES ENVIRONMENT
                "QT_QPA_PLATFORM=offscreen;SNIM_VIDEO_MODULE=$<TARGET_FILE:snim_video_linux>")
    endforeach()

    # These drive GStreamer themselves: an audio fixture, a recording child killed mid-write
    # with the recorder's own muxer setup, and real encodes through each chain.
    foreach(test_name tst_gstvideoexporter tst_recordingrecovery tst_gstsupport)
        target_include_directories(${test_name} PRIVATE ${GSTREAMER_INCLUDE_DIRS})
        target_link_directories(${test_name} PRIVATE ${GSTREAMER_LIBRARY_DIRS})
        target_link_libraries(${test_name} PRIVATE ${GSTREAMER_LIBRARIES})
    endforeach()
    target_sources(tst_recordingrecovery PRIVATE ${CMAKE_SOURCE_DIR}/src/media/gst/GstSupport.cpp)
    target_sources(tst_gstsupport PRIVATE ${CMAKE_SOURCE_DIR}/src/media/gst/GstSupport.cpp)
endif()

if(WIN32)
    target_link_libraries(tst_h264encoderchain PRIVATE snim_media_ffmpeg)
    target_link_libraries(tst_ffmpegencoder PRIVATE snim_media_ffmpeg)
    # Graphics Capture needs real screens; the test skips where there is no desktop.
    set_tests_properties(tst_windowsrecorder PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=windows")

    # FFmpeg must stay static in snim.exe, or it would clash with Qt Multimedia's DLLs.
    get_filename_component(_snim_linker_dir "${CMAKE_LINKER}" DIRECTORY)
    find_program(SNIM_DUMPBIN dumpbin HINTS "${_snim_linker_dir}" REQUIRED)
    add_test(NAME snim_no_shared_ffmpeg
            COMMAND "${CMAKE_COMMAND}" -DDUMPBIN=${SNIM_DUMPBIN} -DEXE=$<TARGET_FILE:Snim>
                    -P "${CMAKE_SOURCE_DIR}/cmake/CheckNoSharedFfmpeg.cmake")
endif()

# These load :/ icons, styles or the app icon at construction, so the qrc is compiled in.
foreach(test_name tst_desktopintegration tst_recordingoptionsbar tst_annotationhandoff
        tst_imageeditor_smoke tst_videoeditor_smoke)
    target_sources(${test_name} PRIVATE ${CMAKE_SOURCE_DIR}/resources/resources.qrc)
endforeach()

# These reach the keychain through the upload code: a dead session bus keeps them off a
# developer's real keyring (CI has none either), so the keychain just reports no service.
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set_property(TEST tst_uploadconfig tst_uploadprofiles tst_uploaderfactory tst_uploadworkflow
            tst_recordingworkflow APPEND PROPERTY ENVIRONMENT
            "DBUS_SESSION_BUS_ADDRESS=unix:path=${CMAKE_CURRENT_BINARY_DIR}/no-session-bus")
endif()

# The Linux keychain against a fake and a real Secret Service, on a private session bus
# where nothing is activatable, so it can never reach the desktop's own keyring.
set(SNIM_BUS_TESTS)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    find_program(SNIM_DBUS_RUN_SESSION dbus-run-session)
endif()
if(SNIM_DBUS_RUN_SESSION)
    add_executable(tst_keychainstore_linux tst_keychainstore_linux.cpp)
    target_link_libraries(tst_keychainstore_linux PRIVATE snim_lib Qt6::Test)
    add_test(NAME tst_keychainstore_linux
            COMMAND ${SNIM_DBUS_RUN_SESSION}
                    --config-file=${CMAKE_CURRENT_SOURCE_DIR}/data/private-session-bus.conf
                    -- $<TARGET_FILE:tst_keychainstore_linux>)
    set_tests_properties(tst_keychainstore_linux PROPERTIES
            ENVIRONMENT "QT_QPA_PLATFORM=offscreen;SNIM_TEST_PRIVATE_BUS=1")
    list(APPEND SNIM_BUS_TESTS tst_keychainstore_linux)
endif()

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
add_dependencies(check ${SNIM_TESTS} ${SNIM_BUS_TESTS})

# Manual ScreenCast capture and recording checks against the real portal; never run by ctest.
if(GSTREAMER_AVAILABLE)
    foreach(probe screencast recording)
        add_executable(snim_${probe}_probe manual/${probe}_probe.cpp)
        target_link_libraries(snim_${probe}_probe PRIVATE snim_lib)
        add_dependencies(snim_${probe}_probe snim_recorder_linux)
        target_compile_definitions(snim_${probe}_probe PRIVATE
                SNIM_RECORDER_MODULE_PATH="$<TARGET_FILE:snim_recorder_linux>")
        set_target_properties(snim_${probe}_probe PROPERTIES ENABLE_EXPORTS ON)
    endforeach()
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
