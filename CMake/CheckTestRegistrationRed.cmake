# CMake/CheckTestRegistrationRed.cmake
#
# The red half of TestRegistrationComplete's `--test all` skip-list check (#1465):
#   cmake -DREDSHIP_EXE=<redship> -DMANIFEST=<manifest> -DSCRATCH_DIR=<dir>
#         -DGUARD=<CheckTestRegistration.cmake> -P CheckTestRegistrationRed.cmake
#
# Runs the guard four times over copies of the real manifest:
#   control  the copy unchanged                          -> must pass
#   leg 1    + a rando-label row over boot-oot           -> must fail, naming the row
#   leg 2    + redship_test_not_generating(rando-gen)    -> must fail as contradicted
#   leg 3    + redship_test_not_generating(boot-oot)     -> must fail as stale
# boot-oot is display-free, not on the skip list and driven by no rando-label row;
# rando-gen is the oldest skip (#337) and has a rando-label row. A failing leg must
# also print its own problem line, so a guard that fails for an unrelated reason
# does not pass this row.
cmake_policy(SET CMP0057 NEW)

foreach(_var REDSHIP_EXE MANIFEST SCRATCH_DIR GUARD)
    if(NOT DEFINED ${_var})
        message(FATAL_ERROR "CheckTestRegistrationRed: -D${_var}=<path> is required")
    endif()
endforeach()
foreach(_path "${REDSHIP_EXE}" "${MANIFEST}" "${GUARD}")
    if(NOT EXISTS "${_path}")
        message(FATAL_ERROR "CheckTestRegistrationRed: not found: ${_path}")
    endif()
endforeach()

file(READ "${MANIFEST}" _manifest)
file(MAKE_DIRECTORY "${SCRATCH_DIR}")

# Runs the guard over the real manifest plus `extra_line` (none when empty).
# expect_pass TRUE: the guard must exit 0. FALSE: it must exit non-zero and its
# output must contain `expect_text`.
function(_run_leg leg extra_line expect_pass expect_text)
    set(_copy "${SCRATCH_DIR}/manifest-${leg}.txt")
    file(WRITE "${_copy}" "${_manifest}")
    if(NOT extra_line STREQUAL "")
        file(APPEND "${_copy}" "${extra_line}\n")
    endif()
    execute_process(
        COMMAND "${CMAKE_COMMAND}" "-DREDSHIP_EXE=${REDSHIP_EXE}" "-DMANIFEST=${_copy}" -P "${GUARD}"
        OUTPUT_VARIABLE _out
        ERROR_VARIABLE _err
        RESULT_VARIABLE _rc
    )
    set(_all "${_out}${_err}")
    if(expect_pass)
        if(NOT _rc EQUAL 0)
            message(FATAL_ERROR "CheckTestRegistrationRed: ${leg}: the guard failed on the unchanged manifest "
                "(exit ${_rc}), so the red legs below prove nothing.\n${_all}")
        endif()
        message(STATUS "${leg}: guard passes on the real manifest")
        return()
    endif()
    if(_rc EQUAL 0)
        message(FATAL_ERROR "CheckTestRegistrationRed: ${leg}: the guard PASSED with '${extra_line}' added "
            "-- the --test all skip-list check is vacuous.\n${_all}")
    endif()
    # CMake may re-wrap a FATAL_ERROR paragraph, so compare with every run of
    # whitespace folded to one space.
    string(REGEX REPLACE "[ \t\r\n]+" " " _all_folded "${_all}")
    string(FIND "${_all_folded}" "${expect_text}" _at)
    if(_at EQUAL -1)
        message(FATAL_ERROR "CheckTestRegistrationRed: ${leg}: the guard failed, but without the expected "
            "line '${expect_text}'.\n${_all}")
    endif()
    message(STATUS "${leg}: guard refuses '${extra_line}' as expected")
endfunction()

_run_leg(control "" TRUE "")
_run_leg(leg1 "register|rando|SyntheticGenerationRow|unit|boot-oot" FALSE
    "rando-label CTest row 'SyntheticGenerationRow' runs '--test boot-oot'")
_run_leg(leg2 "nogen|rando-gen|synthetic" FALSE
    "contradicted declaration: redship_test_not_generating(rando-gen)")
_run_leg(leg3 "nogen|boot-oot|synthetic" FALSE
    "stale declaration: redship_test_not_generating(boot-oot)")

message(STATUS "CheckTestRegistrationRed: the --test all skip-list check fails by name on all three legs")
