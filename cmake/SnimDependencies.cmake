# Finds every non-Qt dependency. Most set <PREFIX>_AVAILABLE and a few also a global HAVE_*
# define; a missing required one stops the configure.

include(FeatureSummary)

# Windows uses vcpkg packages; a pkg-config on PATH there (Strawberry Perl) hands out MinGW builds.
if(NOT WIN32)
    find_package(PkgConfig)
endif()

macro(_snim_dependency_set _snim_var)
    set(${_snim_var} ${ARGN})
    set(${_snim_var} ${ARGN} PARENT_SCOPE)
endmacro()

# Sets <out> to the dirs plus each one's parent, so both <lib/x.h> and <x.h> resolve.
function(snim_with_parent_dirs out)
    set(dirs ${ARGN})
    foreach(dir ${ARGN})
        get_filename_component(parent "${dir}" DIRECTORY)
        if(parent)
            list(APPEND dirs "${parent}")
        endif()
    endforeach()
    list(REMOVE_DUPLICATES dirs)
    set(${out} ${dirs} PARENT_SCOPE)
endfunction()

# A feature named after a QUIET package would be hidden, so features are named by what they do.
function(snim_add_feature feature enabled library hint)
    if(NOT ${enabled} AND hint)
        string(APPEND library "; install with: ${hint}")
    endif()
    add_feature_info("${feature}" ${enabled} "${library}")
endfunction()

# Tries the vcpkg CONFIG package, pkg-config, FIND_PACKAGE (not on Windows), then HEADER and
# LIBRARIES by hand. A second prefix gets the second CONFIG target and PKG_CONFIG module.
function(snim_find_dependency)
    cmake_parse_arguments(PARSE_ARGV 0 arg
            "REQUIRED;HOMEBREW;NO_WIN32_MANUAL;INCLUDE_PARENTS"
            "NAME;FEATURE;DEFINE;FIND_PACKAGE;INSTALL"
            "CONFIG;PKG_CONFIG;HEADER;LIBRARIES")
    set(prefixes ${arg_UNPARSED_ARGUMENTS})
    list(GET prefixes 0 prefix)
    _snim_dependency_set(${prefix}_AVAILABLE FALSE)

    if(VCPKG_TOOLCHAIN AND arg_CONFIG)
        list(POP_FRONT arg_CONFIG package)
        find_package(${package} CONFIG)
        if(${package}_FOUND)
            _snim_dependency_set(${prefix}_AVAILABLE TRUE)
            foreach(dep_prefix dep_target IN ZIP_LISTS prefixes arg_CONFIG)
                _snim_dependency_set(${dep_prefix}_LIBRARIES ${dep_target})
            endforeach()
        endif()
    endif()

    if(PkgConfig_FOUND AND NOT ${prefix}_AVAILABLE AND arg_PKG_CONFIG)
        set(found TRUE)
        foreach(dep_prefix dep_module IN ZIP_LISTS prefixes arg_PKG_CONFIG)
            pkg_check_modules(${dep_prefix} ${dep_module})
            if(NOT ${dep_prefix}_FOUND)
                set(found FALSE)
            endif()
        endforeach()
        if(found)
            _snim_dependency_set(${prefix}_AVAILABLE TRUE)
        endif()
    endif()

    if(NOT ${prefix}_AVAILABLE AND NOT WIN32 AND arg_FIND_PACKAGE)
        find_package(${arg_FIND_PACKAGE} QUIET)
        if(${arg_FIND_PACKAGE}_FOUND)
            _snim_dependency_set(${prefix}_AVAILABLE TRUE)
            _snim_dependency_set(${prefix}_INCLUDE_DIRS ${${arg_FIND_PACKAGE}_INCLUDE_DIRS})
            _snim_dependency_set(${prefix}_LIBRARIES ${${arg_FIND_PACKAGE}_LIBRARIES})
        endif()
    endif()

    if(NOT ${prefix}_AVAILABLE AND arg_HEADER AND NOT (WIN32 AND arg_NO_WIN32_MANUAL))
        set(include_paths /usr/include /usr/local/include)
        set(library_paths /usr/lib64 /usr/lib /usr/local/lib)
        if(arg_HOMEBREW)
            list(APPEND include_paths /opt/homebrew/include)
            list(APPEND library_paths /opt/homebrew/lib)
        endif()
        list(GET arg_HEADER 0 header_var)
        list(GET arg_HEADER 1 header)
        find_path(${header_var} ${header} PATHS ${include_paths})
        set(found TRUE)
        if(NOT ${header_var})
            set(found FALSE)
        endif()
        set(libraries)
        while(arg_LIBRARIES)
            list(POP_FRONT arg_LIBRARIES library_var library)
            find_library(${library_var} ${library} PATHS ${library_paths})
            if(NOT ${library_var})
                set(found FALSE)
            endif()
            list(APPEND libraries ${${library_var}})
        endwhile()
        if(found)
            _snim_dependency_set(${prefix}_AVAILABLE TRUE)
            _snim_dependency_set(${prefix}_INCLUDE_DIRS ${${header_var}})
            _snim_dependency_set(${prefix}_LIBRARIES ${libraries})
        endif()
    endif()

    if(${prefix}_AVAILABLE)
        if(arg_INCLUDE_PARENTS)
            snim_with_parent_dirs(dirs ${${prefix}_INCLUDE_DIRS})
            _snim_dependency_set(${prefix}_INCLUDE_DIRS ${dirs})
        endif()
        if(arg_DEFINE)
            add_compile_definitions(${arg_DEFINE})
        endif()
    endif()

    snim_add_feature("${arg_FEATURE}" ${prefix}_AVAILABLE "${arg_NAME}" "${arg_INSTALL}")
    if(arg_REQUIRED AND NOT ${prefix}_AVAILABLE)
        message(FATAL_ERROR "${arg_NAME} is required but was not found.\n  Install with: ${arg_INSTALL}")
    endif()
endfunction()

# pkg-config reports bare library names, so its library dirs go along for non-default prefixes.
function(snim_link_dependency target prefix)
    target_include_directories(${target} PRIVATE ${${prefix}_INCLUDE_DIRS})
    target_link_directories(${target} PUBLIC ${${prefix}_LIBRARY_DIRS})
    target_link_libraries(${target} PUBLIC ${${prefix}_LIBRARIES})
endfunction()

# Some Leptonica builds report <prefix>/include/leptonica, but OCRService includes <leptonica/...>.
snim_find_dependency(TESSERACT NAME "Tesseract OCR" FEATURE "Text snipping"
        DEFINE HAVE_TESSERACT
        CONFIG Tesseract Tesseract::libtesseract
        PKG_CONFIG tesseract
        HEADER Tesseract_INCLUDE_DIR tesseract/baseapi.h
        LIBRARIES Tesseract_LIBRARY tesseract Leptonica_LIBRARY lept
        NO_WIN32_MANUAL INCLUDE_PARENTS
        INSTALL "sudo dnf install tesseract-devel")

# vcpkg's curl[ssl] uses Schannel; the macOS SDK ships libcurl without a .pc file.
snim_find_dependency(LIBCURL NAME libcurl FEATURE "FTP/FTPS uploads"
        DEFINE HAVE_LIBCURL
        CONFIG CURL CURL::libcurl
        PKG_CONFIG libcurl
        FIND_PACKAGE CURL
        INSTALL "sudo apt install libcurl4-openssl-dev (macOS ships one)")

# vcpkg's libssh2[openssl]: WinCNG has no ed25519 host or user keys.
snim_find_dependency(LIBSSH2 NAME libssh2 FEATURE "SFTP uploads"
        DEFINE HAVE_LIBSSH2
        CONFIG libssh2 libssh2::libssh2
        PKG_CONFIG libssh2
        HEADER Libssh2_INCLUDE_DIR libssh2.h
        LIBRARIES Libssh2_LIBRARY ssh2
        HOMEBREW
        INSTALL "brew install libssh2 / sudo apt install libssh2-1-dev")

# The animated encoder needs libwebpmux too, so a partial find is not usable. pkg-config
# reports <prefix>/include/webp; the parents let <webp/encode.h> resolve.
snim_find_dependency(LIBWEBP LIBWEBPMUX NAME libwebp FEATURE "Animated WebP export" REQUIRED
        CONFIG WebP WebP::webp WebP::libwebpmux
        PKG_CONFIG libwebp libwebpmux
        HEADER WebP_INCLUDE_DIR webp/encode.h
        LIBRARIES WebPMux_LIBRARY webpmux WebP_LIBRARY webp
        HOMEBREW INCLUDE_PARENTS
        INSTALL "sudo apt install libwebp-dev / sudo dnf install libwebp-devel / brew install webp")

# 5.1 is the first giflib with the current API.
find_package(GIF 5.1)
set_package_properties(GIF PROPERTIES TYPE REQUIRED PURPOSE "Animated GIF export")
if(NOT GIF_FOUND)
    message(FATAL_ERROR
            "giflib 5.1 or newer is required but was not found.\n"
            "  Install with: sudo apt install libgif-dev / sudo dnf install giflib-devel / brew install giflib")
endif()

snim_find_dependency(IMAGEQUANT NAME libimagequant FEATURE "GIF palettes" REQUIRED
        CONFIG imagequant imagequant::imagequant
        PKG_CONFIG imagequant
        HEADER Imagequant_INCLUDE_DIR libimagequant.h
        LIBRARIES Imagequant_LIBRARY imagequant
        HOMEBREW
        INSTALL "sudo apt install libimagequant-dev / sudo dnf install libimagequant-devel / brew install libimagequant")

set(GSTREAMER_AVAILABLE FALSE)
set(LAYER_SHELL_AVAILABLE FALSE)
set(XCB_AVAILABLE FALSE)
set(SCREENCOPY_AVAILABLE FALSE)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    if(PkgConfig_FOUND)
        pkg_check_modules(GSTREAMER gstreamer-1.0>=1.18)
        if(GSTREAMER_FOUND)
            set(GSTREAMER_AVAILABLE TRUE)
        endif()
    endif()
    snim_add_feature("Linux screen recording" GSTREAMER_AVAILABLE GStreamer
            "sudo dnf install gstreamer1-devel")

    # Without it the recording overlay is a toplevel the compositor sinks below other windows.
    find_package(LayerShellQt CONFIG QUIET)
    if(LayerShellQt_FOUND)
        set(LAYER_SHELL_AVAILABLE TRUE)
    endif()
    snim_add_feature("Layer-shell recording overlay" LAYER_SHELL_AVAILABLE LayerShellQt
            "sudo dnf install layer-shell-qt-devel")

    if(PkgConfig_FOUND)
        pkg_check_modules(XCB IMPORTED_TARGET xcb)
        if(XCB_FOUND)
            set(XCB_AVAILABLE TRUE)
        endif()
    endif()
    snim_add_feature("X11 window picking and recording" XCB_AVAILABLE XCB
            "sudo apt install libxcb1-dev / sudo dnf install libxcb-devel")

    # The protocol XMLs are vendored, so only libwayland-client and wayland-scanner are needed.
    if(PkgConfig_FOUND)
        pkg_check_modules(WAYLAND_CLIENT IMPORTED_TARGET wayland-client>=1.20)
        pkg_get_variable(_wayland_scanner_pc wayland-scanner wayland_scanner)
        find_program(WAYLAND_SCANNER_EXECUTABLE NAMES wayland-scanner HINTS "${_wayland_scanner_pc}")
        if(WAYLAND_CLIENT_FOUND AND WAYLAND_SCANNER_EXECUTABLE)
            set(SCREENCOPY_AVAILABLE TRUE)
        endif()
    endif()
    snim_add_feature("Native Wayland screencopy" SCREENCOPY_AVAILABLE
            "wayland-client + wayland-scanner"
            "sudo apt install libwayland-dev / sudo dnf install wayland-devel")
endif()

# vcpkg's x64-windows-snim triplet builds FFmpeg static, so it cannot clash with Qt's FFmpeg DLLs.
if(WIN32)
    # vcpkg's module, not Qt's FindFFmpeg.cmake: only it lists the static deps (x264, VPL).
    list(PREPEND CMAKE_MODULE_PATH "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/share/ffmpeg")
    find_package(FFMPEG REQUIRED)
    list(POP_FRONT CMAKE_MODULE_PATH)
endif()
