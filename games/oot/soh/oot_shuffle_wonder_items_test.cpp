/**
 * @file oot_shuffle_wonder_items_test.cpp
 * @brief #1405: Ship of Harkinian's Shuffle Wonder Items (SoH 412e9e262) in Ocarina
 *        of Time's randomizer (Enhancements/randomizer/ShuffleWonderItems.cpp). Row
 *        OoTShuffleWonderItems (CTest label `rando`), dispatch `oot-shuffle-wonder-items`
 *        in src/common/test_runner.cpp; the bring-up is oot-shuffle-silver's
 *        (OTRGlobals' randomizer context). No ROM: the generations run in whatever
 *        archive set the build directory holds, as rando-settings-fold-excludes'.
 *
 *  M  THE FLAG MAP. Every RCTYPE_WONDER_ITEM check (the 201 contiguous ids from
 *     RC_KF_WONDER_TRAINING_1 to RC_GANONS_CASTLE_MQ_WONDER_SHADOW_TRIAL) is in the
 *     location table as a wonder item check, has a RandomizerInf of its own in
 *     rcToRandomizerInf (not RAND_INF_MAX, not shared with another wonder check, and
 *     the flag its spoiler collection check names), and that flag maps back to it.
 *     Every non-MQ wonder check is also identified from its own key by the real
 *     IdentifyWonderItem, the lookup EnWonderItem_RandomizerHoldsItem runs: a dungeon
 *     check is keyed by (room, actor index), an overworld one by (x, z).
 *  D  THE OPTION. Shuffle Wonder Items ships Off with its key cleared.
 *  F  THE FOLD AT DEFAULT SETTINGS. A real generation with the option's key cleared:
 *     the settings fold reads none of the lines #1405 added (the option's own rows,
 *     the RCTYPE_WONDER_ITEM exclude rows) and leaves out exactly as many as the
 *     option groups hold, counted here by pointer. No wonder check is a location.
 *  O  THE OPTION ON IN A PAIRED WORLD. The option is not pinned off in a paired
 *     world (its items are junk-class, as pots and grass are): the same generation
 *     with the option keyed All succeeds, keeps it All, folds every one of its lines
 *     and leaves none out, so its fingerprint differs from leg F's, and every wonder
 *     check the world's dungeon quests select is a location holding an item.
 *
 * RED HALVES, derived, not run (this lane had no build): with the wonder rows removed
 * from rcToRandomizerInf M fails (RAND_INF_MAX for every check, and IdentifyWonderItem
 * finds no check); with RegisterWonderItemLocations dropped from InitLocationTable M
 * fails wherever the actor map was built before ShipInit registered the checks; with
 * the fold's skip removed at its call site F fails (lines folded); with the classifier
 * returning false F's left-out count is 0; with the wonder line dropped from
 * Context::GenerateLocationPool's skip list F fails (wonder checks are locations);
 * with the wonder item checks skipped there unconditionally O fails (no wonder
 * location); with a paired pin added O fails (the option generated Off).
 *
 * Every key, option and save field a leg changes is put back (in memory: nothing here
 * writes a config file).
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
#include "soh/Enhancements/randomizer/randomizer.h"
#include "soh/Enhancements/randomizer/static_data.h"
#include "soh/Enhancements/randomizer/SeedContext.h"
#include "soh/Enhancements/randomizer/settings.h"
#include "soh/Enhancements/randomizer/logic.h"
#include "soh/Enhancements/randomizer/item_location.h"
#include "soh/Enhancements/randomizer/dungeon.h"

#include "context.h" // src/common: gComboCtx, Context_GetCurrentGame, Context_SetCurrentGame
#include "game.h"

// At file scope, not inside an extern "C" block (see oot_shared_kokiri_sword_test.cpp).
#include <z64.h>
extern "C" {
extern SaveContext gSaveContext;
extern PlayState* OoT_gPlayState;
}

// 3drando/menu.cpp: the headless generation bridge every rando row uses.
extern "C" int Rando_HeadlessSeedTest(const char* seedStr);
// 3drando/playthrough.cpp: what the last settings fold folded, in total and of
// Shuffle Wonder Items' lines.
extern "C" int OoT_Rando_LastSettingsFold(uint32_t* outTotalLines, uint32_t* outExcludeLines, int groupCap);
extern "C" int OoT_Rando_LastSettingsFoldWonderItem(uint32_t* outFolded, uint32_t* outSkipped);

// THE APPEND-ONLY LOCKS: each id #1405 adds sits after the last id before it (#1363's
// silver rupees, #1404's setting keys), so no id a world or a save already holds moves.
// The numbers themselves are locked in GameExports_SingleExe.cpp.
static_assert(RC_KF_WONDER_TRAINING_1 == RC_GANONS_CASTLE_MQ_MOVING_PLATFORM_SHADOW_SILVER + 1,
              "the wonder item checks are appended after #1363's silver rupee checks (#1405)");
static_assert(RC_GANONS_CASTLE_MQ_WONDER_SHADOW_TRIAL - RC_KF_WONDER_TRAINING_1 + 1 == 201,
              "SoH 412e9e262 adds 201 wonder item checks, contiguous (#1405)");
static_assert(RAND_INF_KF_WONDER_TRAINING_1 == RAND_INF_GANONS_CASTLE_MQ_MOVING_PLATFORM_SHADOW_SILVER + 1,
              "the wonder item flags are appended after #1363's silver rupee flags (#1405)");
static_assert(RSK_SHUFFLE_WONDER_ITEMS == RSK_STARTING_MASK_OF_TRUTH + 1,
              "RSK_SHUFFLE_WONDER_ITEMS is appended after #1404's setting keys (#1405)");
static_assert(RO_SHUFFLE_WONDER_ITEMS_OFF == 0,
              "#1405: a world saved before the option reads its missing entry as 0, Off");

namespace {

constexpr int kWonderChecks = 201;

int sFailures = 0;

void WiFail(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    printf("[TEST] oot-shuffle-wonder-items: FAIL: ");
    vprintf(fmt, args);
    printf("\n");
    va_end(args);
    sFailures++;
}

#define WI_CHECK(cond, ...)      \
    do {                         \
        if (!(cond)) {           \
            WiFail(__VA_ARGS__); \
        }                        \
    } while (0)

const char* const kShuffleKey = CVAR_RANDOMIZER_SETTING("ShuffleWonderItems");

/** A key's stored state, put back by Restore. */
struct WonderSavedKey {
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

// ---- M: the flag map ----------------------------------------------------------------

void CheckFlagMap() {
    Randomizer* randomizer = OTRGlobals::Instance->gRandomizer.get();
    if (randomizer == nullptr) {
        WiFail("M: needs OTRGlobals' randomizer (the flag map is its rcToRandomizerInf)");
        return;
    }
    if (Rando::StaticData::CheckFromActorMultimap.empty()) {
        Rando::StaticData::InitHashMaps();
    }
    int wonderChecks = 0;
    int unmapped = 0;
    int shared = 0;
    int wrongFlag = 0;
    int noReverse = 0;
    int identifyDungeon = 0;
    int identifyOverworld = 0;
    int identifyWrong = 0;
    std::set<int> flags;
    PlayState* const priorPlay = OoT_gPlayState;
    OoT_gPlayState = nullptr; // ResourceMgr_IsGameMasterQuest() reads vanilla with no play state
    // IdentifyWonderItem moves the Colossus oasis keys for an adult; the table holds the child's.
    const int32_t priorAge = gSaveContext.linkAge;
    gSaveContext.linkAge = LINK_AGE_CHILD;
    for (int c = RC_KF_WONDER_TRAINING_1; c <= RC_GANONS_CASTLE_MQ_WONDER_SHADOW_TRIAL; c++) {
        const RandomizerCheck rc = (RandomizerCheck)c;
        Rando::Location* loc = Rando::StaticData::GetLocation(rc);
        if (loc->GetRandomizerCheck() != rc || loc->GetRCType() != RCTYPE_WONDER_ITEM) {
            WiFail("M: check %d is not in the location table as a wonder item check (type %d)", c,
                   (int)loc->GetRCType());
            continue;
        }
        wonderChecks++;
        const RandomizerInf inf = randomizer->GetRandomizerInfFromCheck(rc);
        if (inf == RAND_INF_MAX || (int)inf < 0 || (int)inf > (int)RAND_INF_MAX) {
            printf("[TEST]   M: %s has no flag of its own (%d)\n", loc->GetName().c_str(), (int)inf);
            unmapped++;
            continue;
        }
        if (!flags.insert((int)inf).second) {
            printf("[TEST]   M: %s shares flag %d with another wonder check\n", loc->GetName().c_str(), (int)inf);
            shared++;
        }
        const Rando::SpoilerCollectionCheck collection = loc->GetCollectionCheck();
        if (collection.type != SPOILER_CHK_RANDOMIZER_INF || collection.flag != (uint16_t)inf) {
            printf("[TEST]   M: %s maps to flag %d, its spoiler collection check names %u\n", loc->GetName().c_str(),
                   (int)inf, (unsigned)collection.flag);
            wrongFlag++;
        }
        if (randomizer->GetCheckFromRandomizerInf(inf) != rc) {
            printf("[TEST]   M: flag %d of %s maps back to check %d\n", (int)inf, loc->GetName().c_str(),
                   (int)randomizer->GetCheckFromRandomizerInf(inf));
            noReverse++;
        }
        if (loc->GetQuest() == RCQUEST_MQ) {
            continue; // the actor lookup keeps an MQ check only in an MQ scene
        }
        // The key the check is stored under, TWO_ACTOR_PARAMS(a, b): (room, actor index)
        // in a dungeon, (x, z) in the overworld, each passed as the hook passes it.
        const int32_t params = loc->GetActorParams();
        const s32 par1 = (s32)(int16_t)((params >> 16) & 0xFFFF);
        const s32 par2 = (s32)(int16_t)(params & 0xFFFF);
        const CheckIdentity id = randomizer->IdentifyWonderItem((s32)loc->GetScene(), par1, par2);
        if (loc->IsDungeon()) {
            identifyDungeon++;
        } else {
            identifyOverworld++;
        }
        if (id.randomizerCheck != rc || id.randomizerInf != inf) {
            printf("[TEST]   M: IdentifyWonderItem at %s's key (%d, %d) found check %d flag %d\n",
                   loc->GetName().c_str(), (int)par1, (int)par2, (int)id.randomizerCheck, (int)id.randomizerInf);
            identifyWrong++;
        }
    }
    gSaveContext.linkAge = priorAge;
    OoT_gPlayState = priorPlay;
    printf("[TEST] M: %d wonder check(s): %d without a flag, %d sharing one, %d disagreeing with their collection "
           "check, %d not mapping back; IdentifyWonderItem %d of %d (%d dungeon, %d overworld) wrong\n",
           wonderChecks, unmapped, shared, wrongFlag, noReverse, identifyWrong, identifyDungeon + identifyOverworld,
           identifyDungeon, identifyOverworld);
    WI_CHECK(wonderChecks == kWonderChecks, "M: %d of the %d wonder checks are in the location table", wonderChecks,
             kWonderChecks);
    WI_CHECK(unmapped == 0, "M: %d wonder check(s) have no RandomizerInf (they would share flag 0 or none)", unmapped);
    WI_CHECK(shared == 0, "M: %d wonder check(s) share a RandomizerInf", shared);
    WI_CHECK(wrongFlag == 0, "M: %d wonder check(s) map to a flag their collection check does not name", wrongFlag);
    WI_CHECK(noReverse == 0, "M: %d wonder flag(s) do not map back to their check", noReverse);
    WI_CHECK(identifyDungeon > 0 && identifyOverworld > 0,
             "M: IdentifyWonderItem was tried on %d dungeon and %d overworld check(s), want both kinds",
             identifyDungeon, identifyOverworld);
    WI_CHECK(identifyWrong == 0, "M: IdentifyWonderItem identified %d of %d non-MQ wonder checks wrongly",
             identifyWrong, identifyDungeon + identifyOverworld);
}

// ---- D: the option -------------------------------------------------------------------

void CheckOption() {
    auto settings = Rando::Settings::GetInstance();
    settings->CreateOptions(); // idempotent; what Rando_HeadlessSeedTest calls first
    WonderSavedKey shuffle;
    shuffle.Save(kShuffleKey);
    CVarClear(kShuffleKey);
    Rando::Option& option = settings->GetOption(RSK_SHUFFLE_WONDER_ITEMS);
    const int shipped = option.GetOptionIndex();
    printf("[TEST] D: '%s' (key %s): %d with no key stored, %zu choice(s)\n", option.GetName().c_str(),
           option.GetCVarName().c_str(), shipped, (size_t)option.GetOptionCount());
    WI_CHECK(option.GetName() == "Shuffle Wonder Items", "D: the option is named '%s'", option.GetName().c_str());
    WI_CHECK(option.GetCVarName() == kShuffleKey, "D: the option is not keyed %s", kShuffleKey);
    WI_CHECK(shipped == RO_SHUFFLE_WONDER_ITEMS_OFF, "D: the shipped default is %d, not Off", shipped);
    WI_CHECK(option.GetOptionCount() == (size_t)RO_SHUFFLE_WONDER_ITEMS_ALL + 1,
             "D: the option has %zu choices, want %d", (size_t)option.GetOptionCount(),
             (int)RO_SHUFFLE_WONDER_ITEMS_ALL + 1);
    shuffle.Restore();
}

// ---- F and O: the fold -------------------------------------------------------------------

/** The fold lines #1405 added, counted from the option groups by pointer. Same walk
 *  as the fold (every Setting with options, in every group that is not a group of
 *  groups). */
uint32_t CountWonderLines() {
    auto settings = Rando::Settings::GetInstance();
    std::set<const Rando::Option*> excludeRows;
    for (auto& loc : Rando::StaticData::GetLocationTable()) {
        if (loc.GetRCType() == RCTYPE_WONDER_ITEM) {
            excludeRows.insert(loc.GetExcludedOption());
        }
    }
    const Rando::Option* shuffle = &settings->GetOption(RSK_SHUFFLE_WONDER_ITEMS);
    uint32_t excludes = 0;
    uint32_t optionRows = 0;
    const auto& groups = settings->GetOptionGroups();
    for (size_t g = 0; g < groups.size(); g++) {
        if (groups[g].GetContainsType() == Rando::OptionGroupType::SUBGROUP || g == RSG_TRICKS) {
            continue;
        }
        for (const Rando::Option* option : groups[g].GetOptions()) {
            if (!option->IsCategory(Rando::OptionCategory::Setting) || option->GetOptionCount() == 0) {
                continue;
            }
            if (g >= RSG_EXCLUDES_KOKIRI_FOREST && g <= RSG_EXCLUDES_GANONS_CASTLE) {
                excludes += excludeRows.count(option) != 0 ? 1 : 0;
            } else {
                optionRows += option == shuffle ? 1 : 0;
            }
        }
    }
    printf("[TEST] the option groups hold %u #1405 line(s): %u wonder exclude row(s) (%zu distinct options), %u "
           "row(s) of the option itself\n",
           (unsigned)(excludes + optionRows), (unsigned)excludes, excludeRows.size(), (unsigned)optionRows);
    // Every wonder check's name is its own within its area, so each has a row.
    WI_CHECK(excludes == (uint32_t)kWonderChecks, "the exclude groups hold %u of the %d wonder checks' rows",
             (unsigned)excludes, kWonderChecks);
    WI_CHECK(optionRows > 0, "no folded group holds the option's own row");
    return excludes + optionRows;
}

struct FoldRecord {
    bool generated;
    int runs;
    uint32_t total;
    uint32_t folded;
    uint32_t skipped;
    uint32_t settingsHash;
    uint8_t shuffleOption;
    size_t locations;
    size_t wonderLocations;
    size_t wonderExpected;
    int wonderEmpty;
};

FoldRecord WonderGenerate(const char* leg) {
    FoldRecord r = {};
    const int rc = Rando_HeadlessSeedTest("RSBSUNIFIED1");
    r.generated = rc == 0;
    r.runs = OoT_Rando_LastSettingsFold(&r.total, nullptr, 0);
    (void)OoT_Rando_LastSettingsFoldWonderItem(&r.folded, &r.skipped);
    r.settingsHash = gComboCtx.sharedRandoSettingsHash;
    auto world = Rando::Context::GetInstance();
    if (world != nullptr) {
        r.shuffleOption = world->GetOption(RSK_SHUFFLE_WONDER_ITEMS).Get();
        r.locations = world->allLocations.size();
        for (RandomizerCheck check : world->allLocations) {
            if (Rando::StaticData::GetLocation(check)->GetRCType() == RCTYPE_WONDER_ITEM) {
                r.wonderLocations++;
                const RandomizerGet placed = world->GetItemLocation(check)->GetPlacedRandomizerGet();
                if (placed == RG_NONE) {
                    r.wonderEmpty++;
                }
            }
        }
        // What the world's dungeon quests select under All (Context::GenerateLocationPool's rule).
        for (int c = RC_KF_WONDER_TRAINING_1; c <= RC_GANONS_CASTLE_MQ_WONDER_SHADOW_TRIAL; c++) {
            Rando::Location* loc = Rando::StaticData::GetLocation((RandomizerCheck)c);
            if (loc->IsOverworld()) {
                r.wonderExpected++;
                continue;
            }
            auto* dungeon = world->GetDungeon(loc->GetArea() - RCAREA_DEKU_TREE);
            if (dungeon != nullptr &&
                (loc->GetQuest() == RCQUEST_BOTH || (loc->GetQuest() == RCQUEST_MQ) == dungeon->IsMQ())) {
                r.wonderExpected++;
            }
        }
    }
    printf("[TEST] %s: generation rc %d, fold run %d: %u line(s), #1405 lines folded %u / left out %u, settingsHash "
           "%08X; option %u; %zu location(s), %zu of them wonder checks (%zu selected by the quests), %d empty\n",
           leg, rc, r.runs, (unsigned)r.total, (unsigned)r.folded, (unsigned)r.skipped, (unsigned)r.settingsHash,
           (unsigned)r.shuffleOption, r.locations, r.wonderLocations, r.wonderExpected, r.wonderEmpty);
    return r;
}

void CheckFold() {
    WonderSavedKey shuffle;
    shuffle.Save(kShuffleKey);

    CVarClear(kShuffleKey);
    const FoldRecord f = WonderGenerate("F");
    const uint32_t expected = CountWonderLines();
    WI_CHECK(f.generated && f.runs > 0, "F: the generation failed or ran no settings fold");
    WI_CHECK(f.folded == 0,
             "F: the settings fold read %u line(s) #1405 added while the option is off: every default world's "
             "fingerprint, and so every placement, moves",
             (unsigned)f.folded);
    WI_CHECK(f.skipped == expected, "F: the fold left out %u line(s), the option groups hold %u", (unsigned)f.skipped,
             (unsigned)expected);
    WI_CHECK(f.shuffleOption == RO_SHUFFLE_WONDER_ITEMS_OFF, "F: the world was generated with Shuffle Wonder Items %u",
             (unsigned)f.shuffleOption);
    WI_CHECK(f.locations > 0 && f.wonderLocations == 0,
             "F: %zu of the world's %zu locations are wonder checks, want none of a non-empty pool", f.wonderLocations,
             f.locations);

    // Never stored: a stored rando key poisons later rows (Restore below).
    CVarSetInteger(kShuffleKey, RO_SHUFFLE_WONDER_ITEMS_ALL);
    const FoldRecord o = WonderGenerate("O");
    shuffle.Restore();
    WI_CHECK(o.generated && o.runs > f.runs, "O: the paired generation with Shuffle Wonder Items on failed");
    WI_CHECK(o.shuffleOption == RO_SHUFFLE_WONDER_ITEMS_ALL,
             "O: a paired world keyed All was generated with Shuffle Wonder Items %u", (unsigned)o.shuffleOption);
    WI_CHECK(o.folded == expected && o.skipped == 0,
             "O: with the option on the fold read %u and left out %u of %u line(s)", (unsigned)o.folded,
             (unsigned)o.skipped, (unsigned)expected);
    WI_CHECK(o.settingsHash != f.settingsHash,
             "O: the world with the option on has the default world's fingerprint %08X", (unsigned)f.settingsHash);
    WI_CHECK(o.wonderExpected > 0 && o.wonderLocations == o.wonderExpected,
             "O: %zu wonder check(s) are locations, the world's dungeon quests select %zu", o.wonderLocations,
             o.wonderExpected);
    WI_CHECK(o.wonderEmpty == 0, "O: %d wonder location(s) hold no item after the fill", o.wonderEmpty);
}

} // namespace

/**
 * @return 0 when every leg holds, 1 otherwise.
 */
extern "C" int OoT_ShuffleWonderItemsTest_Run(void) {
    printf("[TEST] oot-shuffle-wonder-items: every wonder item check has its own flag and is identified from its key, "
           "the option ships off, default worlds fold none of its lines, and a paired world with it on places every "
           "wonder check (#1405)\n");
    if (OTRGlobals::Instance == nullptr || OTRGlobals::Instance->gRandoContext == nullptr) {
        printf("[TEST] FAIL: oot-shuffle-wonder-items needs OTRGlobals' randomizer context\n");
        return 1;
    }
    sFailures = 0;
    const GameId prevGame = Context_GetCurrentGame();
    Context_SetCurrentGame(GAME_OOT);
    std::unique_ptr<SaveContext> priorSave = std::make_unique<SaveContext>();
    std::memcpy(priorSave.get(), &gSaveContext, sizeof(SaveContext));

    CheckFlagMap();
    CheckOption();
    CheckFold();

    std::memcpy(&gSaveContext, priorSave.get(), sizeof(SaveContext));
    Context_SetCurrentGame(prevGame);
    if (sFailures != 0) {
        printf("[TEST] FAIL (oot-shuffle-wonder-items): %d failure(s)\n", sFailures);
        return 1;
    }
    printf("[TEST] PASS: oot-shuffle-wonder-items\n");
    return 0;
}

#endif // RSBS_SINGLE_EXECUTABLE
