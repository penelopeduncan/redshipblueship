# CMake/RowHomeSharedNames.cmake
#
# #1692: the names a row writes into its APP DIRECTORY, which before per-row homes
# was the build directory every row shared (on Windows libultraship's
# Context::GetAppDirectoryPath is "."; on Linux it is SHIP_HOME or "."). Included by
# CheckRowHome.cmake (a probe in a home must not create any of these where it
# started) and CheckSharedDirUntouched.cmake (a rando tier must not change any of
# them at the top of the build directory). One list, so the two cannot drift.
#
# Writers, at the libultraship / game commits this tree pins:
#   shipofharkinian.json   Ship::Context config (libultraship Context.cpp InitConfiguration)
#   imgui.ini, imgui_log.txt  Ship::Gui (libultraship Gui.cpp)
#   cvars.cfg              ConsoleVariable save (libultraship ConsoleVariable.cpp)
#   logs/                  Ship::Context InitLogging, a rotating sink
#   Randomizer/, randomizer/  SoH's spoiler writer and the seed package (spoiler_log.cpp,
#                          SeedPackageSingleExe.cpp); lower case for case-sensitive hosts
#   randomizer-mm/         2Ship's rando spoiler (Rando Spoiler/File.cpp)
#   Save/, saves/          SoH's and 2Ship's save managers, rsbs::SaveManager's default
#   presets/               the preset menus
#   rsbs_test_boot_home/   CreateHarnessStyleContext's SHIP_HOME (src/common/test_runner.cpp)
#   default.sav, oot_save.sav  os_eeprom.cpp and the flash stubs
#   timesplitdata.json, 2S2HTimeSplitData.json  both games' time splits
set(RSBS_ROW_HOME_SHARED_NAMES
    shipofharkinian.json
    imgui.ini
    imgui_log.txt
    cvars.cfg
    logs
    Randomizer
    randomizer
    randomizer-mm
    Save
    saves
    presets
    rsbs_test_boot_home
    default.sav
    oot_save.sav
    timesplitdata.json
    2S2HTimeSplitData.json
)
