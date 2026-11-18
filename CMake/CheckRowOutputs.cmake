# CMake/CheckRowOutputs.cmake
#
# #1692: the files rows write into the build directory for CI or the gate to
# collect, read from the manifest's `output|` lines (redship_add_test's OUTPUTS
# keyword, CMake/RedshipTests.cmake), never from a glob. A glob that matches
# nothing is a silent empty upload: digest-crossos-diff only warns when one leg's
# digests are missing. This fails instead, naming the row.
#
#   cmake -DMANIFEST=<manifest> -DBUILD_DIR=<build dir> -DGROUP=<crossos|compare|package>
#         [-DLABEL=<regex over a row's labels>] [-DCOPY_TO=<dir>] [-DPRINT=ON]
#         [-DREQUIRE_OPTIONAL=ON] -P CheckRowOutputs.cmake
#
# GROUP compare is every crossos file plus the compare files (what a serial run and
# a parallel run must agree on). A path declared with a leading `?` is optional:
# skipped when absent unless REQUIRE_OPTIONAL is ON. Every other declared file must
# exist and be non-empty. COPY_TO copies each file there (flat, by file name);
# PRINT prints each one. Prints `[row-outputs] <group>: N of N`, and fails when the
# group selects nothing at all.
cmake_policy(SET CMP0057 NEW)

foreach(_required MANIFEST BUILD_DIR GROUP)
    if(NOT DEFINED ${_required})
        message(FATAL_ERROR "CheckRowOutputs: -D${_required}=<value> is required")
    endif()
endforeach()
if(NOT GROUP MATCHES "^(crossos|compare|package)$")
    message(FATAL_ERROR "CheckRowOutputs: GROUP must be crossos, compare or package, not '${GROUP}'")
endif()
if(NOT EXISTS "${MANIFEST}")
    message(FATAL_ERROR "CheckRowOutputs: manifest not found: ${MANIFEST}")
endif()

set(_groups "${GROUP}")
if(GROUP STREQUAL "compare")
    set(_groups crossos compare)
endif()

file(STRINGS "${MANIFEST}" _lines)
set(_paths "")
set(_rows "")
set(_optional "")
foreach(_line IN LISTS _lines)
    if(NOT _line MATCHES "^output\\|([^|]*)\\|([^|]*)\\|([^|]*)\\|(.*)$")
        continue()
    endif()
    set(_row "${CMAKE_MATCH_1}")
    string(REPLACE "," ";" _labels "${CMAKE_MATCH_2}")
    set(_group "${CMAKE_MATCH_3}")
    set(_path "${CMAKE_MATCH_4}")
    if(NOT _group IN_LIST _groups)
        continue()
    endif()
    if(DEFINED LABEL AND NOT LABEL STREQUAL "")
        set(_label_hit FALSE)
        foreach(_l IN LISTS _labels)
            if(_l MATCHES "${LABEL}")
                set(_label_hit TRUE)
            endif()
        endforeach()
        if(NOT _label_hit)
            continue()
        endif()
    endif()
    set(_opt FALSE)
    if(_path MATCHES "^\\?(.*)$")
        set(_path "${CMAKE_MATCH_1}")
        set(_opt TRUE)
    endif()
    if(_path IN_LIST _paths)
        continue() # the same file in two selected groups of one row
    endif()
    list(APPEND _paths "${_path}")
    list(APPEND _rows "${_row}")
    list(APPEND _optional "${_opt}")
endforeach()

list(LENGTH _paths _declared)
if(_declared EQUAL 0)
    message(FATAL_ERROR
        "CheckRowOutputs: the manifest declares no '${GROUP}' output (LABEL '${LABEL}'), so there would be nothing to "
        "collect. Re-configure, or declare the files with redship_add_test(... OUTPUTS ...).")
endif()

if(DEFINED COPY_TO AND NOT COPY_TO STREQUAL "")
    file(MAKE_DIRECTORY "${COPY_TO}")
endif()

set(_missing "")
set(_found 0)
set(_skipped 0)
set(_copied_names "")
math(EXPR _last "${_declared} - 1")
foreach(_i RANGE ${_last})
    list(GET _paths ${_i} _path)
    list(GET _rows ${_i} _row)
    list(GET _optional ${_i} _opt)
    set(_file "${BUILD_DIR}/${_path}")
    set(_size 0)
    if(EXISTS "${_file}" AND NOT IS_DIRECTORY "${_file}")
        file(SIZE "${_file}" _size)
    endif()
    if(_size EQUAL 0)
        if(_opt AND NOT REQUIRE_OPTIONAL)
            math(EXPR _skipped "${_skipped} + 1")
            continue()
        endif()
        if(EXISTS "${_file}")
            list(APPEND _missing "${_path} (row ${_row}): EMPTY")
        else()
            list(APPEND _missing "${_path} (row ${_row}): MISSING")
        endif()
        continue()
    endif()
    math(EXPR _found "${_found} + 1")
    if(DEFINED COPY_TO AND NOT COPY_TO STREQUAL "")
        get_filename_component(_name "${_path}" NAME)
        if(_name IN_LIST _copied_names)
            message(FATAL_ERROR "CheckRowOutputs: two declared outputs share the file name ${_name}; COPY_TO is flat")
        endif()
        list(APPEND _copied_names "${_name}")
        file(COPY_FILE "${_file}" "${COPY_TO}/${_name}")
    endif()
    if(PRINT)
        file(READ "${_file}" _body)
        message("===== ${_path} (row ${_row}) =====\n${_body}")
    endif()
endforeach()

math(EXPR _required "${_declared} - ${_skipped}")
if(_missing)
    list(LENGTH _missing _nm)
    string(REPLACE ";" "\n  - " _pretty "${_missing}")
    message(FATAL_ERROR
        "CheckRowOutputs: ${_nm} of ${_required} declared '${GROUP}' output(s) are missing or empty in ${BUILD_DIR}:\n"
        "  - ${_pretty}\n"
        "A row that writes one of these into its private home instead of the build directory (RSBS_ROW_OUTPUT_DIR, "
        "src/common/test_home.h), a row that did not run, or a row that failed before writing it.")
endif()
message(STATUS "[row-outputs] ${GROUP}: ${_found} of ${_required}"
    " (${_skipped} optional output(s) absent in this environment)")
