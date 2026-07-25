cmake_minimum_required(VERSION 3.21)

file(READ "${SOURCE_DIR}/CMakeLists.txt" cmake_text)
file(READ "${SOURCE_DIR}/CMakePresets.json" presets_text)

set(required_targets
    media_transcode_core_tests
    media_transcode_planner_tests
    media_transcode_builder_tests
    media_transcode_runtime_tests
    media_transcode_node_tests
    media_transcode_integration_tests
    media_transcode_hardware_tests
    media_transcode_performance_tests
)

foreach(target IN LISTS required_targets)
    if(NOT cmake_text MATCHES "add_executable\\(${target}")
        message(FATAL_ERROR "missing test target: ${target}")
    endif()
endforeach()

foreach(label IN ITEMS core planner builder runtime node integration hardware performance)
    if(NOT cmake_text MATCHES "media_transcode_register_test\\([^\n]*\"${label}(;[^\"]*)?\"" AND
       NOT cmake_text MATCHES "LABELS[^\n]*\"${label}(;[^\"]*)?\"")
        message(FATAL_ERROR "missing CTest label: ${label}")
    endif()
endforeach()

foreach(preset IN ITEMS deterministic integration hardware performance)
    if(NOT presets_text MATCHES "\"name\"[ \t]*:[ \t]*\"${preset}\"")
        message(FATAL_ERROR "missing test preset: ${preset}")
    endif()
endforeach()

if(NOT cmake_text MATCHES "SKIP_RETURN_CODE[ \t]+77")
    message(FATAL_ERROR "hardware test must expose unsupported as CTest skip code 77")
endif()

message(STATUS "test tier contract verified")
