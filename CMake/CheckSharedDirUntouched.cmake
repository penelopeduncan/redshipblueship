# CMake/CheckSharedDirUntouched.cmake
#
# #1692: a tier whose rows all work in private homes (rsbs-rows/<row>) leaves the
# build directory's own app-directory files alone. Snapshot before the tier,
# verify after it:
#
#   cmake -DMODE=snapshot -DBUILD_DIR=<build dir> -DSTATE=<file> -P CheckSharedDirUntouched.cmake
#   cmake -DMODE=verify   -DBUILD_DIR=<build dir> -DSTATE=<file> -P CheckSharedDirUntouched.cmake
#
# Records, for every name in RowHomeSharedNames.cmake at the TOP of BUILD_DIR, the
# size and SHA-256 of the file (or of every file under the directory), or that it
# is absent; verify fails, naming each one, when anything was added, removed or
# changed. Content, not mtime: a row that rewrote a file with the same bytes did
# not share it with anyone.

cmake_policy(SET CMP0057 NEW)
include("${CMAKE_CURRENT_LIST_DIR}/RowHomeSharedNames.cmake")

foreach(_required MODE BUILD_DIR STATE)
    if(NOT DEFINED ${_required})
        message(FATAL_ERROR "CheckSharedDirUntouched: -D${_required}=<value> is required")
    endif()
endforeach()
if(NOT MODE MATCHES "^(snapshot|verify)$")
    message(FATAL_ERROR "CheckSharedDirUntouched: MODE must be snapshot or verify, not '${MODE}'")
endif()
if(NOT IS_DIRECTORY "${BUILD_DIR}")
    message(FATAL_ERROR "CheckSharedDirUntouched: BUILD_DIR '${BUILD_DIR}' is not a directory")
endif()

function(_shared_dir_state out_var)
    set(_lines "")
    foreach(_name IN LISTS RSBS_ROW_HOME_SHARED_NAMES)
        set(_path "${BUILD_DIR}/${_name}")
        if(IS_DIRECTORY "${_path}")
            list(APPEND _lines "d|${_name}")
            file(GLOB_RECURSE _files LIST_DIRECTORIES false RELATIVE "${_path}" "${_path}/*")
            list(SORT _files)
            foreach(_rel IN LISTS _files)
                file(SIZE "${_path}/${_rel}" _size)
                file(SHA256 "${_path}/${_rel}" _hash)
                list(APPEND _lines "f|${_name}/${_rel}|${_size}|${_hash}")
            endforeach()
        elseif(EXISTS "${_path}")
            file(SIZE "${_path}" _size)
            file(SHA256 "${_path}" _hash)
            list(APPEND _lines "f|${_name}|${_size}|${_hash}")
        else()
            list(APPEND _lines "-|${_name}")
        endif()
    endforeach()
    set(${out_var} "${_lines}" PARENT_SCOPE)
endfunction()

_shared_dir_state(_now)
list(LENGTH _now _n)

if(MODE STREQUAL "snapshot")
    string(REPLACE ";" "\n" _body "${_now}")
    file(WRITE "${STATE}" "${_body}\n")
    message(STATUS "[shared-dir] snapshot of ${_n} entries under ${BUILD_DIR} written to ${STATE}")
    return()
endif()

if(NOT EXISTS "${STATE}")
    message(FATAL_ERROR "CheckSharedDirUntouched: no snapshot at ${STATE}; run MODE=snapshot before the tier")
endif()
file(STRINGS "${STATE}" _before)
set(_changes "")
foreach(_line IN LISTS _now)
    if(NOT _line IN_LIST _before)
        list(APPEND _changes "now:    ${_line}")
    endif()
endforeach()
foreach(_line IN LISTS _before)
    if(NOT _line IN_LIST _now)
        list(APPEND _changes "before: ${_line}")
    endif()
endforeach()
if(_changes)
    list(LENGTH _changes _nc)
    string(REPLACE ";" "\n  " _pretty "${_changes}")
    message(FATAL_ERROR
        "CheckSharedDirUntouched: ${_nc} change(s) to the build directory's shared app-directory files since the "
        "snapshot, so some row of the tier still works in ${BUILD_DIR} instead of its home (#1692):\n  ${_pretty}")
endif()
message(STATUS "[shared-dir] ${_n} entries under ${BUILD_DIR} unchanged since the snapshot")
