# cmake -DSRC=<repo>/src -P CheckLayers.cmake
# Fails when core/, media/ or upload/ include a Qt GUI or Widgets header: those layers
# sit below the UI. Only the files listed here may, for the reason given.
# Script mode starts with no policies: IN_LIST needs CMP0057, which CMake 3.x leaves OLD.
cmake_minimum_required(VERSION 3.31)
set(allowed
        core/IconUtil.cpp core/IconUtil.h   # tray and menu icons
        core/MacTrayWorkaround.mm           # the macOS tray menu
        core/SelfTest.cpp                   # probes the image plugins and the style
        core/Settings.h                     # the editor's stroke and fill colors
        upload/UploadMenu.h)                # the editors' upload destination menu
# Module includes, widget-like names, events, then the plain GUI classes.
set(gui "Qt(Gui|Widgets|Svg|MultimediaWidgets)[/>]"
        "Q[A-Za-z]*(Widget|Dialog|Layout|Button|Edit|Box)>"
        "Q(Graphics|List|Tree|Table|Column|Header|Undo|AbstractItem)View>"
        "Q(Key|Mouse|Wheel|Paint|Resize|Show|Hide|Close|Focus|Hover|Drop|Drag[A-Za-z]*|Touch)Event>"
        "Q(Application|GuiApplication|Pixmap|Painter|PainterPath|Image|ImageReader|ImageWriter)>"
        "Q(Icon|Color|Action|Menu|Screen|Window|Cursor|Clipboard|Font|FontMetrics|FontDatabase)>"
        "Q(Palette|Style|StyleFactory|SystemTrayIcon|KeySequence|DesktopServices|SvgRenderer)>"
        "Q(Shortcut|Transform|Region|Bitmap|Brush|Pen|Polygon|PolygonF|TextDocument)>"
        "Q(StandardItemModel|Drag|PdfWriter|[A-Za-z]*Gradient)>")
list(JOIN gui "|" gui)
file(GLOB_RECURSE sources RELATIVE "${SRC}" "${SRC}/core/*" "${SRC}/media/*" "${SRC}/upload/*")
set(offenders "")
foreach(file IN LISTS sources)
    file(STRINGS "${SRC}/${file}" hits REGEX "^[ \t]*#[ \t]*include[ \t]*<(${gui})")
    if(hits AND NOT file IN_LIST allowed)
        string(REPLACE ";" ", " hits "${hits}")
        list(APPEND offenders "${file}: ${hits}")
    endif()
endforeach()
if(offenders)
    list(JOIN offenders "\n" offenders)
    message(FATAL_ERROR "Qt GUI headers below the UI layer:\n${offenders}")
endif()
