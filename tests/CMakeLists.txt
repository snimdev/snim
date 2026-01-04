find_package(Qt6 COMPONENTS Test REQUIRED)

# Each entry is a single-file Qt Test (QTEST_MAIN). They link the app's static
# library + Qt6::Test and run headless via the offscreen platform (set per-test
# below; CI also exports QT_QPA_PLATFORM=offscreen as a belt-and-suspenders).
# The list grows tier by tier as tests are added.
set(NICESHOT_TESTS
        tst_settings
        tst_layer
        tst_toolregistry
        tst_tools
        tst_editorcommands
        tst_backdroppresets
        tst_backdrop_memento
        tst_layermanager
        tst_capturefactory
        tst_ocrservice
        tst_integration_scene
        tst_recordingfactory
        tst_recordtarget
        tst_recordingcontroller
        tst_pcmmixbuffer
        tst_trimstate
        tst_gifparams
        tst_sigv4
        tst_uploaderfactory
        tst_uploadprofiles
        tst_uploadconfig
        tst_uploadutil
)

foreach(test_name ${NICESHOT_TESTS})
    add_executable(${test_name} ${test_name}.cpp)
    target_link_libraries(${test_name} PRIVATE niceshot_lib Qt6::Test)
    add_test(NAME ${test_name} COMMAND ${test_name})
    set_tests_properties(${test_name} PROPERTIES
            ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endforeach()

# The ImageEditor smoke test also needs the qrc (it loads :/styles + themed
# :/icons at construction), so it gets resources.qrc compiled into it directly.
add_executable(tst_imageeditor_smoke tst_imageeditor_smoke.cpp
        ${CMAKE_SOURCE_DIR}/resources/resources.qrc)
target_link_libraries(tst_imageeditor_smoke PRIVATE niceshot_lib Qt6::Test)
add_test(NAME tst_imageeditor_smoke COMMAND tst_imageeditor_smoke)
set_tests_properties(tst_imageeditor_smoke PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

# Same deal for the VideoEditor smoke test (themed toolbar icons at construction).
add_executable(tst_videoeditor_smoke tst_videoeditor_smoke.cpp
        ${CMAKE_SOURCE_DIR}/resources/resources.qrc)
target_link_libraries(tst_videoeditor_smoke PRIVATE niceshot_lib Qt6::Test)
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
        COMMENT "Running all Niceshot tests")
add_dependencies(check ${NICESHOT_TESTS} tst_imageeditor_smoke tst_videoeditor_smoke)
