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
        Core Gui Widgets Concurrent Svg Multimedia MultimediaWidgets Network REQUIRED)
# Only the Linux portal and KWin code talks D-Bus; Windows has no session bus.
if(NOT WIN32)
    find_package(Qt6 COMPONENTS DBus REQUIRED)
endif()

include(cmake/SnimDependencies.cmake)

# snim_lib holds every cross-platform source, so the executable and the tests share it.
if(APPLE)
    enable_language(OBJCXX)
endif()
add_library(snim_lib STATIC)

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/cmake/Version.h.in"
        "${CMAKE_BINARY_DIR}/generated/core/Version.h" @ONLY)
target_include_directories(snim_lib PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_BINARY_DIR}/generated)

# Source properties are directory-scoped, so the modules set ARC in snim_lib's directory.
function(snim_objc_arc)
    set_source_files_properties(${ARGN} TARGET_DIRECTORY snim_lib
            PROPERTIES COMPILE_OPTIONS "-fobjc-arc")
endfunction()

# Every module's sources, platform files and helper targets. The SNIM_HAVE_* defines they set
# are PUBLIC because the tests check them.
add_subdirectory(src)

# Everything below links after the modules' own platform libraries.
if(APPLE)
    target_link_libraries(snim_lib PUBLIC
            "-framework CoreGraphics"
            "-framework CoreFoundation"
            "-framework AppKit"
            "-framework ScreenCaptureKit"
            "-framework AVFoundation"
            "-framework CoreMedia"
            "-framework CoreVideo"
            "-framework Security"
            "-framework Carbon")
endif()
# Svg and Multimedia are linked explicitly so macdeployqt bundles their plugins too.
target_link_libraries(snim_lib PUBLIC
        Qt::Core Qt::Gui Qt::Widgets Qt::Concurrent
        Qt::Svg Qt::Multimedia Qt::MultimediaWidgets Qt::Network)
if(NOT WIN32)
    target_link_libraries(snim_lib PUBLIC Qt::DBus)
endif()
# Winsock behind upload/SocketShim; DWM for window bounds and cloaking.
if(WIN32)
    target_link_libraries(snim_lib PUBLIC ws2_32 dwmapi)
endif()

# PUBLIC: HAVE_* are global defines, so the executable and the tests need the libraries too.
foreach(_snim_dep TESSERACT LIBCURL LIBSSH2)
    if(${_snim_dep}_AVAILABLE)
        snim_link_dependency(snim_lib ${_snim_dep})
    endif()
endforeach()
# libwebp's pkg-config reports <prefix>/include/webp; the parents let <webp/encode.h> resolve.
snim_with_parent_dirs(_webp_inc ${LIBWEBP_INCLUDE_DIRS})
target_include_directories(snim_lib PRIVATE ${_webp_inc})
target_link_directories(snim_lib PUBLIC ${LIBWEBP_LIBRARY_DIRS})
target_link_libraries(snim_lib PUBLIC ${LIBWEBPMUX_LIBRARIES} ${LIBWEBP_LIBRARIES} GIF::GIF)
snim_link_dependency(snim_lib IMAGEQUANT)

# The qrc stays on the executable: in the static lib AUTORCC's initializer would be stripped.
add_executable(Snim src/main.cpp resources/resources.qrc)
target_link_libraries(Snim PRIVATE snim_lib)

# Lowercase binary everywhere but macOS, where the bundle must stay Snim.app.
if(NOT APPLE)
    set_target_properties(Snim PROPERTIES OUTPUT_NAME "snim")
endif()

# GUI subsystem (no console) with icon, version resource and manifest; tests stay console apps.
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

if(APPLE)
    # Qt's macOS permission backends are static plugins: without them QCamera finds none.
    qt_import_plugins(Snim INCLUDE
            Qt6::QDarwinCameraPermissionPlugin
            Qt6::QDarwinMicrophonePermissionPlugin)
    # Qt links requestPermission() in only for targets with these, which plain add_executable
    # never sets; without them every request is denied with no TCC prompt.
    set_target_properties(Snim PROPERTIES
            _qt_has_QDarwinCameraPermissionPlugin_usage_description TRUE
            _qt_has_QDarwinMicrophonePermissionPlugin_usage_description TRUE)
endif()

# The dlopened modules bind RecordingStrategy and VideoExporter to the executable's exports.
if(GSTREAMER_AVAILABLE)
    set_target_properties(Snim PROPERTIES ENABLE_EXPORTS ON)
    add_dependencies(Snim snim_recorder_linux snim_video_linux)
endif()

if(APPLE)
    include(cmake/PackagingMacOS.cmake)
endif()
if(UNIX AND NOT APPLE)
    include(cmake/PackagingLinux.cmake)
endif()
if(WIN32)
    include(cmake/PackagingWindows.cmake)
endif()

# Run with: cmake --build <build> --target check
option(SNIM_BUILD_TESTS "Build the Qt Test suite" ON)
if(SNIM_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

feature_summary(WHAT ALL)
