/**
 * @file oot_ice_paired_test.cpp
 * @brief #1408 / #1562: Ocarina of Time's Shuffle Icicles (SoH 49e740b6f,
 *        RSK_SHUFFLE_ICICLES) and Shuffle Red Ice (SoH 68d4c6049, RSK_SHUFFLE_RED_ICE)
 *        in a PAIRED world. Row OoTIcePaired (CTest label `rando`), dispatch
 *        `oot-ice-paired` in src/common/test_runner.cpp. The operator ruled both options
 *        stay live in paired worlds ("these shuffles stay live in paired worlds, with
 *        tests"); this row is that test. No ROM: the generations run in whatever archive
 *        set the build directory holds, as oot-wonder-items-paired's do.
 *
 * One fixed seed, generated and FINISHED twice the way a creation finishes it:
 * Rando_HeadlessSeedTest stops OoT at its general pass, MM_Rando_HeadlessPairedHalf
 * runs MM's half (whose OnSaveInit chain runs the single bag over both games' hosts),
 * then OoT_Creation_FinishPairedHalf runs OoT's crossings and remainder. Run OFF has
 * both keys cleared (Off, their default); run ON keys both On.
 *
 *  A  BOTH RUNS FINISH: both halves generate, the single bag reports OK and the goal
 *     is proven, and OoT's tail succeeds.
 *  B  THE ICICLE AND RED ICE CHECKS ARE PART OF THE ON WORLD. Both options stay On (not
 *     pinned off), and the #1408 checks in the world are the exact ones
 *     Context::GenerateLocationPool's quest rule selects (both counts > 0). At the
 *     general pass each is a host the bag can fill or a restricted-pass prefilled
 *     location (and hosts > 0). After the finish none is empty. The ON world's location
 *     set minus RCTYPE_ICICLE | RCTYPE_RED_ICE equals the OFF world's, and placed count
 *     grew by exactly those checks.
 *  C  THE SHUFFLE MIXES THEM AND KEEPS THEM OFF CROSSING HOSTS: at least one #1408
 *     check holds an item other than its vanilla item (RG_NONE); neither
 *     OoT_ComboLogic_TestHostAcceptsForeign nor Combo_Crossings_Lookup admits or crosses
 *     an icicle (ACTOR_BG_ICE_TURARA) or red ice (ACTOR_BG_ICE_SHELTER) check (both 0).
 *  D  OFF: both options are RO_GENERIC_OFF, the pool is non-empty, and 0 icicle or red
 *     ice checks are in it.
 *  E  MAJORA'S MASK'S OWN OPTIONS stay untouched (via MM_WonderItemsTest_LiveShuffled and
 *     read-only checks of MM's gRando.Options.RO_SHUFFLE_* CVars).
 *
 * RED HALVES, derived, not run (this lane had no build): with SeedContext.cpp's icicle
 * or red ice skip made unconditional B fails; with either made never-true D fails and
 * B's set equality fails; with a paired pin in Playthrough_Init B fails (generated Off);
 * with OoT_ComboLogic_HostAcceptsForeign admitting every host C's accepts == 0 fails;
 * if an MM option CVar were mutated E fails.
 *
 * Every key and the OoT save are put back; the combo record and the single bag are
 * reset on the way out, as oot-wonder-items-paired resets them.
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
// mm_wonder_items_test.cpp (test bridge, read-only): MM's live save's wonder item
// checks that are shuffled; its saved RO_SHUFFLE_WONDER_ITEMS and the table's count.
extern "C" int MM_WonderItemsTest_LiveShuffled(int* outOption, int* outTable);

namespace {

constexpr const char* kSeed = "RSBSICEPAIRED1";
constexpr const char* kMMOptionKeys[] = {
    "gRando.Options.RO_SHUFFLE_WONDER_ITEMS", "gRando.Options.RO_SHUFFLE_SNOWBALL_DROPS",
    "gRando.Options.RO_SHUFFLE_POT_DROPS",    "gRando.Options.RO_SHUFFLE_GRASS_DROPS",
    "gRando.Options.RO_SHUFFLE_CRATE_DROPS",  "gRando.Options.RO_SHUFFLE_BARREL_DROPS",
    "gRando.Options.RO_SHUFFLE_HIVE_DROPS",
};

int sFailures = 0;

void IpFail(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    printf("[TEST] oot-ice-paired: FAIL: ");
    vprintf(fmt, args);
    printf("\n");
    va_end(args);
    sFailures++;
}

#define IP_CHECK(cond, ...)      \
    do {                         \
        if (!(cond)) {           \
            IpFail(__VA_ARGS__); \
        }                        \
    } while (0)

const char* const kIciclesKey = CVAR_RANDOMIZER_SETTING("ShuffleIcicles");
const char* const kRedIceKey = CVAR_RANDOMIZER_SETTING("ShuffleRedIce");

/** A key's stored state, put back by Restore. */
struct IpSavedKey {
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
};

bool IpIsIce(RandomizerCheck rc) {
    const RandomizerCheckType type = Rando::StaticData::GetLocation(rc)->GetRCType();
    return type == RCTYPE_ICICLE || type == RCTYPE_RED_ICE;
}

/** What one finished paired world did with the icicle and red ice checks. */
struct IpRun {
    bool generated;        // OoT's generation stopped at its general pass
    int mmHalf;            // MM_Rando_HeadlessPairedHalf's rc
    int bagStatus;         // the single bag's status
    bool goalProven;       // ... and its proof
    int crossings;         // OoT_Creation_FinishPairedHalf's rc (-1: not run or failed)
    int ootRows;           // OoT general-pass pool rows the bag took (printed only)
    int hostsOoT;          // OoT's empty hosts after the bag's roll-back (printed only)
    uint8_t iciclesOption; // the world's RSK_SHUFFLE_ICICLES
    uint8_t redIceOption;  // the world's RSK_SHUFFLE_RED_ICE
    std::set<RandomizerCheck> locations;
    std::set<RandomizerCheck> iceActual;
    std::set<RandomizerCheck> iceSelectedSet;
    int icicleLocations;
    int redIceLocations;
    int iceLocations;
    int icicleSelected;
    int redIceSelected;
    int iceSelected;
    int iceHosts;     // general pass: #1408 checks the engine lists as empty hosts
    int icePrefilled; // general pass: #1408 checks a restricted pass already filled
    int placed;       // finished: locations holding an item
    int iceEmpty;     // finished: #1408 locations holding none
    int iceMixed;     // finished: #1408 locations holding an item other than their vanilla one
    int iceAccepts;   // #1408 locations OoT's host rule would give an MM item
    int iceCrossed;   // #1408 locations in the crossing store
    int mmOption;     // MM's saved RO_SHUFFLE_WONDER_ITEMS (-1 unread)
    int mmTable;      // MM's wonder item checks in its table
    int mmShuffled;   // ... shuffled in MM's finished half
};

IpRun IpGenerate(const char* leg, bool on) {
    IpRun r = {};
    r.crossings = -1;
    r.mmOption = -1;
    Combo_SingleBag_Forget();
    ComboContext_Init();
    if (on) {
        CVarSetInteger(kIciclesKey, RO_GENERIC_ON);
        CVarSetInteger(kRedIceKey, RO_GENERIC_ON);
    } else {
        CVarClear(kIciclesKey);
        CVarClear(kRedIceKey);
    }
    const int gen = Rando_HeadlessSeedTest(kSeed);
    r.generated = gen == 0 && OoT_ComboLogic_GeneralPassDeferred() != 0;
    auto world = Rando::Context::GetInstance();
    if (r.generated && world != nullptr) {
        r.iciclesOption = world->GetOption(RSK_SHUFFLE_ICICLES).Get();
        r.redIceOption = world->GetOption(RSK_SHUFFLE_RED_ICE).Get();
        for (RandomizerCheck rc : world->allLocations) {
            r.locations.insert(rc);
        }
        // What Context::GenerateLocationPool selects under On (its quest rule).
        for (int c = RC_ICE_CAVERN_ENTRANCE_LEFT_STALAGMITE;
             c <= RC_GANONS_CASTLE_MQ_WATER_TRIAL_SECOND_DOOR_RED_ICE_5; c++) {
            Rando::Location* loc = Rando::StaticData::GetLocation((RandomizerCheck)c);
            const bool isIcicle = loc->GetRCType() == RCTYPE_ICICLE;
            bool selected = loc->IsOverworld();
            if (!selected) {
                auto* dungeon = world->GetDungeon(loc->GetArea() - RCAREA_DEKU_TREE);
                selected = dungeon != nullptr &&
                           (loc->GetQuest() == RCQUEST_BOTH || (loc->GetQuest() == RCQUEST_MQ) == dungeon->IsMQ());
            }
            if (selected) {
                r.iceSelectedSet.insert((RandomizerCheck)c);
                r.iceSelected++;
                if (isIcicle) {
                    r.icicleSelected++;
                } else {
                    r.redIceSelected++;
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
            if (!IpIsIce(rc)) {
                continue;
            }
            r.iceActual.insert(rc);
            r.iceLocations++;
            if (Rando::StaticData::GetLocation(rc)->GetRCType() == RCTYPE_ICICLE) {
                r.icicleLocations++;
            } else {
                r.redIceLocations++;
            }
            if (emptyHosts.count((uint16_t)rc) != 0) {
                r.iceHosts++;
            } else if (world->GetItemLocation(rc)->GetPlacedRandomizerGet() != RG_NONE) {
                r.icePrefilled++;
            }
            r.iceAccepts += OoT_ComboLogic_TestHostAcceptsForeign((uint16_t)rc) != 0 ? 1 : 0;
        }
    }
    r.mmHalf = r.generated ? MM_Rando_HeadlessPairedHalf() : -1;
    const ComboSingleBagReport* bag = Combo_SingleBag_LastReport();
    r.bagStatus = bag->status;
    r.goalProven = bag->fill.goalProven;
    r.ootRows = bag->ootRows;
    r.hostsOoT = bag->hostsOoT;
    const bool proven = r.mmHalf == 0 && r.bagStatus == RSBS_COMBO_LOGIC_OK && r.goalProven;
    if (proven) {
        r.crossings = OoT_Creation_FinishPairedHalf(0);
    }
    if (r.crossings >= 0 && world != nullptr) {
        for (RandomizerCheck rc : r.locations) {
            const RandomizerGet placed = world->GetItemLocation(rc)->GetPlacedRandomizerGet();
            r.placed += placed != RG_NONE ? 1 : 0;
            if (!IpIsIce(rc)) {
                continue;
            }
            if (placed == RG_NONE) {
                r.iceEmpty++;
            } else if (placed != Rando::StaticData::GetLocation(rc)->GetVanillaItem()) {
                r.iceMixed++;
            }
            r.iceCrossed += Combo_Crossings_Lookup(GAME_OOT, (uint16_t)rc) != nullptr ? 1 : 0;
        }
        r.mmShuffled = MM_WonderItemsTest_LiveShuffled(&r.mmOption, &r.mmTable);
    }
    Combo_SingleBag_Forget();
    printf("[TEST] oot-ice-paired: %s (%s, options %u/%u): gen %d, MM half %d, bag %s, goal proven %d, crossings %d, "
           "OoT pool rows %d, OoT hosts left %d; %zu location(s), %d placed; ice: %d+%d=%d location(s) (%d+%d=%d "
           "selected), %d host(s) / %d prefilled at the general pass, %d empty / %d not vanilla after finish, %d "
           "accepting MM / %d crossed; MM wonder: option %d, %d of %d shuffled\n",
           leg, kSeed, (unsigned)r.iciclesOption, (unsigned)r.redIceOption, gen, r.mmHalf,
           Combo_Logic_StatusName(r.bagStatus), r.goalProven ? 1 : 0, r.crossings, r.ootRows, r.hostsOoT,
           r.locations.size(), r.placed, r.icicleLocations, r.redIceLocations, r.iceLocations, r.icicleSelected,
           r.redIceSelected, r.iceSelected, r.iceHosts, r.icePrefilled, r.iceEmpty, r.iceMixed, r.iceAccepts,
           r.iceCrossed, r.mmOption, r.mmShuffled, r.mmTable);
    return r;
}

/** Leg A for one run. @return whether the run finished (the rest of its legs mean something). */
bool IpCheckFinished(const IpRun& r, const char* leg) {
    IP_CHECK(r.generated, "%s A: the generation failed or did not stop at OoT's general pass", leg);
    IP_CHECK(r.mmHalf == 0, "%s A: MM's half returned %d", leg, r.mmHalf);
    IP_CHECK(r.bagStatus == RSBS_COMBO_LOGIC_OK && r.goalProven, "%s A: the single bag reported %s, goal proven %d",
             leg, Combo_Logic_StatusName(r.bagStatus), r.goalProven ? 1 : 0);
    IP_CHECK(r.crossings >= 0, "%s A: OoT's crossings and remainder after the bag failed (%d)", leg, r.crossings);
    return r.generated && r.mmHalf == 0 && r.bagStatus == RSBS_COMBO_LOGIC_OK && r.goalProven && r.crossings >= 0;
}

} // namespace

/**
 * @return 0 when every leg holds, 1 otherwise.
 */
extern "C" int OoT_IcePairedTest_Run(void) {
    printf("[TEST] oot-ice-paired: a paired world with Shuffle Icicles and Shuffle Red Ice on places every check they "
           "select, mixes them, keeps them off the crossing hosts and leaves MM's own options alone; off, none is a "
           "location (#1408, #1562)\n");
    if (OTRGlobals::Instance == nullptr || OTRGlobals::Instance->gRandoContext == nullptr) {
        printf("[TEST] FAIL: oot-ice-paired needs OTRGlobals' randomizer context\n");
        return 1;
    }
    sFailures = 0;
    std::unique_ptr<SaveContext> priorSave = std::make_unique<SaveContext>();
    std::memcpy(priorSave.get(), &gSaveContext, sizeof(SaveContext));
    IpSavedKey icicles;
    IpSavedKey redIce;
    icicles.Save(kIciclesKey);
    redIce.Save(kRedIceKey);

    constexpr size_t kMMKeyCount = sizeof(kMMOptionKeys) / sizeof(kMMOptionKeys[0]);
    IpSavedKey mmBefore[kMMKeyCount];
    for (size_t i = 0; i < kMMKeyCount; i++) {
        mmBefore[i].Save(kMMOptionKeys[i]);
        IP_CHECK(mmBefore[i].value == 0, "E: MM's %s reads %d before the row: it must be at its default, Off",
                 kMMOptionKeys[i], mmBefore[i].value);
    }

    // Never stored: a stored rando key poisons later rows (Restore immediately below).
    const IpRun off = IpGenerate("OFF", false);
    const IpRun on = IpGenerate("ON", true);
    icicles.Restore();
    redIce.Restore();

    const bool offDone = IpCheckFinished(off, "OFF");
    const bool onDone = IpCheckFinished(on, "ON");

    // D: off.
    IP_CHECK(off.iciclesOption == RO_GENERIC_OFF && off.redIceOption == RO_GENERIC_OFF,
             "OFF D: the world generated with Shuffle Icicles %u / Shuffle Red Ice %u", (unsigned)off.iciclesOption,
             (unsigned)off.redIceOption);
    IP_CHECK(!off.locations.empty() && off.iceLocations == 0,
             "OFF D: %d of the world's %zu locations are icicle/red ice checks, want none of a non-empty pool",
             off.iceLocations, off.locations.size());

    // B: on.
    IP_CHECK(on.iciclesOption == RO_GENERIC_ON && on.redIceOption == RO_GENERIC_ON,
             "ON B: a paired world keyed On generated with Shuffle Icicles %u / Shuffle Red Ice %u (pinned off?)",
             (unsigned)on.iciclesOption, (unsigned)on.redIceOption);
    IP_CHECK(on.icicleSelected > 0 && on.redIceSelected > 0,
             "ON B: expected >0 quest-selected icicle (%d) and red ice (%d) checks", on.icicleSelected,
             on.redIceSelected);
    IP_CHECK(on.icicleLocations == on.icicleSelected && on.redIceLocations == on.redIceSelected &&
                 on.iceActual == on.iceSelectedSet,
             "ON B: %d+%d=%d #1408 check(s) in locations, quest selection expected %d+%d=%d", on.icicleLocations,
             on.redIceLocations, on.iceLocations, on.icicleSelected, on.redIceSelected, on.iceSelected);
    IP_CHECK(on.iceHosts + on.icePrefilled == on.iceLocations && on.iceHosts > 0,
             "ON B: at the general pass %d #1408 location(s) are hosts the bag can fill and %d are prefilled, of %d",
             on.iceHosts, on.icePrefilled, on.iceLocations);
    std::vector<RandomizerCheck> onWithoutIce;
    std::vector<RandomizerCheck> offList(off.locations.begin(), off.locations.end());
    for (RandomizerCheck rc : on.locations) {
        if (!IpIsIce(rc)) {
            onWithoutIce.push_back(rc);
        }
    }
    IP_CHECK(onWithoutIce == offList,
             "ON B: the ON world's %zu locations less its #1408 checks (%zu) do not match the OFF world's %zu: the "
             "options moved locations they do not own",
             on.locations.size(), onWithoutIce.size(), off.locations.size());
    if (offDone && onDone) {
        IP_CHECK(on.iceEmpty == 0, "ON B: %d #1408 location(s) hold no item after the finish", on.iceEmpty);
        IP_CHECK(on.placed - off.placed == on.iceLocations,
                 "ON B: the placed count grew by %d, the #1408 checks shuffled are %d", on.placed - off.placed,
                 on.iceLocations);

        // C: mixed, and off the crossing hosts.
        IP_CHECK(on.iceMixed > 0, "ON C: none of the %d #1408 checks holds an item other than its vanilla RG_NONE",
                 on.iceLocations);
        IP_CHECK(on.iceAccepts == 0 && on.iceCrossed == 0,
                 "ON C: %d #1408 check(s) accept an MM item and %d hold one in the crossing store (the host rule "
                 "admits no ACTOR_BG_ICE_TURARA / ACTOR_BG_ICE_SHELTER)",
                 on.iceAccepts, on.iceCrossed);

        // E: MM's own options.
        IP_CHECK(off.mmOption == 0 && on.mmOption == 0,
                 "E: MM's saved Shuffle Wonder Items is %d (OFF) / %d (ON), want its default Off in both", off.mmOption,
                 on.mmOption);
        IP_CHECK(off.mmTable > 0 && off.mmShuffled == on.mmShuffled && off.mmShuffled == 0,
                 "E: MM shuffled %d (OFF) / %d (ON) of %d checks, want 0 in both", off.mmShuffled, on.mmShuffled,
                 off.mmTable);
    }
    for (size_t i = 0; i < kMMKeyCount; i++) {
        IP_CHECK(CVarExists(kMMOptionKeys[i]) == mmBefore[i].existed &&
                     CVarGetInteger(kMMOptionKeys[i], 0) == mmBefore[i].value,
                 "E: the row changed MM's %s", kMMOptionKeys[i]);
    }

    Combo_SingleBag_Forget();
    ComboContext_Init();
    std::memcpy(&gSaveContext, priorSave.get(), sizeof(SaveContext));
    if (sFailures != 0) {
        printf("[TEST] FAIL (oot-ice-paired): %d failure(s)\n", sFailures);
        return 1;
    }
    printf("[TEST] PASS: oot-ice-paired\n");
    return 0;
}

#else

extern "C" int OoT_IcePairedTest_Run(void) {
    return 0;
}

#endif // RSBS_SINGLE_EXECUTABLE
