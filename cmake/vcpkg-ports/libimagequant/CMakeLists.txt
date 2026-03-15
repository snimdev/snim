cmake_minimum_required(VERSION 3.20)

project(imagequant VERSION 2.18.0 LANGUAGES C)

add_library(imagequant STATIC
    blur.c
    kmeans.c
    libimagequant.c
    mediancut.c
    mempool.c
    nearest.c
    pam.c
    remap.c
)

target_include_directories(imagequant PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
)

if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(AMD64|x86_64|x86|i[3-6]86)$")
    target_compile_definitions(imagequant PRIVATE USE_SSE=1)
endif()

if(MSVC)
    target_compile_definitions(imagequant PRIVATE _CRT_SECURE_NO_WARNINGS)
else()
    # sqrtf and friends live in libm outside MSVC.
    target_link_libraries(imagequant PUBLIC m)
endif()

include(CMakePackageConfigHelpers)

install(TARGETS imagequant EXPORT imagequant-targets
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)
install(FILES libimagequant.h DESTINATION include)
install(EXPORT imagequant-targets
    NAMESPACE imagequant::
    FILE imagequant-config.cmake
    DESTINATION lib/cmake/imagequant
)
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/imagequant-config-version.cmake"
    COMPATIBILITY SameMajorVersion
)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/imagequant-config-version.cmake"
    DESTINATION lib/cmake/imagequant
)
