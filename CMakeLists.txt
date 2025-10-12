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
        REQUIRED)

add_executable(Niceshort2
    main.cpp
    screenshotapp.cpp
    screenshotapp.h
    DrawingGraphicsView.cpp
    DrawingGraphicsView.h
    EditableTextItem.cpp
    EditableTextItem.h
    ArrowItem.cpp
    ArrowItem.h
    Layer.cpp
    Layer.h
    LayerManager.cpp
    LayerManager.h
    LayerProperties.cpp
    LayerProperties.h
    AreaSelector.cpp
    AreaSelector.h
    ScreenshotDialog.cpp
    ScreenshotDialog.h
    ImageEditor.cpp
    ImageEditor.h
)

target_link_libraries(Niceshort2
        Qt::Core
        Qt::Gui
        Qt::Widgets
)
