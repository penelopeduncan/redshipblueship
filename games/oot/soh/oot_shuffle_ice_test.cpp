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
 *  D  THE OPTIONS. Both ship off with their keys cleared and are greyed out with a
 *     reason (every world here is a paired creation, which generates them off: leg
 *     P), and a stored On does not list their checks under Exclude Locations.
 *  F  THE FOLD AT DEFAULT SETTINGS. A real generation with both keys cleared: the
 *     settings fold reads none of the lines #1408 added (each option's own rows, its
 *     checks' exclude rows) and leaves out exactly as many as the option groups hold,
 *     counted here by pointer from the groups and the static location table. The
 *     counts are taken inside the fold loop at the line itself (playthrough.cpp), so
 *     a skip deleted at the call site shows up as a folded line. No icicle or red
 *     ice check is a location.
 *  P  A PAIRED WORLD GENERATES THEM OFF. The same generation with both keyed On: the
 *     generation pins each off, the fold again reads none of their lines, and the
 *     settings fingerprint and fold length are leg F's.
 *
 * RED HALVES, derived, not run (this pass had no build): with the fold's skip removed
 * at its call site F and P fail (lines folded); with a classifier returning false F's
 * and P's left-out count is 0; without the paired pin P fails (pinned 0, the option
 * on, icicle or red ice locations in the pool, a different fingerprint); with the
 * menu greying removed D fails; with the exclude list reading the stored key again
 * (randomizer_check_objects.cpp) D fails; with a row dropped from rcToRandomizerInf
 * M fails.
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
// option's lines, and whether that generation pinned the option off.
extern "C" int OoT_Rando_LastSettingsFold(uint32_t* outTotalLines, uint32_t* outExcludeLines, int groupCap);
extern "C" int OoT_Rando_LastSettingsFoldIcicles(uint32_t* outFolded, uint32_t* outSkipped, int* outPinned);
extern "C" int OoT_Rando_LastSettingsFoldRedIce(uint32_t* outFolded, uint32_t* outSkipped, int* outPinned);

static_assert(RC_GANONS_CASTLE_MQ_WATER_TRIAL_SECOND_DOOR_RED_ICE_5 - RC_ICE_CAVERN_ENTRANCE_LEFT_STALAGMITE + 1 == 191,
              "SoH 49e740b6f adds 191 icicle and red ice checks, contiguous (#1408)");
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
        printf("[TEST] D: '%s' (key %s): %d with no key stored; greyed %d ('%s')\n", option.GetName().c_str(),
               option.GetCVarName().c_str(), shipped, option.IsDisabled() ? 1 : 0, option.GetDisabledText().c_str());
        ICE_CHECK(option.GetName() == kOptions[o].name, "D: the option is named '%s', want '%s'",
                  option.GetName().c_str(), kOptions[o].name);
        ICE_CHECK(option.GetCVarName() == kOptions[o].cvar, "D: %s is not keyed %s", kOptions[o].name,
                  kOptions[o].cvar);
        ICE_CHECK(shipped == RO_GENERIC_OFF, "D: %s's shipped default is %d, not Off", kOptions[o].name, shipped);
        ICE_CHECK(option.IsDisabled() && !option.GetDisabledText().empty(),
                  "D: %s is not greyed out with a reason, though every world here generates it off", kOptions[o].name);
    }

    // A stored On (a preset) must not list the checks under Exclude Locations: the
    // world is generated with the options off (leg P) and the fold leaves those rows
    // out (leg F). Vanilla-quest and both-quest checks are counted, so the default MQ
    // setting (none) does not hide them for another reason.
    for (int o = 0; o < 2; o++) {
        CVarSetInteger(kOptions[o].cvar, RO_GENERIC_ON);
    }
    RandomizerCheckObjects::UpdateImGuiVisibility();
    int counted = 0;
    int listed = 0;
    for (auto& loc : Rando::StaticData::GetLocationTable()) {
        if ((loc.GetRCType() != RCTYPE_ICICLE && loc.GetRCType() != RCTYPE_RED_ICE) || loc.GetQuest() == RCQUEST_MQ) {
            continue;
        }
        counted++;
        if (Rando::Context::GetInstance()->GetItemLocation(loc.GetRandomizerCheck())->IsVisible()) {
            listed++;
        }
    }
    printf("[TEST] D: keyed On, the exclude list shows %d of %d non-MQ icicle and red ice checks\n", listed, counted);
    ICE_CHECK(counted > 0, "D: the location table holds no non-MQ icicle or red ice check to look at");
    ICE_CHECK(listed == 0, "D: keyed On, the exclude list shows %d checks of a world generated off", listed);
    for (int o = 0; o < 2; o++) {
        saved[o].Restore();
    }
    RandomizerCheckObjects::UpdateImGuiVisibility();
}

// ---- F and P: the fold -------------------------------------------------------------------

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
    // If either is 0, F and P below would be asserting nothing about it.
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
    int pinned[2];
    uint8_t option[2];
    uint32_t settingsHash;
    size_t locations;
    size_t iceLocations[2];
};

FoldRecord IceGenerate(const char* leg) {
    FoldRecord r = {};
    const int rc = Rando_HeadlessSeedTest("RSBSUNIFIED1");
    r.generated = rc == 0;
    r.runs = OoT_Rando_LastSettingsFold(&r.total, nullptr, 0);
    (void)OoT_Rando_LastSettingsFoldIcicles(&r.folded[0], &r.skipped[0], &r.pinned[0]);
    (void)OoT_Rando_LastSettingsFoldRedIce(&r.folded[1], &r.skipped[1], &r.pinned[1]);
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
                r.iceLocations[0]++;
            } else if (type == RCTYPE_RED_ICE) {
                r.iceLocations[1]++;
            }
        }
    }
    printf("[TEST] %s: generation rc %d, fold run %d: %u line(s), settingsHash %08X, %zu location(s)\n", leg, rc,
           r.runs, (unsigned)r.total, (unsigned)r.settingsHash, r.locations);
    for (int o = 0; o < 2; o++) {
        printf("[TEST] %s:   %s: lines folded %u / left out %u, pinned %d, option %u, %zu of the locations\n", leg,
               kOptions[o].name, (unsigned)r.folded[o], (unsigned)r.skipped[o], r.pinned[o], (unsigned)r.option[o],
               r.iceLocations[o]);
    }
    return r;
}

void CheckOffWorld(const char* leg, const FoldRecord& r, const uint32_t (&expectedSkipped)[2]) {
    ICE_CHECK(r.generated && r.runs > 0, "%s: the generation failed or ran no settings fold", leg);
    ICE_CHECK(r.locations > 0, "%s: the world has no locations to look at", leg);
    for (int o = 0; o < 2; o++) {
        ICE_CHECK(r.folded[o] == 0,
                  "%s: the settings fold read %u line(s) %s added while the option is off: every default world's "
                  "fingerprint, and so every placement, moves",
                  leg, (unsigned)r.folded[o], kOptions[o].name);
        ICE_CHECK(r.skipped[o] == expectedSkipped[o], "%s: the fold left out %u %s line(s), the option groups hold %u",
                  leg, (unsigned)r.skipped[o], kOptions[o].name, (unsigned)expectedSkipped[o]);
        ICE_CHECK(r.option[o] == RO_GENERIC_OFF, "%s: the world was generated with %s %u", leg, kOptions[o].name,
                  (unsigned)r.option[o]);
        ICE_CHECK(r.iceLocations[o] == 0, "%s: %zu of the world's locations are %s checks", leg, r.iceLocations[o],
                  kOptions[o].name);
    }
}

void CheckFold() {
    IceSavedKey saved[2];
    for (int o = 0; o < 2; o++) {
        saved[o].Save(kOptions[o].cvar);
        CVarClear(kOptions[o].cvar);
    }
    const FoldRecord f = IceGenerate("F");
    const uint32_t expectedF[2] = { CountLines(0), CountLines(1) };
    CheckOffWorld("F", f, expectedF);
    for (int o = 0; o < 2; o++) {
        ICE_CHECK(f.pinned[o] == 0, "F: the generation reports pinning %s, which was already off", kOptions[o].name);
    }

    // Never stored: a stored rando key poisons later rows (Restore below).
    for (int o = 0; o < 2; o++) {
        CVarSetInteger(kOptions[o].cvar, RO_GENERIC_ON);
    }
    const FoldRecord p = IceGenerate("P");
    for (int o = 0; o < 2; o++) {
        saved[o].Restore();
    }
    const uint32_t expectedP[2] = { CountLines(0), CountLines(1) };
    CheckOffWorld("P", p, expectedP);
    for (int o = 0; o < 2; o++) {
        ICE_CHECK(p.pinned[o] == 1, "P: a paired generation with %s keyed On did not pin it off", kOptions[o].name);
    }
    ICE_CHECK(f.generated && p.generated && p.settingsHash == f.settingsHash,
              "P: the pinned world's settings fingerprint %08X is not the default world's %08X",
              (unsigned)p.settingsHash, (unsigned)f.settingsHash);
    ICE_CHECK(p.total == f.total, "P: the pinned world folded %u line(s), the default world %u", (unsigned)p.total,
              (unsigned)f.total);
}

} // namespace

/**
 * @return 0 when every leg holds, 1 otherwise.
 */
extern "C" int OoT_ShuffleIceTest_Run(void) {
    printf("[TEST] oot-shuffle-ice: every icicle and red ice check has its own flag, both options ship off and "
           "greyed out, default worlds fold none of their lines, and a paired world generates them off (#1408)\n");
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
