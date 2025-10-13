cmake_minimum_required(VERSION 3.31)
project(Niceshort2)

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

add_executable(Niceshort2
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
    src/capture/CaptureStrategy.cpp
    src/capture/CaptureStrategy.h
    src/capture/CaptureFactory.cpp
    src/capture/CaptureFactory.h
    src/capture/NativeCaptureStrategy.cpp
    src/capture/NativeCaptureStrategy.h
    src/capture/WaylandCaptureStrategy.cpp
    src/capture/WaylandCaptureStrategy.h

    # Image editor functionality
    src/editor/ImageEditor.cpp
    src/editor/ImageEditor.h
    src/editor/DrawingGraphicsView.cpp
    src/editor/DrawingGraphicsView.h
        src/editor/tools/ArrowTool.cpp
        src/editor/tools/ArrowTool.h
    src/editor/Layer.cpp
    src/editor/Layer.h
    src/editor/LayerManager.cpp
    src/editor/LayerManager.h
    src/editor/LayerProperties.cpp
    src/editor/LayerProperties.h
        src/editor/tools/TextTool.cpp
        src/editor/tools/TextTool.h
)

# Set include directories for the organized structure
target_include_directories(Niceshort2 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(Niceshort2
        Qt::Core
        Qt::Gui
        Qt::Widgets
        Qt::DBus
)
