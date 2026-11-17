/**
 * @file test_home.h
 * @brief A private working directory for each CTest row that asks for one (#1692)
 *
 * Every row used to run in the build directory, which on Windows IS the app
 * directory (libultraship's Context::GetAppDirectoryPath returns "." there and
 * reads SHIP_HOME only on Linux and macOS). So every row shared one
 * shipofharkinian.json, imgui.ini, logs/, Randomizer/, randomizer-mm/ and Save/,
 * and two rows could not run at the same time (`ctest -j`).
 *
 * redship_add_test (CMake/RedshipTests.cmake) gives each rando-label row two
 * environment entries: RSBS_TEST_HOME (`<build>/rsbs-rows/<row>`) and
 * RSBS_TEST_HOME_FROM (`<build>`). TestHome_EnterIfAsked, called first in
 * main.cpp's `--test` branch, then makes that home the process's working
 * directory and so its app directory. The archives are still found beside the
 * executable (LocateFileAcrossAppDirs tries the app directory, then the
 * executable's own directory), so nothing is copied but the staged config.
 */

#ifndef RSBS_COMMON_TEST_HOME_H
#define RSBS_COMMON_TEST_HOME_H

#include <string>

/**
 * Enters the row's private home when RSBS_TEST_HOME is set. In order:
 *   1. RSBS_TEST_HOME unset or empty: does nothing (a hand run is unchanged).
 *   2. RSBS_TEST_HOME_FROM unset or empty: `[HOME] FAIL`, non-zero.
 *   3. The working directory is not RSBS_TEST_HOME_FROM: `[HOME] kept`, 0. A cmake
 *      -P row that starts its child in a directory of its own on purpose (the
 *      golden rows' archive-free sandbox, CheckHandRunDigest) keeps it.
 *   3b. (not macOS) This executable's directory is not RSBS_TEST_HOME_FROM:
 *      `[HOME] FAIL: exe dir`, non-zero. A row in a home finds its archives only
 *      beside the executable.
 *   4. Creates the home and seeds it with FROM/shipofharkinian.json when FROM has
 *      one and the home does not; a failed copy is a failure (a row without the
 *      staged config would fall back to the DXGI default on Windows, which takes
 *      focus).
 *   5. Makes the home the working directory.
 *   6. Sets SHIP_HOME to the home's absolute path, so a SHIP_HOME exported by a
 *      developer on Linux cannot point the row back at a shared directory.
 *   7. Prints `[HOME] <home> config=<seeded|kept|none>`.
 *
 * @param mode the command-line mode, for the printed lines ("--test")
 * @return 0 to continue, non-zero to stop the process with that code
 */
int TestHome_EnterIfAsked(const char* mode);

/**
 * Where a row writes an output file that CI collects by name from the build
 * directory: `<RSBS_ROW_OUTPUT_DIR>/<name>` when that variable is set (a row in a
 * home), otherwise @p name itself, relative to the working directory, as before.
 */
std::string TestHome_RowOutputPath(const char* name);

#endif // RSBS_COMMON_TEST_HOME_H
