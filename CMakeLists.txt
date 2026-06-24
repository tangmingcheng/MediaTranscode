cmake_minimum_required(VERSION 3.21.1)

project(MediaTranscode LANGUAGES CXX)

set(TARGET_CLI "media_transcode_cli")
set(TARGET_CORE "media_transcode_core")
set(THIRD_PARTY_DIR "${CMAKE_CURRENT_SOURCE_DIR}/3rds")

option(MEDIA_TRANSCODE_BUILD_CLI "Build the media_transcode_cli example program" ON)

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
    src/MediaTranscode.cpp
    src/local/FFmpegLocalFileTranscodeEngine.cpp
    src/local/FFmpegLocalFileTranscodeEngine.h
    src/internal/TranscodeTypes.h
    src/internal/FFmpegAudioAdapter.cpp
    src/internal/FFmpegAudioAdapter.h
    src/internal/FFmpegAudioCopyPipeline.cpp
    src/internal/FFmpegAudioCopyPipeline.h
    src/internal/FFmpegAudioEncodePipeline.cpp
    src/internal/FFmpegAudioEncodePipeline.h
    src/internal/FFmpegAudioFifo.cpp
    src/internal/FFmpegAudioFifo.h
    src/internal/FFmpegAudioPacketWriter.cpp
    src/internal/FFmpegAudioPacketWriter.h
    src/internal/FFmpegAudioPipeline.cpp
    src/internal/FFmpegAudioPipeline.h
    src/internal/FFmpegAudioPipelineStrategy.cpp
    src/internal/FFmpegAudioPipelineStrategy.h
    src/internal/FFmpegAudioPipelineTypes.h
    src/internal/FFmpegAudioProcessDiagnostics.cpp
    src/internal/FFmpegAudioProcessDiagnostics.h
    src/internal/FFmpegAudioStrategyPlanner.cpp
    src/internal/FFmpegAudioStrategyPlanner.h
    src/internal/FFmpegBufferSourceMetadataApplier.cpp
    src/internal/FFmpegBufferSourceMetadataApplier.h
    src/internal/FFmpegCapability.cpp
    src/internal/FFmpegCapability.h
    src/internal/FFmpegEncoderCapabilityMatrix.cpp
    src/internal/FFmpegEncoderCapabilityMatrix.h
    src/internal/FFmpegError.h
    src/internal/FFmpegHardwareBackend.cpp
    src/internal/FFmpegHardwareBackend.h
    src/internal/FFmpegHardwareContext.cpp
    src/internal/FFmpegHardwareContext.h
    src/internal/FFmpegHardwareDecoder.cpp
    src/internal/FFmpegHardwareDecoder.h
    src/internal/FFmpegHardwareEncoderSelector.cpp
    src/internal/FFmpegHardwareEncoderSelector.h
    src/internal/FFmpegHardwareFrames.cpp
    src/internal/FFmpegHardwareFrames.h
    src/internal/FFmpegHardwareTypes.h
    src/internal/FFmpegHardwareVideoFilterGraph.cpp
    src/internal/FFmpegHardwareVideoFilterGraph.h
    src/internal/FFmpegHardwareVideoFilterPipeline.cpp
    src/internal/FFmpegHardwareVideoFilterPipeline.h
    src/internal/FFmpegPhaseDiagnostics.cpp
    src/internal/FFmpegPhaseDiagnostics.h
    src/internal/FFmpegPipelinePlanner.cpp
    src/internal/FFmpegPipelinePlanner.h
    src/internal/FFmpegRAII.h
    src/internal/FFmpegTimelineNormalizer.cpp
    src/internal/FFmpegTimelineNormalizer.h
    src/internal/FFmpegTranscodeLoopDiagnostics.cpp
    src/internal/FFmpegTranscodeLoopDiagnostics.h
    src/internal/FFmpegUtils.cpp
    src/internal/FFmpegUtils.h
    src/internal/FFmpegVideoAdapter.cpp
    src/internal/FFmpegVideoAdapter.h
    src/internal/FFmpegVideoBitrateControlPlanner.cpp
    src/internal/FFmpegVideoBitrateControlPlanner.h
    src/internal/FFmpegVideoBitrateOptionAdapter.cpp
    src/internal/FFmpegVideoBitrateOptionAdapter.h
    src/internal/FFmpegVideoColorMetadata.cpp
    src/internal/FFmpegVideoColorMetadata.h
    src/internal/FFmpegVideoDecoderStage.cpp
    src/internal/FFmpegVideoDecoderStage.h
    src/internal/FFmpegVideoEncodeOptionsApplier.cpp
    src/internal/FFmpegVideoEncodeOptionsApplier.h
    src/internal/FFmpegVideoEncoderSelector.cpp
    src/internal/FFmpegVideoEncoderSelector.h
    src/internal/FFmpegVideoEncoderStage.cpp
    src/internal/FFmpegVideoEncoderStage.h
    src/internal/FFmpegVideoFilterGraph.cpp
    src/internal/FFmpegVideoFilterGraph.h
    src/internal/FFmpegVideoFilterStage.cpp
    src/internal/FFmpegVideoFilterStage.h
    src/internal/FFmpegVideoFlushDiagnostics.cpp
    src/internal/FFmpegVideoFlushDiagnostics.h
    src/internal/FFmpegVideoFrameRateStage.cpp
    src/internal/FFmpegVideoFrameRateStage.h
    src/internal/FFmpegVideoFrameRoutingStrategy.cpp
    src/internal/FFmpegVideoFrameRoutingStrategy.h
    src/internal/FFmpegVideoHardwareTransferStage.cpp
    src/internal/FFmpegVideoHardwareTransferStage.h
    src/internal/FFmpegVideoInputMetadata.cpp
    src/internal/FFmpegVideoInputMetadata.h
    src/internal/FFmpegVideoPacketWriterStage.cpp
    src/internal/FFmpegVideoPacketWriterStage.h
    src/internal/FFmpegVideoTimestampStage.cpp
    src/internal/FFmpegVideoTimestampStage.h
    src/internal/FFmpegVideoTranscodePipeline.cpp
    src/internal/FFmpegVideoTranscodePipeline.h
    include/media_transcode/MediaTranscode.h
    include/media_transcode/Result.h
)

add_library(MediaTranscode::core ALIAS ${TARGET_CORE})

target_include_directories(${TARGET_CORE}
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# --------------------------
# CLI 示例程序
# --------------------------
if(MEDIA_TRANSCODE_BUILD_CLI)
    add_executable(${TARGET_CLI}
        examples/transcode_cli/main.cpp
    )

    target_link_libraries(${TARGET_CLI}
        PRIVATE ${TARGET_CORE}
    )
endif()

# --------------------------
# 编译选项
# --------------------------
target_compile_features(${TARGET_CORE} PUBLIC cxx_std_20)

if(TARGET ${TARGET_CLI})
    target_compile_features(${TARGET_CLI} PRIVATE cxx_std_20)
endif()

if(MSVC)
    target_compile_options(${TARGET_CORE} PRIVATE /utf-8)
    if(TARGET ${TARGET_CLI})
        target_compile_options(${TARGET_CLI} PRIVATE /utf-8)
    endif()
endif()

# --------------------------
# spdlog
# --------------------------
add_library(thirdparty_spdlog INTERFACE)
target_include_directories(thirdparty_spdlog INTERFACE
    "${THIRD_PARTY_DIR}/include"
)

target_link_libraries(${TARGET_CORE} PRIVATE thirdparty_spdlog)
if(TARGET ${TARGET_CLI})
    target_link_libraries(${TARGET_CLI} PRIVATE thirdparty_spdlog)
endif()

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

else()
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

target_link_libraries(${TARGET_CORE} PUBLIC thirdparty_ffmpeg)
