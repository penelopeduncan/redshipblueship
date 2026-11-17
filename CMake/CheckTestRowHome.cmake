# CMake/CheckTestRowHome.cmake
#
# #1692 lock: every rando-label CTest row runs in its own home. redship_add_test()
# (CMake/RedshipTests.cmake) gives such a row RSBS_TEST_HOME=<build>/rsbs-rows/<row>
# and RSBS_TEST_HOME_FROM=<build> in its ENVIRONMENT and the rsbs_row_homes fixture,
# and src/common/test_home.cpp enters the home. A row that loses either variable
# runs in the shared build directory and nothing else notices: two rows at once
# then share one config, one logs/ and one Randomizer/. So this reads the
# REGISTERED rows back through `ctest --show-only=json-v1`, as CheckTestAudioEnv.cmake
# does for SDL_AUDIODRIVER, rather than the manifest: it catches a deleted PREPEND
# line, a later set_tests_properties() that replaces ENVIRONMENT, and a row
# registered with a bare add_test(... LABELS rando).
#
#   cmake -DCTEST_EXE=<ctest> -DBUILD_DIR=<build dir> -DMANIFEST=<manifest>
#         -P CheckTestRowHome.cmake
#
# Fails on: a rando row (except RowHomesReset) without both variables or without
# rsbs_row_homes in FIXTURES_REQUIRED; two homes equal after case-folding; a home
# that is not exactly <BUILD_DIR>/rsbs-rows/<one name>; fewer than 100 rando rows
# (a vacuous listing); no row carrying FIXTURES_REQUIRED at all (a listing that
# leaves the property out). In a multi-config tree (manifest line
# `homes|disabled|...`) no row may carry a home, and the check says so.
cmake_policy(SET CMP0057 NEW)

foreach(_required CTEST_EXE BUILD_DIR MANIFEST)
    if(NOT DEFINED ${_required})
        message(FATAL_ERROR "CheckTestRowHome: -D${_required}=<value> is required")
    endif()
endforeach()
if(NOT EXISTS "${MANIFEST}")
    message(FATAL_ERROR "CheckTestRowHome: manifest not found: ${MANIFEST}")
endif()

set(_disabled FALSE)
file(STRINGS "${MANIFEST}" _manifest_lines)
foreach(_line IN LISTS _manifest_lines)
    if(_line MATCHES "^homes\\|disabled\\|")
        set(_disabled TRUE)
    endif()
endforeach()

execute_process(
    COMMAND "${CTEST_EXE}" --test-dir "${BUILD_DIR}" --show-only=json-v1
    RESULT_VARIABLE _rc
    OUTPUT_VARIABLE _json
    ERROR_VARIABLE _err)
if(NOT _rc EQUAL 0)
    message(FATAL_ERROR "CheckTestRowHome: `ctest --show-only=json-v1` exited ${_rc}:\n${_err}")
endif()

string(JSON _count LENGTH "${_json}" tests)
if(_count LESS 20)
    message(FATAL_ERROR
        "CheckTestRowHome: only ${_count} rows listed; the check would be vacuous. "
        "Is BUILD_DIR '${BUILD_DIR}' the configured build tree?")
endif()

set(_prefix "${BUILD_DIR}/rsbs-rows/")
string(LENGTH "${_prefix}" _prefix_len)
set(_problems "")
set(_home_keys "")
set(_home_owners "")
set(_rando_rows 0)
set(_rows_with_fixtures 0)
set(_rows_with_home 0)
math(EXPR _last "${_count} - 1")
foreach(_i RANGE ${_last})
    string(JSON _name GET "${_json}" tests ${_i} name)
    set(_labels "")
    set(_home "")
    set(_from "")
    set(_fixtures "")
    string(JSON _nprops LENGTH "${_json}" tests ${_i} properties)
    if(_nprops GREATER 0)
        math(EXPR _plast "${_nprops} - 1")
        foreach(_p RANGE ${_plast})
            string(JSON _pname GET "${_json}" tests ${_i} properties ${_p} name)
            if(_pname STREQUAL "LABELS" OR _pname STREQUAL "ENVIRONMENT" OR _pname STREQUAL "FIXTURES_REQUIRED")
                string(JSON _ptype TYPE "${_json}" tests ${_i} properties ${_p} value)
                set(_values "")
                if(_ptype STREQUAL "ARRAY")
                    string(JSON _nv LENGTH "${_json}" tests ${_i} properties ${_p} value)
                    if(_nv GREATER 0)
                        math(EXPR _vlast "${_nv} - 1")
                        foreach(_v RANGE ${_vlast})
                            string(JSON _entry GET "${_json}" tests ${_i} properties ${_p} value ${_v})
                            list(APPEND _values "${_entry}")
                        endforeach()
                    endif()
                else()
                    string(JSON _entry GET "${_json}" tests ${_i} properties ${_p} value)
                    list(APPEND _values "${_entry}")
                endif()
                if(_pname STREQUAL "LABELS")
                    set(_labels "${_values}")
                elseif(_pname STREQUAL "FIXTURES_REQUIRED")
                    set(_fixtures "${_values}")
                else()
                    foreach(_entry IN LISTS _values)
                        if(_entry MATCHES "^RSBS_TEST_HOME=(.*)$")
                            set(_home "${CMAKE_MATCH_1}")
                        elseif(_entry MATCHES "^RSBS_TEST_HOME_FROM=(.*)$")
                            set(_from "${CMAKE_MATCH_1}")
                        endif()
                    endforeach()
                endif()
            endif()
        endforeach()
    endif()
    if(_fixtures)
        math(EXPR _rows_with_fixtures "${_rows_with_fixtures} + 1")
    endif()
    if(NOT _home STREQUAL "")
        math(EXPR _rows_with_home "${_rows_with_home} + 1")
    endif()
    if(NOT "rando" IN_LIST _labels OR _name STREQUAL "RowHomesReset")
        continue()
    endif()
    math(EXPR _rando_rows "${_rando_rows} + 1")
    if(_disabled)
        continue()
    endif()
    if(_home STREQUAL "")
        list(APPEND _problems "${_name}: no RSBS_TEST_HOME in its ENVIRONMENT (it would run in the shared build directory)")
        continue()
    endif()
    if(_from STREQUAL "")
        list(APPEND _problems "${_name}: RSBS_TEST_HOME without RSBS_TEST_HOME_FROM (test_home.cpp refuses to start)")
    elseif(NOT _from STREQUAL BUILD_DIR)
        list(APPEND _problems "${_name}: RSBS_TEST_HOME_FROM=${_from}, not the build directory ${BUILD_DIR}")
    endif()
    if(NOT "rsbs_row_homes" IN_LIST _fixtures)
        list(APPEND _problems
            "${_name}: no rsbs_row_homes in FIXTURES_REQUIRED (a stale home from an earlier run would be reused)")
    endif()
    string(LENGTH "${_home}" _home_len)
    set(_leaf "")
    if(_home_len GREATER _prefix_len)
        string(SUBSTRING "${_home}" 0 ${_prefix_len} _head)
        string(SUBSTRING "${_home}" ${_prefix_len} -1 _leaf)
    else()
        set(_head "")
    endif()
    if(NOT _head STREQUAL _prefix OR _leaf STREQUAL "" OR _leaf MATCHES "[/\\\\]" OR _leaf MATCHES "^\\.\\.?$")
        list(APPEND _problems "${_name}: home ${_home} is not ${_prefix}<one name> (outside rsbs-rows/, or nested)")
        continue()
    endif()
    string(TOLOWER "${_home}" _key)
    list(FIND _home_keys "${_key}" _at)
    if(NOT _at EQUAL -1)
        list(GET _home_owners ${_at} _other)
        list(APPEND _problems "${_name}: home ${_home} is ${_other}'s too, after case-folding")
    else()
        list(APPEND _home_keys "${_key}")
        list(APPEND _home_owners "${_name}")
    endif()
endforeach()

if(_disabled)
    if(_rows_with_home GREATER 0)
        message(FATAL_ERROR
            "CheckTestRowHome: the manifest says homes are disabled (multi-config generator) but ${_rows_with_home} "
            "row(s) carry RSBS_TEST_HOME.")
    endif()
    message(STATUS "[row-home] disabled: multi-config generator; ${_rando_rows} rando rows run in the build directory, "
        "serially (no ctest -j for them in this tree)")
    return()
endif()

if(_rando_rows LESS 100)
    message(FATAL_ERROR
        "CheckTestRowHome: only ${_rando_rows} rando-label rows listed; the check would be close to vacuous.")
endif()
if(_rows_with_fixtures EQUAL 0)
    message(FATAL_ERROR
        "CheckTestRowHome: no listed row carries FIXTURES_REQUIRED at all, so this ctest's json-v1 listing leaves the "
        "property out and the fixture half of this check cannot be read.")
endif()

if(_problems)
    list(LENGTH _problems _np)
    string(REPLACE ";" "\n  - " _pretty "${_problems}")
    message(FATAL_ERROR
        "CheckTestRowHome: ${_np} problem(s) with the rando rows' private homes (#1692):\n  - ${_pretty}\n"
        "Register rando rows with redship_add_test() (CMake/RedshipTests.cmake gives them their home), and do not "
        "replace their ENVIRONMENT afterwards.")
endif()
list(LENGTH _home_keys _nh)
message(STATUS "[row-home] ${_nh} rando rows, each with its own home under ${_prefix} and the rsbs_row_homes fixture "
    "(${_count} rows listed)")
