/**
 * ROM-free locks for Majora's Mask's Tingle maps (#1499). CTest label "redship".
 * Two rows, two entry points:
 *
 * ============================================================================
 * mm-tingle-map-pool (MMTingleMapPool) -- one copy of each map, none at start
 * ============================================================================
 *
 * The twelve Tingle checks carry the six maps twice (StaticData/Checks.cpp), and
 * owning one copy turns every other copy into junk (ConvertItem: the map's
 * WEEKEVENTREG_TINGLE_MAP_BOUGHT_* flag). Over the real GeneratePools, with
 * Shuffle Tingle Maps on:
 *  (1) Starting Maps and Compasses off: each RI_TINGLE_MAP_* is in the item
 *      pool exactly once, with Plentiful off and with it on (Plentiful never adds
 *      a second copy).
 *  (2) Starting Maps and Compasses on: no RI_TINGLE_MAP_* is in the item pool,
 *      with Plentiful off and on.
 *  (3) Every leg: the twelve Tingle checks are still in the check pool, and the
 *      K9 seam (gRsbsComboPlentifulFrom / To / gRsbsComboPoolSize) describes the
 *      pool returned.
 *
 * ============================================================================
 * mm-tingle-map-display (MMTingleMapDisplay) -- every surface names one item
 * ============================================================================
 *
 * Over MM's live save: Tingle's Clock Town Map placed on RC_BOMB_SHOP_ITEM_01,
 * seen and uncollected.
 *  (4) Flag clear (non-vacuity): the owner's hand, the shelf, the shelf's name,
 *      the MM Check Tracker's label and the Combo Tracker's MM pane all name the
 *      map.
 *  (5) WEEKEVENTREG_TINGLE_MAP_BOUGHT_CLOCK_TOWN set: all five name the junk the
 *      slot sells (RI_JUNK, "Junk"), never the map.
 *  (6) The junk sale: that shelf is not sold out before the sale and is sold out
 *      after it (obtained); a Tingle slot holding an owned map is likewise
 *      refused once collected.
 *  (7) A paid junk sale never gives RI_NONE: over 256 seeds with the junk cache
 *      cold, Rando::CurrentPaidJunkItem never answers RI_NONE, while
 *      Rando::CurrentJunkItem answers it at least once (non-vacuity).
 *
 * Restores the SaveContext image, gComboCtx and the junk cache on the way out.
 */

#ifndef RSBS_SINGLE_EXECUTABLE
extern "C" int MM_TingleMapPool_RunHeadless(void) {
    return 0;
}
extern "C" int MM_TingleMapDisplay_RunHeadless(void) {
    return 0;
}
#else

#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include <libultraship/bridge/consolevariablebridge.h>
#include <ship/Context.h>

#include "2s2h/Rando/ActorBehavior/ActorBehavior.h"
#include "2s2h/Rando/Logic/Logic.h"
#include "2s2h/Rando/Rando.h"
#include "2s2h/Rando/StaticData/StaticData.h"
#include "2s2h/ShipUtils.h"

// src/common: the combo record (no foreign placement while the display legs run).
#include "context.h"

extern "C" {
#include "variables.h"
#include "functions.h"
void MM_Rando_Init(void);
void MM_Rando_TestColdJunkCache(void);
int MM_CheckTracker_TestSeenItemName(uint16_t randoCheckId, char* out, int cap);
const char* MM_TrackerAdapter_TestPlacedItemName(const void* save, uint16_t checkId);
}

// GeneratePools.cpp (#645 lane K9): where Plentiful's copies sit in the last pool.
extern size_t gRsbsComboPlentifulFrom;
extern size_t gRsbsComboPlentifulTo;
extern size_t gRsbsComboPoolSize;

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

// clang-format off
const RandoItemId kMaps[] = {
    RI_TINGLE_MAP_CLOCK_TOWN,
    RI_TINGLE_MAP_GREAT_BAY,
    RI_TINGLE_MAP_ROMANI_RANCH,
    RI_TINGLE_MAP_SNOWHEAD,
    RI_TINGLE_MAP_STONE_TOWER,
    RI_TINGLE_MAP_WOODFALL,
};
// clang-format on

const char* ItemName(RandoItemId id) {
    const auto it = Rando::StaticData::Items.find(id);
    return (it != Rando::StaticData::Items.end() && it->second.name != nullptr) ? it->second.name : "(unnamed)";
}

/** One pool generation over a fresh save info, every option at its default but
 *  Shuffle Tingle Maps (on) and the two named here. Seeded, so deterministic. */
void GenerateTinglePool(bool startingMaps, bool plentiful, std::vector<RandoCheckId>& checkPool,
                        std::vector<RandoItemId>& itemPool) {
    auto info = std::make_unique<RandoSaveInfo>();
    memset(info.get(), 0, sizeof(RandoSaveInfo));
    for (auto& [randoOptionId, row] : Rando::StaticData::Options) {
        info->randoSaveOptions[randoOptionId] = row.defaultValue;
    }
    info->randoSaveOptions[RO_LOGIC] = RO_LOGIC_GLITCHLESS;
    info->randoSaveOptions[RO_SHUFFLE_TINGLE_SHOPS] = RO_GENERIC_YES;
    info->randoSaveOptions[RO_STARTING_MAPS_AND_COMPASSES] = startingMaps ? RO_GENERIC_YES : RO_GENERIC_NO;
    info->randoSaveOptions[RO_PLENTIFUL_ITEMS] = plentiful ? RO_GENERIC_YES : RO_GENERIC_NO;
    checkPool.clear();
    itemPool.clear();
    Ship_Random_Seed(0x1499u);
    Rando::Logic::GeneratePools(*info, checkPool, itemPool);
}

int RunPoolLeg(bool startingMaps, bool plentiful) {
    const char* leg = startingMaps ? (plentiful ? "Starting Maps + Plentiful" : "Starting Maps")
                                   : (plentiful ? "Plentiful" : "Tingle shuffle only");
    std::vector<RandoCheckId> checkPool;
    std::vector<RandoItemId> itemPool;
    GenerateTinglePool(startingMaps, plentiful, checkPool, itemPool);

    // ---- (1) / (2) the maps ------------------------------------------------
    const size_t expected = startingMaps ? 0u : 1u;
    for (RandoItemId map : kMaps) {
        const size_t count = (size_t)std::count(itemPool.begin(), itemPool.end(), map);
        if (count != expected) {
            return Fail(startingMaps ? 12 : 11,
                        "%s: the item pool holds %zu x %s, expected %zu (one copy of each map with Tingle "
                        "shuffle on, none of a map the file starts with)",
                        leg, count, ItemName(map), expected);
        }
    }

    // ---- (3) the checks and the K9 seam ------------------------------------
    int tingleChecks = 0;
    for (RandoCheckId id : checkPool) {
        if (Rando::StaticData::Checks.at(id).randoCheckType == RCTYPE_TINGLE_SHOP) {
            tingleChecks++;
        }
    }
    if (tingleChecks != 12) {
        return Fail(13, "%s: %d Tingle checks are in the check pool, expected 12", leg, tingleChecks);
    }
    if (gRsbsComboPoolSize != itemPool.size() || gRsbsComboPlentifulFrom > gRsbsComboPlentifulTo ||
        gRsbsComboPlentifulTo > gRsbsComboPoolSize) {
        return Fail(14,
                    "%s: the K9 seam says from %zu to %zu of %zu, the pool holds %zu: "
                    "MM_ComboLogic_MarkPoolRows would refuse it",
                    leg, gRsbsComboPlentifulFrom, gRsbsComboPlentifulTo, gRsbsComboPoolSize, itemPool.size());
    }
    if (!plentiful && gRsbsComboPlentifulFrom != gRsbsComboPlentifulTo) {
        return Fail(15, "%s: Plentiful is off but the seam records %zu copies", leg,
                    gRsbsComboPlentifulTo - gRsbsComboPlentifulFrom);
    }
    printf("[TEST] mm-tingle-map-pool: %s: %zu items, %zu of each map, Plentiful copies %zu\n", leg,
           itemPool.size(), expected, gRsbsComboPlentifulTo - gRsbsComboPlentifulFrom);
    return 0;
}

int RunPool() {
    MM_Rando_Init(); // fills Rando::Logic::Regions (irreversible; ShipInit::InitAll)
    if (Rando::Logic::Regions.empty()) {
        return Fail(10, "MM's region graph is empty: no pool can be generated");
    }
    // An excluded check moves its item without its check and would shift this row.
    const std::string savedExcluded = CVarGetString("gRando.ExcludedChecks", "");
    CVarClear("gRando.ExcludedChecks");
    int rc = 0;
    const bool legs[4][2] = { { false, false }, { false, true }, { true, false }, { true, true } };
    for (const auto& leg : legs) {
        rc = RunPoolLeg(leg[0], leg[1]);
        if (rc != 0) {
            break;
        }
    }
    if (!savedExcluded.empty()) {
        CVarSetString("gRando.ExcludedChecks", savedExcluded.c_str());
    }
    return rc;
}

// ---------------------------------------------------------------------------
// The display row.
// ---------------------------------------------------------------------------

constexpr u16 kBoughtFlag = WEEKEVENTREG_TINGLE_MAP_BOUGHT_CLOCK_TOWN;

void SetBought(bool bought) {
    if (bought) {
        WEEKEVENTREG(kBoughtFlag >> 8) |= (u8)(kBoughtFlag & 0xFF);
    } else {
        WEEKEVENTREG(kBoughtFlag >> 8) &= (u8)~(kBoughtFlag & 0xFF);
    }
}

void PlaceOn(RandoCheckId rc, RandoItemId item) {
    RandoSaveCheck& row = RANDO_SAVE_CHECKS[rc];
    row.randoItemId = item;
    row.shuffled = true;
    row.eligible = false;
    row.cycleObtained = false;
    row.obtained = false;
    row.skipped = false;
    row.seen = RCSEEN_IDENTIFIED;
}

/** Leg (4)/(5): every surface names `expected` for RC_BOMB_SHOP_ITEM_01. */
int CheckSurfaces(const char* leg, RandoItemId expected, int code) {
    const RandoCheckId rc = RC_BOMB_SHOP_ITEM_01;
    const RandoItemId hand = Rando::ActorBehavior::BombShopHandItem();
    const RandoItemId shelf = Rando::ActorBehavior::ShopShelfItem(rc);
    const std::string shelfName = Rando::ActorBehavior::ShopShelfItemName(rc);
    char tracker[128] = {};
    const int trackerOk = MM_CheckTracker_TestSeenItemName((uint16_t)rc, tracker, (int)sizeof(tracker));
    const char* pane = MM_TrackerAdapter_TestPlacedItemName(&gSaveContext, (uint16_t)rc);
    const char* want = ItemName(expected);
    printf("[TEST]   %s: hand %s, shelf %s, shelf text \"%s\", tracker \"%s\", pane \"%s\"\n", leg, ItemName(hand),
           ItemName(shelf), shelfName.c_str(), trackerOk ? tracker : "(none)", pane ? pane : "(none)");
    if (hand != expected) {
        return Fail(code, "%s: the Bomb Shop owner holds %s, expected %s", leg, ItemName(hand), want);
    }
    if (shelf != expected) {
        return Fail(code + 1, "%s: the shelf draws %s, expected %s", leg, ItemName(shelf), want);
    }
    if (shelfName != want) {
        return Fail(code + 2, "%s: the shelf's line names \"%s\", expected \"%s\"", leg, shelfName.c_str(), want);
    }
    if (!trackerOk || strcmp(tracker, want) != 0) {
        return Fail(code + 3, "%s: the MM Check Tracker's row names \"%s\", expected \"%s\"", leg,
                    trackerOk ? tracker : "(none)", want);
    }
    if (pane == nullptr || strcmp(pane, want) != 0) {
        return Fail(code + 4, "%s: the Combo Tracker's MM pane names \"%s\", expected \"%s\"", leg,
                    pane ? pane : "(none)", want);
    }
    return 0;
}

int RunDisplayLegs() {
    const RandoCheckId shop = RC_BOMB_SHOP_ITEM_01;
    const RandoCheckId tingle = RC_CLOCK_TOWN_NORTH_TINGLE_MAP_02;
    gSaveContext.save.shipSaveInfo.saveType = SAVETYPE_RANDO;
    PlaceOn(shop, RI_TINGLE_MAP_CLOCK_TOWN);
    PlaceOn(tingle, RI_TINGLE_MAP_CLOCK_TOWN);

    // ---- (4) not owned: every surface names the map ------------------------
    SetBought(false);
    int rc = CheckSurfaces("map not owned", RI_TINGLE_MAP_CLOCK_TOWN, 20);
    if (rc != 0) {
        return rc;
    }
    if (Rando::ActorBehavior::ShopShelfSlotSold(shop) || Rando::ActorBehavior::TingleMapSlotSold(tingle)) {
        return Fail(25, "with the map not owned, a slot selling it already reads sold out");
    }

    // ---- (5) owned: every surface names the junk the slot sells -------------
    SetBought(true);
    rc = CheckSurfaces("map owned", RI_JUNK, 30);
    if (rc != 0) {
        return rc;
    }

    // ---- (6) the junk sale sells once ---------------------------------------
    if (Rando::ActorBehavior::ShopShelfSlotSold(shop)) {
        return Fail(40, "the Bomb Shop shelf reads sold out before its junk sale");
    }
    if (Rando::ActorBehavior::TingleMapSlotSold(tingle)) {
        return Fail(41, "Tingle refuses the slot before its junk sale");
    }
    RANDO_SAVE_CHECKS[shop].obtained = true;
    RANDO_SAVE_CHECKS[shop].cycleObtained = true;
    RANDO_SAVE_CHECKS[tingle].obtained = true;
    if (!Rando::ActorBehavior::ShopShelfSlotSold(shop)) {
        return Fail(42, "after its junk sale the Bomb Shop shelf is not sold out: it sells junk forever");
    }
    if (!Rando::ActorBehavior::TingleMapSlotSold(tingle)) {
        return Fail(43, "after its junk sale Tingle sells the slot again: it sells junk forever");
    }

    // ---- (7) a paid junk sale never gives RI_NONE ----------------------------
    PlayState* const savedPlay = MM_gPlayState;
    MM_gPlayState = nullptr;
    int noneSeen = 0;
    int paidNone = 0;
    for (uint32_t seed = 1; seed <= 256; seed++) {
        Ship_Random_Seed(seed);
        MM_Rando_TestColdJunkCache();
        noneSeen += Rando::CurrentJunkItem() == RI_NONE ? 1 : 0;
        Ship_Random_Seed(seed);
        MM_Rando_TestColdJunkCache();
        paidNone += Rando::CurrentPaidJunkItem() == RI_NONE ? 1 : 0;
    }
    MM_Rando_TestColdJunkCache();
    MM_gPlayState = savedPlay;
    if (noneSeen == 0) {
        return Fail(44, "Rando::CurrentJunkItem never answered RI_NONE over 256 seeds: the paid probe is vacuous");
    }
    if (paidNone != 0) {
        return Fail(45, "Rando::CurrentPaidJunkItem answered RI_NONE for %d of 256 seeds: a paid sale gives nothing",
                    paidNone);
    }
    printf("[TEST]   junk: CurrentJunkItem picked RI_NONE %d times in 256, CurrentPaidJunkItem never\n", noneSeen);
    return 0;
}

} // namespace

extern "C" int MM_TingleMapPool_RunHeadless(void) {
    printf("[TEST] mm-tingle-map-pool: one copy of each Tingle map, none of a starting map (#1499)\n");
    auto ctx = Ship::Context::GetInstance();
    if (ctx == nullptr || ctx->GetConsoleVariables() == nullptr) {
        return Fail(1, "Ship::Context or its ConsoleVariables are missing; run the shared bring-up first");
    }
    const int rc = RunPool();
    if (rc != 0) {
        return rc;
    }
    printf("[TEST] mm-tingle-map-pool: PASS\n");
    return 0;
}

extern "C" int MM_TingleMapDisplay_RunHeadless(void) {
    printf("[TEST] mm-tingle-map-display: an owned map placed in a shop is named and sold as junk, once (#1499)\n");
    auto ctx = Ship::Context::GetInstance();
    if (ctx == nullptr || ctx->GetConsoleVariables() == nullptr) {
        return Fail(1, "Ship::Context or its ConsoleVariables are missing; run the shared bring-up first");
    }

    auto savedSave = std::make_unique<uint8_t[]>(sizeof(gSaveContext));
    memcpy(savedSave.get(), &gSaveContext, sizeof(gSaveContext));
    auto savedCombo = std::make_unique<uint8_t[]>(sizeof(gComboCtx));
    memcpy(savedCombo.get(), &gComboCtx, sizeof(gComboCtx));
    ComboContext_Init(); // no foreign placement anywhere

    const int rc = RunDisplayLegs();

    memcpy(&gComboCtx, savedCombo.get(), sizeof(gComboCtx));
    memcpy(&gSaveContext, savedSave.get(), sizeof(gSaveContext));
    if (rc != 0) {
        return rc;
    }
    printf("[TEST] mm-tingle-map-display: PASS\n");
    return 0;
}

#endif /* RSBS_SINGLE_EXECUTABLE */
