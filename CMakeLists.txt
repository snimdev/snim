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
        REQUIRED)

add_executable(Niceshot
        main.cpp

        # Core application files
        src/core/ScreenshotApp.cpp
        src/core/ScreenshotApp.h
        src/core/ScreenshotDialog.cpp
        src/core/ScreenshotDialog.h
        src/core/SettingsDialog.cpp
        src/core/SettingsDialog.h

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
)
