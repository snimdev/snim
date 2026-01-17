# Resolves the single source of truth for the app version, before project() so the
# numeric part can seed project(VERSION) and the macOS bundle keys.
#
#   SNIM_VERSION          full version, e.g. 1.2.3, 1.2.3-4-gabc1234 or 0.0.0-dev
#   SNIM_VERSION_NUMERIC  the leading X.Y.Z of the above, e.g. 1.2.3
#
# Release builds pass -DSNIM_VERSION=<tag without the v>; a plain checkout derives it
# from git, and a tarball without git falls back to 0.0.0-dev.

set(SNIM_VERSION "" CACHE STRING "Release version (empty derives it from git describe)")

if(NOT SNIM_VERSION)
    find_program(SNIM_GIT_EXECUTABLE git)
    if(SNIM_GIT_EXECUTABLE)
        execute_process(
                COMMAND "${SNIM_GIT_EXECUTABLE}" describe --tags --match "v*" --dirty
                WORKING_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}/.."
                OUTPUT_VARIABLE _snim_describe
                OUTPUT_STRIP_TRAILING_WHITESPACE
                ERROR_QUIET
                RESULT_VARIABLE _snim_describe_result)
        if(_snim_describe_result EQUAL 0 AND _snim_describe)
            string(REGEX REPLACE "^v" "" SNIM_VERSION "${_snim_describe}")
        endif()
    endif()
endif()

if(NOT SNIM_VERSION)
    set(SNIM_VERSION "0.0.0-dev")
endif()

# project(VERSION) and CFBundleVersion accept digits and dots only, so keep the head.
if(SNIM_VERSION MATCHES "^v?([0-9]+)\\.([0-9]+)\\.([0-9]+)")
    set(SNIM_VERSION_NUMERIC "${CMAKE_MATCH_1}.${CMAKE_MATCH_2}.${CMAKE_MATCH_3}")
else()
    set(SNIM_VERSION_NUMERIC "0.0.0")
endif()

message(STATUS "Snim version: ${SNIM_VERSION} (numeric: ${SNIM_VERSION_NUMERIC})")
