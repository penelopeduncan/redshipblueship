# CMake/CheckRowHome.cmake
#
# #1692: the RowHomeIsolation row. A rando-label row runs with RSBS_TEST_HOME and
# RSBS_TEST_HOME_FROM set (CMake/RedshipTests.cmake), and TestHome_EnterIfAsked
# (src/common/test_home.cpp) makes the home its working directory before any row
# code runs. This drives the real binary's row-home-probe dispatch
# (src/common/tests/test_row_home.c) in scratch folders and checks, leg by leg:
#
#   (a) cwd = FROM = F (the binary linked into F, F holding a seed config and two
#       placeholder .o2r archives), and on Linux and macOS SHIP_HOME exported as
#       F/sub: exit 0, `[HOME] ... config=seeded`, the probe's working directory AND
#       app directory are the home, the home holds the seed's exact bytes and every
#       file the probe wrote, no archive was copied, and F is exactly as it was: its
#       listing gains only rsbs-rows/ and its config keeps its bytes. Red if the home
#       is not entered (current_path dropped), if the home does not override an
#       exported SHIP_HOME (the setenv dropped; Linux and macOS), if an archive is
#       copied into the home, or if a writer resolves outside it.
#   (b) FROM with no config: exit 0, `config=none`. Red if a missing seed refuses.
#   (c) cwd = F/sub (not FROM): `[HOME] kept`, the home is never created. Red if
#       the cwd rule (the equivalent() guard) is dropped.
#   (d) row-home-probe-pair: two probes AT ONCE in F with homes H2 and H3, meeting at
#       a barrier before they write: both exit 0, each home holds only its own
#       probe's files, each child's log shows its own `[HOME]` line. Red if the two
#       share a home.
#   (e) FROM = G, a folder without the binary (not macOS, where the guard is off):
#       exit non-zero with `[HOME] FAIL: exe dir`, and G gains no rsbs-rows/. Red if
#       the exe-directory guard is dropped.
#   (f) FROM whose shipofharkinian.json cannot be copied (it is a directory): exit
#       non-zero with `[HOME] FAIL: could not seed`. Red if a failed seed only warns.
#
#   cmake -DREDSHIP_EXE=<redship> -DSCRATCH_DIR=<dir> -P CheckRowHome.cmake
cmake_policy(SET CMP0057 NEW)
include("${CMAKE_CURRENT_LIST_DIR}/RowHomeSharedNames.cmake")

foreach(_required REDSHIP_EXE SCRATCH_DIR)
    if(NOT DEFINED ${_required})
        message(FATAL_ERROR "CheckRowHome: -D${_required}=<value> is required")
    endif()
endforeach()
if(NOT EXISTS "${REDSHIP_EXE}")
    message(FATAL_ERROR "CheckRowHome: redship binary not found: ${REDSHIP_EXE}")
endif()

set(_root "${SCRATCH_DIR}")
file(REMOVE_RECURSE "${_root}")
set(_F "${_root}/from")
set(_N "${_root}/noseed")
set(_S "${_root}/badseed")
set(_G "${_root}/noexe")
set(_R "${_root}/reports")
foreach(_d "${_F}" "${_F}/sub" "${_N}" "${_S}" "${_G}" "${_R}")
    file(MAKE_DIRECTORY "${_d}")
endforeach()
get_filename_component(_exe_name "${REDSHIP_EXE}" NAME)
# A hard link where the file system allows one (the golden rows' sandbox does the
# same): the probe's executable must live in FROM for the exe-directory rule.
foreach(_d "${_F}" "${_N}" "${_S}")
    file(CREATE_LINK "${REDSHIP_EXE}" "${_d}/${_exe_name}" RESULT _link COPY_ON_ERROR)
    if(NOT _link STREQUAL "0" OR NOT EXISTS "${_d}/${_exe_name}")
        message(FATAL_ERROR "CheckRowHome: could not place the binary in ${_d}: ${_link}")
    endif()
endforeach()
set(_seed_bytes "{\"rsbsRowHomeIsolation\": \"seed bytes, #1692\"}\n")
file(WRITE "${_F}/shipofharkinian.json" "${_seed_bytes}")
file(MAKE_DIRECTORY "${_S}/shipofharkinian.json")
# Placeholder archives beside the binary, where the real ones are staged: leg (a)'s
# "no archive copied into the home" check has something a faulty home could copy.
file(WRITE "${_F}/oot.o2r" "row-home placeholder archive, #1692\n")
file(WRITE "${_F}/mm.o2r" "row-home placeholder archive, #1692\n")

file(GLOB _F_before LIST_DIRECTORIES true RELATIVE "${_F}" "${_F}/*")
list(SORT _F_before)

set(_failures "")
# A function, not a macro: a macro would paste the probe's output (Windows paths,
# backslashes) into its body and re-parse it.
function(_rh_fail leg text)
    list(APPEND _failures "(${leg}) ${text}")
    set(_failures "${_failures}" PARENT_SCOPE)
endfunction()

# Runs `<exe> --test <dispatch>` in <cwd> with the home variables of the caller's
# environment removed and the given NAME=VALUE pairs set.
function(_rh_run cwd exe dispatch out_rc out_text)
    execute_process(
        COMMAND "${CMAKE_COMMAND}" -E env --unset=RSBS_TEST_HOME --unset=RSBS_TEST_HOME_FROM --unset=SHIP_HOME
                --unset=RSBS_ROW_OUTPUT_DIR --unset=RSBS_ROW_HOME_REPORT --unset=RSBS_ROW_HOME_BARRIER
                ${ARGN} "${exe}" --test "${dispatch}"
        WORKING_DIRECTORY "${cwd}"
        RESULT_VARIABLE _rc
        OUTPUT_VARIABLE _out
        ERROR_VARIABLE _err
        TIMEOUT 60)
    set(${out_rc} "${_rc}" PARENT_SCOPE)
    set(${out_text} "${_out}${_err}" PARENT_SCOPE)
endfunction()

# The value of `key=` in a probe report, or "" when absent.
function(_rh_report_value report key out_var)
    set(_value "")
    if(EXISTS "${report}")
        file(STRINGS "${report}" _lines)
        foreach(_line IN LISTS _lines)
            if(_line MATCHES "^${key}=(.*)$")
                set(_value "${CMAKE_MATCH_1}")
                break()
            endif()
        endforeach()
    endif()
    set(${out_var} "${_value}" PARENT_SCOPE)
endfunction()

# The files the probe writes, relative to its app directory (test_row_home.c kRhpWrites).
set(_probe_files
    Randomizer/row-home-probe.json randomizer-mm/row-home-probe.json logs/row-home-probe.log imgui.ini
    Save/row-home-probe.sav saves/row-home-probe.sav)

# Checks that <home> holds every probe file, each naming probe <id>.
macro(_rh_expect_probe_files leg home id)
    foreach(_rel IN LISTS _probe_files)
        if(NOT EXISTS "${home}/${_rel}")
            _rh_fail(${leg} "${home}/${_rel} was not written in the home")
        else()
            file(READ "${home}/${_rel}" _body)
            if(NOT _body STREQUAL "row-home-probe id=${id}\n")
                _rh_fail(${leg} "${home}/${_rel} holds '${_body}', not probe ${id}'s line (another probe wrote it)")
            endif()
        endif()
    endforeach()
endmacro()

macro(_rh_expect_entered leg report)
    _rh_report_value("${report}" entered _entered)
    _rh_report_value("${report}" app_dir_is_home _app_home)
    if(NOT _entered STREQUAL "1")
        _rh_fail(${leg} "the probe's working directory is not its home (entered='${_entered}', report ${report})")
    endif()
    if(NOT _app_home STREQUAL "1")
        _rh_fail(${leg} "the probe's app directory is not its home (app_dir_is_home='${_app_home}'): a NON_PORTABLE "
                        "build, or SHIP_HOME pointing elsewhere")
    endif()
endmacro()

# ---- (a) ---------------------------------------------------------------------
set(_H1 "${_F}/rsbs-rows/H1")
# Linux and macOS: a SHIP_HOME exported by the caller, pointing elsewhere (F/sub),
# must be overridden by the home (TestHome_EnterIfAsked step 6), or the app
# directory is F/sub and app_dir_is_home turns 0. Windows never reads SHIP_HOME.
set(_a_ship_home "")
if(NOT CMAKE_HOST_WIN32)
    set(_a_ship_home "SHIP_HOME=${_F}/sub")
endif()
_rh_run("${_F}" "${_F}/${_exe_name}" row-home-probe _rc _text
    "RSBS_TEST_HOME=${_H1}" "RSBS_TEST_HOME_FROM=${_F}" "RSBS_ROW_HOME_REPORT=${_R}/a.txt" "RSBS_ROW_HOME_ID=1"
    ${_a_ship_home})
if(NOT _rc STREQUAL "0")
    _rh_fail(a "exited ${_rc}:\n${_text}")
endif()
if(NOT _text MATCHES "\\[HOME\\] [^\n]* config=seeded")
    _rh_fail(a "no `[HOME] <home> config=seeded` line:\n${_text}")
endif()
_rh_expect_entered(a "${_R}/a.txt")
if(NOT EXISTS "${_H1}/shipofharkinian.json")
    _rh_fail(a "the home was not seeded with the staged config")
else()
    file(READ "${_H1}/shipofharkinian.json" _seeded)
    if(NOT _seeded STREQUAL _seed_bytes)
        _rh_fail(a "the home's config is not the seed's bytes: '${_seeded}'")
    endif()
endif()
_rh_expect_probe_files(a "${_H1}" 1)
file(GLOB_RECURSE _archives "${_H1}/*.o2r" "${_H1}/*.otr")
if(_archives)
    _rh_fail(a "archives were copied into the home: ${_archives}")
endif()

# ---- (b) ---------------------------------------------------------------------
set(_H4 "${_N}/rsbs-rows/H4")
_rh_run("${_N}" "${_N}/${_exe_name}" row-home-probe _rc _text
    "RSBS_TEST_HOME=${_H4}" "RSBS_TEST_HOME_FROM=${_N}" "RSBS_ROW_HOME_REPORT=${_R}/b.txt" "RSBS_ROW_HOME_ID=4")
if(NOT _rc STREQUAL "0")
    _rh_fail(b "exited ${_rc} with no config to seed:\n${_text}")
endif()
if(NOT _text MATCHES "config=none")
    _rh_fail(b "no `config=none` on the [HOME] line:\n${_text}")
endif()
_rh_expect_entered(b "${_R}/b.txt")
if(EXISTS "${_H4}/shipofharkinian.json")
    _rh_fail(b "a config appeared in the home with nothing to seed it from")
endif()

# ---- (c) ---------------------------------------------------------------------
set(_H5 "${_F}/rsbs-rows/H5")
_rh_run("${_F}/sub" "${_F}/${_exe_name}" row-home-probe _rc _text
    "RSBS_TEST_HOME=${_H5}" "RSBS_TEST_HOME_FROM=${_F}" "RSBS_ROW_HOME_REPORT=${_R}/c.txt" "RSBS_ROW_HOME_ID=5")
if(NOT _rc STREQUAL "0")
    _rh_fail(c "exited ${_rc}:\n${_text}")
endif()
if(NOT _text MATCHES "\\[HOME\\] kept ")
    _rh_fail(c "started outside RSBS_TEST_HOME_FROM but printed no `[HOME] kept` line:\n${_text}")
endif()
_rh_report_value("${_R}/c.txt" entered _entered_c)
if(NOT _entered_c STREQUAL "0" OR EXISTS "${_H5}")
    _rh_fail(c "the home was entered or created although the process started outside RSBS_TEST_HOME_FROM")
endif()

# ---- (d) ---------------------------------------------------------------------
_rh_run("${_F}" "${_F}/${_exe_name}" row-home-probe-pair _rc _text
    "RSBS_ROW_HOME_PAIR_DIR=${_R}/pair" "RSBS_ROW_HOME_PAIR_FROM=${_F}")
if(NOT _rc STREQUAL "0")
    _rh_fail(d "the pair exited ${_rc}:\n${_text}")
endif()
foreach(_n 2 3)
    set(_home "${_F}/rsbs-rows/H${_n}")
    set(_report "${_R}/pair/R${_n}.txt")
    _rh_expect_entered(d "${_report}")
    _rh_report_value("${_report}" barrier _barrier)
    if(NOT _barrier STREQUAL "ok")
        _rh_fail(d "probe ${_n} did not meet the other at the barrier (barrier='${_barrier}'), so the writes did not "
                   "overlap")
    endif()
    _rh_expect_probe_files(d "${_home}" ${_n})
    if(EXISTS "${_R}/pair/child${_n}.log")
        file(READ "${_R}/pair/child${_n}.log" _log)
        string(FIND "${_log}" "[HOME] " _at_home)
        string(FIND "${_log}" "H${_n}" _at_name)
        if(_at_home EQUAL -1 OR _at_name EQUAL -1)
            _rh_fail(d "child ${_n}'s log has no [HOME] line naming H${_n}:\n${_log}")
        endif()
    else()
        _rh_fail(d "child ${_n} left no log")
    endif()
endforeach()

# ---- (e) ---------------------------------------------------------------------
if(CMAKE_HOST_APPLE)
    message(STATUS "CheckRowHome (e): skipped on macOS, where the exe-directory guard is off (test_home.h)")
else()
    _rh_run("${_G}" "${_F}/${_exe_name}" row-home-probe _rc _text
        "RSBS_TEST_HOME=${_G}/rsbs-rows/H7" "RSBS_TEST_HOME_FROM=${_G}" "RSBS_ROW_HOME_REPORT=${_R}/e.txt"
        "RSBS_ROW_HOME_ID=7")
    if(_rc STREQUAL "0")
        _rh_fail(e "the probe ran although its executable is not in RSBS_TEST_HOME_FROM:\n${_text}")
    endif()
    if(NOT _text MATCHES "\\[HOME\\] FAIL: exe dir")
        _rh_fail(e "no `[HOME] FAIL: exe dir` line:\n${_text}")
    endif()
    if(EXISTS "${_G}/rsbs-rows")
        _rh_fail(e "a home was created under ${_G} before the refusal")
    endif()
endif()

# ---- (f) ---------------------------------------------------------------------
_rh_run("${_S}" "${_S}/${_exe_name}" row-home-probe _rc _text
    "RSBS_TEST_HOME=${_S}/rsbs-rows/H6" "RSBS_TEST_HOME_FROM=${_S}" "RSBS_ROW_HOME_REPORT=${_R}/f.txt"
    "RSBS_ROW_HOME_ID=6")
if(_rc STREQUAL "0")
    _rh_fail(f "the probe ran although its staged config could not be copied:\n${_text}")
endif()
if(NOT _text MATCHES "\\[HOME\\] FAIL: could not seed")
    _rh_fail(f "no `[HOME] FAIL: could not seed` line:\n${_text}")
endif()

# ---- F, the shared starting folder, after every leg that started there ---------
file(GLOB _F_after LIST_DIRECTORIES true RELATIVE "${_F}" "${_F}/*")
list(SORT _F_after)
set(_F_expected ${_F_before} rsbs-rows)
list(SORT _F_expected)
if(NOT _F_after STREQUAL _F_expected)
    _rh_fail(F "the starting folder's listing changed: before '${_F_before}', after '${_F_after}' (only rsbs-rows may "
               "appear)")
endif()
file(READ "${_F}/shipofharkinian.json" _F_config)
if(NOT _F_config STREQUAL _seed_bytes)
    _rh_fail(F "the starting folder's config was rewritten: '${_F_config}'")
endif()
foreach(_name IN LISTS RSBS_ROW_HOME_SHARED_NAMES)
    if(NOT _name STREQUAL "shipofharkinian.json" AND EXISTS "${_F}/${_name}")
        _rh_fail(F "${_F}/${_name} appeared in the starting folder")
    endif()
endforeach()

if(_failures)
    list(LENGTH _failures _nf)
    string(REPLACE ";" "\n  - " _pretty "${_failures}")
    message(FATAL_ERROR "CheckRowHome: ${_nf} failure(s):\n  - ${_pretty}\nScratch kept in ${_root}.")
endif()
file(REMOVE_RECURSE "${_root}")
message(STATUS "CheckRowHome: every leg passed: a row in a home works and writes only there, alone and in a pair")
