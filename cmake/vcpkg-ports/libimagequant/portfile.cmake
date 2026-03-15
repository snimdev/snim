# Upstream exports no symbols for a DLL build, so the port is static only.
vcpkg_check_linkage(ONLY_STATIC_LIBRARY)

vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO ImageOptim/libimagequant
    REF "${VERSION}"
    SHA512 ff2d4c2b901108525f403567ed2cc81dcda6e3600c58661de6324abb191ec04d954987e23b9c4d0e02cbec64ab905d717e3a0e1aecc6e1399ecc6126e889a356
    HEAD_REF master
    PATCHES
        fix-msvc.patch
)

# Upstream's own CMakeLists leaves out remap.c and builds both linkages at once.
file(COPY "${CMAKE_CURRENT_LIST_DIR}/CMakeLists.txt" DESTINATION "${SOURCE_PATH}")

vcpkg_cmake_configure(SOURCE_PATH "${SOURCE_PATH}")
vcpkg_cmake_install()
vcpkg_cmake_config_fixup(PACKAGE_NAME imagequant CONFIG_PATH lib/cmake/imagequant)

file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")
file(INSTALL "${CMAKE_CURRENT_LIST_DIR}/usage" DESTINATION "${CURRENT_PACKAGES_DIR}/share/${PORT}")
vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/COPYRIGHT")
