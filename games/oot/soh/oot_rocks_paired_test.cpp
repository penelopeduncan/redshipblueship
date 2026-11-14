/**
 * @file oot_rocks_paired_test.cpp
 * @brief #1409: Ocarina of Time's Shuffle Rocks and Shuffle Boulders
 *        (RSK_SHUFFLE_ROCKS, RSK_SHUFFLE_BOULDERS) in a PAIRED world. Row
 *        OoTRocksPaired (CTest label `rando`), dispatch `oot-rocks-paired` in
 *        src/common/test_runner.cpp. The operator ruled the options stay live
 *        in paired worlds (2026-11-13: with tests); this row is that test,
 *        modelled on games/oot/soh/oot_wonder_items_paired_test.cpp (#1405 / #1558).
 *        No ROM: the generations run in whatever archive set the build directory
 *        holds, as the song-pool and wonder-items paired rows do.
 *
 * One fixed seed, generated and FINISHED twice the way a creation finishes it:
 * Rando_HeadlessSeedTest stops OoT at its general pass, MM_Rando_HeadlessPairedHalf
 * runs MM's half (whose OnSaveInit chain runs the single bag over both games' hosts),
 * then OoT_Creation_FinishPairedHalf runs OoT's crossings and remainder. Run OFF has
 * both option keys cleared (Off, their default); run ON keys ShuffleRocks On
 * (RO_GENERIC_ON) and ShuffleBoulders All (RO_SHUFFLE_BOULDERS_ALL, widest
 * setting).
 *
 *  A  BOTH RUNS FINISH: both halves generate, the single bag reports OK and the goal
 *     is proven, and OoT's tail succeeds.
 *  B  THE ROCK AND BOULDER CHECKS ARE PART OF THE ON WORLD. The rock and boulder
 *     checks in the world are exactly the ones Context::GenerateLocationPool selects
 *     under On / All (every overworld one, and each dungeon one whose quest the
 *     world's dungeon selects: both counts derived and printed, both > 0). At the
 *     general pass each is a host the bag can fill (the OoT engine's allEmptyHosts
 *     lists it) or already holds an item a restricted pass placed, and the bag is
 *     offered at least one. After the finish none is empty. The world's location set
 *     is the OFF world's plus exactly those rock and boulder checks (no other
 *     location moves), and the finished worlds' placed-location count grew by
 *     exactly the rock and boulder checks shuffled.
 *  C  THE SHUFFLE MIXES THEM: at least one rock or boulder check holds an item other
 *     than its vanilla item. Crossings: OoT_ComboLogic_HostAcceptsForeign admits only
 *     chests (ACTOR_EN_BOX), song checks, shop shelves, scrubs and four merchants,
 *     so no rock or boulder check (ACTOR_EN_ISHI) accepts a Majora's Mask item and
 *     none is in the crossing store; both counts are asserted 0 and printed.
 *  D  OFF: no rock or boulder check is a location (so none is a host), the pool is
 *     non-empty, and the world's location set is the baseline B compares against.
 *  E  MAJORA'S MASK'S OWN OPTIONS stay untouched: every MM gRando.Options.RO_* CVar
 *     keeps its exact pre-run state across both runs, MM_WonderItemsTest_LiveShuffled
 *     reports RO_SHUFFLE_WONDER_ITEMS Off and 0 shuffled MM wonder checks in both
 *     runs, and MM_ComboLogic_TestShuffledItems reports the same positive MM
 *     shuffled-check count in both runs.
 *
 * Every CVar touched and the OoT save are put back; the combo record and the
 * single bag are reset on the way out.
 */
#ifdef RSBS_SINGLE_EXECUTABLE

#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <set>
#include <vector>

#include <libultraship/bridge.h>

#include "soh/OTRGlobals.h"
#include "soh/cvar_prefixes.h"
#include "soh/Enhancements/randomizer/randomizerTypes.h"
#include "soh/Enhancements/randomizer/static_data.h"
#include "soh/Enhancements/randomizer/SeedContext.h"
#include "soh/Enhancements/randomizer/settings.h"
#include "soh/Enhancements/randomizer/item_location.h"
#include "soh/Enhancements/randomizer/dungeon.h"

#include "context.h" // src/common: ComboContext_Init
#include "game.h"
#include "combo_logic.h"      // src/common: the OoT engine, Combo_Logic_StatusName
#include "combo_single_bag.h" // src/common: the single bag's report
#include "crossing_store.h"   // src/common: Combo_Crossings_Lookup

// At file scope, not inside an extern "C" block (see oot_shared_kokiri_sword_test.cpp).
#include <z64.h>
extern "C" {
extern SaveContext gSaveContext;
}

// 3drando/menu.cpp: the headless generation bridge every rando row uses.
extern "C" int Rando_HeadlessSeedTest(const char* seedStr);
// The rest of a paired creation: is OoT waiting at its general pass
// (ComboLogicEngineOoT.cpp), MM's half (mm_rando_gen_test.cpp), OoT's crossings and
// remainder (ForeignItemsSingleExe.cpp).
extern "C" int OoT_ComboLogic_GeneralPassDeferred(void);
extern "C" int MM_Rando_HeadlessPairedHalf(void);
extern "C" int OoT_Creation_FinishPairedHalf(int writeSpoiler);
// ComboLogicEngineOoT.cpp: the OoT engine's host rule.
extern "C" int OoT_ComboLogic_TestHostAcceptsForeign(uint16_t rc);
// mm_wonder_items_test.cpp / ComboLogicEngineSingleExe.cpp (read-only): MM's live save state.
extern "C" int MM_WonderItemsTest_LiveShuffled(int* outOption, int* outTable);
extern "C" int MM_ComboLogic_TestShuffledItems(uint16_t* outItems, uint16_t* outChecks, int cap);

namespace {

constexpr const char* kSeed = "RSBSROCKSPAIRED1";

constexpr const char* kMMOptionKeys[] = {
    "gRando.Options.RO_ACCESS_DUNGEONS",
    "gRando.Options.RO_ACCESS_MAJORA_MASKS_COUNT",
    "gRando.Options.RO_ACCESS_MAJORA_REMAINS_COUNT",
    "gRando.Options.RO_ACCESS_MAJORA_REMAINS",
    "gRando.Options.RO_ACCESS_MOON_MASKS_COUNT",
    "gRando.Options.RO_ACCESS_MOON_REMAINS_COUNT",
    "gRando.Options.RO_ACCESS_TRIALS",
    "gRando.Options.RO_HINTS_BOSS_REMAINS",
    "gRando.Options.RO_HINTS_GOSSIP_STONES",
    "gRando.Options.RO_HINTS_HOOKSHOT",
    "gRando.Options.RO_HINTS_OATH_TO_ORDER",
    "gRando.Options.RO_HINTS_PURCHASEABLE",
    "gRando.Options.RO_HINTS_SPIDER_HOUSES",
    "gRando.Options.RO_TRAP_AMOUNT",
    "gRando.Options.RO_LOGIC",
    "gRando.Options.RO_MINIMUM_SKULLTULA_TOKENS",
    "gRando.Options.RO_MINIMUM_STRAY_FAIRIES",
    "gRando.Options.RO_PLENTIFUL_ITEMS",
    "gRando.Options.RO_SHUFFLE_BARREL_DROPS",
    "gRando.Options.RO_SHUFFLE_BOSS_REMAINS",
    "gRando.Options.RO_SHUFFLE_BOSS_SOULS",
    "gRando.Options.RO_SHUFFLE_COWS",
    "gRando.Options.RO_SHUFFLE_CRATE_DROPS",
    "gRando.Options.RO_SHUFFLE_ENEMY_DROPS",
    "gRando.Options.RO_SHUFFLE_ENEMY_SOULS",
    "gRando.Options.RO_SHUFFLE_FREESTANDING_ITEMS",
    "gRando.Options.RO_SHUFFLE_FROGS",
    "gRando.Options.RO_SHUFFLE_GOLD_SKULLTULAS",
    "gRando.Options.RO_SHUFFLE_GRASS_DROPS",
    "gRando.Options.RO_SHUFFLE_TRAPS",
    "gRando.Options.RO_SHUFFLE_OWL_STATUES",
    "gRando.Options.RO_SHUFFLE_OCARINA_BUTTONS",
    "gRando.Options.RO_SHUFFLE_POT_DROPS",
    "gRando.Options.RO_SHUFFLE_SHOPS",
    "gRando.Options.RO_SHUFFLE_SKELETON_KEY",
    "gRando.Options.RO_SHUFFLE_SNOWBALL_DROPS",
    "gRando.Options.RO_SHUFFLE_SONG_DOUBLE_TIME",
    "gRando.Options.RO_SHUFFLE_SONG_INVERTED_TIME",
    "gRando.Options.RO_SHUFFLE_SONG_SARIA",
    "gRando.Options.RO_SHUFFLE_SONG_SUN",
    "gRando.Options.RO_SHUFFLE_SWIM",
    "gRando.Options.RO_SHUFFLE_TINGLE_SHOPS",
    "gRando.Options.RO_SHUFFLE_TRIFORCE_PIECES",
    "gRando.Options.RO_SKULLTULA_SHUFFLED",
    "gRando.Options.RO_STARTING_CONSUMABLES",
    "gRando.Options.RO_STARTING_HEALTH",
    "gRando.Options.RO_STARTING_MAPS_AND_COMPASSES",
    "gRando.Options.RO_STARTING_RUPEES",
    "gRando.Options.RO_STRAY_FAIRIES_MAX",
    "gRando.Options.RO_TRIFORCE_PIECES_MAX",
    "gRando.Options.RO_TRIFORCE_PIECES_REQUIRED",
    "gRando.Options.RO_CLOCK_SHUFFLE",
    "gRando.Options.RO_CLOCK_SHUFFLE_PROGRESSIVE",
    "gRando.Options.RO_CLOCK_TERMINAL_TIME",
    "gRando.Options.RO_SHUFFLE_TREE_DROPS",
    "gRando.Options.RO_SHOP_PRICES",
    "gRando.Options.RO_TINGLE_PRICES",
    "gRando.Options.RO_PLACEMENT_BOSS_KEYS",
    "gRando.Options.RO_PLACEMENT_SMALL_KEYS",
    "gRando.Options.RO_PLACEMENT_STRAY_FAIRIES",
    "gRando.Options.RO_SHUFFLE_SONGS",
    "gRando.Options.RO_SHUFFLE_WONDER_ITEMS",
    "gRando.Options.RO_SHUFFLE_HIVE_DROPS",
    "gRando.Options.RO_SHUFFLE_BUTTERFLIES",
};

int sFailures = 0;

void RpFail(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    printf("[TEST] oot-rocks-paired: FAIL: ");
    vprintf(fmt, args);
    printf("\n");
    va_end(args);
    sFailures++;
}

#define RP_CHECK(cond, ...)      \
    do {                         \
        if (!(cond)) {           \
            RpFail(__VA_ARGS__); \
        }                        \
    } while (0)

const char* const kShuffleRocksKey = CVAR_RANDOMIZER_SETTING("ShuffleRocks");
const char* const kShuffleBouldersKey = CVAR_RANDOMIZER_SETTING("ShuffleBoulders");

/** A CVar's stored state, put back by Restore or checked by Unchanged. */
struct RpSavedKey {
    const char* key;
    bool existed;
    int32_t value;
    void Save(const char* k) {
        key = k;
        existed = CVarExists(k);
        value = CVarGetInteger(k, 0);
    }
    void Restore() const {
        if (existed) {
            CVarSetInteger(key, value);
        } else {
            CVarClear(key);
        }
    }
    bool Unchanged() const {
        return CVarExists(key) == existed && CVarGetInteger(key, 0) == value;
    }
};

bool RpIsRock(RandomizerCheck rc) {
    return Rando::StaticData::GetLocation(rc)->GetRCType() == RCTYPE_ROCK;
}

bool RpIsBoulder(RandomizerCheck rc) {
    return Rando::StaticData::GetLocation(rc)->GetRCType() == RCTYPE_BOULDER;
}

bool RpIsRockOrBoulder(RandomizerCheck rc) {
    return RpIsRock(rc) || RpIsBoulder(rc);
}

/** What one finished paired world did with the rock and boulder checks. */
struct RpRun {
    bool generated;         // OoT's generation stopped at its general pass
    int mmHalf;             // MM_Rando_HeadlessPairedHalf's rc
    int bagStatus;          // the single bag's status
    bool goalProven;        // ... and its proof
    int crossings;          // OoT_Creation_FinishPairedHalf's rc (-1: not run or failed)
    int ootRows;            // OoT general-pass pool rows the bag took (printed only)
    int hostsOoT;           // OoT's empty hosts after the bag's roll-back (printed only)
    uint8_t rocksOption;    // the world's RSK_SHUFFLE_ROCKS
    uint8_t bouldersOption; // the world's RSK_SHUFFLE_BOULDERS
    std::set<RandomizerCheck> locations;
    int rockLocations;        // RCTYPE_ROCK checks among the world's locations
    int boulderLocations;     // RCTYPE_BOULDER checks among the world's locations
    int rockBoulderLocations; // rockLocations + boulderLocations
    int rockSelected;         // ... what GenerateLocationPool selects under On
    int boulderSelected;      // ... what GenerateLocationPool selects under All
    int unexpectedInRange;    // non-rock/non-boulder checks inside [RC_KF_CIRCLE_ROCK_1..RC_BOTW_MQ_BOULDER_3]
    int rockBoulderHosts;     // general pass: rock/boulder checks the engine lists as empty hosts
    int rockBoulderPrefilled; // general pass: rock/boulder checks a restricted pass already filled
    int placed;               // finished: locations holding an item
    int rockBoulderEmpty;     // finished: rock/boulder locations holding none
    int rockMixed;            // finished: rock locations holding non-vanilla item
    int boulderMixed;         // finished: boulder locations holding non-vanilla item
    int rockBoulderMixed;     // finished: rock/boulder locations holding non-vanilla item
    int rockBoulderAccepts;   // rock/boulder locations OoT's host rule would give an MM item
    int rockBoulderCrossed;   // rock/boulder locations in the crossing store
    int mmWonderOption;       // MM's saved RO_SHUFFLE_WONDER_ITEMS (-1 unread)
    int mmWonderTable;        // MM's wonder item checks in its table
    int mmWonderShuffled;     // ... shuffled in MM's finished half
    int mmShuffledTotal;      // total MM shuffled checks in MM's finished half
};

RpRun RpGenerate(const char* leg, bool on) {
    RpRun r = {};
    r.crossings = -1;
    r.mmWonderOption = -1;
    Combo_SingleBag_Forget();
    ComboContext_Init();
    if (on) {
        CVarSetInteger(kShuffleRocksKey, RO_GENERIC_ON);
        CVarSetInteger(kShuffleBouldersKey, RO_SHUFFLE_BOULDERS_ALL);
    } else {
        CVarClear(kShuffleRocksKey);
        CVarClear(kShuffleBouldersKey);
    }
    const int gen = Rando_HeadlessSeedTest(kSeed);
    r.generated = gen == 0 && OoT_ComboLogic_GeneralPassDeferred() != 0;
    auto world = Rando::Context::GetInstance();
    if (r.generated && world != nullptr) {
        r.rocksOption = world->GetOption(RSK_SHUFFLE_ROCKS).Get();
        r.bouldersOption = world->GetOption(RSK_SHUFFLE_BOULDERS).Get();
        for (RandomizerCheck rc : world->allLocations) {
            r.locations.insert(rc);
        }
        // What Context::GenerateLocationPool selects under On / All (its quest rule).
        for (int c = RC_KF_CIRCLE_ROCK_1; c <= RC_BOTW_MQ_BOULDER_3; c++) {
            Rando::Location* loc = Rando::StaticData::GetLocation((RandomizerCheck)c);
            const RandomizerCheckType rctype = loc->GetRCType();
            if (rctype != RCTYPE_ROCK && rctype != RCTYPE_BOULDER) {
                r.unexpectedInRange++;
                continue;
            }
            bool selected = false;
            if (loc->IsOverworld()) {
                selected = true;
            } else {
                auto* dungeon = world->GetDungeon(loc->GetArea() - RCAREA_DEKU_TREE);
                selected = dungeon != nullptr &&
                           (loc->GetQuest() == RCQUEST_BOTH || (loc->GetQuest() == RCQUEST_MQ) == dungeon->IsMQ());
            }
            if (selected) {
                if (rctype == RCTYPE_ROCK) {
                    r.rockSelected++;
                } else {
                    r.boulderSelected++;
                }
            }
        }
        // The general pass: the hosts the bag will be offered (the engine's own list).
        std::set<uint16_t> emptyHosts;
        const ComboLogicEngine* oot = Combo_Logic_GetEngine(GAME_OOT);
        if (oot != nullptr && oot->allEmptyHosts != nullptr) {
            const int total = oot->allEmptyHosts(oot->self, nullptr, 0);
            std::vector<uint16_t> hosts((size_t)(total > 0 ? total : 1), 0);
            const int got = oot->allEmptyHosts(oot->self, hosts.data(), total);
            for (int i = 0; i < got && i < total; i++) {
                emptyHosts.insert(hosts[(size_t)i]);
            }
        }
        for (RandomizerCheck rc : r.locations) {
            const bool isRock = RpIsRock(rc);
            const bool isBoulder = RpIsBoulder(rc);
            if (!isRock && !isBoulder) {
                continue;
            }
            if (isRock) {
                r.rockLocations++;
            } else {
                r.boulderLocations++;
            }
            r.rockBoulderLocations++;
            if (emptyHosts.count((uint16_t)rc) != 0) {
                r.rockBoulderHosts++;
            } else if (world->GetItemLocation(rc)->GetPlacedRandomizerGet() != RG_NONE) {
                r.rockBoulderPrefilled++;
            }
            r.rockBoulderAccepts += OoT_ComboLogic_TestHostAcceptsForeign((uint16_t)rc) != 0 ? 1 : 0;
        }
    }
    r.mmHalf = r.generated ? MM_Rando_HeadlessPairedHalf() : -1;
    const ComboSingleBagReport* bag = Combo_SingleBag_LastReport();
    r.bagStatus = bag->status;
    r.goalProven = bag->fill.goalProven;
    r.ootRows = bag->ootRows;
    r.hostsOoT = bag->hostsOoT;
    const bool proven = r.mmHalf == 0 && r.bagStatus == RSBS_COMBO_LOGIC_OK && r.goalProven;
    // OoT's tail only over a world the bag decided (oot_wonder_items_paired_test.cpp).
    if (proven) {
        r.crossings = OoT_Creation_FinishPairedHalf(0);
    }
    if (r.crossings >= 0 && world != nullptr) {
        for (RandomizerCheck rc : r.locations) {
            const RandomizerGet placed = world->GetItemLocation(rc)->GetPlacedRandomizerGet();
            r.placed += placed != RG_NONE ? 1 : 0;
            const bool isRock = RpIsRock(rc);
            const bool isBoulder = RpIsBoulder(rc);
            if (!isRock && !isBoulder) {
                continue;
            }
            if (placed == RG_NONE) {
                r.rockBoulderEmpty++;
            } else if (placed != Rando::StaticData::GetLocation(rc)->GetVanillaItem()) {
                if (isRock) {
                    r.rockMixed++;
                } else {
                    r.boulderMixed++;
                }
                r.rockBoulderMixed++;
            }
            r.rockBoulderCrossed += Combo_Crossings_Lookup(GAME_OOT, (uint16_t)rc) != nullptr ? 1 : 0;
        }
        r.mmWonderShuffled = MM_WonderItemsTest_LiveShuffled(&r.mmWonderOption, &r.mmWonderTable);
        r.mmShuffledTotal = MM_ComboLogic_TestShuffledItems(nullptr, nullptr, 0);
    }
    Combo_SingleBag_Forget();
    printf("[TEST] oot-rocks-paired: %s (%s, rocks %u, boulders %u): gen %d, MM half %d, bag %s, goal proven %d, "
           "crossings %d, OoT pool rows %d, OoT hosts left %d; %zu location(s), %d placed; rocks+boulders: %d+%d=%d "
           "location(s) (%d+%d selected), %d host(s) / %d prefilled at the general pass, %d empty / %d+%d=%d not "
           "vanilla after the finish, %d accepting an MM item, %d crossed; MM: wonder option %d (%d of %d), "
           "shuffled total %d\n",
           leg, kSeed, (unsigned)r.rocksOption, (unsigned)r.bouldersOption, gen, r.mmHalf,
           Combo_Logic_StatusName(r.bagStatus), r.goalProven ? 1 : 0, r.crossings, r.ootRows, r.hostsOoT,
           r.locations.size(), r.placed, r.rockLocations, r.boulderLocations, r.rockBoulderLocations, r.rockSelected,
           r.boulderSelected, r.rockBoulderHosts, r.rockBoulderPrefilled, r.rockBoulderEmpty, r.rockMixed,
           r.boulderMixed, r.rockBoulderMixed, r.rockBoulderAccepts, r.rockBoulderCrossed, r.mmWonderOption,
           r.mmWonderShuffled, r.mmWonderTable, r.mmShuffledTotal);
    return r;
}

/** Leg A for one run. @return whether the run finished (the rest of its legs mean something). */
bool RpCheckFinished(const RpRun& r, const char* leg) {
    RP_CHECK(r.generated, "%s A: the generation failed or did not stop at OoT's general pass", leg);
    RP_CHECK(r.mmHalf == 0, "%s A: MM's half returned %d", leg, r.mmHalf);
    RP_CHECK(r.bagStatus == RSBS_COMBO_LOGIC_OK && r.goalProven, "%s A: the single bag reported %s, goal proven %d",
             leg, Combo_Logic_StatusName(r.bagStatus), r.goalProven ? 1 : 0);
    RP_CHECK(r.crossings >= 0, "%s A: OoT's crossings and remainder after the bag failed (%d)", leg, r.crossings);
    return r.generated && r.mmHalf == 0 && r.bagStatus == RSBS_COMBO_LOGIC_OK && r.goalProven && r.crossings >= 0;
}

} // namespace

/**
 * @return 0 when every leg holds, 1 otherwise.
 */
extern "C" int OoT_RocksPairedTest_Run(void) {
    printf("[TEST] oot-rocks-paired: a paired world with Shuffle Rocks On and Shuffle Boulders All places every "
           "rock and boulder check it selects, mixes them, keeps them off the crossing hosts and leaves MM's options "
           "untouched; off, none is a location (#1409)\n");
    if (OTRGlobals::Instance == nullptr || OTRGlobals::Instance->gRandoContext == nullptr) {
        printf("[TEST] FAIL: oot-rocks-paired needs OTRGlobals' randomizer context\n");
        return 1;
    }
    sFailures = 0;
    std::unique_ptr<SaveContext> priorSave = std::make_unique<SaveContext>();
    std::memcpy(priorSave.get(), &gSaveContext, sizeof(SaveContext));

    RpSavedKey shuffleRocks;
    RpSavedKey shuffleBoulders;
    shuffleRocks.Save(kShuffleRocksKey);
    shuffleBoulders.Save(kShuffleBouldersKey);

    std::vector<RpSavedKey> mmKeys;
    mmKeys.reserve(sizeof(kMMOptionKeys) / sizeof(kMMOptionKeys[0]));
    for (const char* key : kMMOptionKeys) {
        RpSavedKey saved;
        saved.Save(key);
        mmKeys.push_back(saved);
    }
    const int32_t mmWonderBefore = CVarGetInteger("gRando.Options.RO_SHUFFLE_WONDER_ITEMS", 0);
    RP_CHECK(mmWonderBefore == 0, "E: MM's gRando.Options.RO_SHUFFLE_WONDER_ITEMS reads %d before the row, want Off",
             mmWonderBefore);

    // Never stored: a stored rando key poisons later rows (Restore below).
    const RpRun off = RpGenerate("OFF", false);
    for (const RpSavedKey& saved : mmKeys) {
        RP_CHECK(saved.Unchanged(), "E: OFF run changed MM option %s", saved.key);
    }
    const RpRun on = RpGenerate("ON", true);
    for (const RpSavedKey& saved : mmKeys) {
        RP_CHECK(saved.Unchanged(), "E: ON run changed MM option %s", saved.key);
    }
    shuffleRocks.Restore();
    shuffleBoulders.Restore();

    const bool offDone = RpCheckFinished(off, "OFF");
    const bool onDone = RpCheckFinished(on, "ON");

    // D: off.
    RP_CHECK(off.rocksOption == RO_GENERIC_OFF && off.bouldersOption == RO_SHUFFLE_BOULDERS_OFF,
             "OFF D: the world generated with Shuffle Rocks %u, Shuffle Boulders %u", (unsigned)off.rocksOption,
             (unsigned)off.bouldersOption);
    RP_CHECK(!off.locations.empty() && off.rockLocations == 0 && off.boulderLocations == 0,
             "OFF D: %d rock and %d boulder checks among %zu locations, want none of a non-empty pool",
             off.rockLocations, off.boulderLocations, off.locations.size());

    // B: on.
    RP_CHECK(on.rocksOption == RO_GENERIC_ON && on.bouldersOption == RO_SHUFFLE_BOULDERS_ALL,
             "ON B: a paired world keyed On/All generated with Shuffle Rocks %u, Shuffle Boulders %u (pinned off?)",
             (unsigned)on.rocksOption, (unsigned)on.bouldersOption);
    RP_CHECK(on.unexpectedInRange == 0,
             "ON B: %d check(s) inside [RC_KF_CIRCLE_ROCK_1..RC_BOTW_MQ_BOULDER_3] are not rock/boulder checks",
             on.unexpectedInRange);
    RP_CHECK(on.rockSelected > 0 && on.rockLocations == on.rockSelected,
             "ON B: %d rock check(s) are locations, the world's quests select %d", on.rockLocations, on.rockSelected);
    RP_CHECK(on.boulderSelected > 0 && on.boulderLocations == on.boulderSelected,
             "ON B: %d boulder check(s) are locations, the world's quests select %d", on.boulderLocations,
             on.boulderSelected);
    RP_CHECK(on.rockBoulderHosts + on.rockBoulderPrefilled == on.rockBoulderLocations && on.rockBoulderHosts > 0,
             "ON B: at the general pass %d rock/boulder location(s) are hosts the bag can fill and %d are filled, "
             "of %d",
             on.rockBoulderHosts, on.rockBoulderPrefilled, on.rockBoulderLocations);

    std::vector<RandomizerCheck> onWithoutRocks;
    std::vector<RandomizerCheck> offList(off.locations.begin(), off.locations.end());
    for (RandomizerCheck rc : on.locations) {
        if (!RpIsRockOrBoulder(rc)) {
            onWithoutRocks.push_back(rc);
        }
    }
    RP_CHECK(onWithoutRocks == offList,
             "ON B: the ON world's %zu locations less its rock/boulder checks (%zu) are not the OFF world's %zu: the "
             "options moved locations they do not own",
             on.locations.size(), onWithoutRocks.size(), off.locations.size());

    if (offDone && onDone) {
        RP_CHECK(on.rockBoulderEmpty == 0, "ON B: %d rock/boulder location(s) hold no item after the finish",
                 on.rockBoulderEmpty);
        RP_CHECK(on.placed - off.placed == on.rockBoulderLocations,
                 "ON B: the placed count grew by %d, the rock+boulder checks shuffled are %d", on.placed - off.placed,
                 on.rockBoulderLocations);

        // C: mixed, and off the crossing hosts.
        RP_CHECK(on.rockBoulderMixed > 0,
                 "ON C: every one of the %d rock/boulder checks holds its vanilla item (rocks %d mixed, boulders %d "
                 "mixed)",
                 on.rockBoulderLocations, on.rockMixed, on.boulderMixed);
        RP_CHECK(on.rockBoulderAccepts == 0 && on.rockBoulderCrossed == 0,
                 "ON C: %d rock/boulder check(s) accept an MM item and %d hold one in the crossing store (the host "
                 "rule admits no ACTOR_EN_ISHI)",
                 on.rockBoulderAccepts, on.rockBoulderCrossed);

        // E: MM's own options and half untouched.
        RP_CHECK(off.mmWonderOption == 0 && on.mmWonderOption == 0,
                 "E: MM's saved Shuffle Wonder Items is %d (OFF) / %d (ON), want its default Off in both",
                 off.mmWonderOption, on.mmWonderOption);
        RP_CHECK(off.mmWonderTable > 0 && off.mmWonderShuffled == 0 && on.mmWonderShuffled == 0,
                 "E: MM shuffles %d (OFF) / %d (ON) of its %d wonder checks, want none", off.mmWonderShuffled,
                 on.mmWonderShuffled, off.mmWonderTable);
        RP_CHECK(off.mmShuffledTotal > 0 && off.mmShuffledTotal == on.mmShuffledTotal,
                 "E: MM's total shuffled checks changed from %d (OFF) to %d (ON): OoT's rock/boulder options reached "
                 "MM",
                 off.mmShuffledTotal, on.mmShuffledTotal);
    }

    Combo_SingleBag_Forget();
    ComboContext_Init();
    std::memcpy(&gSaveContext, priorSave.get(), sizeof(SaveContext));
    if (sFailures != 0) {
        printf("[TEST] FAIL (oot-rocks-paired): %d failure(s)\n", sFailures);
        return 1;
    }
    printf("[TEST] PASS: oot-rocks-paired\n");
    return 0;
}

#else

extern "C" int OoT_RocksPairedTest_Run(void) {
    return 0;
}

#endif // RSBS_SINGLE_EXECUTABLE
