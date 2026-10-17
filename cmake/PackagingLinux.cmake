# A normal FHS tree; the packages' /opt/snim tree and the portable tarball use the same rules.
include(GNUInstallDirs)

install(TARGETS Snim RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

# The dlopened modules need their own rule: no deployment tool finds them by walking the binary.
if(GSTREAMER_AVAILABLE)
    install(TARGETS snim_recorder_linux snim_video_linux
            LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
endif()

# KWin authorizes ScreenShot2 only when Exec names the real binary; linuxdeploy and Flatpak pass
# a bare command name instead.
set(SNIM_DESKTOP_EXEC "" CACHE STRING "Exec of the installed desktop entry (default: the installed binary)")
if(NOT SNIM_DESKTOP_EXEC)
    set(SNIM_DESKTOP_EXEC "${CMAKE_INSTALL_FULL_BINDIR}/snim")
endif()
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/deploy/dev.snim.Snim.desktop.in"
               "${CMAKE_CURRENT_BINARY_DIR}/dev.snim.Snim.desktop" @ONLY)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/dev.snim.Snim.desktop"
        DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)

install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/deploy/dev.snim.Snim.metainfo.xml"
        DESTINATION ${CMAKE_INSTALL_DATADIR}/metainfo)

install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/resources/icons/app-icon.svg"
        DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps
        RENAME dev.snim.Snim.svg)

foreach(_snim_icon_size 16 32 48 64 128 256 512)
    install(FILES
            "${CMAKE_CURRENT_SOURCE_DIR}/resources/icons/hicolor/${_snim_icon_size}x${_snim_icon_size}/apps/dev.snim.Snim.png"
            DESTINATION
            "${CMAKE_INSTALL_DATADIR}/icons/hicolor/${_snim_icon_size}x${_snim_icon_size}/apps")
endforeach()
