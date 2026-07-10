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

    add_executable(media_transcode_realtime_graph_tests
        tests/unit/test_realtime_rtp_graph.cpp
    )
    media_transcode_configure_test(media_transcode_realtime_graph_tests)
    target_compile_definitions(media_transcode_realtime_graph_tests PRIVATE
        MEDIA_TRANSCODE_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
    )
    if(WIN32)
        target_link_libraries(media_transcode_realtime_graph_tests PRIVATE ws2_32)
    endif()
    add_test(NAME media_transcode_realtime_graph_tests COMMAND media_transcode_realtime_graph_tests)

    add_executable(media_transcode_event_runtime_tests
        tests/unit/test_event_driven_runtime.cpp
    )
    media_transcode_configure_test(media_transcode_event_runtime_tests)
    add_test(NAME media_transcode_event_runtime_tests COMMAND media_transcode_event_runtime_tests)
endif()
