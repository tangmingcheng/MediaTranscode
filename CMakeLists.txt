cmake_minimum_required(VERSION 3.21.1)

project(MediaTranscode LANGUAGES CXX)

set(TARGET_CORE "media_transcode_core")
set(TARGET_LOCAL_TRANSCODE_PROBE "media_transcode_local_transcode_probe")
set(THIRD_PARTY_DIR "${CMAKE_CURRENT_SOURCE_DIR}/3rds")

option(MEDIA_TRANSCODE_BUILD_EXAMPLES "Build small public API examples" ON)
option(MEDIA_TRANSCODE_BUILD_TOOLS "Build internal validation/debug tools" ON)
option(MEDIA_TRANSCODE_BUILD_P1_TOOLS "Build internal P1 realtime debug tools" ON)
option(MEDIA_TRANSCODE_BUILD_TESTS "Build MediaTranscode tests" ON)
option(MEDIA_TRANSCODE_ENABLE_INTEGRATION_TESTS "Build and register integration tests" ON)
option(MEDIA_TRANSCODE_ENABLE_HARDWARE_TESTS "Enable hardware-dependent tests" OFF)

set(MEDIA_TRANSCODE_TEST_SAMPLES_DIR
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/samples"
    CACHE PATH
    "Directory containing media samples used by integration/regression tests"
)

if(POLICY CMP0141)
    cmake_policy(SET CMP0141 NEW)
    set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT
        "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,
$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,
$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>"
    )
endif()

add_library(${TARGET_CORE} STATIC
    src/MediaTranscode.cpp
    src/local/FFmpegLocalFileTranscodeEngine.cpp
    src/local/FFmpegLocalFileTranscodeEngine.h
    src/realtime/FFmpegRealtimeStreamTranscodeEngine.cpp
    src/realtime/FFmpegRealtimeStreamTranscodeEngine.h
    src/realtime/FFmpegRealtimeVideoPipelineRuntime.cpp
    src/realtime/FFmpegRealtimeVideoPipelineRuntime.h
    src/internal/TranscodeTypes.h
    src/internal/core/video/FFmpegVideoEncodedPacketDrainStage.h
    src/internal/core/video/FFmpegVideoProcessingPipeline.cpp
    src/internal/core/video/FFmpegVideoProcessingPipeline.h
    src/internal/input/FFmpegRealtimeInputSource.cpp
    src/internal/input/FFmpegRealtimeInputSource.h
    src/internal/input/FFmpegRealtimeInterruptController.cpp
    src/internal/input/FFmpegRealtimeInterruptController.h
    src/internal/output/AudioOutputStreamProvider.h
    src/internal/output/FFmpegMuxerOutputNode.h
    src/internal/output/FFmpegRtpMuxer.cpp
    src/internal/output/FFmpegRtpMuxer.h
    src/internal/output/FFmpegRtpOutputNode.h
    src/internal/output/FFmpegRtpUrlBuilder.cpp
    src/internal/output/FFmpegRtpUrlBuilder.h
    src/internal/output/FFmpegSdpWriter.cpp
    src/internal/output/FFmpegSdpWriter.h
    src/internal/output/PacketOutputGraph.h
    src/internal/output/PacketOutputGraphController.h
    src/internal/output/PacketOutputNode.h
    src/internal/output/VideoOutputStreamProvider.h
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
    src/internal/FFmpegVideoTimestampStage.cpp
    src/internal/FFmpegVideoTimestampStage.h
    include/media_transcode/MediaTranscode.h
    include/media_transcode/MediaTypes.h
    include/media_transcode/LocalVideoTranscode.h
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

target_compile_features(${TARGET_CORE} PUBLIC cxx_std_20)

if(MSVC)
    target_compile_options(${TARGET_CORE} PRIVATE /utf-8)
endif()

add_library(thirdparty_spdlog INTERFACE)
target_include_directories(thirdparty_spdlog INTERFACE
    "${THIRD_PARTY_DIR}/include"
)

target_link_libraries(${TARGET_CORE} PRIVATE thirdparty_spdlog)

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

function(media_transcode_configure_app target_name)
    target_link_libraries(${target_name} PRIVATE MediaTranscode::core)
    target_compile_features(${target_name} PRIVATE cxx_std_20)
    if(MSVC)
        target_compile_options(${target_name} PRIVATE /utf-8)
    endif()
endfunction()

function(media_transcode_configure_test target_name)
    media_transcode_configure_app(${target_name})
    target_include_directories(${target_name} PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/tests"
    )
endfunction()

if(MEDIA_TRANSCODE_BUILD_TOOLS)
    add_executable(${TARGET_LOCAL_TRANSCODE_PROBE}
        tools/local_transcode_probe/main.cpp
    )
    media_transcode_configure_app(${TARGET_LOCAL_TRANSCODE_PROBE})
    target_link_libraries(${TARGET_LOCAL_TRANSCODE_PROBE} PRIVATE thirdparty_spdlog)
endif()

if(MEDIA_TRANSCODE_BUILD_EXAMPLES)
    add_executable(media_transcode_example_local_sync
        examples/api/local_transcode_sync.cpp
    )
    media_transcode_configure_app(media_transcode_example_local_sync)

    add_executable(media_transcode_example_local_async
        examples/api/local_transcode_async.cpp
    )
    media_transcode_configure_app(media_transcode_example_local_async)

    add_executable(media_transcode_example_local_cancel
        examples/api/local_transcode_cancel.cpp
    )
    media_transcode_configure_app(media_transcode_example_local_cancel)

    add_executable(media_transcode_example_local_error_handling
        examples/api/local_transcode_error_handling.cpp
    )
    media_transcode_configure_app(media_transcode_example_local_error_handling)
endif()

if(MEDIA_TRANSCODE_BUILD_P1_TOOLS)
    add_executable(media_transcode_p1_realtime_probe
        tools/realtime_probe/main.cpp
    )
    media_transcode_configure_app(media_transcode_p1_realtime_probe)
    target_include_directories(media_transcode_p1_realtime_probe PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
    )
    target_link_libraries(media_transcode_p1_realtime_probe PRIVATE thirdparty_spdlog)
endif()

if(MEDIA_TRANSCODE_BUILD_TESTS)
    enable_testing()

    add_executable(media_transcode_public_header_tests
        tests/compile/test_public_headers.cpp
    )
    media_transcode_configure_app(media_transcode_public_header_tests)
    add_test(NAME media_transcode_public_header_tests COMMAND media_transcode_public_header_tests)

    add_executable(media_transcode_unit_tests
        tests/unit/test_local_transcode_api.cpp
    )
    media_transcode_configure_test(media_transcode_unit_tests)
    add_test(NAME media_transcode_unit_tests COMMAND media_transcode_unit_tests)

    if(MEDIA_TRANSCODE_ENABLE_INTEGRATION_TESTS)
        add_executable(media_transcode_missing_input_tests
            tests/integration/test_local_transcode_missing_input.cpp
        )
        media_transcode_configure_test(media_transcode_missing_input_tests)
        add_test(NAME media_transcode_missing_input_tests COMMAND media_transcode_missing_input_tests)

        add_executable(media_transcode_integration_tests
            tests/common/MediaProbe.cpp
            tests/integration/test_local_transcode_integration.cpp
        )
        media_transcode_configure_test(media_transcode_integration_tests)
        add_test(
            NAME media_transcode_integration_tests
            COMMAND media_transcode_integration_tests "${MEDIA_TRANSCODE_TEST_SAMPLES_DIR}"
        )
        set_tests_properties(media_transcode_integration_tests PROPERTIES SKIP_RETURN_CODE 77)
    endif()
endif()
