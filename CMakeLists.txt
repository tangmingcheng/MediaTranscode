cmake_minimum_required(VERSION 3.21.1)

project(MediaTranscode LANGUAGES CXX)

set(TARGET_NAME MediaTranscode)
set(THIRD_PARTY_DIR "${CMAKE_SOURCE_DIR}/3rds")

if(POLICY CMP0141)
    cmake_policy(SET CMP0141 NEW)

    set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT
        "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,\
$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,\
$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>"
    )
endif()

add_executable(${TARGET_NAME}
    mian.cpp
)

target_compile_features(${TARGET_NAME} PRIVATE cxx_std_20)

if(MSVC)
    target_compile_options(${TARGET_NAME} PRIVATE /utf-8)
endif()


# spdlog
add_library(thirdparty_spdlog INTERFACE)

target_include_directories(thirdparty_spdlog INTERFACE
    "${THIRD_PARTY_DIR}/include"
)

target_link_libraries(${TARGET_NAME} PRIVATE
    thirdparty_spdlog
)


# FFmpeg
add_library(thirdparty_ffmpeg INTERFACE)

if(WIN32)

    set(FFMPEG_ROOT "${THIRD_PARTY_DIR}/ffmpeg")

    target_include_directories(thirdparty_ffmpeg INTERFACE
        "${FFMPEG_ROOT}/include"
    )

    target_link_directories(thirdparty_ffmpeg INTERFACE
        "$<$<CONFIG:Debug>:${FFMPEG_ROOT}/debug/lib>"
        "$<$<NOT:$<CONFIG:Debug>>:${FFMPEG_ROOT}/lib>"
    )

    target_link_libraries(thirdparty_ffmpeg INTERFACE
        avcodec
        avformat
        avutil
        swscale
        swresample
    )

elseif(USE_LOCAL_FFMPEG AND CMAKE_SYSTEM_PROCESSOR STREQUAL "aarch64")

    find_package(PkgConfig REQUIRED)

    pkg_check_modules(FFMPEG REQUIRED IMPORTED_TARGET
        libavcodec
        libavformat
        libavutil
        libswscale
        libswresample
    )

    target_link_libraries(thirdparty_ffmpeg INTERFACE
        PkgConfig::FFMPEG
    )

endif()

target_link_libraries(${TARGET_NAME} PRIVATE
    thirdparty_ffmpeg
)