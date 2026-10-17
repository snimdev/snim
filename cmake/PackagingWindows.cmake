# A self-contained folder for the installer and the portable zip.
install(TARGETS Snim RUNTIME DESTINATION .)

# Applocal deployment already copied the DLLs snim.exe needs beside it: mirror that set.
install(CODE "
    file(GLOB _snim_vcpkg_dlls \"${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/bin/*.dll\")
    foreach(_snim_dll IN LISTS _snim_vcpkg_dlls)
        get_filename_component(_snim_dll_name \"\${_snim_dll}\" NAME)
        if(EXISTS \"$<TARGET_FILE_DIR:Snim>/\${_snim_dll_name}\")
            file(INSTALL \"\${_snim_dll}\" DESTINATION \"\${CMAKE_INSTALL_PREFIX}\")
        endif()
    endforeach()")

# vcpkg's tesseract ships no language data: tessdata 4.1.0 (Debian's tesseract-ocr-eng model).
set(SNIM_ENG_TRAINEDDATA "" CACHE FILEPATH "Local eng.traineddata to bundle (offline builds)")
set(_snim_traineddata_sha256 daa0c97d651c19fba3b25e81317cd697e9908c8208090c94c3905381c23fc047)
set(_snim_traineddata "${CMAKE_BINARY_DIR}/tessdata/eng.traineddata")
if(SNIM_ENG_TRAINEDDATA)
    file(SHA256 "${SNIM_ENG_TRAINEDDATA}" _snim_traineddata_actual)
    if(NOT _snim_traineddata_actual STREQUAL _snim_traineddata_sha256)
        message(FATAL_ERROR "SNIM_ENG_TRAINEDDATA does not match tessdata 4.1.0 eng.traineddata")
    endif()
    file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/tessdata")
    file(COPY_FILE "${SNIM_ENG_TRAINEDDATA}" "${_snim_traineddata}" ONLY_IF_DIFFERENT)
else()
    # Skipped when the cached file already has the expected hash.
    file(DOWNLOAD
            "https://raw.githubusercontent.com/tesseract-ocr/tessdata/4767ea922bcc460e70b87b1d303ebdfed0897da8/eng.traineddata"
            "${_snim_traineddata}"
            EXPECTED_HASH SHA256=${_snim_traineddata_sha256}
            TLS_VERIFY ON)
endif()
install(FILES "${_snim_traineddata}" DESTINATION tessdata)

set(CMAKE_INSTALL_SYSTEM_RUNTIME_DESTINATION .)
include(InstallRequiredSystemLibraries)

get_target_property(_qmake_executable Qt6::qmake IMPORTED_LOCATION)
get_filename_component(_qt_bin_dir "${_qmake_executable}" DIRECTORY)
find_program(WINDEPLOYQT_EXECUTABLE windeployqt HINTS "${_qt_bin_dir}" REQUIRED)

# The CRT is installed above, so windeployqt must not drop vc_redist.x64.exe in too.
install(CODE "
    execute_process(
            COMMAND \"${WINDEPLOYQT_EXECUTABLE}\" --release --no-translations
                    --no-compiler-runtime \"\${CMAKE_INSTALL_PREFIX}/snim.exe\"
            COMMAND_ERROR_IS_FATAL ANY)")
