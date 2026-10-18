if(NOT DEFINED SOURCE_DIR OR NOT DEFINED OUTPUT_FILE)
    message(FATAL_ERROR "SOURCE_DIR and OUTPUT_FILE are required")
endif()
if(NOT DEFINED EMBED_SOURCE_REVISION)
    set(EMBED_SOURCE_REVISION ON)
endif()
set(REDLIGHT_BUILD_REVISION "unversioned")
set(REDLIGHT_SOURCE_STATE "unknown")
if(EMBED_SOURCE_REVISION AND EXISTS "${SOURCE_DIR}/.git")
    find_package(Git QUIET)
    if(GIT_FOUND)
        execute_process(COMMAND "${GIT_EXECUTABLE}" -C "${SOURCE_DIR}" rev-parse --verify --short=12 HEAD
            OUTPUT_VARIABLE revision OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET RESULT_VARIABLE revision_result)
        if(revision_result EQUAL 0 AND revision MATCHES "^[0-9a-f]+$")
            set(REDLIGHT_BUILD_REVISION "${revision}")
            # Avoid index refresh writes during concurrent build/analysis metadata reads.
            execute_process(COMMAND "${CMAKE_COMMAND}" -E env GIT_OPTIONAL_LOCKS=0
                "${GIT_EXECUTABLE}" -c core.fsmonitor=false -C "${SOURCE_DIR}"
                status --porcelain=v1 --untracked-files=normal
                OUTPUT_VARIABLE changes ERROR_QUIET RESULT_VARIABLE status_result)
            if(status_result EQUAL 0)
                if(changes STREQUAL "")
                    set(REDLIGHT_SOURCE_STATE "clean")
                else()
                    set(REDLIGHT_SOURCE_STATE "modified")
                endif()
            endif()
        endif()
    endif()
elseif(NOT EMBED_SOURCE_REVISION)
    set(REDLIGHT_SOURCE_STATE "not recorded")
endif()
get_filename_component(output_directory "${OUTPUT_FILE}" DIRECTORY)
file(MAKE_DIRECTORY "${output_directory}")
# CONFIGURE preserves the timestamp when identity is unchanged.
file(CONFIGURE OUTPUT "${OUTPUT_FILE}" CONTENT [=[#pragma once
#define REDLIGHT_BUILD_REVISION "@REDLIGHT_BUILD_REVISION@"
#define REDLIGHT_SOURCE_STATE "@REDLIGHT_SOURCE_STATE@"
]=] @ONLY)
