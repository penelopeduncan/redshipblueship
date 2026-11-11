/**
 * ROM-free locks for Majora's Mask's beehive shuffle ("Shuffle Hive Drops") and
 * the extra / Keaton grass rows (#1410; upstream 2Ship's Beehive Shuffle,
 * 4ce76f3de, and "Shuffle keaton grass and some more missed grass", dd5a36d60).
 * CTest label "redship". Three rows, three entry points, on the
 * mm_tree_shuffle_test.cpp pattern:
 *
 * ============================================================================
 * mm-hive-shuffle (MMHiveShuffle) -- the option, the page row and the pool
 * ============================================================================
 *
 *  (1) The option exists under its upstream key (RO_SHUFFLE_HIVE_DROPS, CVar
 *      gRando.Options.RO_SHUFFLE_HIVE_DROPS) and defaults OFF.
 *  (2) The MM Randomizer page draws it: a live, default-off checkbox labelled
 *      "Shuffle Hive Drops" under "Shuffle Items", right after "Shuffle
 *      Butterflies" (#1407), which follows "Shuffle Tree Drops".
 *  (3) The check table holds exactly the 19 hives as the only RCTYPE_BEEHIVE
 *      rows; each has a name, sits in EXACTLY one logic region, is not a
 *      cross-game host class, and is a row of Randomizer > MM Locations.
 *  (4) Pool generation: with the option off no hive enters the check pool and
 *      the pool is exactly the pool with it on minus the hives, in the same
 *      order; with it on every hive enters it, with one item each.
 *
 * ============================================================================
 * mm-extra-grass-shuffle (MMExtraGrassShuffle) -- the 171 grass rows
 * ============================================================================
 *
 *  (1) Each of the 171 grass rows dd5a36d60 added is an RCTYPE_GRASS row with a
 *      name, in EXACTLY one logic region, not a cross-game host class, and a
 *      row of Randomizer > MM Locations.
 *  (2) The three Keaton grass rings own nine CONTIGUOUS ids each
 *      (<ring>_KEATON_GRASS_01 + i for i in 0..8): ObjGrass.cpp's
 *      VB_KEATON_GRASS_ATTACH_CHILD derives a bush's check as base + index.
 *  (3) The three Keaton grass ring rewards (each ring's bonus red rupee) are
 *      RCTYPE_WONDER_ITEM rows, #1406's check type, and sit in one region each.
 *      mm-wonder-items owns the type's whole membership (116 rows).
 *  (4) Pool generation behind the EXISTING "Shuffle Grass Drops": off, none of
 *      the 171 enters; on, all 171 enter, and the on pool minus every grass row
 *      is the off pool. The three ring rewards follow Shuffle Wonder Items
 *      (#1406), not grass or hives: none enters with grass and hives both on and
 *      wonder items off, and all three enter with wonder items on.
 *  (5) The coordinator's host universe agrees (RunHostUniverseLegs): MM's
 *      engine offers every ring reward as a graph host, MM_ComboLogic_SetHostPool
 *      keeps a handed-in one, and with every boolean RO_SHUFFLE_* option on (the
 *      wonder option included) it keeps every pool row, the ring rewards among
 *      them.
 *
 * ============================================================================
 * mm-hive-grass-shuffle-hooks (MMHiveGrassShuffleHooks) -- the providers
 * ============================================================================
 *
 * ObjComb.cpp and ObjGrass.cpp register from Rando::ActorBehavior::OnFileLoad
 * under IS_RANDO. This row drives THAT function, never a symbol of either TU (a
 * reference from here would pull the object into the link on its own and hide
 * an elided provider, the #516/#1071 class):
 *
 *  - Registry legs: a rando save gains EXACTLY one registrant over a vanilla
 *    save for OnActorInit[ACTOR_OBJ_COMB], OnActorInit[ACTOR_EN_KUSA2],
 *    OnActorUpdate[ACTOR_EN_KUSA2] and the VBs VB_BEEHIVE_SPAWN_ACTOR,
 *    VB_BEEHIVE_ALREADY_BROKEN, VB_KEATON_GRASS_ATTACH_CHILD,
 *    VB_KEATON_GRASS_DRAW, VB_KUSA_DRAW_BE_OVERRIDDEN and
 *    VB_GRASS_DROP_COLLECTIBLE. The last two predate #1410: main's ObjGrass.cpp
 *    already registered VB_GRASS_DROP_COLLECTIBLE, and VB_KUSA_DRAW_BE_OVERRIDDEN
 *    is its VB_KUSA_BUSH_DRAW_BE_OVERRIDDEN renamed. Those two probes lock that
 *    the providers still arm, not anything #1410 added. Of the branches #1410
 *    added inside them, the sprout-grass draw is locked by the drive legs below;
 *    the En_Kusa2 drop branch is not (its live leg spawns an item).
 *  - Hive identification: OnActorInit on a hive in the Swamp Spider House gold
 *    room attaches its check; the reused cow-grotto room resolves to the Termina
 *    Field hive when the respawn data says so and to the Great Bay Coast hive
 *    otherwise; an unshuffled hive gets nothing.
 *  - Keaton ring: OnActorInit on the Clock Town North ring manager, then
 *    VB_KEATON_GRASS_ATTACH_CHILD per bush, gives bush i the check base + i,
 *    skips a bush whose check was already obtained this cycle, and gives a bush
 *    of an unmapped ring nothing.
 *  - New sprout grass: OnActorInit on an En_Kusa in the Ikana Castle courtyard
 *    attaches the courtyard grass check; VB_KUSA_DRAW_BE_OVERRIDDEN now swaps
 *    the draw of a grass-type En_Kusa as well as a bush, with two different
 *    draws, and leaves an obtained check's draw alone.
 *
 * The legs that would spawn a custom item or draw a display list (a hive or
 * bush with a live check through VB_BEEHIVE_SPAWN_ACTOR, VB_BEEHIVE_ALREADY_
 * BROKEN, VB_KEATON_GRASS_DRAW or VB_GRASS_DROP_COLLECTIBLE) need a real play
 * state and are not driven here.
 *
 * THE OVERLAY DISPATCH SITES ARE UNLOCKED. Every VB leg calls
 * GameInteractor_Should itself rather than going through the overlay, so this
 * row stays green if a dispatch is removed: z_obj_comb.c (VB_BEEHIVE_SPAWN_ACTOR,
 * VB_BEEHIVE_ALREADY_BROKEN), z_en_kusa2.c (VB_KEATON_GRASS_ATTACH_CHILD,
 * VB_KEATON_GRASS_DRAW, VB_GRASS_DROP_COLLECTIBLE for ACTOR_EN_KUSA2) and
 * z_en_kusa.c (VB_GRASS_DROP_COLLECTIBLE for the grass type, the widened
 * VB_KUSA_DRAW_BE_OVERRIDDEN). Those sites were read-verified when #1410 landed;
 * a hook registered with no dispatch is the dead-dispatch class (#512/#517).
 *
 * Every id is looked up BY NAME, so the rows compile at a tree that has none of
 * them, and fail there on their first leg. The hooks row restores the
 * SaveContext image, the play state and the arm state on the way out.
 */

#ifndef RSBS_SINGLE_EXECUTABLE
extern "C" int MM_HiveShuffle_RunHeadless(void) {
    return 0;
}
extern "C" int MM_ExtraGrassShuffle_RunHeadless(void) {
    return 0;
}
extern "C" int MM_HiveGrassShuffleHooks_RunHeadless(void) {
    return 0;
}
#else

#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
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

// src/common: the page's descriptor table, Randomizer > MM Locations, and the
// combo logic engine vtable (the host-universe legs).
#include "combo_logic.h"
#include "combo_mm_locations_view.h"
#include "combo_mm_options_view.h"

extern "C" {
#include "variables.h"
#include "functions.h"
#include "overlays/actors/ovl_En_Kusa/z_en_kusa.h"
#include "overlays/actors/ovl_En_Kusa2/z_en_kusa2.h"
#include "overlays/actors/ovl_Obj_Comb/z_obj_comb.h"
void MM_Rando_Init(void);
// The MM engine's pool seam and diagnostics (ComboLogicEngineSingleExe.cpp).
void MM_ComboLogic_SetHostPool(const uint16_t* checks, int count);
int MM_ComboLogic_HeldPlacementCount(void);
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

constexpr const char* kHiveOptionName = "RO_SHUFFLE_HIVE_DROPS";
constexpr const char* kHiveOptionCVar = "gRando.Options.RO_SHUFFLE_HIVE_DROPS";
constexpr const char* kHiveRowLabel = "Shuffle Hive Drops";
constexpr const char* kHiveRowTooltip = "Adds the item dropped by breaking a beehive to the check pool.";

// The rows #1410 added: 4ce76f3de's 19 beehives (kHives), dd5a36d60's 171 grass
// checks (kNewGrass, the Keaton rings' 27 bushes included) and its three wonder
// items, each Keaton ring's bonus red rupee (kWonderItems).
const char* const kHives[] = {
    "RC_GREAT_BAY_COAST_COW_GROTTO_BEEHIVE",          "RC_MOUNTAIN_VILLAGE_SPRING_BEEHIVE",
    "RC_PIRATE_FORTRESS_CAPTAIN_ROOM_UPPER_BEEHIVE",  "RC_SOUTHERN_SWAMP_POISON_BEEHIVE",
    "RC_SWAMP_SPIDER_HOUSE_GOLD_ROOM_BEEHIVE_01",     "RC_SWAMP_SPIDER_HOUSE_GOLD_ROOM_BEEHIVE_02",
    "RC_SWAMP_SPIDER_HOUSE_GOLD_ROOM_BEEHIVE_03",     "RC_SWAMP_SPIDER_HOUSE_POT_ROOM_BEEHIVE_01",
    "RC_SWAMP_SPIDER_HOUSE_POT_ROOM_BEEHIVE_02",      "RC_SWAMP_SPIDER_HOUSE_POT_ROOM_BEEHIVE_03",
    "RC_SWAMP_SPIDER_HOUSE_TREE_ROOM_BEEHIVE_01",     "RC_SWAMP_SPIDER_HOUSE_TREE_ROOM_BEEHIVE_02",
    "RC_SWAMP_SPIDER_HOUSE_TREE_ROOM_BEEHIVE_03",     "RC_TERMINA_FIELD_BIO_BABA_GROTTO_BEEHIVE_01",
    "RC_TERMINA_FIELD_BIO_BABA_GROTTO_BEEHIVE_02",    "RC_TERMINA_FIELD_COW_GROTTO_BEEHIVE",
    "RC_TERMINA_FIELD_GOSSIP_STONE_GROTTO_3_BEEHIVE", "RC_WOODFALL_TEMPLE_ENTRANCE_BEEHIVE",
    "RC_WOODFALL_TEMPLE_MAZE_ROOM_BEEHIVE",
};
const char* const kNewGrass[] = {
    "RC_ANCIENT_CASTLE_OF_IKANA_COURTYARD_GRASS_01",
    "RC_ANCIENT_CASTLE_OF_IKANA_COURTYARD_GRASS_02",
    "RC_ANCIENT_CASTLE_OF_IKANA_COURTYARD_GRASS_03",
    "RC_ANCIENT_CASTLE_OF_IKANA_COURTYARD_GRASS_04",
    "RC_ANCIENT_CASTLE_OF_IKANA_COURTYARD_GRASS_05",
    "RC_ANCIENT_CASTLE_OF_IKANA_COURTYARD_GRASS_06",
    "RC_ANCIENT_CASTLE_OF_IKANA_COURTYARD_GRASS_07",
    "RC_ANCIENT_CASTLE_OF_IKANA_COURTYARD_GRASS_08",
    "RC_ANCIENT_CASTLE_OF_IKANA_COURTYARD_GRASS_09",
    "RC_ANCIENT_CASTLE_OF_IKANA_COURTYARD_GRASS_10",
    "RC_ANCIENT_CASTLE_OF_IKANA_COURTYARD_GRASS_11",
    "RC_ANCIENT_CASTLE_OF_IKANA_COURTYARD_GRASS_12",
    "RC_BENEATH_THE_WELL_COW_ROOM_GRASS_01",
    "RC_BENEATH_THE_WELL_COW_ROOM_GRASS_02",
    "RC_BENEATH_THE_WELL_COW_ROOM_GRASS_03",
    "RC_BENEATH_THE_WELL_FOUR_SPIKED_BARS_GRASS_01",
    "RC_BENEATH_THE_WELL_FOUR_SPIKED_BARS_GRASS_02",
    "RC_BENEATH_THE_WELL_FOUR_SPIKED_BARS_GRASS_03",
    "RC_BENEATH_THE_WELL_FOUR_SPIKED_BARS_GRASS_04",
    "RC_BENEATH_THE_WELL_FOUR_SPIKED_BARS_GRASS_05",
    "RC_BENEATH_THE_WELL_RIGHT_FIRE_KEESE_GRASS_01",
    "RC_BENEATH_THE_WELL_RIGHT_FIRE_KEESE_GRASS_02",
    "RC_BENEATH_THE_WELL_RIGHT_FIRE_KEESE_GRASS_03",
    "RC_BENEATH_THE_WELL_RIGHT_FIRE_KEESE_GRASS_04",
    "RC_BENEATH_THE_WELL_TWO_SPIKED_BARS_GRASS_01",
    "RC_BENEATH_THE_WELL_TWO_SPIKED_BARS_GRASS_02",
    "RC_CLOCK_TOWN_NORTH_KEATON_GRASS_01",
    "RC_CLOCK_TOWN_NORTH_KEATON_GRASS_02",
    "RC_CLOCK_TOWN_NORTH_KEATON_GRASS_03",
    "RC_CLOCK_TOWN_NORTH_KEATON_GRASS_04",
    "RC_CLOCK_TOWN_NORTH_KEATON_GRASS_05",
    "RC_CLOCK_TOWN_NORTH_KEATON_GRASS_06",
    "RC_CLOCK_TOWN_NORTH_KEATON_GRASS_07",
    "RC_CLOCK_TOWN_NORTH_KEATON_GRASS_08",
    "RC_CLOCK_TOWN_NORTH_KEATON_GRASS_09",
    "RC_IKANA_CANYON_GRASS_01",
    "RC_IKANA_CANYON_GRASS_02",
    "RC_IKANA_CANYON_GRASS_03",
    "RC_IKANA_CANYON_GRASS_04",
    "RC_IKANA_GRAVEYARD_LOWER_GRASS_01",
    "RC_IKANA_GRAVEYARD_LOWER_GRASS_02",
    "RC_IKANA_GRAVEYARD_LOWER_GRASS_03",
    "RC_IKANA_GRAVEYARD_LOWER_GRASS_04",
    "RC_IKANA_GRAVEYARD_LOWER_GRASS_05",
    "RC_MILK_ROAD_GRASS_01",
    "RC_MILK_ROAD_GRASS_02",
    "RC_MILK_ROAD_GRASS_03",
    "RC_MILK_ROAD_KEATON_GRASS_01",
    "RC_MILK_ROAD_KEATON_GRASS_02",
    "RC_MILK_ROAD_KEATON_GRASS_03",
    "RC_MILK_ROAD_KEATON_GRASS_04",
    "RC_MILK_ROAD_KEATON_GRASS_05",
    "RC_MILK_ROAD_KEATON_GRASS_06",
    "RC_MILK_ROAD_KEATON_GRASS_07",
    "RC_MILK_ROAD_KEATON_GRASS_08",
    "RC_MILK_ROAD_KEATON_GRASS_09",
    "RC_MOUNTAIN_VILLAGE_SPRING_KEATON_GRASS_01",
    "RC_MOUNTAIN_VILLAGE_SPRING_KEATON_GRASS_02",
    "RC_MOUNTAIN_VILLAGE_SPRING_KEATON_GRASS_03",
    "RC_MOUNTAIN_VILLAGE_SPRING_KEATON_GRASS_04",
    "RC_MOUNTAIN_VILLAGE_SPRING_KEATON_GRASS_05",
    "RC_MOUNTAIN_VILLAGE_SPRING_KEATON_GRASS_06",
    "RC_MOUNTAIN_VILLAGE_SPRING_KEATON_GRASS_07",
    "RC_MOUNTAIN_VILLAGE_SPRING_KEATON_GRASS_08",
    "RC_MOUNTAIN_VILLAGE_SPRING_KEATON_GRASS_09",
    "RC_SECRET_SHRINE_DINOLFOS_GRASS_01",
    "RC_SECRET_SHRINE_DINOLFOS_GRASS_02",
    "RC_SECRET_SHRINE_DINOLFOS_GRASS_03",
    "RC_SECRET_SHRINE_DINOLFOS_GRASS_04",
    "RC_SECRET_SHRINE_ENTRANCE_GRASS_01",
    "RC_SECRET_SHRINE_ENTRANCE_GRASS_02",
    "RC_SECRET_SHRINE_ENTRANCE_GRASS_03",
    "RC_SECRET_SHRINE_ENTRANCE_GRASS_04",
    "RC_SECRET_SHRINE_ENTRANCE_GRASS_05",
    "RC_SECRET_SHRINE_ENTRANCE_GRASS_06",
    "RC_SECRET_SHRINE_GARO_MASTER_GRASS_01",
    "RC_SECRET_SHRINE_GARO_MASTER_GRASS_02",
    "RC_SECRET_SHRINE_GARO_MASTER_GRASS_03",
    "RC_SECRET_SHRINE_GARO_MASTER_GRASS_04",
    "RC_SECRET_SHRINE_GARO_MASTER_GRASS_05",
    "RC_SECRET_SHRINE_GARO_MASTER_GRASS_06",
    "RC_SECRET_SHRINE_WART_GRASS_01",
    "RC_SECRET_SHRINE_WART_GRASS_02",
    "RC_SECRET_SHRINE_WART_GRASS_03",
    "RC_SECRET_SHRINE_WART_GRASS_04",
    "RC_SECRET_SHRINE_WART_GRASS_05",
    "RC_SECRET_SHRINE_WART_GRASS_06",
    "RC_SECRET_SHRINE_WART_GRASS_07",
    "RC_SECRET_SHRINE_WART_GRASS_08",
    "RC_SECRET_SHRINE_WIZZROBE_GRASS_01",
    "RC_SECRET_SHRINE_WIZZROBE_GRASS_02",
    "RC_SECRET_SHRINE_WIZZROBE_GRASS_03",
    "RC_SECRET_SHRINE_WIZZROBE_GRASS_04",
    "RC_SECRET_SHRINE_WIZZROBE_GRASS_05",
    "RC_SNOWHEAD_TEMPLE_CENTRAL_ROOM_BOTTOM_GRASS_01",
    "RC_SNOWHEAD_TEMPLE_CENTRAL_ROOM_BOTTOM_GRASS_02",
    "RC_SNOWHEAD_TEMPLE_CENTRAL_ROOM_BOTTOM_GRASS_03",
    "RC_SNOWHEAD_TEMPLE_CENTRAL_ROOM_BOTTOM_GRASS_04",
    "RC_SNOWHEAD_TEMPLE_CENTRAL_ROOM_BOTTOM_GRASS_05",
    "RC_SNOWHEAD_TEMPLE_CENTRAL_ROOM_BOTTOM_GRASS_06",
    "RC_SNOWHEAD_TEMPLE_CENTRAL_ROOM_BOTTOM_GRASS_07",
    "RC_SNOWHEAD_TEMPLE_CENTRAL_ROOM_BOTTOM_GRASS_08",
    "RC_SNOWHEAD_TEMPLE_CENTRAL_ROOM_BOTTOM_GRASS_09",
    "RC_SNOWHEAD_TEMPLE_CENTRAL_ROOM_BOTTOM_GRASS_10",
    "RC_SOUTHERN_SWAMP_CLEARED_TOURIST_GRASS_01",
    "RC_SOUTHERN_SWAMP_CLEARED_TOURIST_GRASS_02",
    "RC_SOUTHERN_SWAMP_CLEARED_WOODS_GRASS_01",
    "RC_SOUTHERN_SWAMP_CLEARED_WOODS_GRASS_02",
    "RC_SOUTHERN_SWAMP_POISON_TOURIST_GRASS_01",
    "RC_SOUTHERN_SWAMP_POISON_TOURIST_GRASS_02",
    "RC_SOUTHERN_SWAMP_POISON_WOODS_GRASS_01",
    "RC_SOUTHERN_SWAMP_POISON_WOODS_GRASS_02",
    "RC_STONE_TOWER_TEMPLE_ENTRANCE_GRASS_01",
    "RC_STONE_TOWER_TEMPLE_ENTRANCE_GRASS_02",
    "RC_STONE_TOWER_TEMPLE_ENTRANCE_GRASS_03",
    "RC_STONE_TOWER_TEMPLE_SWITCH_ROOM_GRASS_01",
    "RC_STONE_TOWER_TEMPLE_SWITCH_ROOM_GRASS_02",
    "RC_STONE_TOWER_TEMPLE_SWITCH_ROOM_GRASS_03",
    "RC_STONE_TOWER_TEMPLE_SWITCH_ROOM_GRASS_04",
    "RC_STONE_TOWER_TEMPLE_SWITCH_ROOM_GRASS_05",
    "RC_STONE_TOWER_TEMPLE_SWITCH_ROOM_GRASS_06",
    "RC_WOODFALL_TEMPLE_BOSS_GRASS_01",
    "RC_WOODFALL_TEMPLE_BOSS_GRASS_02",
    "RC_WOODFALL_TEMPLE_BOSS_GRASS_03",
    "RC_WOODFALL_TEMPLE_BOSS_GRASS_04",
    "RC_WOODFALL_TEMPLE_BOSS_GRASS_05",
    "RC_WOODFALL_TEMPLE_BOSS_GRASS_06",
    "RC_WOODFALL_TEMPLE_BOSS_GRASS_07",
    "RC_WOODFALL_TEMPLE_BOSS_GRASS_08",
    "RC_WOODFALL_TEMPLE_BOSS_GRASS_09",
    "RC_WOODFALL_TEMPLE_BOSS_GRASS_10",
    "RC_WOODFALL_TEMPLE_BOSS_GRASS_11",
    "RC_WOODFALL_TEMPLE_BOSS_GRASS_12",
    "RC_WOODFALL_TEMPLE_BOSS_GRASS_13",
    "RC_WOODFALL_TEMPLE_BOSS_GRASS_14",
    "RC_WOODFALL_TEMPLE_BOSS_GRASS_15",
    "RC_WOODFALL_TEMPLE_BOSS_GRASS_16",
    "RC_WOODFALL_TEMPLE_COMPASS_ROOM_GRASS_01",
    "RC_WOODFALL_TEMPLE_COMPASS_ROOM_GRASS_02",
    "RC_WOODFALL_TEMPLE_COMPASS_ROOM_GRASS_03",
    "RC_WOODFALL_TEMPLE_ENTRANCE_GRASS_01",
    "RC_WOODFALL_TEMPLE_ENTRANCE_GRASS_02",
    "RC_WOODFALL_TEMPLE_ENTRANCE_GRASS_03",
    "RC_WOODFALL_TEMPLE_ENTRANCE_GRASS_04",
    "RC_WOODFALL_TEMPLE_ENTRANCE_GRASS_05",
    "RC_WOODFALL_TEMPLE_MAIN_ROOM_LOWER_GRASS_01",
    "RC_WOODFALL_TEMPLE_MAIN_ROOM_LOWER_GRASS_02",
    "RC_WOODFALL_TEMPLE_MAIN_ROOM_UPPER_GRASS_01",
    "RC_WOODFALL_TEMPLE_MAP_ROOM_GRASS_01",
    "RC_WOODFALL_TEMPLE_MAP_ROOM_GRASS_02",
    "RC_WOODFALL_TEMPLE_MAP_ROOM_GRASS_03",
    "RC_WOODFALL_TEMPLE_MAP_ROOM_GRASS_04",
    "RC_WOODFALL_TEMPLE_MAP_ROOM_GRASS_05",
    "RC_WOODFALL_TEMPLE_PRE_BOSS_GRASS_01",
    "RC_WOODFALL_TEMPLE_PRE_BOSS_GRASS_02",
    "RC_WOODFALL_TEMPLE_PRE_BOSS_GRASS_03",
    "RC_WOODFALL_TEMPLE_PRE_BOSS_GRASS_04",
    "RC_WOODFALL_TEMPLE_PRE_BOSS_GRASS_05",
    "RC_WOODFALL_TEMPLE_UPPER_WALKWAY_GRASS_01",
    "RC_WOODFALL_TEMPLE_UPPER_WALKWAY_GRASS_02",
    "RC_WOODFALL_TEMPLE_UPPER_WALKWAY_GRASS_03",
    "RC_WOODFALL_TEMPLE_UPPER_WALKWAY_GRASS_04",
    "RC_WOODFALL_TEMPLE_UPPER_WALKWAY_GRASS_05",
    "RC_WOODFALL_TEMPLE_UPPER_WALKWAY_GRASS_06",
    "RC_WOODFALL_TEMPLE_UPPER_WALKWAY_GRASS_07",
    "RC_WOODFALL_TEMPLE_UPPER_WALKWAY_GRASS_08",
    "RC_WOODFALL_TEMPLE_UPPER_WALKWAY_GRASS_09",
    "RC_WOODFALL_TEMPLE_UPPER_WALKWAY_GRASS_10",
    "RC_WOODFALL_TEMPLE_UPPER_WALKWAY_GRASS_11",
    "RC_WOODFALL_TEMPLE_WATER_ROOM_GRASS_01",
    "RC_WOODFALL_TEMPLE_WATER_ROOM_GRASS_02",
};
const char* const kWonderItems[] = {
    "RC_CLOCK_TOWN_NORTH_KEATON_GRASS_WONDER_ITEM",
    "RC_MILK_ROAD_KEATON_GRASS_WONDER_ITEM",
    "RC_MOUNTAIN_VILLAGE_SPRING_KEATON_GRASS_WONDER_ITEM",
};
constexpr int kHiveCount = (int)(sizeof(kHives) / sizeof(kHives[0]));
constexpr int kGrassCount = (int)(sizeof(kNewGrass) / sizeof(kNewGrass[0]));
constexpr int kWonderCount = (int)(sizeof(kWonderItems) / sizeof(kWonderItems[0]));

// The three Keaton rings, by the name of their bushes without the _NN suffix.
const char* const kKeatonRings[] = {
    "RC_CLOCK_TOWN_NORTH_KEATON_GRASS",
    "RC_MILK_ROAD_KEATON_GRASS",
    "RC_MOUNTAIN_VILLAGE_SPRING_KEATON_GRASS",
};
constexpr int kKeatonRingCount = (int)(sizeof(kKeatonRings) / sizeof(kKeatonRings[0]));
constexpr int kKeatonRingSize = 9; // EnKusa2::unk_194[9]

using OptionOverrides = std::vector<std::pair<RandoOptionId, uint32_t>>;

/** Resolves every name, or fails naming the first missing one. */
int ResolveAll(const char* const* names, int count, std::set<RandoCheckId>& out, int code) {
    for (int i = 0; i < count; i++) {
        const RandoCheckId id = Rando::StaticData::GetCheckIdFromName(names[i]);
        if (id == RC_UNKNOWN) {
            return Fail(code, "MM's check table has no %s (#1410)", names[i]);
        }
        out.insert(id);
    }
    if ((int)out.size() != count) {
        return Fail(code, "%d names resolve to %zu distinct checks", count, out.size());
    }
    return 0;
}

/** How many logic regions list each check. */
std::map<RandoCheckId, int> RegionListings() {
    std::map<RandoCheckId, int> listed;
    for (const auto& [regionId, region] : Rando::Logic::Regions) {
        for (const auto& [checkId, condition] : region.checks) {
            listed[checkId]++;
        }
    }
    return listed;
}

int RegionCount(const std::map<RandoCheckId, int>& listed, RandoCheckId id) {
    const auto it = listed.find(id);
    return it == listed.end() ? 0 : it->second;
}

/** The table legs both table rows share: the type, a name, exactly one region,
 *  no cross-game host class, a row of Randomizer > MM Locations. */
int CheckRows(const std::set<RandoCheckId>& ids, RandoCheckType type, const char* what, int code) {
    MM_Rando_Init(); // fills Rando::Logic::Regions (irreversible; ShipInit::InitAll)
    const std::map<RandoCheckId, int> listed = RegionListings();
    MM_RandoLocationsUi_Register();
    std::set<RandoCheckId> located;
    for (int i = 0; i < Combo_MMLocationCount(); i++) {
        located.insert((RandoCheckId)Combo_MMLocationAt(i)->id);
    }
    for (RandoCheckId id : ids) {
        const auto& check = Rando::StaticData::Checks.at(id);
        if (check.randoCheckType != type) {
            return Fail(code, "%s is not of the %s check type", check.name, what);
        }
        if (check.name == nullptr || check.name[0] == '\0') {
            return Fail(code + 1, "check %d has no name", (int)id);
        }
        const int regions = RegionCount(listed, id);
        if (regions != 1) {
            return Fail(code + 2, "%s is listed by %d logic regions, expected exactly one", check.name, regions);
        }
        if (Rando::Foreign::IsForeignHostClass(id)) {
            return Fail(code + 3, "%s is a cross-game host class: an Ocarina of Time item would wait on it",
                        check.name);
        }
        if (located.count(id) == 0) {
            return Fail(code + 4, "%s is not a row of Randomizer > MM Locations", check.name);
        }
    }
    return 0;
}

/** One pool generation over a fresh save info, every option at its default but
 *  the ones in `overrides`. Seeded, so the run is deterministic. */
void GenerateWith(const OptionOverrides& overrides, std::vector<RandoCheckId>& checkPool,
                  std::vector<RandoItemId>& itemPool) {
    auto info = std::make_unique<RandoSaveInfo>();
    memset(info.get(), 0, sizeof(RandoSaveInfo));
    for (auto& [randoOptionId, row] : Rando::StaticData::Options) {
        info->randoSaveOptions[randoOptionId] = row.defaultValue;
    }
    info->randoSaveOptions[RO_LOGIC] = RO_LOGIC_GLITCHLESS;
    for (const auto& [randoOptionId, value] : overrides) {
        info->randoSaveOptions[randoOptionId] = value;
    }
    checkPool.clear();
    itemPool.clear();
    Ship_Random_Seed(0xB1E5u);
    Rando::Logic::GeneratePools(*info, checkPool, itemPool);
}

/** GenerateWith for two settings, with the user's exclusion list cleared around
 *  both and put back afterwards. */
void GeneratePair(const OptionOverrides& off, const OptionOverrides& on, std::vector<RandoCheckId>& offChecks,
                  std::vector<RandoItemId>& offItems, std::vector<RandoCheckId>& onChecks,
                  std::vector<RandoItemId>& onItems) {
    const std::string savedExcluded = CVarGetString("gRando.ExcludedChecks", "");
    CVarClear("gRando.ExcludedChecks");
    GenerateWith(off, offChecks, offItems);
    GenerateWith(on, onChecks, onItems);
    if (!savedExcluded.empty()) {
        CVarSetString("gRando.ExcludedChecks", savedExcluded.c_str());
    }
}

bool PoolContains(const std::vector<RandoCheckId>& pool, RandoCheckId id) {
    return std::find(pool.begin(), pool.end(), id) != pool.end();
}

/** Puts the MM engine back on the graph's own host set on every exit. */
struct HostPoolReset {
    ~HostPoolReset() {
        MM_ComboLogic_SetHostPool(nullptr, 0);
    }
};

/** The engine's whole `allEmptyHosts` answer, ascending. */
std::vector<uint16_t> EnumerateAllEmptyHosts(const ComboLogicEngine* engine) {
    const int total = engine->allEmptyHosts(engine->self, nullptr, 0);
    std::vector<uint16_t> out(total > 0 ? (size_t)total : (size_t)0);
    if (total > 0) {
        engine->allEmptyHosts(engine->self, out.data(), total);
    }
    return out;
}

/**
 * The coordinator's side of the story (#1410 item 3): MM's engine excludes, with
 * no setting involved, exactly what GeneratePools skips with no setting
 * involved. Since #1406 gave wonder items an option, a wonder item is not among
 * those. Three legs:
 *  - 46: the graph host universe (no pool handed in) offers every ring reward;
 *  - 47: MM_ComboLogic_SetHostPool keeps a handed-in ring reward and an
 *    ordinary check, both;
 *  - 48: with EVERY boolean RO_SHUFFLE_* option on (Shuffle Wonder Items among
 *    them, and any option added later without editing this row), every row of
 *    GeneratePools' check pool survives MM_ComboLogic_SetHostPool, and the ring
 *    rewards are among those rows. When a change lets GeneratePools pool a type
 *    IsUnconditionallyExcluded still refuses, this goes red instead of the
 *    paired fill silently leaving those checks unfilled.
 */
int RunHostUniverseLegs(const std::set<RandoCheckId>& wonder, RandoCheckId ordinary) {
    const ComboLogicEngine* engine = Combo_Logic_GetEngine(GAME_MM);
    if (engine == nullptr || engine->allEmptyHosts == nullptr) {
        return Fail(46, "no combo logic engine is registered for GAME_MM");
    }
    const int held = MM_ComboLogic_HeldPlacementCount();
    if (held != 0) {
        return Fail(46, "the MM engine holds %d placements before this leg, so allEmptyHosts would hide them", held);
    }
    HostPoolReset reset;

    // ---- 46: the graph universe ---------------------------------------------
    MM_ComboLogic_SetHostPool(nullptr, 0);
    const std::vector<uint16_t> universe = EnumerateAllEmptyHosts(engine);
    if (universe.empty()) {
        return Fail(46, "the MM engine's graph host universe is empty; the next assertion would be vacuous");
    }
    for (RandoCheckId id : wonder) {
        if (std::find(universe.begin(), universe.end(), (uint16_t)id) == universe.end()) {
            return Fail(46,
                        "%s is not in the MM engine's graph host universe: Shuffle Wonder Items pools it, so "
                        "IsUnconditionallyExcluded must not refuse it",
                        Rando::StaticData::Checks.at(id).name);
        }
    }

    // ---- 47: the pool seam --------------------------------------------------
    const uint16_t handed[2] = { (uint16_t)*wonder.begin(), (uint16_t)ordinary };
    MM_ComboLogic_SetHostPool(handed, 2);
    const std::vector<uint16_t> kept = EnumerateAllEmptyHosts(engine);
    const std::set<uint16_t> keptSet(kept.begin(), kept.end());
    if (kept.size() != 2 || keptSet.count(handed[0]) == 0 || keptSet.count(handed[1]) == 0) {
        return Fail(47, "a host pool of {%s, %s} enumerates %zu hosts, expected both",
                    Rando::StaticData::Checks.at(*wonder.begin()).name, Rando::StaticData::Checks.at(ordinary).name,
                    kept.size());
    }

    // ---- 48: every pool row is a host ---------------------------------------
    OptionOverrides everyShuffle;
    for (const auto& [randoOptionId, row] : Rando::StaticData::Options) {
        // RO_SHUFFLE_SONGS is a three-way mode, not an off/on switch; its default
        // (Anywhere) already pools every song.
        if (randoOptionId != RO_SHUFFLE_SONGS && row.name != nullptr && strncmp(row.name, "RO_SHUFFLE_", 11) == 0 &&
            row.defaultValue == 0) {
            everyShuffle.emplace_back(randoOptionId, 1u);
        }
    }
    std::vector<RandoCheckId> defaultChecks, everyChecks;
    std::vector<RandoItemId> defaultItems, everyItems;
    GeneratePair({}, everyShuffle, defaultChecks, defaultItems, everyChecks, everyItems);
    std::vector<uint16_t> pool;
    for (RandoCheckId id : everyChecks) {
        pool.push_back((uint16_t)id);
    }
    MM_ComboLogic_SetHostPool(pool.data(), (int)pool.size());
    const std::vector<uint16_t> hosts = EnumerateAllEmptyHosts(engine);
    const std::set<uint16_t> hostSet(hosts.begin(), hosts.end());
    for (uint16_t id : pool) {
        if (hostSet.count(id) == 0) {
            return Fail(48,
                        "%s is in GeneratePools' check pool with every shuffle option on, but "
                        "MM_ComboLogic_SetHostPool dropped it: IsUnconditionallyExcluded and GeneratePools no "
                        "longer tell the same story, and a paired fill would leave that check unfilled",
                        Rando::StaticData::Checks.at((RandoCheckId)id).name);
        }
    }
    if (hostSet.size() != std::set<uint16_t>(pool.begin(), pool.end()).size()) {
        return Fail(48, "the host pool enumerates %zu hosts for a check pool of %zu distinct checks", hostSet.size(),
                    std::set<uint16_t>(pool.begin(), pool.end()).size());
    }
    for (RandoCheckId id : wonder) {
        if (hostSet.count((uint16_t)id) == 0) {
            return Fail(48,
                        "%s is not a host with every shuffle option on: Shuffle Wonder Items no longer pools it, "
                        "so this leg no longer covers the wonder type",
                        Rando::StaticData::Checks.at(id).name);
        }
    }
    printf("[TEST] mm-extra-grass-shuffle: %zu graph hosts; %zu options on -> %zu pool rows, all hosts\n",
           universe.size(), everyShuffle.size(), pool.size());
    return 0;
}

// ---------------------------------------------------------------------------
// mm-hive-shuffle
// ---------------------------------------------------------------------------

int RunHiveOptionAndPool() {
    // ---- (1) the option -----------------------------------------------------
    const RandoOptionId hiveOption = Rando::StaticData::GetOptionIdFromName(kHiveOptionName);
    if (hiveOption >= RO_MAX) {
        return Fail(1, "MM has no %s option: beehive shuffle is missing from MM (#1410)", kHiveOptionName);
    }
    const auto& optionRow = Rando::StaticData::Options.at(hiveOption);
    if (optionRow.defaultValue != RO_GENERIC_OFF) {
        return Fail(2, "%s defaults to %u, expected off", kHiveOptionName, (unsigned)optionRow.defaultValue);
    }
    if (strcmp(optionRow.cvar, kHiveOptionCVar) != 0) {
        return Fail(3, "%s binds '%s', expected '%s'", kHiveOptionName, optionRow.cvar, kHiveOptionCVar);
    }

    // ---- (2) the page row ---------------------------------------------------
    MM_RandoOptionsUi_Register();
    const ComboMMOptionDesc* row = Combo_MMOptionById((uint16_t)hiveOption);
    if (row == NULL) {
        return Fail(4, "the MM Randomizer page has no row for %s", kHiveOptionName);
    }
    if (row->label == NULL || strcmp(row->label, kHiveRowLabel) != 0 || row->tooltip == NULL ||
        strcmp(row->tooltip, kHiveRowTooltip) != 0) {
        return Fail(5, "the %s row reads '%s' / '%s', expected '%s' / '%s'", kHiveOptionName,
                    row->label ? row->label : "(null)", row->tooltip ? row->tooltip : "(null)", kHiveRowLabel,
                    kHiveRowTooltip);
    }
    if (row->widget != (uint8_t)COMBO_MM_WIDGET_CHECKBOX || row->defaultValue != 0 ||
        row->liveness != (uint8_t)COMBO_MM_LIVENESS_LIVE ||
        strcmp(Combo_MMOptionGroupName(row->group), "Shuffle Items") != 0) {
        return Fail(6, "the %s row is not a live, default-off checkbox under Shuffle Items", kHiveOptionName);
    }
    // Upstream's menu order is Tree Drops, Butterflies, Frogs, Hive Drops; Frogs has
    // no row on this page, so the hive row follows Shuffle Butterflies (#1407), which
    // follows Shuffle Tree Drops (mm-butterfly-shuffle leg 7).
    int butterflyAt = -1;
    int hiveAt = -1;
    for (int i = 0; i < Combo_MMOptionCount(); i++) {
        const ComboMMOptionDesc* desc = Combo_MMOptionAt(i);
        if (desc->id == (uint16_t)RO_SHUFFLE_BUTTERFLIES) {
            butterflyAt = i;
        } else if (desc->id == (uint16_t)hiveOption) {
            hiveAt = i;
        }
    }
    if (butterflyAt < 0 || hiveAt != butterflyAt + 1) {
        return Fail(7,
                    "Shuffle Hive Drops is row %d, Shuffle Butterflies row %d: the hive row belongs right after "
                    "the butterfly row (upstream's menu order)",
                    hiveAt, butterflyAt);
    }

    // ---- (3) the check table ------------------------------------------------
    std::set<RandoCheckId> hives;
    int rc = ResolveAll(kHives, kHiveCount, hives, 8);
    if (rc != 0) {
        return rc;
    }
    for (const auto& [randoCheckId, check] : Rando::StaticData::Checks) {
        const bool isHive = hives.count(randoCheckId) != 0;
        if (isHive != (check.randoCheckType == RCTYPE_BEEHIVE)) {
            return Fail(9, "%s %s RCTYPE_BEEHIVE: that type must hold exactly the %d hives", check.name,
                        isHive ? "lacks" : "carries", kHiveCount);
        }
    }
    rc = CheckRows(hives, RCTYPE_BEEHIVE, "beehive", 10);
    if (rc != 0) {
        return rc;
    }

    // ---- (4) pool generation ------------------------------------------------
    std::vector<RandoCheckId> offChecks, onChecks;
    std::vector<RandoItemId> offItems, onItems;
    GeneratePair({ { hiveOption, RO_GENERIC_OFF } }, { { hiveOption, RO_GENERIC_ON } }, offChecks, offItems, onChecks,
                 onItems);
    for (RandoCheckId id : offChecks) {
        if (hives.count(id) != 0) {
            return Fail(15, "with Shuffle Hive Drops off, %s entered the check pool",
                        Rando::StaticData::Checks.at(id).name);
        }
    }
    for (RandoCheckId id : hives) {
        if (!PoolContains(onChecks, id)) {
            return Fail(16, "with Shuffle Hive Drops on, %s did not enter the check pool",
                        Rando::StaticData::Checks.at(id).name);
        }
    }
    std::vector<RandoCheckId> onWithoutHives;
    for (RandoCheckId id : onChecks) {
        if (hives.count(id) == 0) {
            onWithoutHives.push_back(id);
        }
    }
    if (onWithoutHives != offChecks) {
        return Fail(17,
                    "the option-off check pool (%zu) is not the option-on pool minus the hives (%zu): the option "
                    "moves checks it does not own",
                    offChecks.size(), onWithoutHives.size());
    }
    if (onItems.size() != offItems.size() + hives.size()) {
        return Fail(18, "the option-on item pool holds %zu items, expected %zu (one per hive)", onItems.size(),
                    offItems.size() + hives.size());
    }
    printf("[TEST] mm-hive-shuffle: %d hives, option off -> %zu checks, option on -> %zu checks\n", kHiveCount,
           offChecks.size(), onChecks.size());
    return 0;
}

// ---------------------------------------------------------------------------
// mm-extra-grass-shuffle
// ---------------------------------------------------------------------------

int RunExtraGrassTableAndPool() {
    // ---- (1) the 171 rows ---------------------------------------------------
    std::set<RandoCheckId> grass;
    int rc = ResolveAll(kNewGrass, kGrassCount, grass, 30);
    if (rc != 0) {
        return rc;
    }
    rc = CheckRows(grass, RCTYPE_GRASS, "grass", 31);
    if (rc != 0) {
        return rc;
    }

    // ---- (2) the Keaton rings -----------------------------------------------
    char name[96];
    for (const char* ring : kKeatonRings) {
        snprintf(name, sizeof(name), "%s_01", ring);
        const RandoCheckId base = Rando::StaticData::GetCheckIdFromName(name);
        if (base == RC_UNKNOWN) {
            return Fail(36, "MM's check table has no %s", name);
        }
        for (int i = 1; i < kKeatonRingSize; i++) {
            snprintf(name, sizeof(name), "%s_%02d", ring, i + 1);
            const RandoCheckId id = Rando::StaticData::GetCheckIdFromName(name);
            if (id != (RandoCheckId)(base + i)) {
                return Fail(37,
                            "%s is check %d, expected %d (%s_01 + %d): ObjGrass.cpp gives ring bush i the check "
                            "base + i, so a ring's ids must be contiguous",
                            name, (int)id, (int)(base + i), ring, i);
            }
        }
    }

    // ---- (3) the wonder items -----------------------------------------------
    std::set<RandoCheckId> wonder;
    rc = ResolveAll(kWonderItems, kWonderCount, wonder, 38);
    if (rc != 0) {
        return rc;
    }
    const std::map<RandoCheckId, int> listed = RegionListings();
    for (const auto& [randoCheckId, check] : Rando::StaticData::Checks) {
        const bool isWonder = wonder.count(randoCheckId) != 0;
        if (isWonder && check.randoCheckType != RCTYPE_WONDER_ITEM) {
            return Fail(39,
                        "%s is not an RCTYPE_WONDER_ITEM row: Shuffle Wonder Items would not govern a ring's "
                        "reward",
                        check.name);
        }
        if (isWonder && (check.name == nullptr || check.name[0] == '\0')) {
            return Fail(40, "wonder item check %d has no name", (int)randoCheckId);
        }
        if (isWonder && RegionCount(listed, randoCheckId) != 1) {
            return Fail(40, "%s is listed by %d logic regions, expected exactly one", check.name,
                        RegionCount(listed, randoCheckId));
        }
    }

    // ---- (4) pool generation ------------------------------------------------
    std::vector<RandoCheckId> offChecks, onChecks;
    std::vector<RandoItemId> offItems, onItems;
    GeneratePair({ { RO_SHUFFLE_GRASS_DROPS, RO_GENERIC_NO } }, { { RO_SHUFFLE_GRASS_DROPS, RO_GENERIC_YES } },
                 offChecks, offItems, onChecks, onItems);
    for (RandoCheckId id : offChecks) {
        if (grass.count(id) != 0) {
            return Fail(41, "with Shuffle Grass Drops off, %s entered the check pool",
                        Rando::StaticData::Checks.at(id).name);
        }
    }
    for (RandoCheckId id : grass) {
        if (!PoolContains(onChecks, id)) {
            return Fail(42, "with Shuffle Grass Drops on, %s did not enter the check pool",
                        Rando::StaticData::Checks.at(id).name);
        }
    }
    std::vector<RandoCheckId> onWithoutGrass;
    size_t grassRowsOn = 0;
    for (RandoCheckId id : onChecks) {
        if (Rando::StaticData::Checks.at(id).randoCheckType == RCTYPE_GRASS) {
            grassRowsOn++;
        } else {
            onWithoutGrass.push_back(id);
        }
    }
    if (onWithoutGrass != offChecks) {
        return Fail(43,
                    "the grass-off check pool (%zu) is not the grass-on pool minus its grass (%zu): the grass "
                    "option moves checks it does not own",
                    offChecks.size(), onWithoutGrass.size());
    }
    if (onItems.size() != offItems.size() + grassRowsOn) {
        return Fail(44, "the grass-on item pool holds %zu items, expected %zu (one per grass check)", onItems.size(),
                    offItems.size() + grassRowsOn);
    }
    std::vector<RandoCheckId> defaultChecks, allChecks;
    std::vector<RandoItemId> defaultItems, allItems;
    GeneratePair({}, { { RO_SHUFFLE_GRASS_DROPS, RO_GENERIC_YES }, { RO_SHUFFLE_HIVE_DROPS, RO_GENERIC_ON } },
                 defaultChecks, defaultItems, allChecks, allItems);
    const std::vector<RandoCheckId>* pools[] = { &offChecks, &onChecks, &defaultChecks, &allChecks };
    for (const std::vector<RandoCheckId>* pool : pools) {
        for (RandoCheckId id : wonder) {
            if (PoolContains(*pool, id)) {
                return Fail(45,
                            "%s entered a check pool with Shuffle Wonder Items off: a ring's reward follows that "
                            "option (#1406), not grass or hives",
                            Rando::StaticData::Checks.at(id).name);
            }
        }
    }
    std::vector<RandoCheckId> wonderOffChecks, wonderOnChecks;
    std::vector<RandoItemId> wonderOffItems, wonderOnItems;
    GeneratePair({}, { { RO_SHUFFLE_WONDER_ITEMS, RO_GENERIC_YES } }, wonderOffChecks, wonderOffItems, wonderOnChecks,
                 wonderOnItems);
    for (RandoCheckId id : wonder) {
        if (!PoolContains(wonderOnChecks, id)) {
            return Fail(45, "with Shuffle Wonder Items on, %s did not enter the check pool",
                        Rando::StaticData::Checks.at(id).name);
        }
    }
    rc = RunHostUniverseLegs(wonder, *grass.begin());
    if (rc != 0) {
        return rc;
    }
    printf("[TEST] mm-extra-grass-shuffle: %d new grass rows (%d Keaton rings of %d), %d wonder items; grass off "
           "-> %zu checks, grass on -> %zu, grass and hives on -> %zu\n",
           kGrassCount, kKeatonRingCount, kKeatonRingSize, kWonderCount, offChecks.size(), onChecks.size(),
           allChecks.size());
    return 0;
}

// ---------------------------------------------------------------------------
// mm-hive-grass-shuffle-hooks
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

size_t CombInitCount() {
    return SettledCountForId<GameInteractor::OnActorInit>(ACTOR_OBJ_COMB);
}
size_t Kusa2InitCount() {
    return SettledCountForId<GameInteractor::OnActorInit>(ACTOR_EN_KUSA2);
}
size_t Kusa2UpdateCount() {
    return SettledCountForId<GameInteractor::OnActorUpdate>(ACTOR_EN_KUSA2);
}
size_t HiveSpawnCount() {
    return SettledCountForId<GameInteractor::ShouldVanillaBehavior>(VB_BEEHIVE_SPAWN_ACTOR);
}
size_t HiveBrokenCount() {
    return SettledCountForId<GameInteractor::ShouldVanillaBehavior>(VB_BEEHIVE_ALREADY_BROKEN);
}
size_t KeatonAttachCount() {
    return SettledCountForId<GameInteractor::ShouldVanillaBehavior>(VB_KEATON_GRASS_ATTACH_CHILD);
}
size_t KeatonDrawCount() {
    return SettledCountForId<GameInteractor::ShouldVanillaBehavior>(VB_KEATON_GRASS_DRAW);
}
size_t KusaDrawCount() {
    return SettledCountForId<GameInteractor::ShouldVanillaBehavior>(VB_KUSA_DRAW_BE_OVERRIDDEN);
}
size_t GrassDropCount() {
    return SettledCountForId<GameInteractor::ShouldVanillaBehavior>(VB_GRASS_DROP_COLLECTIBLE);
}

/** The production arm path: a save of the given type, then the behaviour
 *  registrars a save load runs (Rando.cpp OnSaveLoadHandler). */
void DriveArmState(bool rando) {
    gSaveContext.save.shipSaveInfo.saveType = rando ? SAVETYPE_RANDO : SAVETYPE_VANILLA;
    Rando::ActorBehavior::OnFileLoad();
}

/** Runs only the ID-keyed OnActorInit registrants for `actor`'s id: the unkeyed
 *  ones belong to other behaviours and are not this row's to exercise. */
void RunIdKeyedInit(Actor* actor) {
    S2H::GameHooks::FlushPendingUnregistrations<GameInteractor::OnActorInit>();
    auto& buckets = S2H::GameHooks::Registry<GameInteractor::OnActorInit>::functionsForID;
    const auto bucket = buckets.find(actor->id);
    if (bucket != buckets.end()) {
        const auto hooks = bucket->second;
        for (const auto& [hookId, hook] : hooks) {
            hook(actor);
        }
    }
}

void ArmCheck(RandoCheckId id, bool shuffled, bool cycleObtained, bool obtained) {
    RANDO_SAVE_CHECKS[id].shuffled = shuffled;
    RANDO_SAVE_CHECKS[id].cycleObtained = cycleObtained;
    RANDO_SAVE_CHECKS[id].obtained = obtained;
}

RandoCheckId CheckOf(Actor* actor) {
    return Rando::ActorBehavior::GetObjectRandoCheckId(actor);
}

/** Zeroed actor-shaped blocks, freed (with every object extension on them) when
 *  the row ends, pass or fail. */
struct Fixtures {
    std::vector<void*> blocks;

    template <typename T> T* Make(s16 actorId, s16 params) {
        T* block = static_cast<T*>(calloc(1, sizeof(T)));
        if (block != nullptr) {
            Actor* actor = reinterpret_cast<Actor*>(block);
            actor->id = actorId;
            actor->params = params;
            blocks.push_back(block);
        }
        return block;
    }

    ~Fixtures() {
        for (void* block : blocks) {
            ObjectExtension::GetInstance().Free(block);
            free(block);
        }
    }
};

int RunHookLegs(Fixtures& fx, PlayState* play) {
    RegistryProbe probes[] = {
        { "OnActorInit[ACTOR_OBJ_COMB]", CombInitCount, 0 },
        { "OnActorInit[ACTOR_EN_KUSA2]", Kusa2InitCount, 0 },
        { "OnActorUpdate[ACTOR_EN_KUSA2]", Kusa2UpdateCount, 0 },
        { "ShouldVanillaBehavior[VB_BEEHIVE_SPAWN_ACTOR]", HiveSpawnCount, 0 },
        { "ShouldVanillaBehavior[VB_BEEHIVE_ALREADY_BROKEN]", HiveBrokenCount, 0 },
        { "ShouldVanillaBehavior[VB_KEATON_GRASS_ATTACH_CHILD]", KeatonAttachCount, 0 },
        { "ShouldVanillaBehavior[VB_KEATON_GRASS_DRAW]", KeatonDrawCount, 0 },
        { "ShouldVanillaBehavior[VB_KUSA_DRAW_BE_OVERRIDDEN]", KusaDrawCount, 0 },
        { "ShouldVanillaBehavior[VB_GRASS_DROP_COLLECTIBLE]", GrassDropCount, 0 },
    };

    const RandoCheckId goldHive = Rando::StaticData::GetCheckIdFromName("RC_SWAMP_SPIDER_HOUSE_GOLD_ROOM_BEEHIVE_01");
    const RandoCheckId coastCowHive = Rando::StaticData::GetCheckIdFromName("RC_GREAT_BAY_COAST_COW_GROTTO_BEEHIVE");
    const RandoCheckId fieldCowHive = Rando::StaticData::GetCheckIdFromName("RC_TERMINA_FIELD_COW_GROTTO_BEEHIVE");
    const RandoCheckId ringBase = Rando::StaticData::GetCheckIdFromName("RC_CLOCK_TOWN_NORTH_KEATON_GRASS_01");
    const RandoCheckId ringWonder =
        Rando::StaticData::GetCheckIdFromName("RC_CLOCK_TOWN_NORTH_KEATON_GRASS_WONDER_ITEM");
    const RandoCheckId courtyard =
        Rando::StaticData::GetCheckIdFromName("RC_ANCIENT_CASTLE_OF_IKANA_COURTYARD_GRASS_01");
    if (goldHive == RC_UNKNOWN || coastCowHive == RC_UNKNOWN || fieldCowHive == RC_UNKNOWN || ringBase == RC_UNKNOWN ||
        ringWonder == RC_UNKNOWN || courtyard == RC_UNKNOWN) {
        return Fail(50, "MM's check table lacks a beehive, Keaton grass or courtyard grass check (#1410)");
    }

    // ---- Leg 1: registry, vanilla save -> rando save -------------------------
    MM_gPlayState = play;
    DriveArmState(false);
    for (auto& probe : probes) {
        probe.before = probe.count();
    }
    DriveArmState(true);
    for (const auto& probe : probes) {
        const size_t after = probe.count();
        if (after != probe.before + 1) {
            return Fail(51,
                        "%s went %zu -> %zu when a rando save loaded, expected exactly +1: ObjComb.cpp or "
                        "ObjGrass.cpp is not in the link, is not reached from ActorBehavior::OnFileLoad, or a second "
                        "registrant made the probe unattributable",
                        probe.what, probe.before, after);
        }
    }

    // ---- Leg 2: hive identification ------------------------------------------
    ObjComb* hive = fx.Make<ObjComb>(ACTOR_OBJ_COMB, 0);
    ObjComb* bareHive = fx.Make<ObjComb>(ACTOR_OBJ_COMB, 0);
    ObjComb* coastHive = fx.Make<ObjComb>(ACTOR_OBJ_COMB, 0);
    ObjComb* fieldHive = fx.Make<ObjComb>(ACTOR_OBJ_COMB, 0);
    if (hive == nullptr || bareHive == nullptr || coastHive == nullptr || fieldHive == nullptr) {
        return Fail(52, "could not allocate the hive fixtures");
    }
    play->sceneId = SCENE_KINSTA1;
    play->roomCtx.curRoom.num = 2;
    SetActorListIndex(&hive->actor, 9);
    SetActorListIndex(&bareHive->actor, 9);
    ArmCheck(goldHive, true, false, false);
    RunIdKeyedInit(&hive->actor);
    if (CheckOf(&hive->actor) != goldHive) {
        return Fail(53,
                    "OnActorInit on the Swamp Spider House gold room's first hive (room 2, actor 9) attached check "
                    "%d, expected %s",
                    (int)CheckOf(&hive->actor), Rando::StaticData::Checks.at(goldHive).name);
    }
    ArmCheck(goldHive, false, false, false);
    RunIdKeyedInit(&bareHive->actor);
    if (CheckOf(&bareHive->actor) != RC_UNKNOWN) {
        return Fail(54, "an unshuffled hive was given check %d", (int)CheckOf(&bareHive->actor));
    }
    play->sceneId = SCENE_KAKUSIANA;
    play->roomCtx.curRoom.num = 10;
    SetActorListIndex(&coastHive->actor, 2);
    SetActorListIndex(&fieldHive->actor, 2);
    ArmCheck(coastCowHive, true, false, false);
    ArmCheck(fieldCowHive, true, false, false);
    gSaveContext.respawn[RESPAWN_MODE_UNK_3].data = 0;
    RunIdKeyedInit(&coastHive->actor);
    gSaveContext.respawn[RESPAWN_MODE_UNK_3].data = 31;
    RunIdKeyedInit(&fieldHive->actor);
    if (CheckOf(&coastHive->actor) != coastCowHive || CheckOf(&fieldHive->actor) != fieldCowHive) {
        return Fail(55,
                    "the shared cow-grotto room's hive resolved to %d / %d, expected %s (respawn data 0) / %s "
                    "(respawn data 31)",
                    (int)CheckOf(&coastHive->actor), (int)CheckOf(&fieldHive->actor),
                    Rando::StaticData::Checks.at(coastCowHive).name, Rando::StaticData::Checks.at(fieldCowHive).name);
    }

    // ---- Leg 3: a Keaton ring ------------------------------------------------
    EnKusa2* ring = fx.Make<EnKusa2>(ACTOR_EN_KUSA2, 0);
    EnKusa2* otherRing = fx.Make<EnKusa2>(ACTOR_EN_KUSA2, 0);
    EnKusa2* strayBush = fx.Make<EnKusa2>(ACTOR_EN_KUSA2, 1);
    EnKusa2* bushes[kKeatonRingSize] = {};
    for (int i = 0; i < kKeatonRingSize; i++) {
        bushes[i] = fx.Make<EnKusa2>(ACTOR_EN_KUSA2, 1);
        if (bushes[i] == nullptr) {
            return Fail(56, "could not allocate the Keaton bush fixtures");
        }
    }
    if (ring == nullptr || otherRing == nullptr || strayBush == nullptr) {
        return Fail(56, "could not allocate the Keaton ring fixtures");
    }
    play->sceneId = SCENE_BACKTOWN;
    ring->actor.room = 0;
    otherRing->actor.room = 0;
    SetActorListIndex(&ring->actor, 16);
    SetActorListIndex(&otherRing->actor, 17);
    ArmCheck(ringWonder, false, false, false); // unshuffled: Shuffle Wonder Items off (its default)
    RunIdKeyedInit(&ring->actor);
    RunIdKeyedInit(&otherRing->actor);
    const int obtainedBush = 4;
    for (int i = 0; i < kKeatonRingSize; i++) {
        ArmCheck((RandoCheckId)(ringBase + i), true, i == obtainedBush, false);
        bushes[i]->unk_1C0 = ring;
        GameInteractor_Should(VB_KEATON_GRASS_ATTACH_CHILD, true, bushes[i], (s32)i);
    }
    strayBush->unk_1C0 = otherRing;
    GameInteractor_Should(VB_KEATON_GRASS_ATTACH_CHILD, true, strayBush, (s32)0);
    for (int i = 0; i < kKeatonRingSize; i++) {
        const RandoCheckId want = i == obtainedBush ? RC_UNKNOWN : (RandoCheckId)(ringBase + i);
        const RandoCheckId got = CheckOf(&bushes[i]->actor);
        if (got != want) {
            return Fail(57, "Clock Town North's ring bush %d holds check %d, expected %d%s", i, (int)got, (int)want,
                        i == obtainedBush ? " (its check was obtained this cycle)" : " (the ring's base + index)");
        }
    }
    if (CheckOf(&strayBush->actor) != RC_UNKNOWN || CheckOf(&ring->actor) != RC_UNKNOWN) {
        return Fail(58, "a bush of an unmapped ring, or the ring manager itself, was given a check");
    }
    // A bush with no check keeps its vanilla draw and drop; the wonder item is
    // unshuffled here, so the ring's last bush still drops the vanilla red rupee.
    ring->unk_1BC = 8;
    if (!GameInteractor_Should(VB_KEATON_GRASS_DRAW, true, bushes[obtainedBush]) ||
        !GameInteractor_Should(VB_GRASS_DROP_COLLECTIBLE, true, (int32_t)ACTOR_EN_KUSA2, bushes[obtainedBush])) {
        return Fail(59, "a Keaton bush with no check lost its vanilla draw or drop");
    }

    // ---- Leg 4: new sprout grass ---------------------------------------------
    EnKusa* sprout = fx.Make<EnKusa>(ACTOR_EN_KUSA, ENKUSA_TYPE_GRASS);
    EnKusa* bush = fx.Make<EnKusa>(ACTOR_EN_KUSA, ENKUSA_TYPE_BUSH);
    EnKusa* picked = fx.Make<EnKusa>(ACTOR_EN_KUSA, ENKUSA_TYPE_GRASS);
    if (sprout == nullptr || bush == nullptr || picked == nullptr) {
        return Fail(60, "could not allocate the En_Kusa fixtures");
    }
    play->sceneId = SCENE_CASTLE;
    sprout->actor.room = 0;
    SetActorListIndex(&sprout->actor, 32);
    ArmCheck(courtyard, true, false, false);
    RunIdKeyedInit(&sprout->actor);
    if (CheckOf(&sprout->actor) != courtyard) {
        return Fail(61,
                    "OnActorInit on the Ikana Castle courtyard's first sprout (room 0, actor 32) attached check %d, "
                    "expected %s",
                    (int)CheckOf(&sprout->actor), Rando::StaticData::Checks.at(courtyard).name);
    }
    Rando::ActorBehavior::SetObjectRandoCheckId(&bush->actor, courtyard);
    Rando::ActorBehavior::SetObjectRandoCheckId(&picked->actor, ringBase);
    ArmCheck(ringBase, true, true, true);
    const bool sproutVanilla = GameInteractor_Should(VB_KUSA_DRAW_BE_OVERRIDDEN, true, &sprout->actor);
    const bool bushVanilla = GameInteractor_Should(VB_KUSA_DRAW_BE_OVERRIDDEN, true, &bush->actor);
    const bool pickedVanilla = GameInteractor_Should(VB_KUSA_DRAW_BE_OVERRIDDEN, true, &picked->actor);
    if (sproutVanilla || sprout->actor.draw == nullptr) {
        return Fail(62, "VB_KUSA_DRAW_BE_OVERRIDDEN kept the vanilla draw of a grass sprout carrying a live check");
    }
    if (bushVanilla || bush->actor.draw == nullptr) {
        return Fail(63, "VB_KUSA_DRAW_BE_OVERRIDDEN kept the vanilla draw of a bush carrying a live check");
    }
    if (sprout->actor.draw == bush->actor.draw) {
        return Fail(64, "a grass sprout and a bush were given the same rando draw: the sprout must use the cuttable "
                        "grass models, not the bush ones");
    }
    if (!pickedVanilla || picked->actor.draw != nullptr) {
        return Fail(65, "VB_KUSA_DRAW_BE_OVERRIDDEN replaced the draw of grass whose check was already obtained");
    }
    return 0;
}

} // namespace

extern "C" int MM_HiveShuffle_RunHeadless(void) {
    printf("[TEST] mm-hive-shuffle: Shuffle Hive Drops is an MM option, a page row, 19 checks and a pool switch "
           "(#1410)\n");
    auto ctx = Ship::Context::GetInstance();
    if (ctx == nullptr || ctx->GetConsoleVariables() == nullptr) {
        return Fail(1, "Ship::Context or its ConsoleVariables are missing; run the shared bring-up first");
    }
    const int rc = RunHiveOptionAndPool();
    if (rc != 0) {
        return rc;
    }
    printf("[TEST] mm-hive-shuffle: PASS\n");
    return 0;
}

extern "C" int MM_ExtraGrassShuffle_RunHeadless(void) {
    printf("[TEST] mm-extra-grass-shuffle: 171 grass rows behind Shuffle Grass Drops, three contiguous Keaton "
           "rings, ring rewards behind Shuffle Wonder Items (#1410)\n");
    auto ctx = Ship::Context::GetInstance();
    if (ctx == nullptr || ctx->GetConsoleVariables() == nullptr) {
        return Fail(1, "Ship::Context or its ConsoleVariables are missing; run the shared bring-up first");
    }
    const int rc = RunExtraGrassTableAndPool();
    if (rc != 0) {
        return rc;
    }
    printf("[TEST] mm-extra-grass-shuffle: PASS\n");
    return 0;
}

extern "C" int MM_HiveGrassShuffleHooks_RunHeadless(void) {
    printf("[TEST] mm-hive-grass-shuffle-hooks: the hive and Keaton grass behaviours arm from "
           "ActorBehavior::OnFileLoad and identify their checks (#1410)\n");
    auto ctx = Ship::Context::GetInstance();
    if (ctx == nullptr || ctx->GetConsoleVariables() == nullptr) {
        return Fail(1, "Ship::Context or its ConsoleVariables are missing; run the shared bring-up first");
    }

    auto saved = std::make_unique<uint8_t[]>(sizeof(gSaveContext));
    memcpy(saved.get(), &gSaveContext, sizeof(gSaveContext));
    PlayState* const savedPlay = MM_gPlayState;
    PlayState* play = static_cast<PlayState*>(calloc(1, sizeof(PlayState)));
    if (play == nullptr) {
        return Fail(1, "could not allocate the play state");
    }

    int rc = 0;
    {
        Fixtures fx;
        rc = RunHookLegs(fx, play);

        // ---- Cleanup (pass and fail) ---------------------------------------
        MM_gPlayState = savedPlay;
        memcpy(&gSaveContext, saved.get(), sizeof(gSaveContext));
        Rando::ActorBehavior::OnFileLoad();
    }
    free(play);

    if (rc != 0) {
        return rc;
    }
    printf("[TEST] mm-hive-grass-shuffle-hooks: PASS (nine single-registrant hooks armed; hives, a Keaton ring and "
           "courtyard grass identified; sprout and bush draws swapped)\n");
    return 0;
}

#endif /* RSBS_SINGLE_EXECUTABLE */
