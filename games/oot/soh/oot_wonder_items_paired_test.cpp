/**
 * @file oot_wonder_items_paired_test.cpp
 * @brief #1405 / QC #1558: Ocarina of Time's Shuffle Wonder Items (SoH 412e9e262,
 *        RSK_SHUFFLE_WONDER_ITEMS) in a PAIRED world. Row OoTWonderItemsPaired (CTest
 *        label `rando`), dispatch `oot-wonder-items-paired` in src/common/test_runner.cpp.
 *        The operator ruled the option stays live in paired worlds ("Keep live, add
 *        tests"); this row is that test. No ROM: the generations run in whatever archive
 *        set the build directory holds, as the song-pool rows' do.
 *
 * One fixed seed, generated and FINISHED twice the way a creation finishes it
 * (oot_butterfly_fairy_test.cpp's leg N and test_song_pool_union.c's SpuGenerate):
 * Rando_HeadlessSeedTest stops OoT at its general pass, MM_Rando_HeadlessPairedHalf
 * runs MM's half (whose OnSaveInit chain runs the single bag over both games' hosts),
 * then OoT_Creation_FinishPairedHalf runs OoT's crossings and remainder. Run OFF has
 * the option's key cleared (Off, its default); run ON keys it All.
 *
 *  A  BOTH RUNS FINISH: both halves generate, the single bag reports OK and the goal
 *     is proven, and OoT's tail succeeds.
 *  B  THE WONDER CHECKS ARE PART OF THE ON WORLD. The wonder checks in the world are
 *     exactly the ones Context::GenerateLocationPool selects under All (every
 *     overworld one, and each dungeon one whose quest the world's dungeon selects:
 *     the derived count, printed). At the general pass each is a host the bag can
 *     fill (the OoT engine's allEmptyHosts lists it) or already holds an item a
 *     restricted pass placed (an own-dungeon key, say), and the bag is offered at
 *     least one. After the finish none is empty. The world's location set is the OFF
 *     world's plus exactly those wonder checks (no other location moves), and the
 *     finished worlds' placed-location count grew by exactly the wonder checks
 *     shuffled (the placed counts and the location counts are both printed).
 *  C  THE SHUFFLE MIXES THEM: at least one wonder check holds an item other than its
 *     vanilla item. Crossings: OoT_ComboLogic_HostAcceptsForeign admits only chests
 *     (ACTOR_EN_BOX), song checks, shop shelves, scrubs and four merchants, so no
 *     wonder check (ACTOR_EN_WONDER_ITEM) accepts a Majora's Mask item and none is in
 *     the crossing store; both counts are asserted 0 and printed.
 *  D  OFF: no wonder check is a location (so none is a host) and the world's location
 *     set is the baseline B compares against.
 *  E  MAJORA'S MASK'S OWN OPTION (RO_SHUFFLE_WONDER_ITEMS, #1406) stays at its
 *     default: its key reads Off and is not touched, MM's saved option is Off in
 *     both runs, MM's shuffled wonder checks are the same count in both runs (the
 *     OoT option does not reach MM's half), and that count is 0 (GeneratePools
 *     skips them while MM's option is Off).
 *
 * Each run prints one summary line with its counts.
 *
 * RED HALVES, derived, not run (this lane had no build): with
 * SeedContext.cpp's wonder skip made unconditional B fails (no wonder location
 * under All); with it made never-true D fails (wonder locations under Off) and B's
 * set equality fails; with a paired pin of RSK_SHUFFLE_WONDER_ITEMS in
 * Playthrough_Init B fails (the world generated Off); with
 * OoT_ComboLogic_HostAcceptsForeign admitting every host C fails if the bag crosses
 * onto a wonder check (not certain on one seed; the acceptance count fails
 * certainly); with MM's GeneratePools wonder skip removed, or reading the OoT
 * option, E fails.
 *
 * Every key and the OoT save are put back; the combo record and the single bag are
 * reset on the way out, as the song-pool rows reset them.
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

constexpr const char* kSeed = "RSBSWONDERPAIRED1";
constexpr const char* kMMOptionKey = "gRando.Options.RO_SHUFFLE_WONDER_ITEMS";

int sFailures = 0;

void WpFail(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    printf("[TEST] oot-wonder-items-paired: FAIL: ");
    vprintf(fmt, args);
    printf("\n");
    va_end(args);
    sFailures++;
}

#define WP_CHECK(cond, ...)      \
    do {                         \
        if (!(cond)) {           \
            WpFail(__VA_ARGS__); \
        }                        \
    } while (0)

const char* const kShuffleKey = CVAR_RANDOMIZER_SETTING("ShuffleWonderItems");

/** A key's stored state, put back by Restore. */
struct WpSavedKey {
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

bool WpIsWonder(RandomizerCheck rc) {
    return Rando::StaticData::GetLocation(rc)->GetRCType() == RCTYPE_WONDER_ITEM;
}

/** What one finished paired world did with the wonder checks. */
struct WpRun {
    bool generated;  // OoT's generation stopped at its general pass
    int mmHalf;      // MM_Rando_HeadlessPairedHalf's rc
    int bagStatus;   // the single bag's status
    bool goalProven; // ... and its proof
    int crossings;   // OoT_Creation_FinishPairedHalf's rc (-1: not run or failed)
    int ootRows;     // OoT general-pass pool rows the bag took (printed only)
    int hostsOoT;    // OoT's empty hosts after the bag's roll-back (printed only)
    uint8_t option;  // the world's RSK_SHUFFLE_WONDER_ITEMS
    std::set<RandomizerCheck> locations;
    int wonderLocations; // wonder checks among the world's locations
    int wonderSelected;  // ... what GenerateLocationPool selects under All
    int wonderHosts;     // general pass: wonder checks the engine lists as empty hosts
    int wonderPrefilled; // general pass: wonder checks a restricted pass already filled
    int placed;          // finished: locations holding an item
    int wonderEmpty;     // finished: wonder locations holding none
    int wonderMixed;     // finished: wonder locations holding an item other than their vanilla one
    int wonderAccepts;   // wonder locations OoT's host rule would give an MM item
    int wonderCrossed;   // wonder locations in the crossing store
    int mmOption;        // MM's saved RO_SHUFFLE_WONDER_ITEMS (-1 unread)
    int mmTable;         // MM's wonder item checks in its table
    int mmShuffled;      // ... shuffled in MM's finished half
};

WpRun WpGenerate(const char* leg, bool on) {
    WpRun r = {};
    r.crossings = -1;
    r.mmOption = -1;
    Combo_SingleBag_Forget();
    ComboContext_Init();
    if (on) {
        CVarSetInteger(kShuffleKey, RO_SHUFFLE_WONDER_ITEMS_ALL);
    } else {
        CVarClear(kShuffleKey);
    }
    const int gen = Rando_HeadlessSeedTest(kSeed);
    r.generated = gen == 0 && OoT_ComboLogic_GeneralPassDeferred() != 0;
    auto world = Rando::Context::GetInstance();
    if (r.generated && world != nullptr) {
        r.option = world->GetOption(RSK_SHUFFLE_WONDER_ITEMS).Get();
        for (RandomizerCheck rc : world->allLocations) {
            r.locations.insert(rc);
        }
        // What Context::GenerateLocationPool selects under All (its quest rule).
        for (int c = RC_KF_WONDER_TRAINING_1; c <= RC_GANONS_CASTLE_MQ_WONDER_SHADOW_TRIAL; c++) {
            Rando::Location* loc = Rando::StaticData::GetLocation((RandomizerCheck)c);
            if (loc->IsOverworld()) {
                r.wonderSelected++;
                continue;
            }
            auto* dungeon = world->GetDungeon(loc->GetArea() - RCAREA_DEKU_TREE);
            if (dungeon != nullptr &&
                (loc->GetQuest() == RCQUEST_BOTH || (loc->GetQuest() == RCQUEST_MQ) == dungeon->IsMQ())) {
                r.wonderSelected++;
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
            if (!WpIsWonder(rc)) {
                continue;
            }
            r.wonderLocations++;
            if (emptyHosts.count((uint16_t)rc) != 0) {
                r.wonderHosts++;
            } else if (world->GetItemLocation(rc)->GetPlacedRandomizerGet() != RG_NONE) {
                r.wonderPrefilled++;
            }
            r.wonderAccepts += OoT_ComboLogic_TestHostAcceptsForeign((uint16_t)rc) != 0 ? 1 : 0;
        }
    }
    r.mmHalf = r.generated ? MM_Rando_HeadlessPairedHalf() : -1;
    const ComboSingleBagReport* bag = Combo_SingleBag_LastReport();
    r.bagStatus = bag->status;
    r.goalProven = bag->fill.goalProven;
    r.ootRows = bag->ootRows;
    r.hostsOoT = bag->hostsOoT;
    const bool proven = r.mmHalf == 0 && r.bagStatus == RSBS_COMBO_LOGIC_OK && r.goalProven;
    // OoT's tail only over a world the bag decided (oot_butterfly_fairy_test.cpp).
    if (proven) {
        r.crossings = OoT_Creation_FinishPairedHalf(0);
    }
    if (r.crossings >= 0 && world != nullptr) {
        for (RandomizerCheck rc : r.locations) {
            const RandomizerGet placed = world->GetItemLocation(rc)->GetPlacedRandomizerGet();
            r.placed += placed != RG_NONE ? 1 : 0;
            if (!WpIsWonder(rc)) {
                continue;
            }
            if (placed == RG_NONE) {
                r.wonderEmpty++;
            } else if (placed != Rando::StaticData::GetLocation(rc)->GetVanillaItem()) {
                r.wonderMixed++;
            }
            r.wonderCrossed += Combo_Crossings_Lookup(GAME_OOT, (uint16_t)rc) != nullptr ? 1 : 0;
        }
        r.mmShuffled = MM_WonderItemsTest_LiveShuffled(&r.mmOption, &r.mmTable);
    }
    Combo_SingleBag_Forget();
    printf("[TEST] oot-wonder-items-paired: %s (%s, option %u): gen %d, MM half %d, bag %s, goal proven %d, "
           "crossings %d, OoT pool rows %d, OoT hosts left %d; %zu location(s), %d placed; wonder: %d location(s) "
           "(%d selected), %d host(s) / %d prefilled at the general pass, %d empty / %d not vanilla after the "
           "finish, %d accepting an MM item, %d crossed; MM wonder: option %d, %d of %d shuffled\n",
           leg, kSeed, (unsigned)r.option, gen, r.mmHalf, Combo_Logic_StatusName(r.bagStatus), r.goalProven ? 1 : 0,
           r.crossings, r.ootRows, r.hostsOoT, r.locations.size(), r.placed, r.wonderLocations, r.wonderSelected,
           r.wonderHosts, r.wonderPrefilled, r.wonderEmpty, r.wonderMixed, r.wonderAccepts, r.wonderCrossed, r.mmOption,
           r.mmShuffled, r.mmTable);
    return r;
}

/** Leg A for one run. @return whether the run finished (the rest of its legs mean something). */
bool WpCheckFinished(const WpRun& r, const char* leg) {
    WP_CHECK(r.generated, "%s A: the generation failed or did not stop at OoT's general pass", leg);
    WP_CHECK(r.mmHalf == 0, "%s A: MM's half returned %d", leg, r.mmHalf);
    WP_CHECK(r.bagStatus == RSBS_COMBO_LOGIC_OK && r.goalProven, "%s A: the single bag reported %s, goal proven %d",
             leg, Combo_Logic_StatusName(r.bagStatus), r.goalProven ? 1 : 0);
    WP_CHECK(r.crossings >= 0, "%s A: OoT's crossings and remainder after the bag failed (%d)", leg, r.crossings);
    return r.generated && r.mmHalf == 0 && r.bagStatus == RSBS_COMBO_LOGIC_OK && r.goalProven && r.crossings >= 0;
}

} // namespace

/**
 * @return 0 when every leg holds, 1 otherwise.
 */
extern "C" int OoT_WonderItemsPairedTest_Run(void) {
    printf("[TEST] oot-wonder-items-paired: a paired world with Shuffle Wonder Items on places every wonder check "
           "it selects, mixes them, keeps them off the crossing hosts and leaves MM's own option alone; off, none "
           "is a location (#1405, QC #1558)\n");
    if (OTRGlobals::Instance == nullptr || OTRGlobals::Instance->gRandoContext == nullptr) {
        printf("[TEST] FAIL: oot-wonder-items-paired needs OTRGlobals' randomizer context\n");
        return 1;
    }
    sFailures = 0;
    // The current game is left as the bring-up set it, as the paired neighbours
    // (oot_butterfly_fairy_test.cpp, test_song_pool_union.c) leave it: MM's half
    // runs MM's own OnSaveInit chain.
    std::unique_ptr<SaveContext> priorSave = std::make_unique<SaveContext>();
    std::memcpy(priorSave.get(), &gSaveContext, sizeof(SaveContext));
    WpSavedKey shuffle;
    shuffle.Save(kShuffleKey);
    const bool mmKeySet = CVarExists(kMMOptionKey);
    const int32_t mmKeyValue = CVarGetInteger(kMMOptionKey, 0);
    WP_CHECK(mmKeyValue == 0, "E: MM's %s reads %d before the row: it must be at its default, Off", kMMOptionKey,
             mmKeyValue);

    // Never stored: a stored rando key poisons later rows (Restore below).
    const WpRun off = WpGenerate("OFF", false);
    const WpRun on = WpGenerate("ON", true);
    shuffle.Restore();

    const bool offDone = WpCheckFinished(off, "OFF");
    const bool onDone = WpCheckFinished(on, "ON");

    // D: off.
    WP_CHECK(off.option == RO_SHUFFLE_WONDER_ITEMS_OFF, "OFF D: the world generated with Shuffle Wonder Items %u",
             (unsigned)off.option);
    WP_CHECK(!off.locations.empty() && off.wonderLocations == 0,
             "OFF D: %d of the world's %zu locations are wonder checks, want none of a non-empty pool",
             off.wonderLocations, off.locations.size());

    // B: on.
    WP_CHECK(on.option == RO_SHUFFLE_WONDER_ITEMS_ALL,
             "ON B: a paired world keyed All generated with Shuffle Wonder Items %u (pinned off?)",
             (unsigned)on.option);
    WP_CHECK(on.wonderSelected > 0 && on.wonderLocations == on.wonderSelected,
             "ON B: %d wonder check(s) are locations, the world's quests select %d", on.wonderLocations,
             on.wonderSelected);
    WP_CHECK(on.wonderHosts + on.wonderPrefilled == on.wonderLocations && on.wonderHosts > 0,
             "ON B: at the general pass %d wonder location(s) are hosts the bag can fill and %d are filled, of %d",
             on.wonderHosts, on.wonderPrefilled, on.wonderLocations);
    std::vector<RandomizerCheck> onWithoutWonder;
    std::vector<RandomizerCheck> offList(off.locations.begin(), off.locations.end());
    for (RandomizerCheck rc : on.locations) {
        if (!WpIsWonder(rc)) {
            onWithoutWonder.push_back(rc);
        }
    }
    WP_CHECK(onWithoutWonder == offList,
             "ON B: the ON world's %zu locations less its wonder checks (%zu) are not the OFF world's %zu: the "
             "option moved locations it does not own",
             on.locations.size(), onWithoutWonder.size(), off.locations.size());
    if (offDone && onDone) {
        WP_CHECK(on.wonderEmpty == 0, "ON B: %d wonder location(s) hold no item after the finish", on.wonderEmpty);
        WP_CHECK(on.placed - off.placed == on.wonderLocations,
                 "ON B: the placed count grew by %d, the wonder checks shuffled are %d", on.placed - off.placed,
                 on.wonderLocations);

        // C: mixed, and off the crossing hosts.
        WP_CHECK(on.wonderMixed > 0, "ON C: every one of the %d wonder checks holds its vanilla item",
                 on.wonderLocations);
        WP_CHECK(on.wonderAccepts == 0 && on.wonderCrossed == 0,
                 "ON C: %d wonder check(s) accept an MM item and %d hold one in the crossing store (the host rule "
                 "admits no ACTOR_EN_WONDER_ITEM; if it now does, the sparkle-colour question is live)",
                 on.wonderAccepts, on.wonderCrossed);

        // E: MM's own option.
        WP_CHECK(off.mmOption == 0 && on.mmOption == 0,
                 "E: MM's saved Shuffle Wonder Items is %d (OFF) / %d (ON), want its default Off in both", off.mmOption,
                 on.mmOption);
        WP_CHECK(off.mmTable > 0 && off.mmShuffled == on.mmShuffled,
                 "E: MM shuffles %d (OFF) and %d (ON) of its %d wonder checks: the OoT option reached MM's half",
                 off.mmShuffled, on.mmShuffled, off.mmTable);
        // MM's GeneratePools skips every RCTYPE_WONDER_ITEM check while its own
        // option is Off, so with it at its default none is shuffled in either run.
        WP_CHECK(off.mmShuffled == 0 && on.mmShuffled == 0,
                 "E: with MM's own option Off MM shuffles %d (OFF) / %d (ON) wonder checks, want none", off.mmShuffled,
                 on.mmShuffled);
    }
    WP_CHECK(CVarExists(kMMOptionKey) == mmKeySet && CVarGetInteger(kMMOptionKey, 0) == mmKeyValue,
             "E: the row changed MM's %s", kMMOptionKey);

    Combo_SingleBag_Forget();
    ComboContext_Init();
    std::memcpy(&gSaveContext, priorSave.get(), sizeof(SaveContext));
    if (sFailures != 0) {
        printf("[TEST] FAIL (oot-wonder-items-paired): %d failure(s)\n", sFailures);
        return 1;
    }
    printf("[TEST] PASS: oot-wonder-items-paired\n");
    return 0;
}

#else

extern "C" int OoT_WonderItemsPairedTest_Run(void) {
    return 0;
}

#endif // RSBS_SINGLE_EXECUTABLE
