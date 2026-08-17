cmake_minimum_required(VERSION 3.21.1)

project(MediaTranscode LANGUAGES CXX)

set(CMAKE_CXX_SCAN_FOR_MODULES OFF)
set(CMAKE_DEPFILE_FLAGS_CXX "")
set(CMAKE_CXX_DEPFILE_FORMAT "")
set(CMAKE_CXX_SCANDEP_SOURCE "")

set(TARGET_CORE "media_transcode_core")
set(TARGET_LOCAL_VIDEO_CLI "media_transcode_local_video_cli")
set(TARGET_REALTIME_VIDEO_CLI "media_transcode_realtime_video_cli")
set(THIRD_PARTY_DIR "${CMAKE_CURRENT_SOURCE_DIR}/3rds")

option(MEDIA_TRANSCODE_BUILD_GRAPH_TOOLS "Build graph command line tools" ON)
if(POLICY CMP0141)
    cmake_policy(SET CMP0141 NEW)
    set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

set(MEDIA_TRANSCODE_GRAPH_DIAGNOSTICS_SOURCES
    src/internal/graph/diagnostics/MediaGraphDiagnostics.cpp
    src/internal/graph/diagnostics/MediaGraphDiagnostics.h
)

set(MEDIA_TRANSCODE_GRAPH_PLANNER_SOURCES
    src/internal/graph/planner/MediaEncoderRateControlPlanner.cpp
    src/internal/graph/planner/MediaEncoderRateControlPlanner.h
    src/internal/graph/planner/MediaPipelineAudioSourceProbe.cpp
    src/internal/graph/planner/MediaPipelineAudioSourceProbe.h
    src/internal/graph/planner/MediaPipelineCapabilityScanner.cpp
    src/internal/graph/planner/MediaPipelineCapabilityScanner.h
    src/internal/graph/planner/MediaPipelineHardwareBackendConstraint.cpp
    src/internal/graph/planner/MediaPipelineHardwareBackendConstraint.h
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
elseif(CMAKE_SYSTEM_PROCESSOR STREQUAL "aarch64")
    find_package(PkgConfig REQUIRED)
    if(DEFINED ENV{PKG_CONFIG_PATH})
        set(ffmpeg_pkg_config_path_was_defined TRUE)
    else()
        set(ffmpeg_pkg_config_path_was_defined FALSE)
    endif()
    if(DEFINED ENV{PKG_CONFIG_LIBDIR})
        set(ffmpeg_pkg_config_libdir_was_defined TRUE)
    else()
        set(ffmpeg_pkg_config_libdir_was_defined FALSE)
    endif()
    set(ffmpeg_original_pkg_config_path "$ENV{PKG_CONFIG_PATH}")
    set(ffmpeg_original_pkg_config_libdir "$ENV{PKG_CONFIG_LIBDIR}")
    include(cmake/MediaTranscodeFFmpegDiscovery.cmake)
    media_transcode_select_runtime_ffmpeg_pkg_config(FFMPEG_PKG_CONFIG_DIR)

    pkg_check_modules(FFMPEG REQUIRED IMPORTED_TARGET
        libavfilter
        libavcodec
        libavformat
        libavutil
        libswscale
        libswresample
    )
    if(ffmpeg_pkg_config_path_was_defined)
        set(ENV{PKG_CONFIG_PATH} "${ffmpeg_original_pkg_config_path}")
    else()
        unset(ENV{PKG_CONFIG_PATH})
    endif()
    if(ffmpeg_pkg_config_libdir_was_defined)
        set(ENV{PKG_CONFIG_LIBDIR} "${ffmpeg_original_pkg_config_libdir}")
    else()
        unset(ENV{PKG_CONFIG_LIBDIR})
    endif()
    unset(ffmpeg_pkg_config_path_was_defined)
    unset(ffmpeg_pkg_config_libdir_was_defined)
    unset(ffmpeg_original_pkg_config_path)
    unset(ffmpeg_original_pkg_config_libdir)

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
