cmake_minimum_required(VERSION 3.21.1)

project(MediaTranscode LANGUAGES CXX)

set(TARGET_CLI "media_transcode_cli")
set(TARGET_CORE "media_transcode_core")
set(THIRD_PARTY_DIR "${CMAKE_SOURCE_DIR}/3rds")

# MSVC Debug Info
if(POLICY CMP0141)
    cmake_policy(SET CMP0141 NEW)
    set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT
        "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,
$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,
$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>"
    )
endif()

# --------------------------
# 核心库
# --------------------------
add_library(${TARGET_CORE} STATIC
    src/FFmpegTranscoder.cpp
    src/internal/FFmpegAudioFifo.cpp
    src/internal/FFmpegAudioFifo.h
    src/internal/FFmpegAudioPipeline.cpp
    src/internal/FFmpegAudioPipeline.h
    src/internal/FFmpegTimelineNormalizer.cpp
    src/internal/FFmpegTimelineNormalizer.h
    src/internal/FFmpegUtils.cpp
    src/internal/FFmpegUtils.h
    src/internal/FFmpegVideoFilterGraph.cpp
    src/internal/FFmpegVideoFilterGraph.h
    src/internal/FFmpegVideoPipeline.cpp
    src/internal/FFmpegVideoPipeline.h
    include/media_transcode/FFmpegTranscoder.h
    include/media_transcode/ITranscoder.h
    include/media_transcode/MediaTranscodeTypes.h
    include/media_transcode/FrameQueue.h
)

target_include_directories(${TARGET_CORE} 
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# --------------------------
# CLI 示例程序
# --------------------------
add_executable(${TARGET_CLI} 
    examples/transcode_cli/main.cpp
)

target_link_libraries(${TARGET_CLI}
    PRIVATE ${TARGET_CORE}
)

# --------------------------
# 编译选项
# --------------------------
target_compile_features(${TARGET_CORE} PUBLIC cxx_std_20)
target_compile_features(${TARGET_CLI} PRIVATE cxx_std_20)

if(MSVC)
    target_compile_options(${TARGET_CORE} PRIVATE /utf-8)
    target_compile_options(${TARGET_CLI} PRIVATE /utf-8)
endif()

# --------------------------
# spdlog
# --------------------------
add_library(thirdparty_spdlog INTERFACE)
target_include_directories(thirdparty_spdlog INTERFACE
    "${THIRD_PARTY_DIR}/include"
)

target_link_libraries(${TARGET_CORE} PRIVATE thirdparty_spdlog)
target_link_libraries(${TARGET_CLI} PRIVATE thirdparty_spdlog)

# --------------------------
# FFmpeg
# --------------------------
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
        avfilter
        avcodec
        avformat
        avutil
        swscale
        swresample
    )

elseif(USE_LOCAL_FFMPEG AND CMAKE_SYSTEM_PROCESSOR STREQUAL "aarch64")
    find_package(PkgConfig REQUIRED)

    pkg_check_modules(FFMPEG REQUIRED IMPORTED_TARGET
        libavfilter
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

# --------------------------
# 链接第三方库
# --------------------------
target_link_libraries(${TARGET_CORE} PRIVATE thirdparty_ffmpeg)
target_link_libraries(${TARGET_CLI} PRIVATE thirdparty_ffmpeg)
