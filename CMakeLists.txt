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

add_executable(Niceshot
        main.cpp

        # Core application files
        src/core/ScreenshotApp.cpp
        src/core/ScreenshotApp.h
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
        src/capture/strategies/CaptureStrategy.cpp
        src/capture/strategies/CaptureStrategy.h
        src/capture/strategies/NativeCaptureStrategy.cpp
        src/capture/strategies/NativeCaptureStrategy.h
        src/capture/strategies/WaylandCaptureStrategy.cpp
        src/capture/strategies/WaylandCaptureStrategy.h
        src/capture/strategies/KWinCaptureStrategy.cpp
        src/capture/strategies/KWinCaptureStrategy.h

        # Image editor functionality
        src/editor/ImageEditor.cpp
        src/editor/ImageEditor.h
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

        # OCR functionality
        src/ocr/OCRService.cpp
        src/ocr/OCRService.h
        src/ocr/OCRResult.h

        # Resources
        resources/resources.qrc
)

# Set include directories for the organized structure
target_include_directories(Niceshot PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(Niceshot
        Qt::Core
        Qt::Gui
        Qt::Widgets
        Qt::DBus
        Qt::Concurrent
)

# Link Tesseract if available
if(TESSERACT_AVAILABLE)
    target_include_directories(Niceshot PRIVATE ${TESSERACT_INCLUDE_DIRS})
    target_link_libraries(Niceshot ${TESSERACT_LIBRARIES})
endif()
