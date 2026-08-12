include_guard(GLOBAL)

function(_media_transcode_runtime_library_version output_variable version_text library_name)
    string(REGEX MATCH
        "${library_name}[ \t]+([0-9]+)\\.[ \t]*([0-9]+)\\.([0-9]+)"
        version_match
        "${version_text}"
    )
    if(NOT version_match)
        message(FATAL_ERROR
            "Runtime FFmpeg did not report a parseable ${library_name} version"
        )
    endif()
    set(${output_variable}
        "${CMAKE_MATCH_1}.${CMAKE_MATCH_2}.${CMAKE_MATCH_3}"
        PARENT_SCOPE
    )
endfunction()

function(_media_transcode_pkg_config_module_version
         output_variable pkg_config_directory module_name)
    execute_process(
        COMMAND
            "${CMAKE_COMMAND}" -E env
            "PKG_CONFIG_PATH=${pkg_config_directory}"
            "PKG_CONFIG_LIBDIR=${pkg_config_directory}"
            "${PKG_CONFIG_EXECUTABLE}" --modversion "${module_name}"
        RESULT_VARIABLE query_result
        OUTPUT_VARIABLE query_output
        ERROR_QUIET
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    if(query_result EQUAL 0)
        set(${output_variable} "${query_output}" PARENT_SCOPE)
    else()
        set(${output_variable} "" PARENT_SCOPE)
    endif()
endfunction()

function(media_transcode_select_runtime_ffmpeg_pkg_config output_variable)
    find_program(runtime_ffmpeg NAMES ffmpeg NO_CACHE REQUIRED)
    execute_process(
        COMMAND "${runtime_ffmpeg}" -version
        RESULT_VARIABLE runtime_result
        OUTPUT_VARIABLE runtime_output
        ERROR_VARIABLE runtime_error
    )
    if(NOT runtime_result EQUAL 0)
        message(FATAL_ERROR
            "PATH-selected FFmpeg failed to report its runtime versions: ${runtime_error}"
        )
    endif()

    _media_transcode_runtime_library_version(
        runtime_avcodec "${runtime_output}" libavcodec
    )
    _media_transcode_runtime_library_version(
        runtime_avfilter "${runtime_output}" libavfilter
    )
    _media_transcode_runtime_library_version(
        runtime_avutil "${runtime_output}" libavutil
    )
    _media_transcode_runtime_library_version(
        runtime_avformat "${runtime_output}" libavformat
    )
    _media_transcode_runtime_library_version(
        runtime_swscale "${runtime_output}" libswscale
    )
    _media_transcode_runtime_library_version(
        runtime_swresample "${runtime_output}" libswresample
    )

    get_filename_component(runtime_ffmpeg_real "${runtime_ffmpeg}" REALPATH)
    get_filename_component(runtime_bin_directory "${runtime_ffmpeg_real}" DIRECTORY)
    get_filename_component(runtime_prefix "${runtime_bin_directory}" DIRECTORY)

    set(search_roots "${runtime_prefix}")
    if(NOT "$ENV{FF_HOME}" STREQUAL "")
        file(TO_CMAKE_PATH "$ENV{FF_HOME}" ff_home)
        list(APPEND search_roots "${ff_home}")
        file(GLOB ff_home_children LIST_DIRECTORIES true "${ff_home}/*")
        foreach(child IN LISTS ff_home_children)
            if(IS_DIRECTORY "${child}")
                list(APPEND search_roots "${child}")
            endif()
        endforeach()
    endif()
    list(REMOVE_DUPLICATES search_roots)

    set(pkg_config_candidates)
    foreach(root IN LISTS search_roots)
        list(APPEND pkg_config_candidates
            "${root}/lib/pkgconfig"
            "${root}/lib64/pkgconfig"
            "${root}/share/pkgconfig"
        )
    endforeach()
    list(REMOVE_DUPLICATES pkg_config_candidates)

    set(selected_directory)
    set(inspected_directories)
    foreach(candidate IN LISTS pkg_config_candidates)
        if(NOT IS_DIRECTORY "${candidate}")
            continue()
        endif()
        get_filename_component(candidate_real "${candidate}" REALPATH)
        list(FIND inspected_directories "${candidate_real}" inspected_index)
        if(NOT inspected_index EQUAL -1)
            continue()
        endif()
        list(APPEND inspected_directories "${candidate_real}")

        _media_transcode_pkg_config_module_version(
            candidate_avcodec "${candidate_real}" libavcodec
        )
        _media_transcode_pkg_config_module_version(
            candidate_avfilter "${candidate_real}" libavfilter
        )
        _media_transcode_pkg_config_module_version(
            candidate_avutil "${candidate_real}" libavutil
        )
        _media_transcode_pkg_config_module_version(
            candidate_avformat "${candidate_real}" libavformat
        )
        _media_transcode_pkg_config_module_version(
            candidate_swscale "${candidate_real}" libswscale
        )
        _media_transcode_pkg_config_module_version(
            candidate_swresample "${candidate_real}" libswresample
        )
        if("${candidate_avcodec}" STREQUAL "${runtime_avcodec}" AND
           "${candidate_avfilter}" STREQUAL "${runtime_avfilter}" AND
           "${candidate_avutil}" STREQUAL "${runtime_avutil}" AND
           "${candidate_avformat}" STREQUAL "${runtime_avformat}" AND
           "${candidate_swscale}" STREQUAL "${runtime_swscale}" AND
           "${candidate_swresample}" STREQUAL "${runtime_swresample}")
            set(selected_directory "${candidate_real}")
            break()
        endif()
    endforeach()

    if(selected_directory STREQUAL "")
        string(JOIN ", " inspected_text ${inspected_directories})
        message(FATAL_ERROR
            "No FFmpeg development package matches PATH runtime ${runtime_ffmpeg_real}; "
            "required libavcodec=${runtime_avcodec}, libavfilter=${runtime_avfilter}, "
            "libavformat=${runtime_avformat}, libavutil=${runtime_avutil}, "
            "libswscale=${runtime_swscale}, libswresample=${runtime_swresample}; "
            "inspected=${inspected_text}"
        )
    endif()

    set(ENV{PKG_CONFIG_PATH} "${selected_directory}")
    set(ENV{PKG_CONFIG_LIBDIR} "${selected_directory}")

    message(STATUS
        "FFmpeg runtime: ${runtime_ffmpeg_real}; "
        "libavcodec=${runtime_avcodec}; libavfilter=${runtime_avfilter}; "
        "libavformat=${runtime_avformat}; libavutil=${runtime_avutil}; "
        "libswscale=${runtime_swscale}; libswresample=${runtime_swresample}"
    )
    message(STATUS "FFmpeg pkg-config directory: ${selected_directory}")
    set(${output_variable} "${selected_directory}" PARENT_SCOPE)
endfunction()
