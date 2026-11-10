/**
 * ROM-free locks for Majora's Mask's wonder item shuffle ("Shuffle Wonder Items",
 * #1406; upstream 2Ship's Wonder Item shuffle, 630536f68, with the 699459aea,
 * 44fffecd3 and 99bb6318c follow-ups). CTest label "redship". Two rows, two entry
 * points, modelled on mm_tree_shuffle_test.cpp (#1121):
 *
 * ============================================================================
 * mm-wonder-items (MMWonderItems) -- the option, the page row, the checks, the
 * id runs and the pool
 * ============================================================================
 *
 *  (1) The option exists under its upstream key (RO_SHUFFLE_WONDER_ITEMS, CVar
 *      gRando.Options.RO_SHUFFLE_WONDER_ITEMS) and defaults OFF.
 *  (2) The MM Randomizer page draws it: a live, default-off checkbox labelled
 *      "Shuffle Wonder Items" under "Shuffle Items".
 *  (3) The check table holds exactly the 113 wonder items, all of one check type
 *      that no other check carries, every one listed by some logic region, none a
 *      cross-game host class, and every one a row of Randomizer > MM
 *      Locations. A check's name is its RC enumerator itself (Checks.cpp's RC()
 *      stringifies the id, PopulateCheckNames converts that spelling and the
 *      locations page falls back to it), so the 113 names resolving to 113
 *      distinct ids (code 7) is the name lock; a non-empty-name assertion on
 *      the page row could not fail, so there is none (code 11 is unused).
 *  (4) The id runs WonderItems.cpp indexes exist: each Hit Tag in its location
 *      map spawns its base check and the two after it (`base + 1`, `base + 2`),
 *      and the Termina Field wall spawns RC_TERMINA_FIELD_WALL_WONDER_ITEM_01 +
 *      sRewardDropsIndex[hour] + 0..2. Every such id must be the wonder item its
 *      name promises (the next number in the same location), so an id inserted
 *      or reordered inside a run fails here.
 *  (5) Pool generation: with the option off no wonder item enters the check
 *      pool and the pool is exactly the pool with it on minus the wonder items,
 *      in the same order; with it on every wonder item enters it.
 *
 * Every id is looked up BY NAME (GetOptionIdFromName / GetCheckIdFromName), so
 * the row compiles at a tree that has none of them, and fails there on (1).
 *
 * ============================================================================
 * mm-wonder-items-hooks (MMWonderItemsHooks) -- the provider arms and runs
 * ============================================================================
 *
 * WonderItems.cpp registers from Rando::ActorBehavior::OnFileLoad, the
 * production path a rando save load runs. This row drives THAT function, never a
 * symbol of WonderItems.cpp itself (a reference from here would pull the object
 * into the link on its own and hide an elided provider, the #516/#1071 class):
 *
 *  - Registry legs: with the option on, OnActorInit[ACTOR_EN_HIT_TAG],
 *    ShouldActorInit[ACTOR_EN_INVISIBLE_RUPPE], ShouldActorInit[ACTOR_EN_SCOPECOIN],
 *    VB_ENEMY_DROP_COLLECTIBLE and VB_GONG_DROP_COLLECTIBLE each gain EXACTLY one
 *    registrant over the option-off drive (Shuffle Enemy Drops, the other
 *    VB_ENEMY_DROP_COLLECTIBLE registrant, is held off in both drives).
 *  - Hit Tag leg: the id-keyed OnActorInit registrants for En_Hit_Tag give a
 *    mapped Hit Tag (Pirates' Fortress plaza, room 0, list index 24) its base
 *    check and a draw function, and leave an unmapped one alone; disarmed they
 *    give nothing (non-vacuity).
 *  - Drop leg: VB_ENEMY_DROP_COLLECTIBLE, dispatched the way z_en_item00.c
 *    dispatches it, suppresses the vanilla drop at a mapped Hit Tag's position
 *    (all three of its checks already collected this cycle, so nothing spawns)
 *    and latches the tag's list index to -1, keeps the vanilla drop anywhere
 *    else, and keeps suppressing at the latched tag. Disarmed, the same dispatch
 *    keeps the vanilla drop (non-vacuity).
 *  - Guard legs: the gong keeps its vanilla rupee once the Swordsman's School
 *    wonder item was collected this cycle, and an unshuffled invisible rupee
 *    keeps spawning.
 *
 * A wonder item that would actually spawn needs a real actor context, so no leg
 * reaches CustomItem::Spawn. Restores the SaveContext image, the play state and
 * the option-off drive on the way out, so it has no ordering constraints.
 */

#ifndef RSBS_SINGLE_EXECUTABLE
extern "C" int MM_WonderItems_RunHeadless(void) {
    return 0;
}
extern "C" int MM_WonderItemsHooks_RunHeadless(void) {
    return 0;
}
#else

#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include <libultraship/bridge/consolevariablebridge.h>
#include <ship/Context.h>

#include "2s2h/GameInteractor/GameInteractor.h"
#include "2s2h/ObjectExtension/ActorListIndex.h"
#include "2s2h/ObjectExtension/ObjectExtension.h"
#include "2s2h/Rando/ActorBehavior/ActorBehavior.h"
#include "2s2h/Rando/Foreign.h"
#include "2s2h/Rando/Logic/Logic.h"
#include "2s2h/Rando/Rando.h"
#include "2s2h/Rando/StaticData/StaticData.h"
#include "2s2h/ShipUtils.h"
#include "mm_game_hooks.h"

// src/common: the page's descriptor table and Randomizer > MM Locations.
#include "combo_mm_locations_view.h"
#include "combo_mm_options_view.h"

extern "C" {
#include "variables.h"
#include "functions.h"
#include "overlays/actors/ovl_Obj_Dora/z_obj_dora.h"
void MM_Rando_Init(void);
// z_en_gakufu.c: the Termina Field wall's reward set per hour, the table
// WonderItems.cpp adds to RC_TERMINA_FIELD_WALL_WONDER_ITEM_01.
extern u8 sRewardDropsIndex[];
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

constexpr const char* kOptionName = "RO_SHUFFLE_WONDER_ITEMS";
constexpr const char* kOptionCVar = "gRando.Options.RO_SHUFFLE_WONDER_ITEMS";
constexpr const char* kRowLabel = "Shuffle Wonder Items";

// Every wonder item check (Types.h, between RC_ZORA_CAPE_TREE_05 and RC_MAX).
const char* const kWonderChecks[] = {
    "RC_CLOCK_TOWN_EAST_WONDER_ITEM_01",
    "RC_CLOCK_TOWN_EAST_WONDER_ITEM_02",
    "RC_CLOCK_TOWN_EAST_WONDER_ITEM_03",
    "RC_CLOCK_TOWN_EAST_WONDER_ITEM_04",
    "RC_CLOCK_TOWN_EAST_WONDER_ITEM_05",
    "RC_CLOCK_TOWN_EAST_WONDER_ITEM_06",
    "RC_CLOCK_TOWN_EAST_WONDER_ITEM_07",
    "RC_CLOCK_TOWN_EAST_WONDER_ITEM_08",
    "RC_CLOCK_TOWN_EAST_WONDER_ITEM_09",
    "RC_CLOCK_TOWN_SOUTH_WONDER_ITEM_01",
    "RC_CLOCK_TOWN_SOUTH_WONDER_ITEM_02",
    "RC_CLOCK_TOWN_SOUTH_WONDER_ITEM_03",
    "RC_CUCCO_SHACK_WONDER_ITEM_01",
    "RC_CUCCO_SHACK_WONDER_ITEM_02",
    "RC_CUCCO_SHACK_WONDER_ITEM_03",
    "RC_CUCCO_SHACK_WONDER_ITEM_04",
    "RC_CUCCO_SHACK_WONDER_ITEM_05",
    "RC_CUCCO_SHACK_WONDER_ITEM_06",
    "RC_IKANA_GRAVEYARD_WONDER_ITEM_01",
    "RC_IKANA_GRAVEYARD_WONDER_ITEM_02",
    "RC_IKANA_GRAVEYARD_WONDER_ITEM_03",
    "RC_IKANA_GRAVEYARD_WONDER_ITEM_04",
    "RC_IKANA_GRAVEYARD_WONDER_ITEM_05",
    "RC_IKANA_GRAVEYARD_WONDER_ITEM_06",
    "RC_IKANA_GRAVEYARD_WONDER_ITEM_07",
    "RC_IKANA_GRAVEYARD_WONDER_ITEM_08",
    "RC_IKANA_GRAVEYARD_WONDER_ITEM_09",
    "RC_IKANA_GRAVEYARD_WONDER_ITEM_10",
    "RC_IKANA_GRAVEYARD_WONDER_ITEM_11",
    "RC_IKANA_GRAVEYARD_WONDER_ITEM_12",
    "RC_OCEAN_SPIDER_HOUSE_WONDER_ITEM_01",
    "RC_OCEAN_SPIDER_HOUSE_WONDER_ITEM_02",
    "RC_OCEAN_SPIDER_HOUSE_WONDER_ITEM_03",
    "RC_OCEAN_SPIDER_HOUSE_WONDER_ITEM_04",
    "RC_OCEAN_SPIDER_HOUSE_WONDER_ITEM_05",
    "RC_OCEAN_SPIDER_HOUSE_WONDER_ITEM_06",
    "RC_OCEAN_SPIDER_HOUSE_WONDER_ITEM_07",
    "RC_OCEAN_SPIDER_HOUSE_WONDER_ITEM_08",
    "RC_OCEAN_SPIDER_HOUSE_WONDER_ITEM_09",
    "RC_PIRATE_FORTRESS_INTERIOR_WONDER_ITEM_01",
    "RC_PIRATE_FORTRESS_INTERIOR_WONDER_ITEM_02",
    "RC_PIRATE_FORTRESS_INTERIOR_WONDER_ITEM_03",
    "RC_PIRATE_FORTRESS_PLAZA_WONDER_ITEM_01",
    "RC_PIRATE_FORTRESS_PLAZA_WONDER_ITEM_02",
    "RC_PIRATE_FORTRESS_PLAZA_WONDER_ITEM_03",
    "RC_PIRATE_FORTRESS_PLAZA_WONDER_ITEM_04",
    "RC_PIRATE_FORTRESS_PLAZA_WONDER_ITEM_05",
    "RC_PIRATE_FORTRESS_PLAZA_WONDER_ITEM_06",
    "RC_ROMANI_RANCH_BARN_WONDER_ITEM_01",
    "RC_ROMANI_RANCH_BARN_WONDER_ITEM_02",
    "RC_ROMANI_RANCH_WONDER_ITEM_01",
    "RC_ROMANI_RANCH_WONDER_ITEM_02",
    "RC_ROMANI_RANCH_WONDER_ITEM_03",
    "RC_ROMANI_RANCH_WONDER_ITEM_04",
    "RC_ROMANI_RANCH_WONDER_ITEM_05",
    "RC_ROMANI_RANCH_WONDER_ITEM_06",
    "RC_STOCK_POT_INN_WONDER_ITEM_01",
    "RC_STOCK_POT_INN_WONDER_ITEM_02",
    "RC_STOCK_POT_INN_WONDER_ITEM_03",
    "RC_SWAMP_SPIDER_HOUSE_WONDER_ITEM_01",
    "RC_SWAMP_SPIDER_HOUSE_WONDER_ITEM_02",
    "RC_SWAMP_SPIDER_HOUSE_WONDER_ITEM_03",
    "RC_SWAMP_SPIDER_HOUSE_WONDER_ITEM_04",
    "RC_SWAMP_SPIDER_HOUSE_WONDER_ITEM_05",
    "RC_SWAMP_SPIDER_HOUSE_WONDER_ITEM_06",
    "RC_SWAMP_SPIDER_HOUSE_WONDER_ITEM_07",
    "RC_SWAMP_SPIDER_HOUSE_WONDER_ITEM_08",
    "RC_SWAMP_SPIDER_HOUSE_WONDER_ITEM_09",
    "RC_SWAMP_SPIDER_HOUSE_WONDER_ITEM_10",
    "RC_SWAMP_SPIDER_HOUSE_WONDER_ITEM_11",
    "RC_SWAMP_SPIDER_HOUSE_WONDER_ITEM_12",
    "RC_SWAMP_SPIDER_HOUSE_WONDER_ITEM_13",
    "RC_SWAMP_SPIDER_HOUSE_WONDER_ITEM_14",
    "RC_SWAMP_SPIDER_HOUSE_WONDER_ITEM_15",
    "RC_SWAMP_SPIDER_HOUSE_WONDER_ITEM_16",
    "RC_SWAMP_SPIDER_HOUSE_WONDER_ITEM_17",
    "RC_SWORDSMAN_SCHOOL_WONDER_ITEM",
    "RC_TERMINA_FIELD_WALL_WONDER_ITEM_01",
    "RC_TERMINA_FIELD_WALL_WONDER_ITEM_02",
    "RC_TERMINA_FIELD_WALL_WONDER_ITEM_03",
    "RC_TERMINA_FIELD_WALL_WONDER_ITEM_04",
    "RC_TERMINA_FIELD_WALL_WONDER_ITEM_05",
    "RC_TERMINA_FIELD_WALL_WONDER_ITEM_06",
    "RC_TERMINA_FIELD_WALL_WONDER_ITEM_07",
    "RC_TERMINA_FIELD_WALL_WONDER_ITEM_08",
    "RC_TERMINA_FIELD_WALL_WONDER_ITEM_09",
    "RC_TERMINA_FIELD_WALL_WONDER_ITEM_10",
    "RC_TERMINA_FIELD_WALL_WONDER_ITEM_11",
    "RC_TERMINA_FIELD_WALL_WONDER_ITEM_12",
    "RC_TERMINA_FIELD_WALL_WONDER_ITEM_13",
    "RC_TERMINA_FIELD_WALL_WONDER_ITEM_14",
    "RC_TERMINA_FIELD_WALL_WONDER_ITEM_15",
    "RC_TERMINA_FIELD_WONDER_ITEM_01",
    "RC_TERMINA_FIELD_WONDER_ITEM_02",
    "RC_TERMINA_FIELD_WONDER_ITEM_03",
    "RC_TERMINA_FIELD_WONDER_ITEM_04",
    "RC_TERMINA_FIELD_WONDER_ITEM_05",
    "RC_TERMINA_FIELD_WONDER_ITEM_06",
    "RC_TERMINA_FIELD_WONDER_ITEM_07",
    "RC_TERMINA_FIELD_WONDER_ITEM_08",
    "RC_TERMINA_FIELD_WONDER_ITEM_09",
    "RC_TERMINA_FIELD_WONDER_ITEM_10",
    "RC_TERMINA_FIELD_WONDER_ITEM_11",
    "RC_TERMINA_FIELD_WONDER_ITEM_12",
    "RC_TERMINA_FIELD_WONDER_ITEM_13",
    "RC_TERMINA_FIELD_WONDER_ITEM_14",
    "RC_TERMINA_FIELD_WONDER_ITEM_15",
    "RC_TERMINA_FIELD_WONDER_ITEM_16",
    "RC_TERMINA_FIELD_WONDER_ITEM_17",
    "RC_TERMINA_FIELD_WONDER_ITEM_18",
    "RC_TERMINA_FIELD_WONDER_ITEM_19",
    "RC_TERMINA_FIELD_WONDER_ITEM_20",
    "RC_TERMINA_FIELD_WONDER_ITEM_21",
};
constexpr int kWonderCheckCount = (int)(sizeof(kWonderChecks) / sizeof(kWonderChecks[0]));

// The base check of every Hit Tag in WonderItems.cpp's enWonderItemMap. Each Hit
// Tag spawns its base and the two checks after it.
const char* const kHitTagBases[] = {
    "RC_PIRATE_FORTRESS_PLAZA_WONDER_ITEM_01",
    "RC_PIRATE_FORTRESS_PLAZA_WONDER_ITEM_04",
    "RC_PIRATE_FORTRESS_INTERIOR_WONDER_ITEM_01",
    "RC_SWAMP_SPIDER_HOUSE_WONDER_ITEM_06",
    "RC_SWAMP_SPIDER_HOUSE_WONDER_ITEM_09",
    "RC_SWAMP_SPIDER_HOUSE_WONDER_ITEM_12",
    "RC_SWAMP_SPIDER_HOUSE_WONDER_ITEM_15",
    "RC_OCEAN_SPIDER_HOUSE_WONDER_ITEM_01",
    "RC_OCEAN_SPIDER_HOUSE_WONDER_ITEM_04",
    "RC_OCEAN_SPIDER_HOUSE_WONDER_ITEM_07",
    "RC_TERMINA_FIELD_WONDER_ITEM_12",
    "RC_TERMINA_FIELD_WONDER_ITEM_15",
    "RC_TERMINA_FIELD_WONDER_ITEM_18",
    "RC_CUCCO_SHACK_WONDER_ITEM_01",
    "RC_CUCCO_SHACK_WONDER_ITEM_04",
    "RC_IKANA_GRAVEYARD_WONDER_ITEM_01",
    "RC_IKANA_GRAVEYARD_WONDER_ITEM_04",
    "RC_IKANA_GRAVEYARD_WONDER_ITEM_07",
    "RC_IKANA_GRAVEYARD_WONDER_ITEM_10",
    "RC_STOCK_POT_INN_WONDER_ITEM_01",
    "RC_CLOCK_TOWN_EAST_WONDER_ITEM_01",
    "RC_CLOCK_TOWN_EAST_WONDER_ITEM_04",
    "RC_CLOCK_TOWN_EAST_WONDER_ITEM_07",
    "RC_CLOCK_TOWN_SOUTH_WONDER_ITEM_01",
};

// TIME_TO_HOURS_F yields 0..23, one sRewardDropsIndex entry per hour.
constexpr int kWallHours = 24;

/** The option's id, or RO_MAX when this tree has no such option. */
RandoOptionId WonderOptionId() {
    return Rando::StaticData::GetOptionIdFromName(kOptionName);
}

bool IsDigit(char c) {
    return c >= '0' && c <= '9';
}

/** `name` with its trailing two-digit number advanced by `step` ("..._04", 2 ->
 *  "..._06"); empty when the name does not end in "_NN". */
std::string AdvanceNumber(const char* name, int step) {
    const size_t len = strlen(name);
    if (len < 3 || name[len - 3] != '_' || !IsDigit(name[len - 2]) || !IsDigit(name[len - 1])) {
        return std::string();
    }
    const int number = (name[len - 2] - '0') * 10 + (name[len - 1] - '0') + step;
    char suffix[16];
    snprintf(suffix, sizeof(suffix), "%02d", number);
    return std::string(name, len - 2) + suffix;
}

/** One pool generation over a fresh save info, every option at its default but
 *  the wonder option, which is `wonderOn`. Seeded, so the run is deterministic. */
void GenerateWithWonder(RandoOptionId wonderOption, bool wonderOn, std::vector<RandoCheckId>& checkPool,
                        std::vector<RandoItemId>& itemPool) {
    auto info = std::make_unique<RandoSaveInfo>();
    memset(info.get(), 0, sizeof(RandoSaveInfo));
    for (auto& [randoOptionId, row] : Rando::StaticData::Options) {
        info->randoSaveOptions[randoOptionId] = row.defaultValue;
    }
    info->randoSaveOptions[RO_LOGIC] = RO_LOGIC_GLITCHLESS;
    info->randoSaveOptions[wonderOption] = wonderOn ? RO_GENERIC_YES : RO_GENERIC_NO;
    checkPool.clear();
    itemPool.clear();
    Ship_Random_Seed(0x3D0Eu);
    Rando::Logic::GeneratePools(*info, checkPool, itemPool);
}

/** (4): `id` must be the check named `expected`, and a wonder item. */
int ExpectRunMember(int code, const char* what, RandoCheckId id, const std::string& expected,
                    const std::set<RandoCheckId>& wonder) {
    const RandoCheckId named = expected.empty() ? RC_UNKNOWN : Rando::StaticData::GetCheckIdFromName(expected.c_str());
    if (named == RC_UNKNOWN || id != named || wonder.count(id) == 0) {
        const auto it = Rando::StaticData::Checks.find(id);
        return Fail(code, "%s indexes id %d (%s), expected %s: the run WonderItems.cpp indexes is broken", what,
                    (int)id, it != Rando::StaticData::Checks.end() ? it->second.name : "no check",
                    expected.empty() ? "(unnumbered)" : expected.c_str());
    }
    return 0;
}

int RunOptionAndPool() {
    // ---- (1) the option -----------------------------------------------------
    const RandoOptionId wonderOption = WonderOptionId();
    if (wonderOption >= RO_MAX) {
        return Fail(1, "MM has no %s option: wonder item shuffle is missing from MM (#1406)", kOptionName);
    }
    const auto& optionRow = Rando::StaticData::Options.at(wonderOption);
    if (optionRow.defaultValue != RO_GENERIC_OFF) {
        return Fail(2, "%s defaults to %u, expected off", kOptionName, (unsigned)optionRow.defaultValue);
    }
    if (strcmp(optionRow.cvar, kOptionCVar) != 0) {
        return Fail(3, "%s binds '%s', expected '%s'", kOptionName, optionRow.cvar, kOptionCVar);
    }

    // ---- (2) the page row ---------------------------------------------------
    MM_RandoOptionsUi_Register();
    const ComboMMOptionDesc* row = Combo_MMOptionById((uint16_t)wonderOption);
    if (row == NULL) {
        return Fail(4, "the MM Randomizer page has no row for %s", kOptionName);
    }
    if (row->label == NULL || strcmp(row->label, kRowLabel) != 0) {
        return Fail(5, "the %s row reads '%s', expected '%s'", kOptionName, row->label ? row->label : "(null)",
                    kRowLabel);
    }
    if (row->widget != (uint8_t)COMBO_MM_WIDGET_CHECKBOX || row->defaultValue != 0 ||
        row->liveness != (uint8_t)COMBO_MM_LIVENESS_LIVE ||
        strcmp(Combo_MMOptionGroupName(row->group), "Shuffle Items") != 0) {
        return Fail(6, "the %s row is not a live, default-off checkbox under Shuffle Items", kOptionName);
    }

    // ---- (3) the check table ------------------------------------------------
    std::set<RandoCheckId> wonder;
    for (const char* name : kWonderChecks) {
        const RandoCheckId id = Rando::StaticData::GetCheckIdFromName(name);
        if (id == RC_UNKNOWN) {
            return Fail(7, "MM's check table has no %s", name);
        }
        wonder.insert(id);
    }
    if ((int)wonder.size() != kWonderCheckCount) {
        return Fail(7, "the %d wonder item names resolve to %zu distinct checks", kWonderCheckCount, wonder.size());
    }
    const RandoCheckType wonderType = Rando::StaticData::Checks.at(*wonder.begin()).randoCheckType;
    for (const auto& [randoCheckId, check] : Rando::StaticData::Checks) {
        const bool isWonder = wonder.count(randoCheckId) != 0;
        if (isWonder != (check.randoCheckType == wonderType)) {
            return Fail(8, "%s %s the wonder item check type: the type must hold exactly the %d wonder items",
                        check.name, isWonder ? "lacks" : "carries", kWonderCheckCount);
        }
    }
    MM_Rando_Init(); // fills Rando::Logic::Regions (irreversible; ShipInit::InitAll)
    std::set<RandoCheckId> listed;
    for (const auto& [regionId, region] : Rando::Logic::Regions) {
        for (const auto& [checkId, condition] : region.checks) {
            listed.insert(checkId);
        }
    }
    for (RandoCheckId id : wonder) {
        if (listed.count(id) == 0) {
            return Fail(9, "%s is in no logic region, so no seed can ever place on it",
                        Rando::StaticData::Checks.at(id).name);
        }
        if (Rando::Foreign::IsForeignHostClass(id)) {
            return Fail(10, "%s is a cross-game host class: an Ocarina of Time item would wait on a wonder item",
                        Rando::StaticData::Checks.at(id).name);
        }
    }
    MM_RandoLocationsUi_Register();
    std::set<RandoCheckId> located;
    for (int i = 0; i < Combo_MMLocationCount(); i++) {
        const ComboMMLocationDesc* desc = Combo_MMLocationAt(i);
        const RandoCheckId id = (RandoCheckId)desc->id;
        located.insert(id);
    }
    for (RandoCheckId id : wonder) {
        if (located.count(id) == 0) {
            return Fail(12, "%s is not a row of Randomizer > MM Locations", Rando::StaticData::Checks.at(id).name);
        }
    }

    // ---- (4) the id runs ----------------------------------------------------
    for (const char* baseName : kHitTagBases) {
        const RandoCheckId base = Rando::StaticData::GetCheckIdFromName(baseName);
        for (int step = 0; step < 3; step++) {
            const int rc =
                ExpectRunMember(13, baseName, (RandoCheckId)(base + step), AdvanceNumber(baseName, step), wonder);
            if (rc != 0) {
                return rc;
            }
        }
    }
    const char* const wallBaseName = "RC_TERMINA_FIELD_WALL_WONDER_ITEM_01";
    const RandoCheckId wallBase = Rando::StaticData::GetCheckIdFromName(wallBaseName);
    for (int hour = 0; hour < kWallHours; hour++) {
        for (int step = 0; step < 3; step++) {
            const int offset = (int)sRewardDropsIndex[hour] + step;
            const int rc = ExpectRunMember(14, "the Termina Field wall", (RandoCheckId)(wallBase + offset),
                                           AdvanceNumber(wallBaseName, offset), wonder);
            if (rc != 0) {
                return rc;
            }
        }
    }
    for (const char* single : { "RC_SWORDSMAN_SCHOOL_WONDER_ITEM", "RC_TERMINA_FIELD_WONDER_ITEM_21" }) {
        const RandoCheckId id = Rando::StaticData::GetCheckIdFromName(single);
        if (wonder.count(id) == 0) {
            return Fail(15, "%s (the gong's or the pillar rupee's check) is not a wonder item", single);
        }
    }

    // ---- (5) pool generation ------------------------------------------------
    const std::string savedExcluded = CVarGetString("gRando.ExcludedChecks", "");
    CVarClear("gRando.ExcludedChecks");
    std::vector<RandoCheckId> offChecks, onChecks;
    std::vector<RandoItemId> offItems, onItems;
    GenerateWithWonder(wonderOption, false, offChecks, offItems);
    GenerateWithWonder(wonderOption, true, onChecks, onItems);
    if (!savedExcluded.empty()) {
        CVarSetString("gRando.ExcludedChecks", savedExcluded.c_str());
    }
    for (RandoCheckId id : offChecks) {
        if (wonder.count(id) != 0) {
            return Fail(16, "with Shuffle Wonder Items off, %s entered the check pool",
                        Rando::StaticData::Checks.at(id).name);
        }
    }
    for (RandoCheckId id : wonder) {
        if (std::find(onChecks.begin(), onChecks.end(), id) == onChecks.end()) {
            return Fail(17, "with Shuffle Wonder Items on, %s did not enter the check pool",
                        Rando::StaticData::Checks.at(id).name);
        }
    }
    std::vector<RandoCheckId> onWithoutWonder;
    for (RandoCheckId id : onChecks) {
        if (wonder.count(id) == 0) {
            onWithoutWonder.push_back(id);
        }
    }
    if (onWithoutWonder != offChecks) {
        return Fail(18,
                    "the option-off check pool (%zu) is not the option-on pool minus the wonder items (%zu): the "
                    "option moves checks it does not own",
                    offChecks.size(), onWithoutWonder.size());
    }
    if (onItems.size() != offItems.size() + wonder.size()) {
        return Fail(19, "the option-on item pool holds %zu items, expected %zu (one per wonder item)", onItems.size(),
                    offItems.size() + wonder.size());
    }
    printf("[TEST] mm-wonder-items: %d wonder items, option off -> %zu checks, option on -> %zu checks\n",
           kWonderCheckCount, offChecks.size(), onChecks.size());
    return 0;
}

// ---------------------------------------------------------------------------
// The provider row.
// ---------------------------------------------------------------------------

// Settled counts: S2H::GameHooks unregistration is deferred until the next
// Execute of that hook type, and the COND_* macros unregister-then-register on
// every drive, so the raw maps can credit a hook already slated for removal.
template <typename H> size_t SettledCountForId(int32_t forId) {
    S2H::GameHooks::FlushPendingUnregistrations<H>();
    auto& buckets = S2H::GameHooks::Registry<H>::functionsForID;
    auto it = buckets.find(forId);
    return it == buckets.end() ? 0u : it->second.size();
}

struct RegistryProbe {
    const char* what;
    size_t (*count)();
    size_t before;
};

size_t HitTagCount() {
    return SettledCountForId<GameInteractor::OnActorInit>(ACTOR_EN_HIT_TAG);
}
size_t InvisibleRuppeCount() {
    return SettledCountForId<GameInteractor::ShouldActorInit>(ACTOR_EN_INVISIBLE_RUPPE);
}
size_t ScopecoinCount() {
    return SettledCountForId<GameInteractor::ShouldActorInit>(ACTOR_EN_SCOPECOIN);
}
size_t EnemyDropCount() {
    return SettledCountForId<GameInteractor::ShouldVanillaBehavior>(VB_ENEMY_DROP_COLLECTIBLE);
}
size_t GongDropCount() {
    return SettledCountForId<GameInteractor::ShouldVanillaBehavior>(VB_GONG_DROP_COLLECTIBLE);
}

/** The production arm path: a rando save with the option at `wonderOn` and
 *  Shuffle Enemy Drops off, then the behaviour registrars a save load runs
 *  (Rando.cpp OnSaveLoadHandler). */
void DriveWonderArmState(RandoOptionId wonderOption, bool wonderOn) {
    gSaveContext.save.shipSaveInfo.saveType = SAVETYPE_RANDO;
    RANDO_SAVE_OPTIONS[RO_SHUFFLE_ENEMY_DROPS] = RO_GENERIC_OFF;
    if (wonderOption < RO_MAX) {
        RANDO_SAVE_OPTIONS[wonderOption] = wonderOn ? RO_GENERIC_ON : RO_GENERIC_OFF;
    }
    Rando::ActorBehavior::OnFileLoad();
}

/** Runs only the id-keyed OnActorInit registrants for En_Hit_Tag: the unkeyed
 *  ones belong to other behaviours and are not this row's to exercise. */
void RunHitTagInit(Actor* actor) {
    S2H::GameHooks::FlushPendingUnregistrations<GameInteractor::OnActorInit>();
    const auto bucket = S2H::GameHooks::Registry<GameInteractor::OnActorInit>::functionsForID.find(ACTOR_EN_HIT_TAG);
    if (bucket != S2H::GameHooks::Registry<GameInteractor::OnActorInit>::functionsForID.end()) {
        const auto hooks = bucket->second;
        for (const auto& [hookId, hook] : hooks) {
            hook(actor);
        }
    }
}

/** The id-keyed ShouldActorInit verdict for an invisible rupee, likewise. */
bool InvisibleRuppeInitVerdict(Actor* actor) {
    bool should = true;
    S2H::GameHooks::FlushPendingUnregistrations<GameInteractor::ShouldActorInit>();
    const auto bucket =
        S2H::GameHooks::Registry<GameInteractor::ShouldActorInit>::functionsForID.find(ACTOR_EN_INVISIBLE_RUPPE);
    if (bucket != S2H::GameHooks::Registry<GameInteractor::ShouldActorInit>::functionsForID.end()) {
        const auto hooks = bucket->second;
        for (const auto& [hookId, hook] : hooks) {
            hook(actor, &should);
        }
    }
    return should;
}

/** MM_Item_DropCollectible's dispatch (z_en_item00.c): the spawn position by
 *  value, then the params. */
bool EnemyDropVerdict(Vec3f position) {
    return GameInteractor_Should(VB_ENEMY_DROP_COLLECTIBLE, true, position, (u32)ITEM00_RUPEE_GREEN);
}

/** ObjDora_UpdateCollision's dispatch (z_obj_dora.c). */
bool GongDropVerdict(ObjDora* gong) {
    return GameInteractor_Should(VB_GONG_DROP_COLLECTIBLE, true, gong);
}

void NoopUpdate(Actor* actor, PlayState* play) {
    (void)actor;
    (void)play;
}

struct Fixtures {
    PlayState* play;
    Actor* player;   // the ACTORCAT_PLAYER list's head, so GET_PLAYER is not null
    Actor* tag;      // a mapped Hit Tag: Pirates' Fortress plaza, room 0, list index 24
    Actor* strayTag; // a Hit Tag WonderItems.cpp does not map (list index 99)
    Actor* ruppe;    // a mapped invisible rupee: Swamp Spider House, room 4, list index 25
    ObjDora* gong;
};

int RunHookLegs(RandoOptionId wonderOption, Fixtures& f) {
    RegistryProbe probes[] = {
        { "OnActorInit[ACTOR_EN_HIT_TAG]", HitTagCount, 0 },
        { "ShouldActorInit[ACTOR_EN_INVISIBLE_RUPPE]", InvisibleRuppeCount, 0 },
        { "ShouldActorInit[ACTOR_EN_SCOPECOIN]", ScopecoinCount, 0 },
        { "ShouldVanillaBehavior[VB_ENEMY_DROP_COLLECTIBLE]", EnemyDropCount, 0 },
        { "ShouldVanillaBehavior[VB_GONG_DROP_COLLECTIBLE]", GongDropCount, 0 },
    };

    if (wonderOption >= RO_MAX) {
        return Fail(20, "MM has no %s option, so no wonder item behaviour can arm (#1406)", kOptionName);
    }
    const RandoCheckId plaza01 = Rando::StaticData::GetCheckIdFromName("RC_PIRATE_FORTRESS_PLAZA_WONDER_ITEM_01");
    const RandoCheckId plaza02 = Rando::StaticData::GetCheckIdFromName("RC_PIRATE_FORTRESS_PLAZA_WONDER_ITEM_02");
    const RandoCheckId plaza03 = Rando::StaticData::GetCheckIdFromName("RC_PIRATE_FORTRESS_PLAZA_WONDER_ITEM_03");
    const RandoCheckId spider01 = Rando::StaticData::GetCheckIdFromName("RC_SWAMP_SPIDER_HOUSE_WONDER_ITEM_01");
    const RandoCheckId gongCheck = Rando::StaticData::GetCheckIdFromName("RC_SWORDSMAN_SCHOOL_WONDER_ITEM");
    if (plaza01 == RC_UNKNOWN || plaza02 == RC_UNKNOWN || plaza03 == RC_UNKNOWN || spider01 == RC_UNKNOWN ||
        gongCheck == RC_UNKNOWN) {
        return Fail(20, "MM's check table lacks a wonder item this row drives (#1406)");
    }

    MM_gPlayState = f.play;
    f.play->sceneId = SCENE_KAIZOKU;
    f.play->roomCtx.curRoom.num = 0;
    const Vec3f tagPos = f.tag->world.pos;
    const Vec3f elsewhere = { tagPos.x + 500.0f, tagPos.y, tagPos.z };
    // Every check the drop and gong legs could spawn is already collected this
    // cycle, so the hooks decide without reaching CustomItem::Spawn.
    RANDO_SAVE_CHECKS[plaza01].cycleObtained = true;
    RANDO_SAVE_CHECKS[plaza02].cycleObtained = true;
    RANDO_SAVE_CHECKS[plaza03].cycleObtained = true;
    RANDO_SAVE_CHECKS[gongCheck].cycleObtained = true;
    RANDO_SAVE_CHECKS[spider01].shuffled = false;
    RANDO_SAVE_CHECKS[spider01].cycleObtained = false;

    // ---- Leg 1: rando save, option off (non-vacuity) -----------------------
    DriveWonderArmState(wonderOption, false);
    for (auto& probe : probes) {
        probe.before = probe.count();
    }
    RunHitTagInit(f.tag);
    if (Rando::ActorBehavior::GetObjectRandoCheckId(f.tag) != RC_UNKNOWN) {
        return Fail(21, "with Shuffle Wonder Items off a Hit Tag was already given a check: the armed probe below "
                        "would be vacuous");
    }
    if (!EnemyDropVerdict(tagPos)) {
        return Fail(22, "with Shuffle Wonder Items off a Hit Tag's vanilla drop is already suppressed: the armed "
                        "probe below would be vacuous");
    }

    // ---- Leg 2: option on, through the production path ---------------------
    DriveWonderArmState(wonderOption, true);
    for (const auto& probe : probes) {
        const size_t after = probe.count();
        if (after != probe.before + 1) {
            return Fail(23,
                        "%s went %zu -> %zu when Shuffle Wonder Items armed, expected exactly +1: the wonder item "
                        "behaviour (Rando/ActorBehavior/WonderItems.cpp) is not in the link, is not reached from "
                        "ActorBehavior::OnFileLoad, or a second registrant made the probe unattributable",
                        probe.what, probe.before, after);
        }
    }

    // ---- Leg 3: the Hit Tag's identity -------------------------------------
    RunHitTagInit(f.tag);
    if (Rando::ActorBehavior::GetObjectRandoCheckId(f.tag) != plaza01 || f.tag->draw == NULL) {
        return Fail(24,
                    "a mapped Hit Tag (Pirates' Fortress plaza, list index 24) got check %d and %s draw, expected "
                    "RC_PIRATE_FORTRESS_PLAZA_WONDER_ITEM_01 and the sparkle draw",
                    (int)Rando::ActorBehavior::GetObjectRandoCheckId(f.tag), f.tag->draw != NULL ? "a" : "no");
    }
    RunHitTagInit(f.strayTag);
    if (Rando::ActorBehavior::GetObjectRandoCheckId(f.strayTag) != RC_UNKNOWN || f.strayTag->draw != NULL) {
        return Fail(25, "an unmapped Hit Tag was given a check or a draw function");
    }

    // ---- Leg 4: the drop ---------------------------------------------------
    if (EnemyDropVerdict(tagPos)) {
        return Fail(26, "with Shuffle Wonder Items on, a mapped Hit Tag still drops its vanilla collectible "
                        "(VB_ENEMY_DROP_COLLECTIBLE)");
    }
    if (GetActorListIndex(f.tag) != -1) {
        return Fail(27, "the Hit Tag's list index reads %d after its drop, expected the -1 dupe latch",
                    (int)GetActorListIndex(f.tag));
    }
    if (!EnemyDropVerdict(elsewhere)) {
        return Fail(28, "with Shuffle Wonder Items on, a drop away from every Hit Tag lost its vanilla collectible");
    }
    if (EnemyDropVerdict(tagPos)) {
        return Fail(29, "a second drop at a latched Hit Tag was not suppressed: its wonder items would spawn twice");
    }

    // ---- Leg 5: guards -----------------------------------------------------
    if (!GongDropVerdict(f.gong)) {
        return Fail(30, "the gong lost its vanilla rupee although the Swordsman's School wonder item was already "
                        "collected this cycle");
    }
    f.play->sceneId = SCENE_KINSTA1;
    f.play->roomCtx.curRoom.num = 4;
    if (!InvisibleRuppeInitVerdict(f.ruppe)) {
        return Fail(31, "an unshuffled invisible rupee (Swamp Spider House) was kept from spawning");
    }
    return 0;
}

Actor* CallocActor(size_t size, s16 id) {
    Actor* actor = static_cast<Actor*>(calloc(1, size));
    if (actor != nullptr) {
        actor->id = id;
    }
    return actor;
}

} // namespace

extern "C" int MM_WonderItems_RunHeadless(void) {
    printf("[TEST] mm-wonder-items: Shuffle Wonder Items is an MM option, a page row, 113 checks, their id runs and "
           "a pool switch (#1406)\n");
    auto ctx = Ship::Context::GetInstance();
    if (ctx == nullptr || ctx->GetConsoleVariables() == nullptr) {
        return Fail(1, "Ship::Context or its ConsoleVariables are missing; run the shared bring-up first");
    }
    const int rc = RunOptionAndPool();
    if (rc != 0) {
        return rc;
    }
    printf("[TEST] mm-wonder-items: PASS\n");
    return 0;
}

extern "C" int MM_WonderItemsHooks_RunHeadless(void) {
    printf("[TEST] mm-wonder-items-hooks: the wonder item behaviour arms from ActorBehavior::OnFileLoad and runs "
           "(#1406)\n");
    auto ctx = Ship::Context::GetInstance();
    if (ctx == nullptr || ctx->GetConsoleVariables() == nullptr) {
        return Fail(1, "Ship::Context or its ConsoleVariables are missing; run the shared bring-up first");
    }

    auto saved = std::make_unique<uint8_t[]>(sizeof(gSaveContext));
    memcpy(saved.get(), &gSaveContext, sizeof(gSaveContext));
    PlayState* const savedPlay = MM_gPlayState;

    Fixtures f = {};
    f.play = static_cast<PlayState*>(calloc(1, sizeof(PlayState)));
    f.player = CallocActor(sizeof(Player), ACTOR_PLAYER);
    f.tag = CallocActor(sizeof(Actor), ACTOR_EN_HIT_TAG);
    f.strayTag = CallocActor(sizeof(Actor), ACTOR_EN_HIT_TAG);
    f.ruppe = CallocActor(sizeof(Actor), ACTOR_EN_INVISIBLE_RUPPE);
    f.gong = reinterpret_cast<ObjDora*>(CallocActor(sizeof(ObjDora), ACTOR_OBJ_DORA));
    int rc = 0;
    if (f.play == nullptr || f.player == nullptr || f.tag == nullptr || f.strayTag == nullptr || f.ruppe == nullptr ||
        f.gong == nullptr) {
        rc = Fail(1, "could not allocate the fixtures");
    } else {
        f.tag->update = NoopUpdate;
        f.tag->room = 0;
        f.tag->world.pos.x = 100.0f;
        f.tag->world.pos.y = 20.0f;
        f.tag->world.pos.z = -300.0f;
        f.strayTag->update = NoopUpdate;
        f.strayTag->room = 0;
        SetActorListIndex(f.tag, 24);
        SetActorListIndex(f.strayTag, 99);
        SetActorListIndex(f.ruppe, 25);
        // SpawnDroppedWonderItems searches ACTORCAT_ITEMACTION for the Hit Tag
        // and, failing that, for En_Gakufu near the player.
        f.play->actorCtx.actorLists[ACTORCAT_PLAYER].first = f.player;
        f.play->actorCtx.actorLists[ACTORCAT_ITEMACTION].first = f.tag;

        rc = RunHookLegs(WonderOptionId(), f);
    }

    // ---- Cleanup (pass and fail) -------------------------------------------
    Actor* const actors[] = { f.player, f.tag, f.strayTag, f.ruppe, reinterpret_cast<Actor*>(f.gong) };
    for (Actor* actor : actors) {
        if (actor != nullptr) {
            ObjectExtension::GetInstance().Free(actor);
        }
    }
    MM_gPlayState = savedPlay;
    memcpy(&gSaveContext, saved.get(), sizeof(gSaveContext));
    Rando::ActorBehavior::OnFileLoad();
    for (Actor* actor : actors) {
        free(actor);
    }
    free(f.play);

    if (rc != 0) {
        return rc;
    }
    printf("[TEST] mm-wonder-items-hooks: PASS (five single-registrant hooks armed, a Hit Tag identified, its drop "
           "suppressed once and latched, vanilla kept elsewhere)\n");
    return 0;
}

#endif /* RSBS_SINGLE_EXECUTABLE */
