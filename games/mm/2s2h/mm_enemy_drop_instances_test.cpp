/**
 * @file mm_enemy_drop_instances_test.cpp
 * The per-instance enemy-drop answer (#1498). An MM enemy drop is one check per
 * species, listed in Logic under every region where the species lives; the per-id
 * answer (EvaluateReachableChecks) says only that one kill somewhere is in logic,
 * and both trackers used to paint that one answer on every place the drop is listed.
 *
 * MM_EnemyDropInstances_Run (row MMEnemyDropInstances), over MM's REAL region graph
 * (MM_Rando_InitCore) and an authored live save: human form, a Kokiri Sword and
 * nothing else, standing at the arrival (South Clock Town) under a "New Cycle" moon
 * crash, Mini Baba's drop shuffled. Clock Town and Termina Field (with its two Mini
 * Baba grottos) are reachable; Beneath the Well is not.
 *   A  Mini Baba is in the tracker's in-logic set, and its instances name a
 *      reachable Termina Field grotto and not Beneath the Well's Baba room.
 *   N  MM's native Check Tracker: the Beneath the Well copy is out of logic, the
 *      Termina Field copy in logic and kept by Only Show Current Scene in Termina
 *      Field (2Ship f8bf9bb3b), and the drop's tooltip names Termina Field and not
 *      Beneath the Well.
 *   G  Only Show Current Scene inside a grotto whose shared entrance names no region
 *      (the Termina Field Mini Baba grottos' ENTRANCE(GROTTOS, 4)) answers the scene
 *      it was entered from (the respawn point's entrance), not the grotto scene.
 *   C  the combo tracker's MM pane: the drop is available and filed under the area
 *      of one of its instances in logic (not Beneath the Well, its static first
 *      region), its tooltip names Termina Field, reading the row twice computes
 *      availability once, a collected drop keeps that area, and with Enable
 *      Available Checks off the row goes back to the static first region.
 *   U  with every check shuffled, MM's native tracker lists each one under a scene it
 *      draws (the root's starting Deku Mask and Song of Healing included).
 *
 * MM_CheckInstancesPerId_Run (row MMCheckInstancesPerIdUnchanged): over the same
 * save, the new per-instance function leaves the per-id answer exactly as it was,
 * raw and under the reset-time guard, and the checks it names are exactly the
 * per-id set (so the fill, which reads only the per-id answer, cannot move).
 *
 * Each entry point returns its failure count.
 */

#include "global.h"

#include <cstdio>
#include <cstring>
#include <memory>
#include <set>
#include <string>

#if !defined(RSBS_SINGLE_EXECUTABLE)
// Single-exe surfaces; test_runner.cpp declares these unconditionally.
extern "C" int MM_EnemyDropInstances_Run(void) {
    return 0;
}
extern "C" int MM_CheckInstancesPerId_Run(void) {
    return 0;
}
#else

#include "2s2h/Rando/Rando.h"
#include "2s2h/Rando/CheckTracker/CheckTracker.h"
#include "2s2h/Rando/Logic/Logic.h"
#include "2s2h/Rando/Logic/ResetTimeGuard.h"
#include "2s2h/Rando/StaticData/StaticData.h"
#include "2s2h/ShipUtils.h"

#include "combo_mm_options_view.h" // RSBS_MM_MOON_CRASH_CYCLE
#include "context.h"
#include "combo_tracker_view.h"

#include <libultraship/bridge/consolevariablebridge.h>

// TrackerAdapterSingleExe.cpp.
extern "C" void MM_TrackerAdapter_TestLiveSeam(int live);
extern "C" int MM_TrackerAdapter_TestInLogicSeam(int authored, const uint16_t* ids, int count);
extern "C" uint16_t MM_TrackerAdapter_TestSceneAreaKey(int sceneId);
extern "C" int MM_TrackerAdapter_TestRegionAreaKey(int regionId, uint16_t* outKey);
extern "C" uint16_t MM_TrackerAdapter_TestStaticAreaKey(uint16_t checkId);
// CheckTracker.cpp.
extern "C" int MM_CheckTracker_TestCheckCopy(uint16_t sceneId, uint16_t randoCheckId, char* tip, int cap,
                                             int currentSceneId, int* outShown);
extern "C" int MM_CheckTracker_TestUnlistedShuffled(uint16_t* out, int cap);
extern "C" int MM_CheckTracker_TestScrollTargetScene(int rawSceneId);
extern "C" void MM_CheckTracker_TestReset(void);

extern "C" {
#include "z64save.h"
extern SaveContext gSaveContext;
void MM_Rando_InitCore(void);
s16 Play_GetOriginalSceneId(s16 sceneId);
}

namespace {

int sFailures = 0;

#define EDI_CHECK(cond, msg)                                              \
    do {                                                                  \
        if (!(cond)) {                                                    \
            printf("[TEST] FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__); \
            sFailures++;                                                  \
        }                                                                 \
    } while (0)

constexpr s32 kArrivalEntrance = ENTRANCE(SOUTH_CLOCK_TOWN, 0);
constexpr RandoCheckId kDrop = RC_ENEMY_DROP_MINI_BABA;
constexpr RandoRegionId kWellRoom = RR_BENEATH_THE_WELL_BABA_AND_POTS_ROOM;

/** Restores the whole live save on scope exit. */
struct EdiSaveGuard {
    std::unique_ptr<SaveContext> saved = std::make_unique<SaveContext>();
    EdiSaveGuard() {
        memcpy(saved.get(), &gSaveContext, sizeof(SaveContext));
    }
    ~EdiSaveGuard() {
        memcpy(&gSaveContext, saved.get(), sizeof(SaveContext));
    }
    EdiSaveGuard(const EdiSaveGuard&) = delete;
    EdiSaveGuard& operator=(const EdiSaveGuard&) = delete;
};

/** The authored save: a Kokiri Sword, nothing else, at the arrival, a New Cycle moon crash, Mini Baba shuffled. */
void EdiAuthorSave(void) {
    memset(&gSaveContext, 0, sizeof(SaveContext));
    gSaveContext.fileNum = 0xFF;
    gSaveContext.gameMode = GAMEMODE_NORMAL;
    gSaveContext.save.playerForm = PLAYER_FORM_HUMAN;
    gSaveContext.save.shipSaveInfo.saveType = SAVETYPE_RANDO;
    memset(gSaveContext.save.saveInfo.inventory.items, ITEM_NONE, sizeof(gSaveContext.save.saveInfo.inventory.items));
    SET_EQUIP_VALUE(EQUIP_TYPE_SWORD, EQUIP_VALUE_SWORD_KOKIRI);
    gSaveContext.save.entrance = kArrivalEntrance;
    // "New Cycle": the reset-time guard reads it as a reset, so the crawl is not held to the arrival nook
    // (mm-tracker-in-logic-arrival's reason; the guard itself is mm-combo-logic-engine leg 16's).
    gSaveContext.save.shipSaveInfo.rando.randoSaveMoonCrash = RSBS_MM_MOON_CRASH_CYCLE;
    memcpy(gSaveContext.save.saveInfo.playerData.newf, "ZELDA3", 6);
    RANDO_SAVE_CHECKS[kDrop].shuffled = true;
}

std::set<RandoCheckId> EdiProject(const std::set<Rando::Logic::CheckInstance>& instances) {
    std::set<RandoCheckId> ids;
    for (const auto& [_, checkId] : instances) {
        ids.insert(checkId);
    }
    return ids;
}

int EdiInstanceCount(const std::set<Rando::Logic::CheckInstance>& instances, RandoCheckId checkId) {
    int count = 0;
    for (const auto& [_, id] : instances) {
        count += id == checkId ? 1 : 0;
    }
    return count;
}

const char* const kEdiAvailableKey = "gTrackers.CheckTracker.EnableAvailableChecks";

} // namespace

extern "C" int MM_EnemyDropInstances_Run(void) {
    printf("[TEST] mm-enemy-drop-instances: each place an MM enemy drop is listed is coloured and filed from its own "
           "kill (#1498)\n");
    sFailures = 0;

    MM_Rando_InitCore();
    if (Rando::Logic::Regions.empty()) {
        printf("[TEST] FAIL: Rando::Logic::Regions is empty after MM_Rando_InitCore, so every leg would be vacuous\n");
        return 1;
    }

    // Released before the end so the scene lists can be rebuilt from the restored save (MM_CheckTracker_TestReset).
    auto guard = std::make_unique<EdiSaveGuard>();
    EdiAuthorSave();

    const s16 fieldScene = Play_GetOriginalSceneId(SCENE_00KEIKOKU);
    const s16 wellScene = Play_GetOriginalSceneId(SCENE_REDEAD);
    const std::string fieldName = Ship_GetSceneName(fieldScene);
    const std::string wellName = Ship_GetSceneName(wellScene);

    // ---- A: the per-instance answer --------------------------------------------------------
    std::set<Rando::Logic::CheckInstance> instances;
    const std::set<RandoCheckId> inLogic =
        Rando::CheckTracker::ComputeChecksInLogic(gSaveContext.save.entrance, &instances);
    const bool dropInLogic = inLogic.count(kDrop) != 0;
    const bool fieldInstance = instances.count({ RR_TERMINA_FIELD_PILLAR_GROTTO, kDrop }) != 0 ||
                               instances.count({ RR_TERMINA_FIELD_TALL_GRASS_GROTTO, kDrop }) != 0;
    const bool wellInstance = instances.count({ kWellRoom, kDrop }) != 0;
    printf("[TEST] A: in logic %d checks, %d instances; Mini Baba in logic %d, %d instance(s), a Termina Field grotto "
           "%d, Beneath the Well %d\n",
           (int)inLogic.size(), (int)instances.size(), dropInLogic ? 1 : 0, EdiInstanceCount(instances, kDrop),
           fieldInstance ? 1 : 0, wellInstance ? 1 : 0);
    EDI_CHECK(dropInLogic, "A: Mini Baba's drop is not in logic with a Kokiri Sword and Termina Field reachable");
    EDI_CHECK(fieldInstance, "A: no Termina Field grotto's Mini Baba instance is in logic");
    EDI_CHECK(!wellInstance, "A: Beneath the Well's Mini Baba instance is in logic with nothing but a sword");
    EDI_CHECK(EdiProject(instances) == inLogic, "A: the instances do not name exactly the in-logic checks");

    // ---- N: MM's native Check Tracker ---------------------------------------------------------
    char tip[512] = {};
    int wellShown = -1;
    const int wellCopy = MM_CheckTracker_TestCheckCopy((uint16_t)wellScene, (uint16_t)kDrop, tip, (int)sizeof(tip),
                                                       fieldScene, &wellShown);
    int fieldShown = -1;
    const int fieldCopy = MM_CheckTracker_TestCheckCopy((uint16_t)fieldScene, (uint16_t)kDrop, tip, (int)sizeof(tip),
                                                        fieldScene, &fieldShown);
    const std::string nativeTip = tip;
    printf("[TEST] N: native copies: Beneath the Well %d (shown in Termina Field %d), Termina Field %d (shown %d); "
           "tooltip '%s'\n",
           wellCopy, wellShown, fieldCopy, fieldShown, nativeTip.c_str());
    EDI_CHECK(wellCopy == 0, "N: the native tracker's Beneath the Well copy of Mini Baba is not listed out of logic");
    EDI_CHECK(fieldCopy == 1, "N: the native tracker's Termina Field copy of Mini Baba is not listed in logic");
    EDI_CHECK(fieldShown == 1 && wellShown == 0,
              "N: Only Show Current Scene in Termina Field does not keep exactly the Termina Field copy");
    EDI_CHECK(nativeTip.find(fieldName) != std::string::npos && nativeTip.find(wellName) == std::string::npos,
              "N: the native drop tooltip does not name Termina Field alone of the two");

    // ---- G: Only Show Current Scene inside a shared-entrance grotto -------------------------------
    // The pillar and tall-grass grottos are entered through ENTRANCE(GROTTOS, 4) from Termina Field; MM sets the
    // respawn point from the field's entrance and not from inside the grotto (Play_SetupRespawnPoint).
    gSaveContext.save.entrance = ENTRANCE(GROTTOS, 4);
    gSaveContext.respawn[RESPAWN_MODE_DOWN].entrance = ENTRANCE(TERMINA_FIELD, 0);
    const int grottoRegion = (int)Rando::Logic::GetRegionIdFromEntrance(gSaveContext.save.entrance);
    const int grottoTarget = MM_CheckTracker_TestScrollTargetScene((int)SCENE_KAKUSIANA);
    gSaveContext.save.entrance = kArrivalEntrance;
    gSaveContext.respawn[RESPAWN_MODE_DOWN].entrance = 0;
    printf("[TEST] G: grotto entrance region %d (RR_MAX %d), current scene in the grotto %d (Termina Field %d)\n       "
           "    ",
           grottoRegion, (int)RR_MAX, grottoTarget, (int)fieldScene);
    EDI_CHECK(grottoTarget == (int)fieldScene,
              "G: Only Show Current Scene in a Termina Field Mini Baba grotto does not answer Termina Field");

    // ---- C: the combo tracker's MM pane ------------------------------------------------------
    MM_TrackerAdapter_Register();
    MM_TrackerAdapter_TestLiveSeam(1);
    const GameId prevGame = Context_GetCurrentGame();
    Context_SetCurrentGame(GAME_MM);
    const bool hadAvailableKey = CVarExists(kEdiAvailableKey);
    const int32_t availableKey = CVarGetInteger(kEdiAvailableKey, 0);
    CVarSetInteger(kEdiAvailableKey, 1);
    (void)MM_TrackerAdapter_TestInLogicSeam(0, nullptr, 0);

    // The summary computes availability (the window reads it before the rows); then the row, twice.
    ComboTrackerGameSummary summary = {};
    Combo_TrackerGameSummary((uint8_t)GAME_MM, &summary);
    ComboTrackerCheckRow row = {};
    const bool haveRow = Combo_TrackerCheckAt((uint8_t)GAME_MM, (int)kDrop, &row);
    ComboTrackerCheckRow again = {};
    const bool haveAgain = Combo_TrackerCheckAt((uint8_t)GAME_MM, (int)kDrop, &again);
    const char* comboTip = Combo_TrackerCheckLogic((uint8_t)GAME_MM, (uint16_t)kDrop);
    const std::string comboTipText = comboTip != nullptr ? comboTip : "";
    const int computations = MM_TrackerAdapter_TestInLogicSeam(0, nullptr, 0);

    std::set<uint16_t> reachableAreas;
    for (const auto& [regionId, checkId] : instances) {
        uint16_t key = 0;
        if (checkId == kDrop && MM_TrackerAdapter_TestRegionAreaKey((int)regionId, &key) != 0) {
            reachableAreas.insert(key);
        }
    }
    const uint16_t wellKey = MM_TrackerAdapter_TestSceneAreaKey(wellScene);
    const uint16_t staticKey = MM_TrackerAdapter_TestStaticAreaKey((uint16_t)kDrop);
    printf("[TEST] C: row %d, available %d (known %d), area %04X '%s'; areas in logic %d; Beneath the Well %04X; "
           "static first region %04X (informational: #1498 saw Beneath the Well); tooltip '%s'; computations %d\n",
           haveRow ? 1 : 0, row.available ? 1 : 0, row.availabilityKnown ? 1 : 0, (unsigned)row.areaKey,
           row.areaName != nullptr ? row.areaName : "(null)", (int)reachableAreas.size(), (unsigned)wellKey,
           (unsigned)staticKey, comboTipText.c_str(), computations);
    EDI_CHECK(haveRow && row.availabilityKnown && row.available, "C: the combo pane does not show Mini Baba available");
    EDI_CHECK(!reachableAreas.empty() && reachableAreas.count(row.areaKey) != 0,
              "C: the combo pane does not file Mini Baba under an area where its kill is in logic");
    EDI_CHECK(row.areaKey != wellKey, "C: the combo pane files Mini Baba under Beneath the Well");
    EDI_CHECK(haveAgain && again.areaKey == row.areaKey, "C: reading the row again moved it to another area");
    EDI_CHECK(computations == 1, "C: reading the summary and the row twice did not compute availability exactly once");
    EDI_CHECK(comboTipText.find(fieldName) != std::string::npos && comboTipText.find(wellName) == std::string::npos,
              "C: the combo pane's drop tooltip does not name Termina Field alone of the two");
    // The issue's claim, and the guard that keeps the area legs from passing against the static rule.
    EDI_CHECK(staticKey == wellKey, "C: Mini Baba's static first region is not Beneath the Well (the issue's claim)");
    EDI_CHECK(reachableAreas.count(staticKey) == 0,
              "C: Mini Baba's static first region is an area in logic, so the area legs would be vacuous");

    // Collected: the drop keeps the area it was filed under instead of going back to the static first region.
    RANDO_SAVE_CHECKS[kDrop].obtained = true;
    (void)MM_TrackerAdapter_TestInLogicSeam(0, nullptr, 0);
    ComboTrackerGameSummary obtainedSummary = {};
    Combo_TrackerGameSummary((uint8_t)GAME_MM, &obtainedSummary);
    ComboTrackerCheckRow obtainedRow = {};
    const bool haveObtained = Combo_TrackerCheckAt((uint8_t)GAME_MM, (int)kDrop, &obtainedRow);
    RANDO_SAVE_CHECKS[kDrop].obtained = false;
    (void)MM_TrackerAdapter_TestInLogicSeam(0, nullptr, 0);
    printf("[TEST] C (collected): area %04X\n", (unsigned)obtainedRow.areaKey);
    EDI_CHECK(haveObtained && obtainedRow.areaKey == row.areaKey,
              "C: collecting Mini Baba's drop moved its row to another area");

    // Enable Available Checks off: nothing is computed, and the row keeps the static first region.
    CVarSetInteger(kEdiAvailableKey, 0);
    ComboTrackerCheckRow off = {};
    const bool haveOff = Combo_TrackerCheckAt((uint8_t)GAME_MM, (int)kDrop, &off);
    printf("[TEST] C (Available Checks off): area %04X, known %d\n", (unsigned)off.areaKey,
           off.availabilityKnown ? 1 : 0);
    EDI_CHECK(haveOff && off.areaKey == staticKey && !off.availabilityKnown,
              "C: with Enable Available Checks off the drop does not go back to the static first region");

    if (hadAvailableKey) {
        CVarSetInteger(kEdiAvailableKey, availableKey);
    } else {
        CVarClear(kEdiAvailableKey);
    }
    MM_TrackerAdapter_TestLiveSeam(-1);
    (void)MM_TrackerAdapter_TestInLogicSeam(0, nullptr, 0);
    Context_SetCurrentGame(prevGame);

    // ---- U: every shuffled check is listed under a scene the native tracker draws -------------------
    for (auto& [checkId, _] : Rando::StaticData::Checks) {
        RANDO_SAVE_CHECKS[checkId].shuffled = true;
    }
    uint16_t unlisted[8] = {};
    const int unlistedCount = MM_CheckTracker_TestUnlistedShuffled(unlisted, 8);
    printf("[TEST] U: %d shuffled check(s) listed under no drawn scene:", unlistedCount);
    for (int i = 0; i < unlistedCount && i < 8; i++) {
        printf(" %u", (unsigned)unlisted[i]);
    }
    printf(" (RC_STARTING_ITEM_DEKU_MASK %d, RC_STARTING_ITEM_SONG_OF_HEALING %d)\n", (int)RC_STARTING_ITEM_DEKU_MASK,
           (int)RC_STARTING_ITEM_SONG_OF_HEALING);
    EDI_CHECK(unlistedCount == 0, "U: a shuffled check is listed under no scene MM's native tracker draws");

    guard.reset();
    MM_CheckTracker_TestReset();
    if (sFailures != 0) {
        printf("[TEST] FAIL: mm-enemy-drop-instances: %d check(s) failed\n", sFailures);
    } else {
        printf("[TEST] PASS: mm-enemy-drop-instances\n");
    }
    return sFailures;
}

extern "C" int MM_CheckInstancesPerId_Run(void) {
    printf("[TEST] mm-check-instances-per-id: the per-instance answer leaves the per-id answer the fill reads "
           "unchanged (#1498)\n");
    sFailures = 0;

    MM_Rando_InitCore();
    if (Rando::Logic::Regions.empty()) {
        printf("[TEST] FAIL: Rando::Logic::Regions is empty after MM_Rando_InitCore, so every leg would be vacuous\n");
        return 1;
    }

    EdiSaveGuard guard;
    EdiAuthorSave();

    Rando::Logic::ReachabilityCrawl crawl = Rando::Logic::CrawlReachableRegions(gSaveContext.save.entrance);
    EDI_CHECK(crawl.reachableRegions.count(RR_TERMINA_FIELD) != 0 && crawl.reachableRegions.count(kWellRoom) == 0,
              "the crawl does not reach Termina Field without Beneath the Well, so the legs would not be the ones "
              "#1498 is about");

    // Raw: per-id, then per-instance, then per-id again.
    const std::set<RandoCheckId> before = Rando::Logic::EvaluateReachableChecks(crawl);
    const std::set<Rando::Logic::CheckInstance> raw = Rando::Logic::EvaluateReachableCheckInstances(crawl, before);
    const std::set<RandoCheckId> after = Rando::Logic::EvaluateReachableChecks(crawl);
    bool rowsReal = true;
    for (const auto& [regionId, checkId] : raw) {
        const auto region = Rando::Logic::Regions.find(regionId);
        rowsReal = rowsReal && crawl.reachableRegions.count(regionId) != 0 && region != Rando::Logic::Regions.end() &&
                   region->second.checks.count(checkId) != 0;
    }
    printf("[TEST] raw: per-id %d checks before, %d after; %d instances naming %d checks; Mini Baba %d instance(s)\n",
           (int)before.size(), (int)after.size(), (int)raw.size(), (int)EdiProject(raw).size(),
           EdiInstanceCount(raw, kDrop));
    EDI_CHECK(before.size() >= 10, "the per-id set is almost empty, so the comparison would be vacuous");
    EDI_CHECK(after == before, "computing the instances changed the per-id answer over the same crawl");
    EDI_CHECK(EdiProject(raw) == before, "the instances do not name exactly the per-id set");
    EDI_CHECK(rowsReal, "an instance names a region the crawl did not reach, or a check its region does not list");
    EDI_CHECK(EdiInstanceCount(raw, kDrop) >= 2,
              "Mini Baba has fewer than two instances in logic (two Termina Field grottos), so the per-instance "
              "answer is not exercised");

    // Guarded: the tracker's and the engine's discipline (ApplyResetTimeGuard, then the instances).
    std::set<RandoCheckId> guarded = before;
    (void)Rando::Logic::ApplyResetTimeGuard(crawl, guarded);
    const std::set<Rando::Logic::CheckInstance> guardedInstances =
        Rando::Logic::EvaluateReachableCheckInstances(crawl, guarded);
    // The guarded per-id answer again, from a fresh crawl made after the instances were computed.
    Rando::Logic::ReachabilityCrawl again = Rando::Logic::CrawlReachableRegions(gSaveContext.save.entrance);
    std::set<RandoCheckId> guardedAgain = Rando::Logic::EvaluateReachableChecks(again);
    (void)Rando::Logic::ApplyResetTimeGuard(again, guardedAgain);
    printf("[TEST] guarded: per-id %d checks (%d from a fresh crawl), %d instances naming %d checks\n",
           (int)guarded.size(), (int)guardedAgain.size(), (int)guardedInstances.size(),
           (int)EdiProject(guardedInstances).size());
    EDI_CHECK(guardedAgain == guarded, "the guarded per-id answer moved after the instances were computed");
    EDI_CHECK(EdiProject(guardedInstances) == guarded, "the guarded instances do not name exactly the guarded set");
    EDI_CHECK(EdiInstanceCount(guardedInstances, kDrop) >= 2, "the guard dropped Mini Baba's Termina Field instances");

    // The tracker's entry point: the one-argument call and the two-argument call agree.
    std::set<Rando::Logic::CheckInstance> trackerInstances;
    const std::set<RandoCheckId> tracker1 = Rando::CheckTracker::ComputeChecksInLogic(gSaveContext.save.entrance);
    const std::set<RandoCheckId> tracker2 =
        Rando::CheckTracker::ComputeChecksInLogic(gSaveContext.save.entrance, &trackerInstances);
    printf("[TEST] tracker: %d checks without instances, %d with\n", (int)tracker1.size(), (int)tracker2.size());
    EDI_CHECK(tracker1 == tracker2, "asking the tracker for instances changed its in-logic set");
    EDI_CHECK(EdiProject(trackerInstances) == tracker2, "the tracker's instances do not name exactly its set");

    if (sFailures != 0) {
        printf("[TEST] FAIL: mm-check-instances-per-id: %d check(s) failed\n", sFailures);
    } else {
        printf("[TEST] PASS: mm-check-instances-per-id\n");
    }
    return sFailures;
}

#endif /* RSBS_SINGLE_EXECUTABLE */
