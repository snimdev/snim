# x64-windows, except FFmpeg and its codec ports: static, release only, still /MD.
# Linked into snim.exe, so they never collide with Qt Multimedia's FFmpeg DLLs.
set(VCPKG_TARGET_ARCHITECTURE x64)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE dynamic)
set(VCPKG_PROVIDED_FORTRAN ON)

if(PORT MATCHES "^(ffmpeg|x264|libvpl|amd-amf|ffnvcodec)$")
    set(VCPKG_LIBRARY_LINKAGE static)
    set(VCPKG_BUILD_TYPE release)
endif()
