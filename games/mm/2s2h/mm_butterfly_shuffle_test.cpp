/**
 * ROM-free locks for Majora's Mask's butterfly shuffle ("Shuffle Butterflies",
 * #1407; upstream 2Ship's Butterfly Shuffle b690db6fd and its Spring Mountain
 * Village logic fix). CTest label "redship". Two rows, two entry points, shaped
 * like mm_tree_shuffle_test.cpp:
 *
 * ============================================================================
 * mm-butterfly-shuffle (MMButterflyShuffle) -- option, page row, checks, pool
 * ============================================================================
 *
 *  (1) The option exists under its upstream key (RO_SHUFFLE_BUTTERFLIES, CVar
 *      gRando.Options.RO_SHUFFLE_BUTTERFLIES) and defaults OFF.
 *  (2) The MM Randomizer page draws it: a live, default-off checkbox under
 *      "Shuffle Items" labelled "Shuffle Butterflies", right after "Shuffle Tree
 *      Drops" (upstream's menu order), whose tooltip says what MM's butterflies
 *      do (no fairy spawns; the item is given at the Deku Stick's tip).
 *  (3) The check table holds exactly the 25 butterflies, all of one check type no
 *      other check carries, each listed by some logic region, none a cross-game
 *      host class (an Ocarina of Time item never lands on a butterfly, as it never
 *      lands on a tree), and each a row of Randomizer > MM Locations.
 *  (4) Hint areas: each of the 12 grotto butterflies (SCENE_KAKUSIANA, which has
 *      no default area) names the area its grotto opens from, exactly as listed
 *      below, and each of the 13 overworld butterflies its scene's area; every
 *      one is hintable (HintArea::HasArea).
 *  (5) Pool generation: with the option off no butterfly enters the check pool and
 *      the pool is exactly the pool with it on minus the butterflies, in the same
 *      order; with it on every butterfly enters it, with one item each.
 *
 * ============================================================================
 * mm-butterfly-shuffle-hooks (MMButterflyShuffleHooks) -- the provider runs
 * ============================================================================
 *
 * The butterfly behaviour (Rando/ActorBehavior/EnButte.cpp) registers from
 * Rando::ActorBehavior::OnFileLoad, the path a rando save load runs. This row
 * drives THAT function, never a symbol of EnButte.cpp (a reference from here
 * would pull the object into the link on its own and hide an elided provider,
 * the #516/#1071 class), and dispatches the hook through the id-keyed leg of
 * MM_GameHooks_ExecuteOnActorUpdate (GameExports_SingleExe.cpp), the call
 * z_actor.c makes after every actor update:
 *
 *  - Off leg: a rando save with the option off registers nothing on
 *    OnActorUpdate[ACTOR_OBJ_MURE], and a Termina Field butterfly group going from
 *    culled to active keeps its two butterflies vanilla (update, draw, params, no
 *    check).
 *  - On leg: the option on adds exactly one registrant, and the same group's two
 *    butterflies take RC_TERMINA_FIELD_BUTTERFLY_01 / _02, a new update and draw,
 *    and lose the golden-butterfly params bit (vanilla's own stick attraction).
 *
 * Restores the SaveContext image, the play state and the option-off drive on the
 * way out, so it has no ordering constraints.
 */

#ifndef RSBS_SINGLE_EXECUTABLE
extern "C" int MM_ButterflyShuffle_RunHeadless(void) {
    return 0;
}
extern "C" int MM_ButterflyShuffleHooks_RunHeadless(void) {
    return 0;
}
#else

#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <set>
#include <vector>

#include <libultraship/bridge/consolevariablebridge.h>
#include <ship/Context.h>

#include "2s2h/GameInteractor/GameInteractor.h"
#include "2s2h/ObjectExtension/ActorListIndex.h"
#include "2s2h/ObjectExtension/ObjectExtension.h"
#include "2s2h/Rando/ActorBehavior/ActorBehavior.h"
#include "2s2h/Rando/Foreign.h"
#include "2s2h/Rando/HintArea.h"
#include "2s2h/Rando/Logic/Logic.h"
#include "2s2h/Rando/Rando.h"
#include "2s2h/Rando/StaticData/StaticData.h"
#include "2s2h/ShipUtils.h"
#include "mm_game_hooks.h"

// src/common: the page's descriptor table, Randomizer > MM Locations and the
// shared area vocabulary.
#include "combo_area.h"
#include "combo_mm_locations_view.h"
#include "combo_mm_options_view.h"
#include "game.h"

extern "C" {
#include "variables.h"
#include "functions.h"
#include "overlays/actors/ovl_Obj_Mure/z_obj_mure.h"
void MM_Rando_Init(void);
void MM_ObjMure_CulledState(ObjMure*, PlayState*);
void MM_ObjMure_ActiveState(ObjMure*, PlayState*);
void MM_EnButte_Draw(Actor* thisx, PlayState* play);
void MM_EnButte_Update(Actor* thisx, PlayState* play);
}

namespace {

int Fail(int code, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    printf("[TEST] FAIL(%d): ", code);
    vprintf(fmt, args);
    printf("\n");
    va_end(args);
    return code;
}

constexpr const char* kOptionCVar = "gRando.Options.RO_SHUFFLE_BUTTERFLIES";
constexpr const char* kRowLabel = "Shuffle Butterflies";
constexpr const char* kRowTooltip = "Certain butterflies carry a randomized item. Hold out a Deku Stick near one and "
                                    "it flies to the stick's tip, giving you the item.";

struct ButterflyRow {
    RandoCheckId id;
    uint8_t area; // the area its hint names
};

// Every butterfly check, with the area its hint names. The grotto rows come from
// HintAreaSingleExe.cpp's overrides (#1407), each the area its grotto's grass
// names; the others from their scene's default.
const ButterflyRow kButterflies[] = {
    { RC_DEKU_PALACE_BEAN_SALESMAN_GROTTO_BUTTERFLY_01, COMBO_AREA_MM_DEKU_PALACE },
    { RC_DEKU_PALACE_BEAN_SALESMAN_GROTTO_BUTTERFLY_02, COMBO_AREA_MM_DEKU_PALACE },
    { RC_DEKU_PALACE_BEAN_SALESMAN_GROTTO_BUTTERFLY_03, COMBO_AREA_MM_DEKU_PALACE },
    { RC_DEKU_PALACE_BEAN_SALESMAN_GROTTO_BUTTERFLY_04, COMBO_AREA_MM_DEKU_PALACE },
    { RC_GREAT_BAY_COAST_BUTTERFLY_01, COMBO_AREA_MM_GREAT_BAY_COAST },
    { RC_GREAT_BAY_COAST_BUTTERFLY_02, COMBO_AREA_MM_GREAT_BAY_COAST },
    { RC_GREAT_BAY_COAST_COW_GROTTO_BUTTERFLY_01, COMBO_AREA_MM_GREAT_BAY_COAST },
    { RC_GREAT_BAY_COAST_COW_GROTTO_BUTTERFLY_02, COMBO_AREA_MM_GREAT_BAY_COAST },
    { RC_GREAT_BAY_COAST_COW_GROTTO_BUTTERFLY_03, COMBO_AREA_MM_GREAT_BAY_COAST },
    { RC_MOUNTAIN_VILLAGE_SPRING_BUTTERFLY_01, COMBO_AREA_MM_MOUNTAIN_VILLAGE },
    { RC_MOUNTAIN_VILLAGE_SPRING_BUTTERFLY_02, COMBO_AREA_MM_MOUNTAIN_VILLAGE },
    { RC_MOUNTAIN_VILLAGE_SPRING_BUTTERFLY_03, COMBO_AREA_MM_MOUNTAIN_VILLAGE },
    { RC_MOUNTAIN_VILLAGE_SPRING_BUTTERFLY_04, COMBO_AREA_MM_MOUNTAIN_VILLAGE },
    { RC_MOUNTAIN_VILLAGE_SPRING_BUTTERFLY_05, COMBO_AREA_MM_MOUNTAIN_VILLAGE },
    { RC_MOUNTAIN_VILLAGE_SPRING_BUTTERFLY_06, COMBO_AREA_MM_MOUNTAIN_VILLAGE },
    { RC_MOUNTAIN_VILLAGE_SPRING_BUTTERFLY_07, COMBO_AREA_MM_MOUNTAIN_VILLAGE },
    { RC_MOUNTAIN_VILLAGE_SPRING_BUTTERFLY_08, COMBO_AREA_MM_MOUNTAIN_VILLAGE },
    { RC_MOUNTAIN_VILLAGE_SPRING_BUTTERFLY_09, COMBO_AREA_MM_MOUNTAIN_VILLAGE },
    { RC_TERMINA_FIELD_BUTTERFLY_01, COMBO_AREA_MM_TERMINA_FIELD },
    { RC_TERMINA_FIELD_BUTTERFLY_02, COMBO_AREA_MM_TERMINA_FIELD },
    { RC_TERMINA_FIELD_COW_GROTTO_BUTTERFLY_01, COMBO_AREA_MM_TERMINA_FIELD },
    { RC_TERMINA_FIELD_COW_GROTTO_BUTTERFLY_02, COMBO_AREA_MM_TERMINA_FIELD },
    { RC_TERMINA_FIELD_COW_GROTTO_BUTTERFLY_03, COMBO_AREA_MM_TERMINA_FIELD },
    { RC_TERMINA_FIELD_GOSSIP_STONE_GROTTO_3_BUTTERFLY_01, COMBO_AREA_MM_TERMINA_FIELD },
    { RC_TERMINA_FIELD_GOSSIP_STONE_GROTTO_3_BUTTERFLY_02, COMBO_AREA_MM_TERMINA_FIELD },
};
constexpr int kButterflyCount = (int)(sizeof(kButterflies) / sizeof(kButterflies[0]));
static_assert(kButterflyCount == 25, "2Ship's butterfly shuffle has 25 checks");
// THE APPEND-ONLY LOCK: the butterflies sit after the last check main had, so no
// check id a world or a save already holds moves.
// The exact numbers (2617..2641, option 56) are locked in GameExports_SingleExe.cpp.
static_assert(RC_DEKU_PALACE_BEAN_SALESMAN_GROTTO_BUTTERFLY_01 > RC_MOUNTAIN_VILLAGE_SPRING_KEATON_GRASS_WONDER_ITEM,
              "the butterfly checks are appended after every check id before them (#1407, after #1406 and #1410)");
static_assert(RO_SHUFFLE_BUTTERFLIES > RO_SHUFFLE_HIVE_DROPS,
              "RO_SHUFFLE_BUTTERFLIES is appended after every option id before it (#1407, after #1406 and #1410)");
// EnButte.cpp's butterflyMap names each group's FIRST check and a count, and
// reaches the rest by adding 0..count-1 (and the Termina Field cow grotto by the
// distance between the two cow grottos' first checks), so each group's checks
// must stay consecutive and in this order.
static_assert(RC_DEKU_PALACE_BEAN_SALESMAN_GROTTO_BUTTERFLY_04 ==
                      RC_DEKU_PALACE_BEAN_SALESMAN_GROTTO_BUTTERFLY_01 + 3 &&
                  RC_GREAT_BAY_COAST_BUTTERFLY_02 == RC_GREAT_BAY_COAST_BUTTERFLY_01 + 1 &&
                  RC_GREAT_BAY_COAST_COW_GROTTO_BUTTERFLY_03 == RC_GREAT_BAY_COAST_COW_GROTTO_BUTTERFLY_01 + 2 &&
                  RC_MOUNTAIN_VILLAGE_SPRING_BUTTERFLY_09 == RC_MOUNTAIN_VILLAGE_SPRING_BUTTERFLY_01 + 8 &&
                  RC_TERMINA_FIELD_BUTTERFLY_02 == RC_TERMINA_FIELD_BUTTERFLY_01 + 1 &&
                  RC_TERMINA_FIELD_COW_GROTTO_BUTTERFLY_03 == RC_TERMINA_FIELD_COW_GROTTO_BUTTERFLY_01 + 2 &&
                  RC_TERMINA_FIELD_GOSSIP_STONE_GROTTO_3_BUTTERFLY_02 ==
                      RC_TERMINA_FIELD_GOSSIP_STONE_GROTTO_3_BUTTERFLY_01 + 1,
              "each butterfly group's checks are consecutive (EnButte.cpp's butterflyMap adds to the first)");
static_assert(RO_GENERIC_OFF == 0, "a world saved before the option reads its missing entry as 0, which must be off");

/** One pool generation over a fresh save info, every option at its default but
 *  the butterfly option, which is `on`. Seeded, so the run is deterministic. */
void GenerateWithButterflies(bool on, std::vector<RandoCheckId>& checkPool, std::vector<RandoItemId>& itemPool) {
    auto info = std::make_unique<RandoSaveInfo>();
    memset(info.get(), 0, sizeof(RandoSaveInfo));
    for (auto& [randoOptionId, row] : Rando::StaticData::Options) {
        info->randoSaveOptions[randoOptionId] = row.defaultValue;
    }
    info->randoSaveOptions[RO_LOGIC] = RO_LOGIC_GLITCHLESS;
    info->randoSaveOptions[RO_SHUFFLE_BUTTERFLIES] = on ? RO_GENERIC_YES : RO_GENERIC_NO;
    checkPool.clear();
    itemPool.clear();
    Ship_Random_Seed(0xB077u);
    Rando::Logic::GeneratePools(*info, checkPool, itemPool);
}

int RunOptionAndPool() {
    // ---- (1) the option -----------------------------------------------------
    if (Rando::StaticData::GetOptionIdFromName("RO_SHUFFLE_BUTTERFLIES") != RO_SHUFFLE_BUTTERFLIES) {
        return Fail(1, "RO_SHUFFLE_BUTTERFLIES is not in MM's option table under its upstream name (#1407)");
    }
    const auto& optionRow = Rando::StaticData::Options.at(RO_SHUFFLE_BUTTERFLIES);
    if (optionRow.defaultValue != RO_GENERIC_OFF) {
        return Fail(2, "Shuffle Butterflies defaults to %u, expected off", (unsigned)optionRow.defaultValue);
    }
    if (strcmp(optionRow.cvar, kOptionCVar) != 0) {
        return Fail(3, "Shuffle Butterflies binds '%s', expected '%s'", optionRow.cvar, kOptionCVar);
    }

    // ---- (2) the page row ---------------------------------------------------
    MM_RandoOptionsUi_Register();
    const ComboMMOptionDesc* row = Combo_MMOptionById((uint16_t)RO_SHUFFLE_BUTTERFLIES);
    if (row == NULL) {
        return Fail(4, "the MM Randomizer page has no row for Shuffle Butterflies");
    }
    if (row->label == NULL || strcmp(row->label, kRowLabel) != 0 || row->tooltip == NULL ||
        strcmp(row->tooltip, kRowTooltip) != 0) {
        return Fail(5, "the Shuffle Butterflies row reads '%s' / '%s', expected '%s' / '%s'",
                    row->label ? row->label : "(null)", row->tooltip ? row->tooltip : "(null)", kRowLabel, kRowTooltip);
    }
    if (row->widget != (uint8_t)COMBO_MM_WIDGET_CHECKBOX || row->defaultValue != 0 ||
        row->liveness != (uint8_t)COMBO_MM_LIVENESS_LIVE ||
        strcmp(Combo_MMOptionGroupName(row->group), "Shuffle Items") != 0) {
        return Fail(6, "the Shuffle Butterflies row is not a live, default-off checkbox under Shuffle Items");
    }
    int treeAt = -1;
    int butterflyAt = -1;
    for (int i = 0; i < Combo_MMOptionCount(); i++) {
        const ComboMMOptionDesc* desc = Combo_MMOptionAt(i);
        if (desc->id == (uint16_t)RO_SHUFFLE_TREE_DROPS) {
            treeAt = i;
        } else if (desc->id == (uint16_t)RO_SHUFFLE_BUTTERFLIES) {
            butterflyAt = i;
        }
    }
    if (treeAt < 0 || butterflyAt != treeAt + 1) {
        return Fail(7, "Shuffle Butterflies is row %d, Shuffle Tree Drops row %d: it belongs right after the tree row",
                    butterflyAt, treeAt);
    }

    // ---- (3) the check table ------------------------------------------------
    std::set<RandoCheckId> butterflies;
    for (const ButterflyRow& b : kButterflies) {
        if (Rando::StaticData::Checks.find(b.id) == Rando::StaticData::Checks.end()) {
            return Fail(8, "MM's check table has no row for butterfly check %d", (int)b.id);
        }
        butterflies.insert(b.id);
    }
    for (const auto& [randoCheckId, check] : Rando::StaticData::Checks) {
        const bool isButterfly = butterflies.count(randoCheckId) != 0;
        if (isButterfly != (check.randoCheckType == RCTYPE_BUTTERFLY)) {
            return Fail(9, "%s %s RCTYPE_BUTTERFLY: the type must hold exactly the %d butterflies", check.name,
                        isButterfly ? "lacks" : "carries", kButterflyCount);
        }
    }
    MM_Rando_Init(); // fills Rando::Logic::Regions (irreversible; ShipInit::InitAll)
    std::set<RandoCheckId> listed;
    for (const auto& [regionId, region] : Rando::Logic::Regions) {
        for (const auto& [checkId, condition] : region.checks) {
            listed.insert(checkId);
        }
    }
    for (RandoCheckId id : butterflies) {
        if (listed.count(id) == 0) {
            return Fail(10, "%s is in no logic region, so no seed can ever place on it",
                        Rando::StaticData::Checks.at(id).name);
        }
        if (Rando::Foreign::IsForeignHostClass(id)) {
            return Fail(11, "%s is a cross-game host class: an Ocarina of Time item would wait on a butterfly",
                        Rando::StaticData::Checks.at(id).name);
        }
    }
    MM_RandoLocationsUi_Register();
    std::set<RandoCheckId> located;
    for (int i = 0; i < Combo_MMLocationCount(); i++) {
        located.insert((RandoCheckId)Combo_MMLocationAt(i)->id);
    }
    for (RandoCheckId id : butterflies) {
        if (located.count(id) == 0) {
            return Fail(12, "%s is not a row of Randomizer > MM Locations", Rando::StaticData::Checks.at(id).name);
        }
    }

    // ---- (4) hint areas -----------------------------------------------------
    int grottoRows = 0;
    for (const ButterflyRow& b : kButterflies) {
        const ComboArea area = Combo_Area_OfCheck((uint8_t)GAME_MM, (uint16_t)b.id);
        if (area.area != b.area) {
            return Fail(13, "%s names area %u in hints, expected %u (%s)", Rando::StaticData::Checks.at(b.id).name,
                        (unsigned)area.area, (unsigned)b.area,
                        Rando::StaticData::Checks.at(b.id).sceneId == SCENE_KAKUSIANA
                            ? "the area its grotto opens from, HintAreaSingleExe.cpp's overrides"
                            : "its scene's area");
        }
        if (!Rando::HintArea::HasArea(b.id)) {
            return Fail(14, "%s has no hint area, so no gossip stone can hint it",
                        Rando::StaticData::Checks.at(b.id).name);
        }
        grottoRows += Rando::StaticData::Checks.at(b.id).sceneId == SCENE_KAKUSIANA ? 1 : 0;
    }
    if (grottoRows != 12) {
        return Fail(15, "%d butterflies are grotto checks, expected 12: leg (4) would not reach every override",
                    grottoRows);
    }

    // ---- (5) pool generation ------------------------------------------------
    const std::string savedExcluded = CVarGetString("gRando.ExcludedChecks", "");
    CVarClear("gRando.ExcludedChecks");
    std::vector<RandoCheckId> offChecks, onChecks;
    std::vector<RandoItemId> offItems, onItems;
    GenerateWithButterflies(false, offChecks, offItems);
    GenerateWithButterflies(true, onChecks, onItems);
    if (!savedExcluded.empty()) {
        CVarSetString("gRando.ExcludedChecks", savedExcluded.c_str());
    }
    for (RandoCheckId id : offChecks) {
        if (butterflies.count(id) != 0) {
            return Fail(16, "with Shuffle Butterflies off, %s entered the check pool",
                        Rando::StaticData::Checks.at(id).name);
        }
    }
    for (RandoCheckId id : butterflies) {
        if (std::find(onChecks.begin(), onChecks.end(), id) == onChecks.end()) {
            return Fail(17, "with Shuffle Butterflies on, %s did not enter the check pool",
                        Rando::StaticData::Checks.at(id).name);
        }
    }
    std::vector<RandoCheckId> onWithoutButterflies;
    for (RandoCheckId id : onChecks) {
        if (butterflies.count(id) == 0) {
            onWithoutButterflies.push_back(id);
        }
    }
    if (onWithoutButterflies != offChecks) {
        return Fail(18,
                    "the option-off check pool (%zu) is not the option-on pool minus the butterflies (%zu): the "
                    "option moves checks it does not own",
                    offChecks.size(), onWithoutButterflies.size());
    }
    if (onItems.size() != offItems.size() + butterflies.size()) {
        return Fail(19, "the option-on item pool holds %zu items, expected %zu (one per butterfly)", onItems.size(),
                    offItems.size() + butterflies.size());
    }
    printf("[TEST] mm-butterfly-shuffle: %d butterflies (%d in grottos), option off -> %zu checks, option on -> "
           "%zu checks\n",
           kButterflyCount, grottoRows, offChecks.size(), onChecks.size());
    return 0;
}

// ---------------------------------------------------------------------------
// The provider row.
// ---------------------------------------------------------------------------

// Settled count: S2H::GameHooks unregistration is deferred until the next
// Execute of that hook type, and COND_ID_HOOK unregisters-then-registers on
// every drive, so the raw map can credit a hook already slated for removal.
size_t MureUpdateCount() {
    S2H::GameHooks::FlushPendingUnregistrations<GameInteractor::OnActorUpdate>();
    auto& buckets = S2H::GameHooks::Registry<GameInteractor::OnActorUpdate>::functionsForID;
    auto it = buckets.find(ACTOR_OBJ_MURE);
    return it == buckets.end() ? 0u : it->second.size();
}

/** The production arm path: a rando save with the option at `on`, then the
 *  behaviour registrars a save load runs (Rando.cpp OnSaveLoadHandler). */
void DriveArmState(bool on) {
    gSaveContext.save.shipSaveInfo.saveType = SAVETYPE_RANDO;
    RANDO_SAVE_OPTIONS[RO_SHUFFLE_BUTTERFLIES] = on ? RO_GENERIC_ON : RO_GENERIC_OFF;
    Rando::ActorBehavior::OnFileLoad();
}

struct Group {
    ObjMure* mure;
    Actor* butterflies[2];
};

/** Termina Field's butterfly group (actor list index 171 in room 0, EnButte.cpp's
 *  butterflyMap) and its two butterflies, as vanilla spawns them. */
void ArmGroup(Group& g) {
    memset(g.mure, 0, sizeof(ObjMure));
    g.mure->actor.id = ACTOR_OBJ_MURE;
    SetActorListIndex(&g.mure->actor, 171);
    for (int i = 0; i < 2; i++) {
        memset(g.butterflies[i], 0, sizeof(Actor));
        g.butterflies[i]->id = ACTOR_EN_BUTTE;
        g.butterflies[i]->params = 1; // a golden butterfly: vanilla's stick attraction
        g.butterflies[i]->update = MM_EnButte_Update;
        g.butterflies[i]->draw = MM_EnButte_Draw;
        g.mure->children[i] = g.butterflies[i];
        RANDO_SAVE_CHECKS[RC_TERMINA_FIELD_BUTTERFLY_01 + i].shuffled = true;
        RANDO_SAVE_CHECKS[RC_TERMINA_FIELD_BUTTERFLY_01 + i].cycleObtained = false;
    }
}

/** Two frames of the group's update, culled then active, each followed by the
 *  id-keyed OnActorUpdate leg MM_GameHooks_ExecuteOnActorUpdate runs. */
void RunCullToActive(Group& g) {
    g.mure->actionFunc = MM_ObjMure_CulledState;
    S2H::GameHooks::ExecuteForID<GameInteractor::OnActorUpdate>(ACTOR_OBJ_MURE, &g.mure->actor);
    g.mure->actionFunc = MM_ObjMure_ActiveState;
    S2H::GameHooks::ExecuteForID<GameInteractor::OnActorUpdate>(ACTOR_OBJ_MURE, &g.mure->actor);
}

int RunHookLegs(PlayState* play, Group& g) {
    play->sceneId = SCENE_00KEIKOKU;
    play->roomCtx.curRoom.num = 0;
    MM_gPlayState = play;

    // ---- Off leg (non-vacuity of the on leg) -------------------------------
    DriveArmState(false);
    const size_t before = MureUpdateCount();
    ArmGroup(g);
    RunCullToActive(g);
    for (int i = 0; i < 2; i++) {
        const Actor* b = g.butterflies[i];
        if (b->update != MM_EnButte_Update || b->draw != MM_EnButte_Draw || (b->params & 1) == 0 ||
            Rando::ActorBehavior::GetObjectRandoCheckId(g.butterflies[i]) != RC_UNKNOWN) {
            return Fail(20, "with Shuffle Butterflies off, butterfly %d of the Termina Field group was changed", i);
        }
    }
    ObjectExtension::GetInstance().Free(&g.mure->actor);

    // ---- On leg, through the production path --------------------------------
    DriveArmState(true);
    const size_t after = MureUpdateCount();
    if (after != before + 1) {
        return Fail(21,
                    "OnActorUpdate[ACTOR_OBJ_MURE] went %zu -> %zu when Shuffle Butterflies armed, expected exactly "
                    "+1: the butterfly behaviour (Rando/ActorBehavior/EnButte.cpp) is not in the link, is not reached "
                    "from ActorBehavior::OnFileLoad, or a second registrant made the probe unattributable",
                    before, after);
    }
    ArmGroup(g);
    RunCullToActive(g);
    for (int i = 0; i < 2; i++) {
        const Actor* b = g.butterflies[i];
        const RandoCheckId want = (RandoCheckId)(RC_TERMINA_FIELD_BUTTERFLY_01 + i);
        const RandoCheckId got = Rando::ActorBehavior::GetObjectRandoCheckId(g.butterflies[i]);
        if (got != want) {
            return Fail(22,
                        "with Shuffle Butterflies on, butterfly %d of the Termina Field group holds check %d, "
                        "expected %d",
                        i, (int)got, (int)want);
        }
        if (b->update == MM_EnButte_Update || b->draw == MM_EnButte_Draw) {
            return Fail(23, "with Shuffle Butterflies on, butterfly %d kept the vanilla update or draw", i);
        }
        if ((b->params & 1) != 0) {
            return Fail(24,
                        "with Shuffle Butterflies on, butterfly %d kept the golden-butterfly bit, so vanilla's "
                        "own stick attraction still runs",
                        i);
        }
    }
    return 0;
}

} // namespace

extern "C" int MM_ButterflyShuffle_RunHeadless(void) {
    printf("[TEST] mm-butterfly-shuffle: Shuffle Butterflies is an MM option, a page row, 25 checks with hint "
           "areas and a pool switch (#1407)\n");
    auto ctx = Ship::Context::GetInstance();
    if (ctx == nullptr || ctx->GetConsoleVariables() == nullptr) {
        return Fail(1, "Ship::Context or its ConsoleVariables are missing; run the shared bring-up first");
    }
    const int rc = RunOptionAndPool();
    if (rc != 0) {
        return rc;
    }
    printf("[TEST] mm-butterfly-shuffle: PASS\n");
    return 0;
}

extern "C" int MM_ButterflyShuffleHooks_RunHeadless(void) {
    printf("[TEST] mm-butterfly-shuffle-hooks: the butterfly behaviour arms from ActorBehavior::OnFileLoad and "
           "runs (#1407)\n");
    auto ctx = Ship::Context::GetInstance();
    if (ctx == nullptr || ctx->GetConsoleVariables() == nullptr) {
        return Fail(1, "Ship::Context or its ConsoleVariables are missing; run the shared bring-up first");
    }

    auto saved = std::make_unique<uint8_t[]>(sizeof(gSaveContext));
    memcpy(saved.get(), &gSaveContext, sizeof(gSaveContext));
    PlayState* const savedPlay = MM_gPlayState;

    PlayState* play = static_cast<PlayState*>(calloc(1, sizeof(PlayState)));
    Group g = {};
    g.mure = static_cast<ObjMure*>(calloc(1, sizeof(ObjMure)));
    g.butterflies[0] = static_cast<Actor*>(calloc(1, sizeof(Actor)));
    g.butterflies[1] = static_cast<Actor*>(calloc(1, sizeof(Actor)));
    int rc = 0;
    if (play == nullptr || g.mure == nullptr || g.butterflies[0] == nullptr || g.butterflies[1] == nullptr) {
        rc = Fail(1, "could not allocate the fixtures");
    } else {
        rc = RunHookLegs(play, g);
    }

    // ---- Cleanup (pass and fail) -------------------------------------------
    if (g.mure != nullptr) {
        ObjectExtension::GetInstance().Free(&g.mure->actor);
    }
    for (Actor* b : g.butterflies) {
        if (b != nullptr) {
            ObjectExtension::GetInstance().Free(b);
        }
    }
    MM_gPlayState = savedPlay;
    memcpy(&gSaveContext, saved.get(), sizeof(gSaveContext));
    Rando::ActorBehavior::OnFileLoad();
    free(play);
    free(g.mure);
    free(g.butterflies[0]);
    free(g.butterflies[1]);

    if (rc != 0) {
        return rc;
    }
    printf("[TEST] mm-butterfly-shuffle-hooks: PASS (one OnActorUpdate[ACTOR_OBJ_MURE] registrant armed, the "
           "Termina Field group's butterflies take their checks only with the option on)\n");
    return 0;
}

#endif /* RSBS_SINGLE_EXECUTABLE */
