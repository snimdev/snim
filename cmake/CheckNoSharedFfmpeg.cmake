# cmake -DDUMPBIN=<dumpbin.exe> -DEXE=<snim.exe> -P CheckNoSharedFfmpeg.cmake
# Fails when EXE imports a shared FFmpeg: Qt Multimedia loads its own FFmpeg DLLs.
execute_process(COMMAND "${DUMPBIN}" /nologo /dependents "${EXE}"
        OUTPUT_VARIABLE output
        RESULT_VARIABLE result)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "dumpbin failed (${result}) on ${EXE}")
endif()

string(TOLOWER "${output}" output)
string(REGEX MATCHALL "[a-z0-9_.-]+\\.dll" imports "${output}")
if(NOT imports)
    message(FATAL_ERROR "dumpbin listed no imports for ${EXE}")
endif()

set(shared_ffmpeg "")
foreach(dll IN LISTS imports)
    if(dll MATCHES "^(avcodec|avformat|avutil|avdevice|avfilter|swscale|swresample|postproc)(-[0-9]+)?\\.dll$")
        list(APPEND shared_ffmpeg "${dll}")
    endif()
endforeach()

list(JOIN imports ", " listed)
message(STATUS "Imports: ${listed}")
if(shared_ffmpeg)
    message(FATAL_ERROR "snim.exe imports a shared FFmpeg: ${shared_ffmpeg}")
endif()
