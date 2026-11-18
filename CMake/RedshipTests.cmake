# CMake/RedshipTests.cmake
# Test registration helpers for the single-executable build.
#
# Why this module exists (#376): registering one test used to take three edits,
# one of which was a SHARED LINE. `add_test(NAME ...)` appends cleanly, but the
# test's name also had to be appended to a single five-line
# `set_tests_properties(<30 names> PROPERTIES TIMEOUT ... LABELS "redship")`
# block. Every parallel lane that added a test rewrote those same lines, so
# every lane after the first conflicted — four Wave 1 lanes and both Wave 2
# lanes collided there over 2026-07-19/20.
#
# The conflict is worse than a rebase: GitHub builds `pull_request` checks
# against `refs/pull/N/merge`, which it cannot create while a PR conflicts, so a
# stale PR gets ZERO check runs rather than failing ones — indistinguishable
# from an Actions outage until someone looks at the mergeability flag.
#
# `redship_add_test()` folds the properties into the registration, so a test is
# one appended line and the shared name list is gone.

include_guard(GLOBAL)

# ============================================================================
# redship_add_test(NAME <name> COMMAND <argv...>
#                  [LABEL <label>...] [TIMEOUT <seconds>] [ENVIRONMENT <var=val>...]
#                  [HOME AUTO|ON|OFF] [OUTPUTS <group>=[?]<path>...])
#
# Registers a CTest row and its properties together.
#
# LABEL defaults to "redship" (the ROM-free tier hosted CI runs) and TIMEOUT
# defaults to ${REDSHIP_TEST_TIMEOUT}, so a plain unit test needs neither.
#
# HOME (#1692) gives the row a private working directory, <build>/rsbs-rows/<NAME>,
# so it can run beside other rows under `ctest -j`: the row gets RSBS_TEST_HOME
# and RSBS_TEST_HOME_FROM, and src/common/test_home.cpp enters that home first in
# every `--test` process the row starts from the build directory (a -P script's
# children inherit both). AUTO, the default, means ON for a row labelled `rando`
# and OFF otherwise. A home row also gets RSBS_ROW_OUTPUT_DIR=<build> (where it
# writes the digests CI collects by name) and requires the rsbs_row_homes fixture
# (RowHomesReset, registered by redship_finalize_tests), which empties
# <build>/rsbs-rows before any home row of a ctest run starts. Under a
# multi-config generator the executable is not in <build>, so a home would lose
# the archives staged there: AUTO is OFF there and ON is refused.
#
# OUTPUTS (#1692) declares files the row writes into the build directory that CI
# or the gate collects by name: `crossos=` (compared across the Linux and Windows
# legs and uploaded), `compare=` (compared between a serial and a parallel run,
# with every crossos file) and `package=` (the seed package upload). Paths are
# exact and relative to the build directory; a leading `?` marks a file that only
# some environments write. CMake/CheckRowOutputs.cmake reads them back and fails on
# a declared file that is missing or empty.
# ============================================================================
function(redship_add_test)
    cmake_parse_arguments(PARSE_ARGV 0 RSBS_T "" "NAME;TIMEOUT;HOME" "COMMAND;LABEL;ENVIRONMENT;OUTPUTS")

    if(NOT RSBS_T_NAME)
        message(FATAL_ERROR "redship_add_test: NAME is required")
    endif()
    if(NOT RSBS_T_COMMAND)
        message(FATAL_ERROR "redship_add_test(${RSBS_T_NAME}): COMMAND is required")
    endif()
    # A typo'd keyword would otherwise be silently swallowed and the row would
    # quietly get default properties — the exact failure class this module exists
    # to remove.
    if(RSBS_T_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR
            "redship_add_test(${RSBS_T_NAME}): unrecognized argument(s): ${RSBS_T_UNPARSED_ARGUMENTS}")
    endif()

    if(NOT RSBS_T_LABEL)
        set(RSBS_T_LABEL "redship")
    endif()
    if(NOT RSBS_T_TIMEOUT)
        if(NOT DEFINED REDSHIP_TEST_TIMEOUT)
            message(FATAL_ERROR
                "redship_add_test(${RSBS_T_NAME}): no TIMEOUT given and REDSHIP_TEST_TIMEOUT is not set yet")
        endif()
        set(RSBS_T_TIMEOUT ${REDSHIP_TEST_TIMEOUT})
    endif()

    # A caller writing ENVIRONMENT "A=1;B=2" hands cmake_parse_arguments ONE
    # argument, which it stores with the semicolons escaped ("A=1\;B=2") to keep
    # it a single list element. Forwarded as-is, CTest receives one malformed
    # variable named A whose value is "1;B=2" instead of two variables — a
    # silent environment regression in a tier that only runs under xvfb. Undo the
    # escaping so both spellings (one ;-joined string, or several separate
    # arguments) produce the same list.
    string(REPLACE "\\;" ";" _labels "${RSBS_T_LABEL}")
    string(REPLACE "\\;" ";" _environment "${RSBS_T_ENVIRONMENT}")

    # Every row, whatever else it sets: a test window is shown WITHOUT activation,
    # so a tier never takes the keyboard from the person at the workstation (lane
    # F1, #310). SDL2 reads a hint from the environment variable of the same name.
    # rsbs/src/main.cpp already sets it in code for every --test and
    # --integration-test process; this carries it to a binary older than that
    # change and to the redship children a -P script row (the golden rows,
    # CheckHandRunDigest) launches. PREPENDED, so a row's own ENVIRONMENT is kept
    # and, should a row ever name this variable itself, its later entry wins.
    list(PREPEND _environment "SDL_WINDOW_NO_ACTIVATION_WHEN_SHOWN=1")
    # Same two layers for the operator's game controller (#1152): main.cpp keeps
    # SDL's joystick background events off in code over libultraship's "1"; this
    # carries "0" to a binary older than that change and to -P script children.
    list(PREPEND _environment "SDL_JOYSTICK_ALLOW_BACKGROUND_EVENTS=0")

    # Every row is also SILENT (S0), whatever else it sets: a test process never
    # plays game audio through the speakers of the person at the workstation. Until
    # now only the ~49 rows that spelled SDL_AUDIODRIVER=dummy themselves were; the
    # ROM-mounted integration rows and the redship tier's boot/switch rows relied on
    # the staged config's Window.AudioBackend being "null". Same two-layer shape as
    # the line above: main.cpp pins the driver in code (TestWindowFocus_ArmSilentAudio,
    # opt out with RSBS_TEST_AUDIO=1 on a direct run); this carries it to the children
    # a -P script row launches and to a binary older than that change. The rows' own
    # SDL_AUDIODRIVER=dummy entries are redundant now and are left as they are
    # (removing 49 of them would only widen the diff); a row that names another
    # driver still wins, since this entry is PREPENDED. CTest always applies it, so
    # RSBS_TEST_AUDIO=1 un-silences direct `redship --test` runs, not ctest.
    list(PREPEND _environment "SDL_AUDIODRIVER=dummy")

    # The row's private home (#1692; see the header). Resolved here, once, so every
    # row of a label is treated alike and a row cannot opt in by half.
    # NOT DEFINED, not NOT: `if(NOT RSBS_T_HOME)` is true for HOME OFF as well ("OFF"
    # is a false constant). That turned the reset row's HOME OFF into AUTO, gave it
    # a home and the very fixture it sets up, and CTest refused the whole tier with
    # "a cycle exists in the test dependency graph".
    if(NOT DEFINED RSBS_T_HOME)
        set(RSBS_T_HOME "AUTO")
    endif()
    if(NOT RSBS_T_HOME MATCHES "^(AUTO|ON|OFF)$")
        message(FATAL_ERROR "redship_add_test(${RSBS_T_NAME}): HOME must be AUTO, ON or OFF, not '${RSBS_T_HOME}'")
    endif()
    get_property(_multi_config GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
    set(_home_on FALSE)
    if(RSBS_T_HOME STREQUAL "ON")
        set(_home_on TRUE)
    elseif(RSBS_T_HOME STREQUAL "AUTO" AND "rando" IN_LIST _labels)
        set(_home_on TRUE)
    endif()
    if(_home_on AND _multi_config)
        if(RSBS_T_HOME STREQUAL "ON")
            message(FATAL_ERROR
                "redship_add_test(${RSBS_T_NAME}): HOME ON under a multi-config generator. The executable is in "
                "<build>/<Config>/, not <build>, so a row in a home would not find the archives staged in <build>.")
        endif()
        set(_home_on FALSE)
        set_property(GLOBAL PROPERTY REDSHIP_TEST_HOMES_DISABLED TRUE)
    endif()
    set(_home "")
    if(_home_on)
        set(_home "${CMAKE_BINARY_DIR}/rsbs-rows/${RSBS_T_NAME}")
        # NTFS and APFS fold case: two rows whose names differ only in case would
        # share one home there.
        string(TOLOWER "${RSBS_T_NAME}" _home_key)
        get_property(_home_keys GLOBAL PROPERTY REDSHIP_TEST_HOME_KEYS)
        if(_home_key IN_LIST _home_keys)
            message(FATAL_ERROR
                "redship_add_test(${RSBS_T_NAME}): another row's home differs from rsbs-rows/${RSBS_T_NAME} only in "
                "case, so on a case-insensitive file system the two rows would share it. Rename one of them.")
        endif()
        set_property(GLOBAL APPEND PROPERTY REDSHIP_TEST_HOME_KEYS "${_home_key}")
    endif()
    # A row's own ENVIRONMENT never names the home's variables: a home without the
    # fixture, or a home row pointed elsewhere, would defeat the checks that read
    # them back (TestRowHomeCoverage).
    foreach(_own IN LISTS _environment)
        if(_own MATCHES "^(RSBS_TEST_HOME|RSBS_TEST_HOME_FROM|RSBS_ROW_OUTPUT_DIR)=")
            message(FATAL_ERROR
                "redship_add_test(${RSBS_T_NAME}): ENVIRONMENT names ${CMAKE_MATCH_1}, which the HOME keyword owns")
        endif()
    endforeach()
    if(_home_on)
        # PREPENDED like the entries above.
        list(PREPEND _environment
            "RSBS_TEST_HOME=${_home}"
            "RSBS_TEST_HOME_FROM=${CMAKE_BINARY_DIR}"
            "RSBS_ROW_OUTPUT_DIR=${CMAKE_BINARY_DIR}")
    endif()

    # Declared outputs (#1692; see the header). Exact paths only: a glob here would
    # let a missing file match nothing and pass.
    set(_output_lines "")
    string(REPLACE ";" "," _output_labels "${_labels}")
    foreach(_out IN LISTS RSBS_T_OUTPUTS)
        if(NOT _out MATCHES "^(crossos|compare|package)=(\\??)([^?*[]+)$")
            message(FATAL_ERROR
                "redship_add_test(${RSBS_T_NAME}): OUTPUTS entry '${_out}' is not <crossos|compare|package>=[?]<path> "
                "with an exact path (no '*', no '?' after the first character, no '[')")
        endif()
        set(_out_group "${CMAKE_MATCH_1}")
        set(_out_opt "${CMAKE_MATCH_2}")
        set(_out_path "${CMAKE_MATCH_3}")
        if(IS_ABSOLUTE "${_out_path}" OR _out_path MATCHES "(^|/)\\.\\.(/|$)" OR _out_path MATCHES "^rsbs-rows(/|$)")
            message(FATAL_ERROR
                "redship_add_test(${RSBS_T_NAME}): OUTPUTS path '${_out_path}' must be relative to the build directory "
                "and outside rsbs-rows/ (the homes are emptied at the start of every run)")
        endif()
        string(TOLOWER "${_out_path}" _out_key)
        get_property(_out_owners GLOBAL PROPERTY REDSHIP_TEST_OUTPUT_OWNERS)
        foreach(_owner IN LISTS _out_owners)
            string(FIND "${_owner}" "|" _bar)
            string(SUBSTRING "${_owner}" 0 ${_bar} _owner_row)
            math(EXPR _bar "${_bar} + 1")
            string(SUBSTRING "${_owner}" ${_bar} -1 _owner_key)
            if(_owner_key STREQUAL _out_key AND NOT _owner_row STREQUAL RSBS_T_NAME)
                message(FATAL_ERROR
                    "redship_add_test(${RSBS_T_NAME}): OUTPUTS path '${_out_path}' is already declared by row "
                    "${_owner_row}; two rows writing one collected file is the race #1692 removes")
            endif()
        endforeach()
        set_property(GLOBAL APPEND PROPERTY REDSHIP_TEST_OUTPUT_OWNERS "${RSBS_T_NAME}|${_out_key}")
        list(APPEND _output_lines "output|${RSBS_T_NAME}|${_output_labels}|${_out_group}|${_out_opt}${_out_path}")
    endforeach()

    add_test(NAME ${RSBS_T_NAME} COMMAND ${RSBS_T_COMMAND})
    set_tests_properties(${RSBS_T_NAME} PROPERTIES
        TIMEOUT "${RSBS_T_TIMEOUT}"
        LABELS "${_labels}"
        ENVIRONMENT "${_environment}"
    )
    if(_home_on)
        set_tests_properties(${RSBS_T_NAME} PROPERTIES FIXTURES_REQUIRED rsbs_row_homes)
        set_property(GLOBAL APPEND PROPERTY REDSHIP_TEST_MANIFEST "home|${RSBS_T_NAME}|rsbs-rows/${RSBS_T_NAME}")
    endif()
    foreach(_line IN LISTS _output_lines)
        set_property(GLOBAL APPEND PROPERTY REDSHIP_TEST_MANIFEST "${_line}")
    endforeach()

    # Record which dispatch-table entry this row drives, for the completeness
    # guard below. The CTest row name is NOT the dispatch name: RandoGen,
    # RandoGenSongsDungeonRewards and RandoGenSongsAnywhere are three rows over
    # the one "rando-gen" entry, differing only by ENVIRONMENT.
    set(_kind "meta")
    set(_dispatch "")
    set(_take_next FALSE)
    foreach(_arg IN LISTS RSBS_T_COMMAND)
        if(_take_next)
            set(_dispatch "${_arg}")
            set(_take_next FALSE)
        elseif(_arg STREQUAL "--test")
            set(_kind "unit")
            set(_take_next TRUE)
        elseif(_arg STREQUAL "--integration-test")
            set(_kind "integration")
            set(_take_next TRUE)
        endif()
    endforeach()

    # Manifest fields are '|'-separated and one-per-line, so flatten any
    # multi-label value rather than letting its ';' split the line.
    string(REPLACE ";" "," _manifest_labels "${_labels}")
    set_property(GLOBAL APPEND PROPERTY REDSHIP_TEST_MANIFEST
        "register|${_manifest_labels}|${RSBS_T_NAME}|${_kind}|${_dispatch}")
endfunction()

# ============================================================================
# redship_test_exempt(<dispatch-name> <reason>)
#
# Declares a dispatch-table entry that deliberately has NO dedicated CTest row.
# The guard hard-fails on a stale exemption (name gone from the table, or since
# registered), so this list cannot rot into a rubber stamp — same rule the
# clang-format path allowlist uses.
# ============================================================================
function(redship_test_exempt name reason)
    set_property(GLOBAL APPEND PROPERTY REDSHIP_TEST_MANIFEST "exempt|${name}|${reason}")
endfunction()

# ============================================================================
# redship_test_not_generating(<dispatch-name> <reason>)
#
# Declares a dispatch-table entry driven by a "rando"-label row, or declared
# row-less with redship_test_exempt(), that runs NO real generation, so
# `redship --test all` may run it (#1465). Every other such entry must be on the
# `--test all` skip list in test_runner.cpp: a
# generation leaves its world alive for the rest of the process
# (Rando::Settings::AssignContext keeps a strong reference), and a later row in
# AllTests then reads a found state instead of an empty world (#1455).
# TestRegistrationComplete fails on such an entry that is in neither place, and
# on a declaration that is stale (no rando-label row drives the name and it is
# not a row-less entry) or
# contradicted (the name is on the skip list).
# ============================================================================
function(redship_test_not_generating name reason)
    set_property(GLOBAL APPEND PROPERTY REDSHIP_TEST_MANIFEST "nogen|${name}|${reason}")
endfunction()

# ============================================================================
# redship_test_cost(<row> <seconds>)
#
# Declares a row's expected run time (#1692), applied as its CTest COST property
# by redship_finalize_tests when the configure option RSBS_TEST_COST is ON (CI
# turns it on; it is OFF by default). Under `ctest -j` CTest starts higher-cost
# rows first, so the slowest rows do not form a one-row-at-a-time tail. Behind an
# option because CTest may order a SERIAL run by COST as well (not verified here),
# and a local serial run keeps its registration order unless asked. A declaration
# naming no registered row fails at configure.
# ============================================================================
option(RSBS_TEST_COST "Apply redship_test_cost() declarations as CTest COST properties (#1692)" OFF)

function(redship_test_cost name seconds)
    if(NOT seconds MATCHES "^[0-9]+$")
        message(FATAL_ERROR "redship_test_cost(${name}): '${seconds}' is not a whole number of seconds")
    endif()
    set_property(GLOBAL APPEND PROPERTY REDSHIP_TEST_COSTS "${name}|${seconds}")
endfunction()

# ============================================================================
# redship_check_test_sources()
#
# Every src/common/tests/*.c must be #included by test_runner.cpp.
#
# These files are deliberately NOT separate translation units: each is pulled
# into test_runner.cpp either inside an `extern "C"` block or at file scope,
# and which one is load-bearing (test_roundtrip_integrity.c calls the C++-linkage
# entrance API and stays at file scope; before #665 its reset call shared a
# spelling with OoT's C-linkage randomizer Entrance_Init). Compiling them as
# their own TUs as well would define every symbol twice.
#
# So this globs to DETECT rather than to build: CONFIGURE_DEPENDS re-runs
# configure when a file is added to the directory, and a file that is never
# #included is a test that compiles into nothing and can never run.
# ============================================================================
function(redship_check_test_sources)
    file(GLOB _test_sources CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/src/common/tests/*.c")

    # A glob that matches nothing would make this check pass vacuously, which is
    # the failure mode it exists to prevent.
    list(LENGTH _test_sources _count)
    if(_count EQUAL 0)
        message(FATAL_ERROR
            "redship_check_test_sources: src/common/tests/*.c matched no files. "
            "If the test sources moved, update this glob — do not leave it matching nothing.")
    endif()

    file(READ "${CMAKE_SOURCE_DIR}/src/common/test_runner.cpp" _runner)
    set(_orphans "")
    foreach(_src IN LISTS _test_sources)
        get_filename_component(_name "${_src}" NAME)
        string(FIND "${_runner}" "#include \"tests/${_name}\"" _pos)
        if(_pos EQUAL -1)
            list(APPEND _orphans "${_name}")
        endif()
    endforeach()

    if(_orphans)
        string(REPLACE ";" "\n  - " _pretty "${_orphans}")
        message(FATAL_ERROR
            "Test source(s) in src/common/tests/ are not #included by "
            "src/common/test_runner.cpp:\n  - ${_pretty}\n"
            "They compile into nothing and can never run. Add "
            "#include \"tests/<file>\" to test_runner.cpp (inside an extern \"C\" block "
            "if the test only touches C-linkage symbols; at file scope if it drives "
            "C++-linkage APIs).")
    endif()

    message(STATUS "Test sources: ${_count} file(s) in src/common/tests/, all #included")
endfunction()

# ============================================================================
# redship_finalize_tests()
#
# Writes the manifest the completeness guard reads. Call AFTER every
# redship_add_test()/redship_test_exempt()/redship_test_not_generating() call.
# ============================================================================
function(redship_finalize_tests manifest_path)
    # #1692: the homes' reset row. Registered here, after every row, so no lane's
    # list edit can move it; CTest runs a FIXTURES_SETUP row before every row that
    # requires the fixture (under -L and -R too) and finishes it before any of them
    # starts, so each ctest run starts with empty homes and a fresh copy of the
    # staged config. Labelled rando so `-L ^rando$` lists it; HOME OFF, it is the
    # one rando row with no home. Deleting rsbs-rows/ by hand is always safe.
    get_property(_home_keys GLOBAL PROPERTY REDSHIP_TEST_HOME_KEYS)
    if(_home_keys)
        redship_add_test(NAME RowHomesReset
            COMMAND ${CMAKE_COMMAND} -E rm -rf "${CMAKE_BINARY_DIR}/rsbs-rows"
            LABEL rando
            HOME OFF
            TIMEOUT 120)
        set_tests_properties(RowHomesReset PROPERTIES FIXTURES_SETUP rsbs_row_homes)
    endif()
    get_property(_homes_disabled GLOBAL PROPERTY REDSHIP_TEST_HOMES_DISABLED)
    if(_homes_disabled)
        set_property(GLOBAL APPEND PROPERTY REDSHIP_TEST_MANIFEST "homes|disabled|multi-config")
    endif()

    # #1692: COST, only when asked (see redship_test_cost).
    get_property(_costs GLOBAL PROPERTY REDSHIP_TEST_COSTS)
    foreach(_cost IN LISTS _costs)
        string(REPLACE "|" ";" _cost_fields "${_cost}")
        list(GET _cost_fields 0 _cost_row)
        list(GET _cost_fields 1 _cost_seconds)
        if(NOT TEST ${_cost_row})
            message(FATAL_ERROR "redship_test_cost(${_cost_row}): no row of that name is registered")
        endif()
        if(RSBS_TEST_COST)
            set_tests_properties(${_cost_row} PROPERTIES COST ${_cost_seconds})
        endif()
    endforeach()
    if(_costs)
        if(RSBS_TEST_COST)
            set_property(GLOBAL APPEND PROPERTY REDSHIP_TEST_MANIFEST "cost|on")
        else()
            set_property(GLOBAL APPEND PROPERTY REDSHIP_TEST_MANIFEST "cost|off")
        endif()
    endif()

    get_property(_entries GLOBAL PROPERTY REDSHIP_TEST_MANIFEST)
    if(NOT _entries)
        message(FATAL_ERROR "redship_finalize_tests: no tests were registered")
    endif()
    string(REPLACE ";" "\n" _body "${_entries}")
    file(WRITE "${manifest_path}"
        "# Generated by redship_finalize_tests() — do not edit.\n"
        "# register|<labels>|<ctest-name>|<kind>|<dispatch-name>\n"
        "# exempt|<dispatch-name>|<reason>\n"
        "# nogen|<dispatch-name>|<reason>\n"
        "# home|<ctest-name>|<home relative to the build directory>\n"
        "# output|<ctest-name>|<labels>|<crossos|compare|package>|[?]<path relative to the build directory>\n"
        "# homes|disabled|<why>  (no row has a home in this tree)\n"
        "# cost|<on|off>  (whether redship_test_cost() declarations became COST properties)\n"
        "${_body}\n")
endfunction()
