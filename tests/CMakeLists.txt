find_package(Qt6 COMPONENTS Test REQUIRED)

# Each entry is a single-file Qt Test (QTEST_MAIN). They link the app's static
# library + Qt6::Test and run headless via the offscreen platform (set per-test
# below; CI also exports QT_QPA_PLATFORM=offscreen as a belt-and-suspenders).
# The list grows tier by tier as tests are added.
set(SNIM_TESTS
        tst_settings
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
        tst_overlayannotations
        tst_annotationhandoff
        tst_capturefactory
        tst_capturegeometry
        tst_ocrservice
        tst_integration_scene
        tst_recordingfactory
        tst_recordtarget
        tst_recordinggeometry
        tst_recordingcontroller
        tst_recordingoptionsbar
        tst_pcmmixbuffer
        tst_trimstate
        tst_animationparams
        tst_webpencoder
        tst_webpexporter
        tst_videoframegrabber
        tst_sigv4
        tst_uploaderfactory
        tst_uploadprofiles
        tst_uploadconfig
        tst_uploadutil
        tst_hotkeyaction
        tst_hotkeybindings
        tst_hotkeymapping
        tst_hotkeymanager
        tst_desktopintegration
        tst_screenshotapp
        tst_updatecheck
        tst_appscope
        tst_bundledpaths
)

# The dlopened Linux recorder module only exists where it is built.
if(GSTREAMER_AVAILABLE)
    list(APPEND SNIM_TESTS tst_linuxrecordermodule)
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
