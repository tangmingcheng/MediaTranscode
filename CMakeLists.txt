cmake_minimum_required(VERSION 3.21.1)

project(MediaTranscode LANGUAGES CXX)

set(TARGET_CORE "media_transcode_core")
set(TARGET_LOCAL_VIDEO_CLI "media_transcode_local_video_cli")
set(TARGET_REALTIME_VIDEO_CLI "media_transcode_realtime_video_cli")
set(THIRD_PARTY_DIR "${CMAKE_CURRENT_SOURCE_DIR}/3rds")

option(MEDIA_TRANSCODE_BUILD_GRAPH_TOOLS "Build graph command line tools" ON)
option(MEDIA_TRANSCODE_BUILD_TESTS "Build MediaTranscode tests" OFF)
option(MEDIA_TRANSCODE_ENABLE_INTEGRATION_TESTS "Build and register integration tests" OFF)
option(MEDIA_TRANSCODE_ENABLE_HARDWARE_TESTS "Enable hardware-dependent tests" OFF)
option(MEDIA_TRANSCODE_ENABLE_PERFORMANCE_TESTS "Build and register performance tests" OFF)

set(MEDIA_TRANSCODE_TEST_SAMPLES_DIR
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/samples"
    CACHE PATH
    "Directory containing media samples used by integration/regression tests"
)

if(POLICY CMP0141)
    cmake_policy(SET CMP0141 NEW)
    set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

set(MEDIA_TRANSCODE_GRAPH_DIAGNOSTICS_SOURCES
    src/internal/graph/diagnostics/MediaGraphDiagnostics.cpp
    src/internal/graph/diagnostics/MediaGraphDiagnostics.h
)

set(MEDIA_TRANSCODE_GRAPH_PLANNER_SOURCES
    src/internal/graph/planner/MediaPipelineAudioSourceProbe.cpp
    src/internal/graph/planner/MediaPipelineAudioSourceProbe.h
    src/internal/graph/planner/MediaPipelineCapabilityScanner.cpp
    src/internal/graph/planner/MediaPipelineCapabilityScanner.h
    src/internal/graph/planner/capability/MediaHardwareCapabilityProbe.cpp
    src/internal/graph/planner/capability/MediaHardwareCapabilityProbe.h
    src/internal/graph/planner/capability/MediaInputCapabilityProbe.cpp
    src/internal/graph/planner/capability/MediaInputCapabilityProbe.h
    src/internal/graph/planner/capability/MediaVideoCapabilityScanner.h
    src/internal/graph/planner/MediaPipelinePlanner.cpp
    src/internal/graph/planner/MediaPipelinePlanner.h
    src/internal/graph/planner/MediaPipelineScorer.cpp
    src/internal/graph/planner/MediaPipelineScorer.h
)

file(GLOB_RECURSE MEDIA_TRANSCODE_GRAPH_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/internal/graph/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/internal/graph/*.h"
)
list(FILTER MEDIA_TRANSCODE_GRAPH_SOURCES EXCLUDE REGEX ".*/src/internal/graph/diagnostics/MediaGraphDiagnostics\\.(cpp|h)$")
list(FILTER MEDIA_TRANSCODE_GRAPH_SOURCES EXCLUDE REGEX ".*/src/internal/graph/planner/MediaPipeline.*\\.(cpp|h)$")
list(APPEND MEDIA_TRANSCODE_GRAPH_SOURCES ${MEDIA_TRANSCODE_GRAPH_DIAGNOSTICS_SOURCES})
list(APPEND MEDIA_TRANSCODE_GRAPH_SOURCES ${MEDIA_TRANSCODE_GRAPH_PLANNER_SOURCES})
list(REMOVE_DUPLICATES MEDIA_TRANSCODE_GRAPH_SOURCES)
list(SORT MEDIA_TRANSCODE_GRAPH_SOURCES)

add_library(${TARGET_CORE} STATIC
    ${MEDIA_TRANSCODE_GRAPH_SOURCES}
    include/media_transcode/Result.h
)

source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES
    ${MEDIA_TRANSCODE_GRAPH_SOURCES}
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
    target_compile_options(${TARGET_CORE} PRIVATE /utf-8 /EHsc)
endif()

add_library(thirdparty_spdlog INTERFACE)
target_include_directories(thirdparty_spdlog INTERFACE
    "${THIRD_PARTY_DIR}/include"
)

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

target_link_libraries(${TARGET_CORE}
    PUBLIC
        thirdparty_ffmpeg
    PRIVATE
        thirdparty_spdlog
)

if(WIN32)
    target_link_libraries(${TARGET_CORE} PUBLIC ws2_32)
endif()

function(media_transcode_configure_app target_name)
    target_link_libraries(${target_name} PRIVATE MediaTranscode::core)
    target_compile_features(${target_name} PRIVATE cxx_std_20)
    target_include_directories(${target_name} PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
    )
    if(MSVC)
        target_compile_options(${target_name} PRIVATE /utf-8 /EHsc)
        target_link_options(${target_name} PRIVATE "$<$<CONFIG:Debug,RelWithDebInfo>:/DEBUG>")
    endif()
endfunction()

function(media_transcode_configure_test target_name)
    media_transcode_configure_app(${target_name})
    target_include_directories(${target_name} PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/tests"
    )
endfunction()

function(media_transcode_register_test target_name test_labels)
    add_test(NAME ${target_name} COMMAND ${target_name})
    set_tests_properties(${target_name} PROPERTIES LABELS "${test_labels}")
endfunction()

if(MEDIA_TRANSCODE_BUILD_GRAPH_TOOLS)
    add_executable(${TARGET_LOCAL_VIDEO_CLI}
        tools/local_video_cli/main.cpp
    )
    media_transcode_configure_app(${TARGET_LOCAL_VIDEO_CLI})

    add_executable(${TARGET_REALTIME_VIDEO_CLI}
        tools/realtime_video_cli/main.cpp
    )
    media_transcode_configure_app(${TARGET_REALTIME_VIDEO_CLI})
endif()

if(MEDIA_TRANSCODE_BUILD_TESTS)
    enable_testing()

    add_executable(media_transcode_core_tests
        tests/unit/test_core.cpp
        tests/unit/audio_drift_servo_tests.cpp
        tests/unit/video_sync_controller_tests.cpp
    )
    media_transcode_configure_test(media_transcode_core_tests)
    media_transcode_register_test(media_transcode_core_tests "core;deterministic")

    add_executable(media_transcode_planner_tests tests/unit/test_planner.cpp)
    media_transcode_configure_test(media_transcode_planner_tests)
    media_transcode_register_test(media_transcode_planner_tests "planner;deterministic")

    add_executable(media_transcode_builder_tests tests/unit/test_builder.cpp)
    media_transcode_configure_test(media_transcode_builder_tests)
    media_transcode_register_test(media_transcode_builder_tests "builder;deterministic")

    add_executable(media_transcode_runtime_tests
        tests/unit/test_event_driven_runtime.cpp
        tests/unit/event_runtime_threading_queue_tests.cpp
        tests/unit/event_runtime_ffmpeg_ownership_tests.cpp
        tests/unit/event_runtime_multi_input_tests.cpp
        tests/unit/av_output_scheduler_tests.cpp
        tests/unit/av_sync_runtime_bootstrap_tests.cpp
    )
    media_transcode_configure_test(media_transcode_runtime_tests)
    media_transcode_register_test(media_transcode_runtime_tests "runtime;deterministic")

    add_executable(media_transcode_av_generation_transition_tests
        tests/unit/av_generation_transition_tests.cpp
    )
    media_transcode_configure_test(media_transcode_av_generation_transition_tests)
    media_transcode_register_test(
        media_transcode_av_generation_transition_tests "runtime;deterministic")

    add_executable(media_transcode_av_playback_epoch_binder_tests
        tests/unit/av_playback_epoch_binder_tests.cpp
    )
    media_transcode_configure_test(media_transcode_av_playback_epoch_binder_tests)
    media_transcode_register_test(
        media_transcode_av_playback_epoch_binder_tests "runtime;deterministic")

    add_executable(media_transcode_node_tests
        tests/unit/test_node.cpp
        tests/unit/audio_swr_compensation_executor_tests.cpp
        tests/unit/audio_resample_node_tests.cpp
        tests/unit/av_startup_coordinator_tests.cpp
        tests/unit/mpeg_ts_packet_tests.cpp
        tests/unit/mpeg_ts_pes_provenance_timeline_tests.cpp
        tests/unit/mpeg_ts_clock_tests.cpp
        tests/unit/mpeg_ts_input_session_tests.cpp
        tests/unit/mpeg_ts_demux_node_tests.cpp
        tests/unit/mpeg_ts_output_serializer_tests.cpp
        tests/unit/mpeg_ts_access_unit_framer_tests.cpp
        tests/unit/mpeg_ts_output_packetizer_tests.cpp
        tests/unit/mpeg_ts_output_session_tests.cpp
        tests/unit/mpeg_ts_mux_runtime_buffer_tests.cpp
        tests/unit/mpeg_ts_ffmpeg_config_materializer_tests.cpp
        tests/unit/project_mpeg_ts_mux_session_adapter_tests.cpp
        tests/unit/project_mpeg_ts_file_mux_node_tests.cpp
        tests/unit/file_mux_session_tests.cpp
        tests/unit/file_output_resource_tests.cpp
        tests/unit/fixtures/MediaTsPesTimestampInspector.cpp
        tests/unit/output_byte_sink_tests.cpp
        tests/unit/rtp_depacketizer_tests.cpp
        tests/unit/rtp_output_clock_tests.cpp
        tests/unit/scheduled_rtp_packetization_tests.cpp
        tests/unit/scheduled_rtp_sender_tests.cpp
        tests/unit/media_rtp_udp_sender_transport_tests.cpp
        tests/unit/media_rtp_udp_sender_composition_tests.cpp
        tests/unit/rtp_source_clock_tests.cpp
    )
    media_transcode_configure_test(media_transcode_node_tests)
    media_transcode_register_test(media_transcode_node_tests "node;deterministic")

    add_executable(media_transcode_atomic_file_publisher_tests
        tests/unit/media_atomic_utf8_file_publisher_tests.cpp
    )
    media_transcode_configure_test(media_transcode_atomic_file_publisher_tests)
    media_transcode_register_test(media_transcode_atomic_file_publisher_tests "runtime;deterministic")

    add_executable(media_transcode_rtp_sdp_description_tests
        tests/unit/media_rtp_sdp_description_tests.cpp
    )
    media_transcode_configure_test(media_transcode_rtp_sdp_description_tests)
    media_transcode_register_test(media_transcode_rtp_sdp_description_tests "core;deterministic")

    add_test(NAME media_transcode_test_tier_contract
        COMMAND ${CMAKE_COMMAND}
            -DSOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/cmake/verify_test_tiers.cmake
    )
    set_tests_properties(media_transcode_test_tier_contract PROPERTIES
        LABELS "core;deterministic"
    )

    if(MEDIA_TRANSCODE_ENABLE_INTEGRATION_TESTS)
        add_executable(media_transcode_rtp_udp_sender_loopback_tests
            tests/unit/media_rtp_udp_sender_loopback_tests.cpp
        )
        media_transcode_configure_test(media_transcode_rtp_udp_sender_loopback_tests)
        media_transcode_register_test(media_transcode_rtp_udp_sender_loopback_tests "integration")
        set_tests_properties(media_transcode_rtp_udp_sender_loopback_tests PROPERTIES
            SKIP_RETURN_CODE 77
        )

        add_executable(media_transcode_rtp_udp_sender_ipv6_loopback_tests
            tests/unit/media_rtp_udp_sender_ipv6_loopback_tests.cpp
        )
        media_transcode_configure_test(media_transcode_rtp_udp_sender_ipv6_loopback_tests)
        media_transcode_register_test(media_transcode_rtp_udp_sender_ipv6_loopback_tests "integration")
        set_tests_properties(media_transcode_rtp_udp_sender_ipv6_loopback_tests PROPERTIES
            SKIP_RETURN_CODE 77
        )

        add_executable(media_transcode_integration_tests
            tests/unit/test_realtime_rtp_graph.cpp
        )
        media_transcode_configure_test(media_transcode_integration_tests)
        target_compile_definitions(media_transcode_integration_tests PRIVATE
            MEDIA_TRANSCODE_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
        )
        if(WIN32)
            target_link_libraries(media_transcode_integration_tests PRIVATE ws2_32)
        endif()
        media_transcode_register_test(media_transcode_integration_tests "integration")

        add_executable(media_transcode_mpeg_ts_integration_tests
            tests/unit/mpeg_ts_observed_avio_integration_tests.cpp
        )
        media_transcode_configure_test(media_transcode_mpeg_ts_integration_tests)
        target_compile_definitions(media_transcode_mpeg_ts_integration_tests PRIVATE
            MEDIA_TRANSCODE_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
        )
        if(WIN32)
            target_link_libraries(media_transcode_mpeg_ts_integration_tests PRIVATE ws2_32)
        endif()
        media_transcode_register_test(media_transcode_mpeg_ts_integration_tests "integration")

        add_executable(media_transcode_mpeg_ts_output_decode_integration_tests
            tests/unit/mpeg_ts_output_decode_integration_tests.cpp
            tests/unit/fixtures/MediaTsSampleStreamConfigFixture.cpp
        )
        media_transcode_configure_test(media_transcode_mpeg_ts_output_decode_integration_tests)
        target_compile_definitions(media_transcode_mpeg_ts_output_decode_integration_tests PRIVATE
            MEDIA_TRANSCODE_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
        )
        media_transcode_register_test(
            media_transcode_mpeg_ts_output_decode_integration_tests "integration")
        set_tests_properties(media_transcode_mpeg_ts_output_decode_integration_tests PROPERTIES
            SKIP_RETURN_CODE 77
        )

        add_executable(media_transcode_mpeg_ts_crafted_udp_fault_tests
            tests/unit/mpeg_ts_crafted_udp_fault_tests.cpp
            tests/unit/fixtures/MpegTsCraftedUdpFixture.cpp
        )
        media_transcode_configure_test(media_transcode_mpeg_ts_crafted_udp_fault_tests)
        target_compile_definitions(media_transcode_mpeg_ts_crafted_udp_fault_tests PRIVATE
            MEDIA_TRANSCODE_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
        )
        if(WIN32)
            target_link_libraries(media_transcode_mpeg_ts_crafted_udp_fault_tests PRIVATE ws2_32)
        endif()
        media_transcode_register_test(media_transcode_mpeg_ts_crafted_udp_fault_tests "integration")
        set_tests_properties(media_transcode_mpeg_ts_crafted_udp_fault_tests PROPERTIES
            SKIP_RETURN_CODE 77
        )
    endif()

    if(MEDIA_TRANSCODE_ENABLE_HARDWARE_TESTS)
        add_executable(media_transcode_hardware_tests tests/hardware/test_hardware.cpp)
        media_transcode_configure_test(media_transcode_hardware_tests)
        media_transcode_register_test(media_transcode_hardware_tests "hardware")
        set_tests_properties(media_transcode_hardware_tests PROPERTIES SKIP_RETURN_CODE 77)
    endif()

    if(MEDIA_TRANSCODE_ENABLE_PERFORMANCE_TESTS)
        add_executable(media_transcode_performance_tests tests/performance/test_performance.cpp)
        media_transcode_configure_test(media_transcode_performance_tests)
        media_transcode_register_test(media_transcode_performance_tests "performance")
    endif()
endif()
