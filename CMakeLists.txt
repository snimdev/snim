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
    add_compile_definitions(HAVE_TESSERACT)
else()
    message(STATUS "Tesseract OCR not found - text snipping feature will be disabled")
    message(STATUS "  Install with: sudo dnf install tesseract-devel")
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
            src/capture/MacOverlay.mm)
    target_link_libraries(niceshot_lib PUBLIC
            "-framework CoreGraphics"
            "-framework CoreFoundation"
            "-framework AppKit")
else()
    target_sources(niceshot_lib PRIVATE src/capture/WindowEnumerator_stub.cpp)
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
    )
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
)

# Link Tesseract if available (PUBLIC link: OCRService.o references its symbols and
# HAVE_TESSERACT is a global define, so consumers — exe and tests — must link it).
if(TESSERACT_AVAILABLE)
    target_include_directories(niceshot_lib PRIVATE ${TESSERACT_INCLUDE_DIRS})
    # pkg-config reports library names (-ltesseract ...) without their location;
    # add the reported library dirs so the linker can find them (needed on
    # macOS/Homebrew where they live under a non-default prefix). Empty for the
    # manual-detection fallback, which already uses absolute library paths.
    target_link_directories(niceshot_lib PUBLIC ${TESSERACT_LIBRARY_DIRS})
    target_link_libraries(niceshot_lib PUBLIC ${TESSERACT_LIBRARIES})
endif()

# ---------------------------------------------------------------------------
# macOS application bundle + deployment
# ---------------------------------------------------------------------------
# Building as a .app bundle (rather than a bare executable) gives the app a
# stable bundle identifier, which is what lets macOS remember the Screen
# Recording permission across launches/rebuilds. The `deploy` target then runs
# macdeployqt to copy the Qt frameworks (and other linked dylibs) into the
# bundle and ad-hoc code-signs it, producing a self-contained, distributable
# Niceshot.app.
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
