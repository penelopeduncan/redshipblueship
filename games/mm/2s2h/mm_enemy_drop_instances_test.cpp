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
 *   C  the combo tracker's MM pane: the drop is available, and (#1127) listed in
 *      one place per area of a region that lists it, as MM's own tracker lists it,
 *      its first place its row's (the static first region, Beneath the Well); the
 *      places in logic are exactly the areas of its instances in logic (Termina
 *      Field in, Beneath the Well out), each place answers as MM's tracker's copy
 *      there does, its tooltip names Termina Field, reading the row twice and its
 *      places computes availability once, a collected drop keeps its places with
 *      none in logic, a non-drop check's places answer -1 while drop areas
 *      apply (#1711), and with Enable Available Checks off no place answers.
 *   U  with every check shuffled, MM's native tracker lists each one under a scene it
 *      draws (the root's starting Deku Mask and Song of Healing included).
 *   P  (#1706) every check of every shop and house interior (MM's tracker's
 *      scenesToCheckParent, 2Ship f8bf9bb3b) is listed by the combo pane exactly
 *      where MM's tracker lists it, the scene its region opens from, never under
 *      the interior, its row's area that scene (the Bomb Shop under West Clock
 *      Town, the Cucco Shack under Romani Ranch, the Poe Sisters' drop under Ikana
 *      Canyon); Randomizer > MM Locations keeps each one's own heading; and inside
 *      each interior the pane's current area is the scene MM's tracker scrolls to
 *      and filters by there (the interior itself when entered from no interior).
 *   R  (#1711) every check MM's tracker lists (including non-drops) is listed by
 *      the combo pane under the exact set of scenes MM's tracker lists it
 *      under, while each non-drop check outside scenesToCheckParent keeps its
 *      own static row area (Moon's Tear under Termina Field with Observatory row
 *      area, Clock Town Postbox in three scenes, Oath to Order in the four boss
 *      scenes with Giants' Chamber row area).
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

#include "combo_mm_locations_view.h" // Combo_MMLocationById: Randomizer > MM Locations' table (#1706)
#include "combo_mm_options_view.h"   // RSBS_MM_MOON_CRASH_CYCLE
#include "context.h"
#include "combo_tracker_view.h"

#include <libultraship/bridge/consolevariablebridge.h>

// TrackerAdapterSingleExe.cpp.
extern "C" void MM_TrackerAdapter_TestLiveSeam(int live);
extern "C" int MM_TrackerAdapter_TestInLogicSeam(int authored, const uint16_t* ids, int count);
extern "C" uint16_t MM_TrackerAdapter_TestSceneAreaKey(int sceneId);
extern "C" int MM_TrackerAdapter_TestRegionAreaKey(int regionId, uint16_t* outKey);
extern "C" uint16_t MM_TrackerAdapter_TestStaticAreaKey(uint16_t checkId);
extern "C" int MM_TrackerAdapter_TestAreaPlaces(uint16_t checkId, uint16_t* outKeys, int* outAvailable, int cap);
// CheckTracker.cpp.
extern "C" int MM_CheckTracker_TestCheckCopy(uint16_t sceneId, uint16_t randoCheckId, char* tip, int cap,
                                             int currentSceneId, int* outShown);
extern "C" int MM_CheckTracker_TestUnlistedShuffled(uint16_t* out, int cap);
extern "C" int MM_CheckTracker_TestScrollTargetScene(int rawSceneId);
extern "C" void MM_CheckTracker_TestReset(void);
extern "C" int MM_CheckTracker_TestListedScenes(uint16_t randoCheckId, uint16_t* out, int cap);
extern "C" void MM_TrackerAdapter_TestSceneSeam(int mode, int sceneId);

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
// More places than any drop has (Mini Baba, the most, is listed under fourteen scenes).
constexpr int kEdiPlaceCap = 32;

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
    // #1127: the places the pane lists the drop in, each with its own answer, read before the seam drops them.
    uint16_t placeKeys[kEdiPlaceCap] = {};
    int placeAvailable[kEdiPlaceCap] = {};
    const int placeCount = MM_TrackerAdapter_TestAreaPlaces((uint16_t)kDrop, placeKeys, placeAvailable, kEdiPlaceCap);
    int postboxAvailable[kEdiPlaceCap] = {};
    const int postboxPlaceCount =
        MM_TrackerAdapter_TestAreaPlaces((uint16_t)RC_CLOCK_TOWN_POSTBOX, nullptr, postboxAvailable, kEdiPlaceCap);
    const int computations = MM_TrackerAdapter_TestInLogicSeam(0, nullptr, 0);

    std::set<uint16_t> reachableAreas;
    for (const auto& [regionId, checkId] : instances) {
        uint16_t key = 0;
        if (checkId == kDrop && MM_TrackerAdapter_TestRegionAreaKey((int)regionId, &key) != 0) {
            reachableAreas.insert(key);
        }
    }
    // MM's tracker lists the drop under the scene of EVERY region that lists it (initializeSceneChecks).
    std::set<uint16_t> regionAreas;
    for (const auto& [regionId, region] : Rando::Logic::Regions) {
        uint16_t key = 0;
        if (region.checks.count(kDrop) != 0 && MM_TrackerAdapter_TestRegionAreaKey((int)regionId, &key) != 0) {
            regionAreas.insert(key);
        }
    }
    const uint16_t wellKey = MM_TrackerAdapter_TestSceneAreaKey(wellScene);
    const uint16_t fieldKey = MM_TrackerAdapter_TestSceneAreaKey(fieldScene);
    const uint16_t staticKey = MM_TrackerAdapter_TestStaticAreaKey((uint16_t)kDrop);
    const int listed = placeCount < kEdiPlaceCap ? placeCount : kEdiPlaceCap;
    std::set<uint16_t> placeAreas;
    std::set<uint16_t> placesInLogic;
    int wellPlace = -2;
    int fieldPlace = -2;
    for (int i = 0; i < listed; i++) {
        placeAreas.insert(placeKeys[i]);
        if (placeAvailable[i] == 1) {
            placesInLogic.insert(placeKeys[i]);
        }
        wellPlace = placeKeys[i] == wellKey ? placeAvailable[i] : wellPlace;
        fieldPlace = placeKeys[i] == fieldKey ? placeAvailable[i] : fieldPlace;
    }
    bool postboxUnanswered = postboxPlaceCount >= 1;
    for (int i = 0; i < postboxPlaceCount && i < kEdiPlaceCap; i++) {
        postboxUnanswered = postboxUnanswered && postboxAvailable[i] == -1;
    }
    printf("[TEST] C: row %d, available %d (known %d), area %04X '%s'; %d place(s) (%d region area(s)), %d in logic; "
           "Beneath the Well %04X (%d), Termina Field %04X (%d); static first region %04X; tooltip '%s'; "
           "computations %d\n",
           haveRow ? 1 : 0, row.available ? 1 : 0, row.availabilityKnown ? 1 : 0, (unsigned)row.areaKey,
           row.areaName != nullptr ? row.areaName : "(null)", placeCount, (int)regionAreas.size(),
           (int)placesInLogic.size(), (unsigned)wellKey, wellPlace, (unsigned)fieldKey, fieldPlace, (unsigned)staticKey,
           comboTipText.c_str(), computations);
    EDI_CHECK(haveRow && row.availabilityKnown && row.available, "C: the combo pane does not show Mini Baba available");
    EDI_CHECK(row.areaKey == staticKey, "C: Mini Baba's row does not carry its first place, the static first region");
    EDI_CHECK(placeCount >= 2 && placeCount == (int)placeAreas.size() && placeAreas == regionAreas,
              "C: the combo pane does not list Mini Baba once under the area of every region that lists it (#1127)");
    EDI_CHECK(listed > 0 && placeKeys[0] == staticKey, "C: Mini Baba's first place is not its row's area");
    EDI_CHECK(placesInLogic == reachableAreas && !placesInLogic.empty(),
              "C: the places where Mini Baba is in logic are not exactly the areas of its instances in logic");
    EDI_CHECK(wellPlace == 0, "C: the combo pane's Beneath the Well place of Mini Baba is not listed out of logic");
    EDI_CHECK(fieldPlace == 1, "C: the combo pane's Termina Field place of Mini Baba is not listed in logic");
    EDI_CHECK(postboxUnanswered, "C: a non-drop check's places do not all answer -1 while drop areas apply (#1711)");
    EDI_CHECK(haveAgain && again.areaKey == row.areaKey, "C: reading the row again moved it to another area");
    EDI_CHECK(computations == 1,
              "C: reading the summary, the row twice and its places did not compute availability exactly once");
    EDI_CHECK(comboTipText.find(fieldName) != std::string::npos && comboTipText.find(wellName) == std::string::npos,
              "C: the combo pane's drop tooltip does not name Termina Field alone of the two");
    // The issue's claim, and the guard that keeps the place legs from passing on the first place alone.
    EDI_CHECK(staticKey == wellKey, "C: Mini Baba's static first region is not Beneath the Well (the issue's claim)");
    EDI_CHECK(reachableAreas.count(staticKey) == 0,
              "C: Mini Baba's static first region is an area in logic, so the place legs would be vacuous");

    // The places match MM's own tracker: each is a scene it lists the drop under, greyed the same (#1127).
    int nativeMismatch = 0;
    for (int i = 0; i < listed; i++) {
        const int nativeCopy =
            MM_CheckTracker_TestCheckCopy((uint16_t)(placeKeys[i] & 0xFF), (uint16_t)kDrop, nullptr, 0, -1, nullptr);
        if (nativeCopy != placeAvailable[i]) {
            printf("[TEST] C: place %04X: the combo pane answers %d, MM's tracker %d\n", (unsigned)placeKeys[i],
                   placeAvailable[i], nativeCopy);
            nativeMismatch++;
        }
    }
    (void)MM_TrackerAdapter_TestInLogicSeam(0, nullptr, 0);
    EDI_CHECK(nativeMismatch == 0, "C: a place of Mini Baba in the combo pane differs from MM's own tracker's copy");

    // Collected: the row keeps its places, none in logic (a found drop is in logic nowhere).
    RANDO_SAVE_CHECKS[kDrop].obtained = true;
    (void)MM_TrackerAdapter_TestInLogicSeam(0, nullptr, 0);
    ComboTrackerGameSummary obtainedSummary = {};
    Combo_TrackerGameSummary((uint8_t)GAME_MM, &obtainedSummary);
    ComboTrackerCheckRow obtainedRow = {};
    const bool haveObtained = Combo_TrackerCheckAt((uint8_t)GAME_MM, (int)kDrop, &obtainedRow);
    int obtainedAvailable[kEdiPlaceCap] = {};
    const int obtainedCount =
        MM_TrackerAdapter_TestAreaPlaces((uint16_t)kDrop, nullptr, obtainedAvailable, kEdiPlaceCap);
    int obtainedInLogic = 0;
    for (int i = 0; i < obtainedCount && i < kEdiPlaceCap; i++) {
        obtainedInLogic += obtainedAvailable[i] == 1 ? 1 : 0;
    }
    RANDO_SAVE_CHECKS[kDrop].obtained = false;
    (void)MM_TrackerAdapter_TestInLogicSeam(0, nullptr, 0);
    printf("[TEST] C (collected): area %04X, %d place(s), %d in logic\n", (unsigned)obtainedRow.areaKey, obtainedCount,
           obtainedInLogic);
    EDI_CHECK(haveObtained && obtainedRow.areaKey == row.areaKey && obtainedCount == placeCount && obtainedInLogic == 0,
              "C: collecting Mini Baba's drop moved its places, or left one in logic");

    // Enable Available Checks off: nothing is computed, the places stay, and none answers for itself.
    CVarSetInteger(kEdiAvailableKey, 0);
    ComboTrackerCheckRow off = {};
    const bool haveOff = Combo_TrackerCheckAt((uint8_t)GAME_MM, (int)kDrop, &off);
    int offAvailable[kEdiPlaceCap] = {};
    const int offCount = MM_TrackerAdapter_TestAreaPlaces((uint16_t)kDrop, nullptr, offAvailable, kEdiPlaceCap);
    bool offUnanswered = true;
    for (int i = 0; i < offCount && i < kEdiPlaceCap; i++) {
        offUnanswered = offUnanswered && offAvailable[i] == -1;
    }
    printf("[TEST] C (Available Checks off): area %04X, known %d, %d place(s), unanswered %d\n", (unsigned)off.areaKey,
           off.availabilityKnown ? 1 : 0, offCount, offUnanswered ? 1 : 0);
    EDI_CHECK(haveOff && off.areaKey == staticKey && !off.availabilityKnown && offCount == placeCount && offUnanswered,
              "C: with Enable Available Checks off the drop does not keep its places with no answer of their own");

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

    // ---- P (#1706): shop and house interiors are listed under the scene they open from --------------
    // MM's native lists rebuilt once from the all-shuffled save; each check of a region in scenesToCheckParent is
    // listed by the combo pane exactly where they list it, which is the scene its region opens from and never the
    // interior itself.
    MM_CheckTracker_TestReset();
    MM_TrackerAdapter_Register();
    MM_RandoLocationsUi_Register();
    std::set<RandoCheckId> interiorChecks;
    int parentlessRegions = 0;
    int listingMismatches = 0;
    int locationsMoved = 0;
    for (const auto& [regionId, region] : Rando::Logic::Regions) {
        const SceneId own = (SceneId)Play_GetOriginalSceneId(region.sceneId);
        if (own == SCENE_KAKUSIANA || !Rando::CheckTracker::IsListedUnderParentScene(own)) {
            continue;
        }
        const SceneId parent = Rando::CheckTracker::GetRegionListScene(regionId);
        if (parent == SCENE_MAX || parent == own) {
            parentlessRegions++;
            continue;
        }
        for (const auto& [checkId, _] : region.checks) {
            if (!interiorChecks.insert(checkId).second) {
                continue;
            }
            uint16_t nativeScenes[kEdiPlaceCap] = {};
            const int nativeCount = MM_CheckTracker_TestListedScenes((uint16_t)checkId, nativeScenes, kEdiPlaceCap);
            std::set<uint16_t> nativeKeys;
            for (int i = 0; i < nativeCount && i < kEdiPlaceCap; i++) {
                nativeKeys.insert(MM_TrackerAdapter_TestSceneAreaKey(nativeScenes[i]));
            }
            uint16_t keys[kEdiPlaceCap] = {};
            const int count = MM_TrackerAdapter_TestAreaPlaces((uint16_t)checkId, keys, nullptr, kEdiPlaceCap);
            const std::set<uint16_t> paneKeys(keys, keys + (count < kEdiPlaceCap ? count : kEdiPlaceCap));
            const uint16_t parentKey = MM_TrackerAdapter_TestSceneAreaKey(parent);
            const uint16_t ownKey = MM_TrackerAdapter_TestSceneAreaKey(own);
            const uint16_t rowKey = MM_TrackerAdapter_TestStaticAreaKey((uint16_t)checkId);
            if (paneKeys != nativeKeys || paneKeys.count(parentKey) == 0 || paneKeys.count(ownKey) != 0 || count < 1 ||
                keys[0] != rowKey) {
                printf("[TEST] P: check %d of region %d (scene %d, parent %d): the pane lists it in %d place(s) (row "
                       "%04X), MM's tracker under %d scene(s)\n",
                       (int)checkId, (int)regionId, (int)own, (int)parent, count, (unsigned)rowKey, nativeCount);
                listingMismatches++;
            }
            // Randomizer > MM Locations keeps an interior's own heading (MM's Check Filter has no areas): a check's
            // own scene, an enemy drop's its first region's.
            const auto check = Rando::StaticData::Checks.find(checkId);
            const bool drop =
                check != Rando::StaticData::Checks.end() && check->second.randoCheckType == RCTYPE_ENEMY_DROP;
            const uint16_t ownHeading =
                drop || check == Rando::StaticData::Checks.end()
                    ? ownKey
                    : MM_TrackerAdapter_TestSceneAreaKey(Play_GetOriginalSceneId(check->second.sceneId));
            const ComboMMLocationDesc* location = Combo_MMLocationById((uint16_t)checkId);
            if (location != nullptr && location->areaKey != ownHeading) {
                printf("[TEST] P: MM Locations lists interior check %d under %04X, not its own heading %04X\n",
                       (int)checkId, (unsigned)location->areaKey, (unsigned)ownHeading);
                locationsMoved++;
            }
        }
    }
    // Three named by hand, so both sides moving together cannot pass: the Bomb Shop opens onto West Clock Town, the
    // Cucco Shack onto Romani Ranch, and the Ghost Hut (the Poe Sisters' drop) onto Ikana Canyon.
    const uint16_t bombShopRow = MM_TrackerAdapter_TestStaticAreaKey((uint16_t)RC_BOMB_SHOP_ITEM_01);
    const uint16_t poeRow = MM_TrackerAdapter_TestStaticAreaKey((uint16_t)RC_ENEMY_DROP_POE_SISTER);
    uint16_t cuccoRow = 0xFFFF;
    for (RandoCheckId checkId : interiorChecks) {
        const auto check = Rando::StaticData::Checks.find(checkId);
        if (check != Rando::StaticData::Checks.end() && check->second.sceneId == SCENE_F01C) {
            cuccoRow = MM_TrackerAdapter_TestStaticAreaKey((uint16_t)checkId);
            break;
        }
    }
    const ComboMMLocationDesc* bombShopLocation = Combo_MMLocationById((uint16_t)RC_BOMB_SHOP_ITEM_01);
    printf("[TEST] P: %d interior check(s), %d region(s) with no parent, %d listing mismatch(es), %d moved in MM "
           "Locations; Bomb Shop row %04X (West Clock Town %04X), Cucco Shack %04X (Romani Ranch %04X), Poe Sisters "
           "%04X (Ikana Canyon %04X)\n",
           (int)interiorChecks.size(), parentlessRegions, listingMismatches, locationsMoved, (unsigned)bombShopRow,
           (unsigned)MM_TrackerAdapter_TestSceneAreaKey(SCENE_ICHIBA), (unsigned)cuccoRow,
           (unsigned)MM_TrackerAdapter_TestSceneAreaKey(SCENE_F01), (unsigned)poeRow,
           (unsigned)MM_TrackerAdapter_TestSceneAreaKey(SCENE_IKANA));
    EDI_CHECK(interiorChecks.size() >= 100, "P: fewer than 100 checks in shop and house interiors, so P is vacuous");
    EDI_CHECK(parentlessRegions == 0, "P: an interior region has no scene it opens from");
    EDI_CHECK(listingMismatches == 0,
              "P: an interior's check is not listed by the combo pane exactly where MM's own tracker lists it");
    EDI_CHECK(bombShopRow == MM_TrackerAdapter_TestSceneAreaKey(SCENE_ICHIBA),
              "P: the Bomb Shop's check is not listed under West Clock Town");
    EDI_CHECK(cuccoRow == MM_TrackerAdapter_TestSceneAreaKey(SCENE_F01),
              "P: the Cucco Shack's checks are not listed under Romani Ranch");
    EDI_CHECK(poeRow == MM_TrackerAdapter_TestSceneAreaKey(SCENE_IKANA),
              "P: the Poe Sisters' drop is not listed under Ikana Canyon");
    EDI_CHECK(locationsMoved == 0 && bombShopLocation != nullptr &&
                  bombShopLocation->areaKey == MM_TrackerAdapter_TestSceneAreaKey(SCENE_BOMYA),
              "P: Randomizer > MM Locations moved an interior's check off its own heading");

    // The current area inside each interior: the pane answers what MM's tracker scrolls to and filters by there, for
    // the entrance into each of its regions; and inside the Bomb Shop entered by an entrance of no interior region
    // (the arrival), both answer the shop itself.
    MM_TrackerAdapter_TestLiveSeam(1);
    const GameId prevAreaGame = Context_GetCurrentGame();
    Context_SetCurrentGame(GAME_MM);
    int currentMismatches = 0;
    int currentAsked = 0;
    for (const auto& [regionId, region] : Rando::Logic::Regions) {
        const SceneId own = (SceneId)Play_GetOriginalSceneId(region.sceneId);
        if (own == SCENE_KAKUSIANA || !Rando::CheckTracker::IsListedUnderParentScene(own)) {
            continue;
        }
        for (const auto& [_, regionExit] : region.exits) {
            if (regionExit.returnEntrance == ONE_WAY_EXIT ||
                Rando::Logic::GetRegionIdFromEntrance(regionExit.returnEntrance) != regionId) {
                continue;
            }
            gSaveContext.save.entrance = regionExit.returnEntrance;
            MM_TrackerAdapter_TestSceneSeam(1, (int)own);
            uint16_t paneKey = 0;
            const bool known = Combo_TrackerCurrentArea((uint8_t)GAME_MM, &paneKey);
            const int nativeScene = MM_CheckTracker_TestScrollTargetScene((int)region.sceneId);
            currentAsked++;
            if (!known || paneKey != MM_TrackerAdapter_TestSceneAreaKey(nativeScene) || nativeScene == (int)own) {
                printf(
                    "[TEST] P: inside region %d (scene %d): the pane's current area %d:%04X, MM's tracker scene %d\n",
                    (int)regionId, (int)own, known ? 1 : 0, (unsigned)paneKey, nativeScene);
                currentMismatches++;
            }
            break;
        }
    }
    gSaveContext.save.entrance = kArrivalEntrance;
    MM_TrackerAdapter_TestSceneSeam(1, (int)SCENE_BOMYA);
    uint16_t arrivalKey = 0;
    const bool arrivalKnown = Combo_TrackerCurrentArea((uint8_t)GAME_MM, &arrivalKey);
    const int arrivalNative = MM_CheckTracker_TestScrollTargetScene((int)SCENE_BOMYA);
    MM_TrackerAdapter_TestSceneSeam(-1, 0);
    MM_TrackerAdapter_TestLiveSeam(-1);
    Combo_TrackerResetCurrentAreas();
    Context_SetCurrentGame(prevAreaGame);
    printf("[TEST] P: current area asked in %d interior region(s), %d mismatch(es); in the Bomb Shop from the "
           "arrival %d:%04X, MM's tracker scene %d\n",
           currentAsked, currentMismatches, arrivalKnown ? 1 : 0, (unsigned)arrivalKey, arrivalNative);
    EDI_CHECK(currentAsked >= 20 && currentMismatches == 0,
              "P: inside an interior the pane's current area is not the scene MM's tracker scrolls to and filters by");
    EDI_CHECK(arrivalKnown && arrivalKey == MM_TrackerAdapter_TestSceneAreaKey(SCENE_BOMYA) &&
                  arrivalNative == (int)SCENE_BOMYA,
              "P: inside the Bomb Shop entered from no interior region, the current area is not the shop itself");

    // ---- R (#1711): every check (including non-drops) is listed where MM's tracker lists it -------
    auto placeKeySet = [](RandoCheckId checkId, int* outCount) {
        uint16_t keys[kEdiPlaceCap] = {};
        const int count = MM_TrackerAdapter_TestAreaPlaces((uint16_t)checkId, keys, nullptr, kEdiPlaceCap);
        if (outCount != nullptr) {
            *outCount = count;
        }
        const int n = count > 0 ? (count < kEdiPlaceCap ? count : kEdiPlaceCap) : 0;
        return std::set<uint16_t>(keys, keys + n);
    };
    int rListedChecks = 0;
    int rPlaceMismatches = 0;
    int rMultiPlaceNonDrops = 0;
    int rRowAreaMoved = 0;
    for (const auto& [checkId, check] : Rando::StaticData::Checks) {
        if (checkId == RC_UNKNOWN) {
            continue;
        }
        uint16_t nativeScenes[kEdiPlaceCap] = {};
        const int nativeCount = MM_CheckTracker_TestListedScenes((uint16_t)checkId, nativeScenes, kEdiPlaceCap);
        int count = 0;
        const std::set<uint16_t> paneKeys = placeKeySet(checkId, &count);
        if (nativeCount >= 1) {
            rListedChecks++;
            std::set<uint16_t> nativeKeys;
            for (int i = 0; i < nativeCount && i < kEdiPlaceCap; i++) {
                nativeKeys.insert(MM_TrackerAdapter_TestSceneAreaKey(nativeScenes[i]));
            }
            if (count < 1 || count > kEdiPlaceCap || nativeCount > kEdiPlaceCap || paneKeys != nativeKeys) {
                printf("[TEST] R: check %d: the pane lists it in %d place(s), MM's tracker under %d scene(s)\n",
                       (int)checkId, count, nativeCount);
                rPlaceMismatches++;
            }
        }
        if (check.randoCheckType != RCTYPE_ENEMY_DROP) {
            if (count > 1) {
                rMultiPlaceNonDrops++;
            }
            const SceneId own = (SceneId)Play_GetOriginalSceneId(check.sceneId);
            if (!Rando::CheckTracker::IsListedUnderParentScene(own)) {
                const uint16_t rowKey = MM_TrackerAdapter_TestStaticAreaKey((uint16_t)checkId);
                const uint16_t ownKey = MM_TrackerAdapter_TestSceneAreaKey(own);
                if (rowKey != ownKey) {
                    printf("[TEST] R: non-drop check %d: row area %04X moved off own scene %04X\n", (int)checkId,
                           (unsigned)rowKey, (unsigned)ownKey);
                    rRowAreaMoved++;
                }
            }
        }
    }
    int moonTearCount = 0;
    const std::set<uint16_t> moonTearPlaces = placeKeySet(RC_ASTRAL_OBSERVATORY_MOON_TEAR, &moonTearCount);
    const uint16_t moonTearRow = MM_TrackerAdapter_TestStaticAreaKey((uint16_t)RC_ASTRAL_OBSERVATORY_MOON_TEAR);
    int postboxCount = 0;
    const std::set<uint16_t> postboxPlaces = placeKeySet(RC_CLOCK_TOWN_POSTBOX, &postboxCount);
    int oathCount = 0;
    const std::set<uint16_t> oathPlaces = placeKeySet(RC_GIANTS_CHAMBER_OATH_TO_ORDER, &oathCount);
    const uint16_t oathRow = MM_TrackerAdapter_TestStaticAreaKey((uint16_t)RC_GIANTS_CHAMBER_OATH_TO_ORDER);
    const std::set<uint16_t> expectedMoonTear = { MM_TrackerAdapter_TestSceneAreaKey(SCENE_00KEIKOKU) };
    const std::set<uint16_t> expectedPostbox = {
        MM_TrackerAdapter_TestSceneAreaKey(SCENE_TOWN),
        MM_TrackerAdapter_TestSceneAreaKey(SCENE_BACKTOWN),
        MM_TrackerAdapter_TestSceneAreaKey(SCENE_CLOCKTOWER),
    };
    const std::set<uint16_t> expectedOath = {
        MM_TrackerAdapter_TestSceneAreaKey(SCENE_SEA_BS),
        MM_TrackerAdapter_TestSceneAreaKey(SCENE_HAKUGIN_BS),
        MM_TrackerAdapter_TestSceneAreaKey(SCENE_INISIE_BS),
        MM_TrackerAdapter_TestSceneAreaKey(SCENE_MITURIN_BS),
    };
    printf("[TEST] R: %d listed check(s), %d place mismatch(es), %d multi-place non-drop(s), %d row-area move(s); "
           "Moon's Tear %d place(s) (row %04X), Postbox %d place(s), Oath %d place(s) (row %04X)\n",
           rListedChecks, rPlaceMismatches, rMultiPlaceNonDrops, rRowAreaMoved, moonTearCount, (unsigned)moonTearRow,
           postboxCount, oathCount, (unsigned)oathRow);
    EDI_CHECK(rListedChecks >= 1000, "R: fewer than 1000 listed checks, so R is vacuous");
    EDI_CHECK(rMultiPlaceNonDrops >= 5, "R: fewer than 5 multi-place non-drop checks, so R is vacuous");
    EDI_CHECK(rPlaceMismatches == 0,
              "R: a check is not listed by the combo pane under the exact scenes MM's own tracker lists it under");
    EDI_CHECK(rRowAreaMoved == 0, "R: a non-drop check's row area moved off its own scene");
    EDI_CHECK(moonTearCount == 1 && moonTearPlaces == expectedMoonTear &&
                  moonTearRow == MM_TrackerAdapter_TestSceneAreaKey(SCENE_TENMON_DAI),
              "R: Moon's Tear is not listed in Termina Field alone while keeping Astral Observatory as its row area");
    EDI_CHECK(postboxCount == 3 && postboxPlaces == expectedPostbox,
              "R: Clock Town Postbox is not listed under East, North and South Clock Town");
    EDI_CHECK(oathCount == 4 && oathPlaces == expectedOath &&
                  oathRow == MM_TrackerAdapter_TestSceneAreaKey(SCENE_KYOJINNOMA),
              "R: Oath to Order is not listed in all four boss scenes while keeping Giants' Chamber as its row area");

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
