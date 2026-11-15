/**
 * @file oot_gameplay_stats_test.cpp
 * ROM-free, display-free lock for two Ship of Harkinian gameplay-stats changes.
 * CTest label "redship", row OoTGameplayStats in CMake/SingleExecutable.cmake,
 * dispatch "oot-gameplay-stats" in src/common/test_runner.cpp.
 *
 * WHAT WAS BROKEN.
 *   1. SoH #7385: the stats section ("sohStats") never saved `gameComplete`. A
 *      finished file reloaded as unfinished, so with RTA timing on, the total
 *      time went back to counting from the first input after a quit and reload,
 *      long past the end.
 *   2. SoH #7404 (the timer order only): with RTA timing on and the game
 *      complete, GAMEPLAYSTAT_TOTAL_TIME read the Ganon defeat's timestamp
 *      before the completed triforce hunt's. A Ganon defeat taken before a hunt
 *      ends still records its timestamp (BossDefeatTimestamps.cpp records every
 *      boss; only `gameComplete` waits for the frozen combo goal), so a finished
 *      hunt showed the earlier Ganon time instead of the hunt's end.
 *
 * THE LEGS (the REAL SaveStats / LoadStatsVersion1 of
 * soh/Enhancements/gameplaystats.cpp, run in memory through SaveManager's test
 * hooks; gSaveContext is put back afterwards):
 *   1. a complete file's stats section carries "gameComplete": true, an
 *      unfinished one's carries false;
 *   2. the complete section loads back complete into a context that was not,
 *      and the unfinished one loads back not complete into one that was;
 *   3. a section without the field (a file saved before it) loads as not
 *      complete;
 *   4. RTA timing, complete, both a Ganon and a hunt timestamp: the total time
 *      is the hunt's; with no hunt timestamp it is Ganon's; with RTA timing off
 *      it is the frame timers'.
 */

#ifdef RSBS_SINGLE_EXECUTABLE

#include "SaveManager.h"
#include "Enhancements/gameplaystats.h"
#include "Enhancements/game-interactor/GameInteractor.h"

extern "C" {
#include <z64.h>
#include "variables.h"
}

#include <cstdio>
#include <cstring>
#include <memory>

// soh/Enhancements/gameplaystats.cpp (C++ linkage): the "sohStats" section's
// save and version-1 load functions, as InitStatTracker registers them.
void SaveStats(SaveContext* saveContext, int sectionID, bool fullSave);
void LoadStatsVersion1();

namespace {

int gFailures = 0;

void Expect(bool ok, const char* what) {
    if (!ok) {
        printf("[TEST] oot-gameplay-stats: FAIL: %s\n", what);
        gFailures++;
    }
}

nlohmann::json SaveSectionWith(bool gameComplete) {
    std::unique_ptr<SaveContext> ctx(new SaveContext);
    memset(ctx.get(), 0, sizeof(SaveContext));
    ctx->ship.stats.gameComplete = gameComplete;
    return SaveManager::Instance->TestSaveSectionToJson(SaveStats, ctx.get());
}

bool LoadSection(nlohmann::json section, bool startingValue) {
    gSaveContext.ship.stats.gameComplete = startingValue;
    SaveManager::Instance->TestLoadSectionFromJson(LoadStatsVersion1, section);
    return gSaveContext.ship.stats.gameComplete;
}

} // namespace

extern "C" int OoT_GameplayStats_RunHeadless(void) {
    printf("[TEST] oot-gameplay-stats: the stats section saves and loads gameComplete (SoH #7385), and a finished "
           "triforce hunt's time outranks Ganon's (SoH #7404)\n");
    gFailures = 0;

    // SaveManager's constructor registers hooks on GameInteractor::Instance,
    // which nothing creates in a ROM-free test process.
    if (GameInteractor::Instance == nullptr) {
        GameInteractor::Instance = new GameInteractor();
    }
    if (SaveManager::Instance == nullptr) {
        SaveManager::Instance = new SaveManager(); // kept alive, like every other *::Instance singleton
    }
    std::unique_ptr<SaveContext> saveBefore(new SaveContext);
    memcpy(saveBefore.get(), &gSaveContext, sizeof(SaveContext));

    // Legs 1-3: the section's field.
    const nlohmann::json complete = SaveSectionWith(true);
    const nlohmann::json unfinished = SaveSectionWith(false);
    Expect(complete.contains("gameComplete") && complete["gameComplete"] == true,
           "leg 1: a complete file's stats section does not carry \"gameComplete\": true");
    Expect(unfinished.contains("gameComplete") && unfinished["gameComplete"] == false,
           "leg 1: an unfinished file's stats section does not carry \"gameComplete\": false");
    Expect(LoadSection(complete, false), "leg 2: a complete section loads back as not complete");
    Expect(!LoadSection(unfinished, true), "leg 2: an unfinished section loads back as complete");
    nlohmann::json older = complete;
    older.erase("gameComplete");
    Expect(!LoadSection(older, true), "leg 3: a section without the field loads as complete");

    // Leg 4: the timer order.
    memset(&gSaveContext.ship.stats, 0, sizeof(gSaveContext.ship.stats));
    gSaveContext.ship.stats.rtaTiming = true;
    // Through a variable: test_combo_goal.c's completion-writer sweep counts a
    // literal `true` write in any OoT source, and this row is not a writer.
    const bool finished = true;
    gSaveContext.ship.stats.gameComplete = finished;
    gSaveContext.ship.stats.firstInput = 1;
    gSaveContext.ship.stats.itemTimestamp[TIMESTAMP_DEFEAT_GANON] = 500;
    gSaveContext.ship.stats.itemTimestamp[TIMESTAMP_TRIFORCE_COMPLETED] = 700;
    const uint64_t both = GAMEPLAYSTAT_TOTAL_TIME;
    gSaveContext.ship.stats.itemTimestamp[TIMESTAMP_TRIFORCE_COMPLETED] = 0;
    const uint64_t ganonOnly = GAMEPLAYSTAT_TOTAL_TIME;
    gSaveContext.ship.stats.rtaTiming = false;
    gSaveContext.ship.stats.playTimer = 40;
    gSaveContext.ship.stats.pauseTimer = 30;
    const uint64_t frames = GAMEPLAYSTAT_TOTAL_TIME;
    printf("[TEST] oot-gameplay-stats: total time hunt+Ganon %llu, Ganon only %llu, frame timers %llu\n",
           (unsigned long long)both, (unsigned long long)ganonOnly, (unsigned long long)frames);
    Expect(both == 700, "leg 4: with both timestamps the total time is not the finished hunt's (700)");
    Expect(ganonOnly == 500, "leg 4: with no hunt timestamp the total time is not Ganon's (500)");
    Expect(frames == 30, "leg 4: with RTA timing off the total time is not playTimer / 2 + pauseTimer / 3 (30)");

    memcpy(&gSaveContext, saveBefore.get(), sizeof(SaveContext));
    printf("[TEST] oot-gameplay-stats: %s (%d failure(s))\n", gFailures == 0 ? "PASS" : "FAIL", gFailures);
    return gFailures == 0 ? 0 : 1;
}

#endif // RSBS_SINGLE_EXECUTABLE
