/**
 * @file oot_shuffle_ice_test.cpp
 * @brief #1408: Ship of Harkinian's Shuffle Icicles and Shuffle Red Ice (SoH 49e740b6f,
 *        68d4c6049) in Ocarina of Time's randomizer (Enhancements/randomizer/
 *        ShuffleIcicles.cpp, ShuffleRedIce.cpp). Row OoTShuffleIce (CTest label
 *        `rando`), dispatch `oot-shuffle-ice` in src/common/test_runner.cpp; the
 *        bring-up is oot-shuffle-silver's (OTRGlobals' randomizer context). No ROM:
 *        the generations run in whatever archive set the build directory holds, as
 *        rando-settings-fold-excludes'.
 *
 *  M  THE FLAG MAP. Every RCTYPE_ICICLE and RCTYPE_RED_ICE check (the 191 contiguous
 *     ids from RC_ICE_CAVERN_ENTRANCE_LEFT_STALAGMITE to
 *     RC_GANONS_CASTLE_MQ_WATER_TRIAL_SECOND_DOOR_RED_ICE_5) is in the location table
 *     with one of the two types, has a RandomizerInf of its own in rcToRandomizerInf
 *     (not RAND_INF_MAX, not shared, the flag its spoiler collection check names), and
 *     that flag maps back to it.
 *  D  THE OPTIONS. Both ship off with their keys cleared, are enabled (not greyed out:
 *     the operator ruled both stay live in paired worlds), and Exclude Locations hides
 *     their non-MQ checks when their keys are Off and shows them when On.
 *  F  THE FOLD AT DEFAULT SETTINGS. A real generation with both keys cleared: the
 *     settings fold reads none of the lines #1408 added (each option's own rows, its
 *     checks' exclude rows) and leaves out exactly as many as the option groups hold,
 *     counted here by pointer from the groups and the static location table. The
 *     counts are taken inside the fold loop at the line itself (playthrough.cpp), so
 *     a skip deleted at the call site shows up as a folded line. No icicle or red
 *     ice check is a location.
 *  O  THE OPTIONS ON IN A PAIRED WORLD. Neither option is pinned off in a paired
 *     world: the same generation with both keyed On succeeds, keeps both On, folds
 *     every one of their lines and leaves none out (so its fingerprint differs from
 *     leg F's), and selects the exact quest-matching icicle and red ice checks into
 *     the world's location pool.
 *
 * RED HALVES, derived, not run (this pass had no build): with a row dropped from
 * rcToRandomizerInf M fails; with the menu greying restored D fails; with the exclude
 * list visibility broken (always false or always true) D fails; with the fold's skip
 * removed at its call site F fails (lines folded); with a classifier returning false
 * F's left-out count is 0; with a paired pin in Playthrough_Init O fails (option Off,
 * checks absent, fingerprint unchanged).
 *
 * Every key a leg changes is put back (in memory: nothing here writes a config file).
 */
#ifdef RSBS_SINGLE_EXECUTABLE

#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <set>

#include <libultraship/bridge.h>

#include "soh/OTRGlobals.h"
#include "soh/cvar_prefixes.h"
#include "soh/Enhancements/randomizer/randomizerTypes.h"
#include "soh/Enhancements/randomizer/randomizer.h"
#include "soh/Enhancements/randomizer/static_data.h"
#include "soh/Enhancements/randomizer/SeedContext.h"
#include "soh/Enhancements/randomizer/settings.h"
#include "soh/Enhancements/randomizer/item_location.h"
#include "soh/Enhancements/randomizer/dungeon.h"
#include "soh/Enhancements/randomizer/randomizer_check_objects.h"

#include "context.h" // src/common: gComboCtx, Context_GetCurrentGame, Context_SetCurrentGame
#include "game.h"

// At file scope, not inside an extern "C" block (see oot_shared_kokiri_sword_test.cpp).
#include <z64.h>
extern "C" {
extern SaveContext gSaveContext;
}

// 3drando/menu.cpp: the headless generation bridge every rando row uses.
extern "C" int Rando_HeadlessSeedTest(const char* seedStr);
// 3drando/playthrough.cpp: what the last settings fold folded, in total and of each
// option's lines.
extern "C" int OoT_Rando_LastSettingsFold(uint32_t* outTotalLines, uint32_t* outExcludeLines, int groupCap);
extern "C" int OoT_Rando_LastSettingsFoldIcicles(uint32_t* outFolded, uint32_t* outSkipped);
extern "C" int OoT_Rando_LastSettingsFoldRedIce(uint32_t* outFolded, uint32_t* outSkipped);

// THE APPEND-ONLY LOCKS: each id #1408 adds sits after #1405's last id, so no id a
// world or a save already holds moves. The numbers themselves are locked in
// GameExports_SingleExe.cpp.
static_assert(RC_ICE_CAVERN_ENTRANCE_LEFT_STALAGMITE == RC_GANONS_CASTLE_MQ_WONDER_SHADOW_TRIAL + 1,
              "the icicle and red ice checks are appended after #1405's wonder item checks (#1408)");
static_assert(RC_GANONS_CASTLE_MQ_WATER_TRIAL_SECOND_DOOR_RED_ICE_5 - RC_ICE_CAVERN_ENTRANCE_LEFT_STALAGMITE + 1 == 191,
              "SoH 49e740b6f adds 191 icicle and red ice checks, contiguous (#1408)");
static_assert(RAND_INF_ICE_CAVERN_ENTRANCE_LEFT_STALAGMITE == RAND_INF_GANONS_CASTLE_MQ_WONDER_SHADOW_TRIAL + 1,
              "the icicle and red ice flags are appended after #1405's wonder item flags (#1408)");
static_assert(RSK_SHUFFLE_ICICLES == RSK_SHUFFLE_WONDER_ITEMS + 1 &&
                  RSK_SHUFFLE_RED_ICE == RSK_SHUFFLE_ICICLES + 1,
              "RSK_SHUFFLE_ICICLES and RSK_SHUFFLE_RED_ICE are appended after #1405's key (#1408)");
static_assert(RO_GENERIC_OFF == 0, "#1408: a world saved before the options reads their missing entries as 0, Off");

namespace {

int sFailures = 0;

void IceFail(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    printf("[TEST] oot-shuffle-ice: FAIL: ");
    vprintf(fmt, args);
    printf("\n");
    va_end(args);
    sFailures++;
}

#define ICE_CHECK(cond, ...)      \
    do {                          \
        if (!(cond)) {            \
            IceFail(__VA_ARGS__); \
        }                         \
    } while (0)

/** One of the two options under test. */
struct IceOption {
    RandomizerSettingKey key;
    RandomizerCheckType type;
    const char* cvar;
    const char* name;
};

const IceOption kOptions[2] = {
    { RSK_SHUFFLE_ICICLES, RCTYPE_ICICLE, CVAR_RANDOMIZER_SETTING("ShuffleIcicles"), "Shuffle Icicles" },
    { RSK_SHUFFLE_RED_ICE, RCTYPE_RED_ICE, CVAR_RANDOMIZER_SETTING("ShuffleRedIce"), "Shuffle Red Ice" },
};

/** A key's stored state, put back by Restore. */
struct IceSavedKey {
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
        IceFail("M: needs OTRGlobals' randomizer (the flag map is its rcToRandomizerInf)");
        return;
    }
    int checks[2] = { 0, 0 };
    int wrongType = 0;
    int unmapped = 0;
    int shared = 0;
    int wrongFlag = 0;
    int noReverse = 0;
    std::set<int> flags;
    for (int c = RC_ICE_CAVERN_ENTRANCE_LEFT_STALAGMITE; c <= RC_GANONS_CASTLE_MQ_WATER_TRIAL_SECOND_DOOR_RED_ICE_5;
         c++) {
        const RandomizerCheck rc = (RandomizerCheck)c;
        Rando::Location* loc = Rando::StaticData::GetLocation(rc);
        if (loc->GetRandomizerCheck() != rc ||
            (loc->GetRCType() != RCTYPE_ICICLE && loc->GetRCType() != RCTYPE_RED_ICE)) {
            printf("[TEST]   M: check %d is not in the location table as an icicle or red ice check (type %d)\n", c,
                   (int)loc->GetRCType());
            wrongType++;
            continue;
        }
        checks[loc->GetRCType() == RCTYPE_ICICLE ? 0 : 1]++;
        const RandomizerInf inf = randomizer->GetRandomizerInfFromCheck(rc);
        if (inf == RAND_INF_MAX || (int)inf < 0 || (int)inf > (int)RAND_INF_MAX) {
            printf("[TEST]   M: %s has no flag of its own (%d)\n", loc->GetName().c_str(), (int)inf);
            unmapped++;
            continue;
        }
        if (!flags.insert((int)inf).second) {
            printf("[TEST]   M: %s shares flag %d with another #1408 check\n", loc->GetName().c_str(), (int)inf);
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
    }
    printf("[TEST] M: %d icicle and %d red ice check(s): %d mistyped, %d without a flag, %d sharing one, %d "
           "disagreeing with their collection check, %d not mapping back\n",
           checks[0], checks[1], wrongType, unmapped, shared, wrongFlag, noReverse);
    ICE_CHECK(checks[0] > 0 && checks[1] > 0 && checks[0] + checks[1] == 191,
              "M: %d icicle and %d red ice checks of the 191 are in the location table", checks[0], checks[1]);
    ICE_CHECK(wrongType == 0, "M: %d id(s) in the #1408 range are not icicle or red ice checks", wrongType);
    ICE_CHECK(unmapped == 0, "M: %d check(s) have no RandomizerInf", unmapped);
    ICE_CHECK(shared == 0, "M: %d check(s) share a RandomizerInf", shared);
    ICE_CHECK(wrongFlag == 0, "M: %d check(s) map to a flag their collection check does not name", wrongFlag);
    ICE_CHECK(noReverse == 0, "M: %d flag(s) do not map back to their check", noReverse);
}

// ---- D: the options -------------------------------------------------------------------

void CountExcludeVisibility(int (&counted)[2], int (&listed)[2]) {
    counted[0] = counted[1] = 0;
    listed[0] = listed[1] = 0;
    RandomizerCheckObjects::UpdateImGuiVisibility();
    for (auto& loc : Rando::StaticData::GetLocationTable()) {
        if ((loc.GetRCType() != RCTYPE_ICICLE && loc.GetRCType() != RCTYPE_RED_ICE) ||
            loc.GetQuest() == RCQUEST_MQ) {
            continue;
        }
        const int idx = loc.GetRCType() == RCTYPE_ICICLE ? 0 : 1;
        counted[idx]++;
        if (Rando::Context::GetInstance()->GetItemLocation(loc.GetRandomizerCheck())->IsVisible()) {
            listed[idx]++;
        }
    }
}

void CheckOptions() {
    auto settings = Rando::Settings::GetInstance();
    settings->CreateOptions(); // idempotent; what Rando_HeadlessSeedTest calls first
    IceSavedKey saved[2];
    for (int o = 0; o < 2; o++) {
        saved[o].Save(kOptions[o].cvar);
        CVarClear(kOptions[o].cvar);
    }
    for (int o = 0; o < 2; o++) {
        Rando::Option& option = settings->GetOption(kOptions[o].key);
        const int shipped = option.GetOptionIndex();
        printf("[TEST] D: '%s' (key %s): %d with no key stored, %zu choice(s); greyed %d\n",
               option.GetName().c_str(), option.GetCVarName().c_str(), shipped, (size_t)option.GetOptionCount(),
               option.IsDisabled() ? 1 : 0);
        ICE_CHECK(option.GetName() == kOptions[o].name, "D: the option is named '%s', want '%s'",
                  option.GetName().c_str(), kOptions[o].name);
        ICE_CHECK(option.GetCVarName() == kOptions[o].cvar, "D: %s is not keyed %s", kOptions[o].name,
                  kOptions[o].cvar);
        ICE_CHECK(shipped == RO_GENERIC_OFF, "D: %s's shipped default is %d, not Off", kOptions[o].name, shipped);
        ICE_CHECK(option.GetOptionCount() == (size_t)RO_GENERIC_ON + 1, "D: %s has %zu choice(s), want %d",
                  kOptions[o].name, (size_t)option.GetOptionCount(), (int)RO_GENERIC_ON + 1);
        ICE_CHECK(!option.IsDisabled(), "D: %s is greyed out ('%s'), though paired worlds keep it live",
                  kOptions[o].name, option.GetDisabledText().c_str());
    }

    // Exclude Locations hides non-MQ icicle/red ice rows when their CVar is Off and
    // shows them independently when On. Non-MQ checks are counted so the default MQ
    // setting (none) does not hide them for another reason.
    int counted[2] = {};
    int listedOff[2] = {};
    CountExcludeVisibility(counted, listedOff);
    printf("[TEST] D: cleared, exclude list shows %d/%d non-MQ icicle and %d/%d red ice checks\n", listedOff[0],
           counted[0], listedOff[1], counted[1]);
    ICE_CHECK(counted[0] > 0 && counted[1] > 0, "D: missing non-MQ icicle (%d) or red ice (%d) checks", counted[0],
              counted[1]);
    ICE_CHECK(listedOff[0] == 0 && listedOff[1] == 0,
              "D: cleared, exclude list shows %d icicle and %d red ice check(s), want none", listedOff[0],
              listedOff[1]);

    CVarSetInteger(kOptions[0].cvar, RO_GENERIC_ON);
    CVarClear(kOptions[1].cvar);
    int listedIcicleOnly[2] = {};
    CountExcludeVisibility(counted, listedIcicleOnly);
    ICE_CHECK(listedIcicleOnly[0] == counted[0] && listedIcicleOnly[1] == 0,
              "D: Shuffle Icicles On only: visible %d/%d icicles, %d/%d red ice", listedIcicleOnly[0], counted[0],
              listedIcicleOnly[1], counted[1]);

    CVarClear(kOptions[0].cvar);
    CVarSetInteger(kOptions[1].cvar, RO_GENERIC_ON);
    int listedRedIceOnly[2] = {};
    CountExcludeVisibility(counted, listedRedIceOnly);
    ICE_CHECK(listedRedIceOnly[0] == 0 && listedRedIceOnly[1] == counted[1],
              "D: Shuffle Red Ice On only: visible %d/%d icicles, %d/%d red ice", listedRedIceOnly[0], counted[0],
              listedRedIceOnly[1], counted[1]);

    CVarSetInteger(kOptions[0].cvar, RO_GENERIC_ON);
    CVarSetInteger(kOptions[1].cvar, RO_GENERIC_ON);
    int listedOn[2] = {};
    CountExcludeVisibility(counted, listedOn);
    printf("[TEST] D: both keyed On, exclude list shows %d/%d non-MQ icicle and %d/%d red ice checks\n", listedOn[0],
           counted[0], listedOn[1], counted[1]);
    ICE_CHECK(listedOn[0] == counted[0] && listedOn[1] == counted[1],
              "D: both On, exclude list shows %d/%d icicle and %d/%d red ice checks", listedOn[0], counted[0],
              listedOn[1], counted[1]);

    for (int o = 0; o < 2; o++) {
        saved[o].Restore();
    }
    RandomizerCheckObjects::UpdateImGuiVisibility();
}

// ---- F and O: the fold -------------------------------------------------------------------

/** The fold lines #1408 added for option @p o, counted from the option groups by
 *  pointer: what the fold must leave out while the option is off. Same walk as the
 *  fold (every Setting with options, in every group that is not a group of groups). */
uint32_t CountLines(int o) {
    auto settings = Rando::Settings::GetInstance();
    std::set<const Rando::Option*> excludeRows;
    for (auto& loc : Rando::StaticData::GetLocationTable()) {
        if (loc.GetRCType() == kOptions[o].type) {
            excludeRows.insert(loc.GetExcludedOption());
        }
    }
    const Rando::Option* shuffle = &settings->GetOption(kOptions[o].key);
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
    printf("[TEST] the option groups hold %u %s line(s): %u exclude row(s) (%zu distinct options), %u row(s) of the "
           "option itself\n",
           (unsigned)(excludes + optionRows), kOptions[o].name, (unsigned)excludes, excludeRows.size(),
           (unsigned)optionRows);
    // If either is 0, F and O below would be asserting nothing about it.
    ICE_CHECK(excludes > 0, "the exclude groups hold none of %s's checks' rows", kOptions[o].name);
    ICE_CHECK(optionRows > 0, "no folded group holds %s's own row", kOptions[o].name);
    return excludes + optionRows;
}

struct FoldRecord {
    bool generated;
    int runs;
    uint32_t total;
    uint32_t folded[2];
    uint32_t skipped[2];
    uint8_t option[2];
    uint32_t settingsHash;
    size_t locations;
    std::set<RandomizerCheck> actual[2];
    std::set<RandomizerCheck> expected[2];
};

FoldRecord IceGenerate(const char* leg) {
    FoldRecord r = {};
    const int rc = Rando_HeadlessSeedTest("RSBSUNIFIED1");
    r.generated = rc == 0;
    r.runs = OoT_Rando_LastSettingsFold(&r.total, nullptr, 0);
    (void)OoT_Rando_LastSettingsFoldIcicles(&r.folded[0], &r.skipped[0]);
    (void)OoT_Rando_LastSettingsFoldRedIce(&r.folded[1], &r.skipped[1]);
    r.settingsHash = gComboCtx.sharedRandoSettingsHash;
    auto world = Rando::Context::GetInstance();
    if (world != nullptr) {
        r.locations = world->allLocations.size();
        for (int o = 0; o < 2; o++) {
            r.option[o] = world->GetOption(kOptions[o].key).Get();
        }
        for (RandomizerCheck check : world->allLocations) {
            const RandomizerCheckType type = Rando::StaticData::GetLocation(check)->GetRCType();
            if (type == RCTYPE_ICICLE) {
                r.actual[0].insert(check);
            } else if (type == RCTYPE_RED_ICE) {
                r.actual[1].insert(check);
            }
        }
        // What the world's dungeon quests select under On (Context::GenerateLocationPool's rule).
        for (int c = RC_ICE_CAVERN_ENTRANCE_LEFT_STALAGMITE;
             c <= RC_GANONS_CASTLE_MQ_WATER_TRIAL_SECOND_DOOR_RED_ICE_5; c++) {
            Rando::Location* loc = Rando::StaticData::GetLocation((RandomizerCheck)c);
            const int o = loc->GetRCType() == RCTYPE_ICICLE ? 0 : 1;
            if (loc->IsOverworld()) {
                r.expected[o].insert((RandomizerCheck)c);
                continue;
            }
            auto* dungeon = world->GetDungeon(loc->GetArea() - RCAREA_DEKU_TREE);
            if (dungeon != nullptr &&
                (loc->GetQuest() == RCQUEST_BOTH || (loc->GetQuest() == RCQUEST_MQ) == dungeon->IsMQ())) {
                r.expected[o].insert((RandomizerCheck)c);
            }
        }
    }
    printf("[TEST] %s: generation rc %d, fold run %d: %u line(s), settingsHash %08X, %zu location(s)\n", leg, rc,
           r.runs, (unsigned)r.total, (unsigned)r.settingsHash, r.locations);
    for (int o = 0; o < 2; o++) {
        printf("[TEST] %s:   %s: lines folded %u / left out %u, option %u, %zu of %zu selected location(s)\n", leg,
               kOptions[o].name, (unsigned)r.folded[o], (unsigned)r.skipped[o], (unsigned)r.option[o],
               r.actual[o].size(), r.expected[o].size());
    }
    return r;
}

void CheckFold() {
    IceSavedKey saved[2];
    for (int o = 0; o < 2; o++) {
        saved[o].Save(kOptions[o].cvar);
        CVarClear(kOptions[o].cvar);
    }
    const FoldRecord f = IceGenerate("F");
    const uint32_t expected[2] = { CountLines(0), CountLines(1) };
    ICE_CHECK(f.generated && f.runs > 0, "F: the generation failed or ran no settings fold");
    ICE_CHECK(f.locations > 0, "F: the world has no locations to look at");
    for (int o = 0; o < 2; o++) {
        ICE_CHECK(f.folded[o] == 0,
                  "F: the settings fold read %u line(s) %s added while the option is off: every default world's "
                  "fingerprint, and so every placement, moves",
                  (unsigned)f.folded[o], kOptions[o].name);
        ICE_CHECK(f.skipped[o] == expected[o], "F: the fold left out %u %s line(s), the option groups hold %u",
                  (unsigned)f.skipped[o], kOptions[o].name, (unsigned)expected[o]);
        ICE_CHECK(f.option[o] == RO_GENERIC_OFF, "F: the world was generated with %s %u", kOptions[o].name,
                  (unsigned)f.option[o]);
        ICE_CHECK(f.actual[o].empty(), "F: %zu of the world's locations are %s checks", f.actual[o].size(),
                  kOptions[o].name);
    }

    // Never stored: a stored rando key poisons later rows (Restore below).
    for (int o = 0; o < 2; o++) {
        CVarSetInteger(kOptions[o].cvar, RO_GENERIC_ON);
    }
    const FoldRecord on = IceGenerate("O");
    for (int o = 0; o < 2; o++) {
        saved[o].Restore();
    }
    ICE_CHECK(on.generated && on.runs > f.runs, "O: the paired generation with both options on failed");
    for (int o = 0; o < 2; o++) {
        ICE_CHECK(on.option[o] == RO_GENERIC_ON, "O: a paired world keyed On generated %s as %u (pinned off?)",
                  kOptions[o].name, (unsigned)on.option[o]);
        ICE_CHECK(on.folded[o] == expected[o] && on.skipped[o] == 0,
                  "O: with %s on the fold read %u and left out %u of %u line(s)", kOptions[o].name,
                  (unsigned)on.folded[o], (unsigned)on.skipped[o], (unsigned)expected[o]);
        ICE_CHECK(!on.expected[o].empty() && on.actual[o] == on.expected[o],
                  "O: %s added %zu location(s), the world's quests select %zu", kOptions[o].name,
                  on.actual[o].size(), on.expected[o].size());
    }
    ICE_CHECK(on.settingsHash != f.settingsHash,
              "O: the world with both options on has the default world's fingerprint %08X", (unsigned)f.settingsHash);
    // Not asserted here: a paired generation stops at OoT's general pass, before the single bag places
    // anything, so the icicle/red ice checks are still empty here. That the finished world holds an item on
    // every one of them is OoTIcePaired's leg B.
}

} // namespace

/**
 * @return 0 when every leg holds, 1 otherwise.
 */
extern "C" int OoT_ShuffleIceTest_Run(void) {
    printf("[TEST] oot-shuffle-ice: every icicle and red ice check has its own flag, both options ship off and "
           "enabled, default worlds fold none of their lines, and a paired world with both on keeps them on and "
           "selects every quest-matching check (#1408)\n");
    if (OTRGlobals::Instance == nullptr || OTRGlobals::Instance->gRandoContext == nullptr) {
        printf("[TEST] FAIL: oot-shuffle-ice needs OTRGlobals' randomizer context\n");
        return 1;
    }
    sFailures = 0;
    const GameId prevGame = Context_GetCurrentGame();
    Context_SetCurrentGame(GAME_OOT);
    std::unique_ptr<SaveContext> priorSave = std::make_unique<SaveContext>();
    std::memcpy(priorSave.get(), &gSaveContext, sizeof(SaveContext));

    CheckFlagMap();
    CheckOptions();
    CheckFold();

    std::memcpy(&gSaveContext, priorSave.get(), sizeof(SaveContext));
    Context_SetCurrentGame(prevGame);
    if (sFailures != 0) {
        printf("[TEST] FAIL (oot-shuffle-ice): %d failure(s)\n", sFailures);
        return 1;
    }
    printf("[TEST] PASS: oot-shuffle-ice\n");
    return 0;
}

#endif // RSBS_SINGLE_EXECUTABLE
