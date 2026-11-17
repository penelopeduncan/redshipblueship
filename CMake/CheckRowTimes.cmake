# CMake/CheckRowTimes.cmake
#
# #1692: which rows of a ctest run came close to their TIMEOUT. Once a tier runs
# two rows at a time each row may run slower than it did alone, and the row that
# matters is the one already near its limit on the slowest machine (on the hosted
# Windows runner GoldenSeedsRoundBudget took 193 s of its old 300 at -j1). So CI
# runs this after its rando step, and the gate's partier.py after its parallel run:
# one rule for both.
#
#   cmake -DJUNIT=<ctest --output-junit file> -DCTEST_EXE=<ctest> -DBUILD_DIR=<build dir>
#         [-DLABEL=<ctest -L regex>] [-DWARN_PCT=60] -P CheckRowTimes.cmake
#
# Reads each row's TIMEOUT from `ctest --show-only=json-v1` and its time from the
# junit file, and prints one line per row above WARN_PCT percent of its TIMEOUT
# (and, under GitHub Actions, a `::warning` annotation). A slow row is a warning,
# not a failure: the TIMEOUT itself fails a row that runs out. Fails only when the
# junit file lists no row, or lists a row the build directory does not register.

foreach(_required JUNIT CTEST_EXE BUILD_DIR)
    if(NOT DEFINED ${_required})
        message(FATAL_ERROR "CheckRowTimes: -D${_required}=<value> is required")
    endif()
endforeach()
if(NOT DEFINED WARN_PCT)
    set(WARN_PCT 60)
endif()
if(NOT EXISTS "${JUNIT}")
    message(FATAL_ERROR "CheckRowTimes: junit file not found: ${JUNIT}")
endif()

set(_label_args "")
if(DEFINED LABEL AND NOT LABEL STREQUAL "")
    set(_label_args -L "${LABEL}")
endif()
execute_process(
    COMMAND "${CTEST_EXE}" --test-dir "${BUILD_DIR}" --show-only=json-v1 ${_label_args}
    RESULT_VARIABLE _rc
    OUTPUT_VARIABLE _json
    ERROR_VARIABLE _err)
if(NOT _rc EQUAL 0)
    message(FATAL_ERROR "CheckRowTimes: `ctest --show-only=json-v1` exited ${_rc}:\n${_err}")
endif()

set(_names "")
set(_timeouts "")
string(JSON _count LENGTH "${_json}" tests)
if(_count GREATER 0)
    math(EXPR _last "${_count} - 1")
    foreach(_i RANGE ${_last})
        string(JSON _name GET "${_json}" tests ${_i} name)
        set(_timeout 0)
        string(JSON _nprops LENGTH "${_json}" tests ${_i} properties)
        if(_nprops GREATER 0)
            math(EXPR _plast "${_nprops} - 1")
            foreach(_p RANGE ${_plast})
                string(JSON _pname GET "${_json}" tests ${_i} properties ${_p} name)
                if(_pname STREQUAL "TIMEOUT")
                    string(JSON _timeout GET "${_json}" tests ${_i} properties ${_p} value)
                endif()
            endforeach()
        endif()
        list(APPEND _names "${_name}")
        list(APPEND _timeouts "${_timeout}")
    endforeach()
endif()

file(READ "${JUNIT}" _xml)
string(REGEX MATCHALL "<testcase [^>]*>" _cases "${_xml}")
list(LENGTH _cases _ncases)
if(_ncases EQUAL 0)
    message(FATAL_ERROR "CheckRowTimes: ${JUNIT} lists no <testcase>; nothing ran, or the file is not ctest's junit")
endif()

set(_unknown "")
set(_near 0)
foreach(_case IN LISTS _cases)
    if(NOT _case MATCHES " name=\"([^\"]*)\"")
        continue()
    endif()
    set(_name "${CMAKE_MATCH_1}")
    set(_time 0)
    if(_case MATCHES " time=\"([0-9.eE+-]*)\"")
        set(_time "${CMAKE_MATCH_1}")
    endif()
    list(FIND _names "${_name}" _at)
    if(_at EQUAL -1)
        list(APPEND _unknown "${_name}")
        continue()
    endif()
    list(GET _timeouts ${_at} _timeout)
    string(REGEX REPLACE "\\..*$" "" _timeout_s "${_timeout}")
    if(_timeout_s STREQUAL "" OR _timeout_s EQUAL 0)
        continue()
    endif()
    # if() compares numbers as doubles, so the limit is written as a decimal.
    math(EXPR _limit_x100 "${_timeout_s} * ${WARN_PCT}")
    math(EXPR _limit_int "${_limit_x100} / 100")
    math(EXPR _limit_frac "${_limit_x100} % 100")
    if(_limit_frac LESS 10)
        set(_limit_frac "0${_limit_frac}")
    endif()
    set(_limit "${_limit_int}.${_limit_frac}")
    if(_time GREATER _limit)
        math(EXPR _near "${_near} + 1")
        string(REGEX REPLACE "\\..*$" "" _time_s "${_time}")
        if(_time_s STREQUAL "")
            set(_time_s 0)
        endif()
        math(EXPR _pct "${_time_s} * 100 / ${_timeout_s}")
        message(STATUS "[row-times] ${_name}: ${_time} s of its ${_timeout_s} s TIMEOUT (${_pct}%)")
        if(DEFINED ENV{GITHUB_ACTIONS})
            # On stdout, where the runner reads workflow commands.
            execute_process(COMMAND "${CMAKE_COMMAND}" -E echo
                "::warning title=row near timeout::${_name} ${_time}s of ${_timeout_s}s (${_pct}%, over ${WARN_PCT}%)")
        endif()
    endif()
endforeach()

if(_unknown)
    string(REPLACE ";" ", " _pretty "${_unknown}")
    message(FATAL_ERROR "CheckRowTimes: ${JUNIT} lists row(s) ${BUILD_DIR} does not register: ${_pretty}")
endif()
message(STATUS "[row-times] ${_ncases} row(s) read; ${_near} above ${WARN_PCT}% of their TIMEOUT")
