cmake_minimum_required(VERSION 3.21.1)

project(MediaTranscode LANGUAGES CXX)

set(TARGET_CORE "media_transcode_core")
set(TARGET_GRAPH_DAG_PROBE "media_transcode_graph_dag_probe")
set(TARGET_GRAPH_QUEUE_PROBE "media_transcode_graph_queue_probe")
set(TARGET_GRAPH_FILE_INPUT_PROBE "media_transcode_graph_file_input_probe")
set(TARGET_GRAPH_DECODE_PROBE "media_transcode_graph_decode_probe")
set(TARGET_GRAPH_ENCODE_PROBE "media_transcode_graph_encode_probe")
set(THIRD_PARTY_DIR "${CMAKE_CURRENT_SOURCE_DIR}/3rds")

option(MEDIA_TRANSCODE_BUILD_GRAPH_TOOLS "Build graph DAG validation/debug tools" ON)
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

file(GLOB_RECURSE MEDIA_TRANSCODE_GRAPH_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/internal/graph/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/internal/graph/*.h"
)
list(SORT MEDIA_TRANSCODE_GRAPH_SOURCES)

add_library(${TARGET_CORE} STATIC
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
    target_compile_options(${TARGET_CORE} PRIVATE /utf-8)
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
        target_compile_options(${target_name} PRIVATE /utf-8)
    endif()
endfunction()

function(media_transcode_configure_test target_name)
    media_transcode_configure_app(${target_name})
    target_include_directories(${target_name} PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/tests"
    )
endfunction()

if(MEDIA_TRANSCODE_BUILD_GRAPH_TOOLS)
    add_executable(${TARGET_GRAPH_DAG_PROBE}
        tools/graph_dag_probe/main.cpp
    )
    media_transcode_configure_app(${TARGET_GRAPH_DAG_PROBE})

    add_executable(${TARGET_GRAPH_QUEUE_PROBE}
        tools/graph_queue_probe/main.cpp
    )
    media_transcode_configure_app(${TARGET_GRAPH_QUEUE_PROBE})

    add_executable(${TARGET_GRAPH_FILE_INPUT_PROBE}
        tools/graph_file_input_probe/main.cpp
    )
    media_transcode_configure_app(${TARGET_GRAPH_FILE_INPUT_PROBE})

    add_executable(${TARGET_GRAPH_DECODE_PROBE}
        tools/graph_decode_probe/main.cpp
    )
    media_transcode_configure_app(${TARGET_GRAPH_DECODE_PROBE})

    add_executable(${TARGET_GRAPH_ENCODE_PROBE}
        tools/graph_encode_probe/main.cpp
    )
    media_transcode_configure_app(${TARGET_GRAPH_ENCODE_PROBE})
endif()

if(MEDIA_TRANSCODE_BUILD_TESTS)
    enable_testing()
endif()
