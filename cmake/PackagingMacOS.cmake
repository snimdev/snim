# The .app bundle gives a stable bundle id; `deploy` makes a self-contained, signed dist/Snim.app.
set_target_properties(Snim PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/cmake/Info.plist.in"
        MACOSX_BUNDLE_BUNDLE_NAME "Snim"
        MACOSX_BUNDLE_GUI_IDENTIFIER "dev.snim.Snim"
        MACOSX_BUNDLE_BUNDLE_VERSION "${SNIM_VERSION_NUMERIC}"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${SNIM_VERSION_NUMERIC}"
        MACOSX_BUNDLE_COPYRIGHT "Copyright © 2026 Darko Gjorgjijoski"
)

set(SNIM_APP_ICON "${CMAKE_CURRENT_SOURCE_DIR}/resources/AppIcon.icns")
target_sources(Snim PRIVATE "${SNIM_APP_ICON}")
set_source_files_properties("${SNIM_APP_ICON}" PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")
set_target_properties(Snim PROPERTIES MACOSX_BUNDLE_ICON_FILE "AppIcon.icns")

# An ad-hoc signature changes on every rebuild and macOS then drops the TCC grants; a stable
# identity keeps them. Empty falls back to ad-hoc.
set(SNIM_CODESIGN_IDENTITY "Snim Dev Signing" CACHE STRING
        "Code-signing identity for the dev build (empty = linker ad-hoc signing)")
if(SNIM_CODESIGN_IDENTITY)
    # Only when the identity is in the keychain, so clones and CI without the cert still build.
    execute_process(COMMAND security find-identity -v -p codesigning
            OUTPUT_VARIABLE _snim_identities ERROR_QUIET)
    string(FIND "${_snim_identities}" "${SNIM_CODESIGN_IDENTITY}" _snim_identity_found)
    if(NOT _snim_identity_found EQUAL -1)
        add_custom_command(TARGET Snim POST_BUILD
                COMMAND codesign --force --sign "${SNIM_CODESIGN_IDENTITY}"
                        --timestamp=none "$<TARGET_BUNDLE_DIR:Snim>"
                COMMENT "Code-signing Snim.app with '${SNIM_CODESIGN_IDENTITY}' (stable TCC identity)")
    else()
        message(STATUS "Code-signing identity '${SNIM_CODESIGN_IDENTITY}' not in keychain - leaving ad-hoc signature")
    endif()
endif()

# macdeployqt sits beside qmake.
get_target_property(_qmake_executable Qt6::qmake IMPORTED_LOCATION)
get_filename_component(_qt_bin_dir "${_qmake_executable}" DIRECTORY)
find_program(MACDEPLOYQT_EXECUTABLE macdeployqt HINTS "${_qt_bin_dir}")

if(MACDEPLOYQT_EXECUTABLE)
    # Deploy a COPY, never in place: after a relink the dev bundle's leftover qt.conf would load
    # the bundled Qt plugins beside Homebrew Qt, two Qts in one process and a SIGABRT.
    set(SNIM_DIST_APP "${CMAKE_BINARY_DIR}/dist/Snim.app")
    # macos_deploy.sh strips the Homebrew rpaths and signs with $SNIM_SIGN_IDENTITY (ad-hoc if unset).
    add_custom_target(deploy
            COMMAND "${CMAKE_COMMAND}" -E rm -rf "${SNIM_DIST_APP}"
            COMMAND "${CMAKE_COMMAND}" -E make_directory "${CMAKE_BINARY_DIR}/dist"
            COMMAND "${CMAKE_COMMAND}" -E copy_directory
                    "$<TARGET_BUNDLE_DIR:Snim>" "${SNIM_DIST_APP}"
            COMMAND bash "${CMAKE_CURRENT_SOURCE_DIR}/cmake/macos_deploy.sh"
                    "${SNIM_DIST_APP}" "${MACDEPLOYQT_EXECUTABLE}"
            DEPENDS Snim
            COMMENT "Creating self-contained dist/Snim.app (Qt bundled, rpaths fixed, signed)"
            VERBATIM
    )
else()
    message(WARNING "macdeployqt not found - the `deploy` target will be unavailable")
endif()
