/**
 * @file oot_shuffle_silver_test.cpp
 * @brief #1363: Ship of Harkinian's Shuffle Silver Rupees (SoH b76e303ce) in Ocarina
 *        of Time's randomizer (Enhancements/randomizer/ShuffleSilver.cpp). Row
 *        OoTShuffleSilver (CTest label `rando`), dispatch `oot-shuffle-silver` in
 *        src/common/test_runner.cpp; the bring-up is oot-time-travel-sword's
 *        (OTRGlobals' randomizer context). No ROM: the generations run in whatever
 *        archive set the build directory holds, as rando-settings-fold-excludes'.
 *
 *  M  THE FLAG MAP. Every RCTYPE_SILVER check (the 154 contiguous ids from
 *     RC_SHADOW_N_UNDER_BLADE_SILVER to RC_GANONS_CASTLE_MQ_MOVING_PLATFORM_SHADOW_SILVER)
 *     is in the location table as a silver check, has a RandomizerInf of its own in
 *     rcToRandomizerInf (Randomizer::GetRandomizerInfFromCheck: not RAND_INF_MAX, not
 *     shared with another silver check, and the flag its spoiler collection check
 *     names), and that flag maps back to it (GetCheckFromRandomizerInf). Every
 *     vanilla-quest silver check is also identified from its own actor position by
 *     the real IdentifySilver, the lookup the silver rupee hooks run, which returns
 *     the check and its flag.
 *  D  THE OPTION. Shuffle Silver Rupees ships off with its key cleared and is greyed
 *     out with a reason (every world here is a paired creation, which generates it
 *     off: leg P).
 *  F  THE FOLD AT DEFAULT SETTINGS. A real generation with the option's key cleared:
 *     the settings fold reads none of the lines #1363 added (the option's own rows,
 *     the RCTYPE_SILVER exclude rows) and leaves out exactly as many as the option
 *     groups hold, counted here by pointer from the groups and the static location
 *     table. The counts are taken inside the fold loop at the line itself
 *     (playthrough.cpp), so a skip deleted at the call site shows up as a folded
 *     line. No silver check is a location and no silver rupee is placed.
 *  P  A PAIRED WORLD GENERATES IT OFF. The same generation with the option keyed On:
 *     the generation pins it off, the fold again reads none of its lines, and the
 *     settings fingerprint and fold length are leg F's.
 *  L  THE LOGIC FALLBACK. With the option off, Logic::HasItem answers each silver
 *     rupee item from its own LOGIC_* event (logic.cpp: LOGIC_SHADOW_SILVER_BLADES +
 *     the item's offset from RG_SHADOW_SILVER_BLADES). For each of the 22 items,
 *     with exactly its event set (named here, not computed), HasItem answers true
 *     for that item and false for the other 21.
 *
 * RED HALVES, derived, not run (this lane had no build): with the 154 silver rows
 * removed from rcToRandomizerInf M fails (RAND_INF_MAX for every check, and
 * IdentifySilver finds no check); with two rows given one flag M fails (shared);
 * with RegisterSilverLocations dropped from InitLocationTable, M's IdentifySilver
 * leg fails wherever the actor map was built before ShipInit registered the checks;
 * with the fold's skip removed at its call site F and P fail (lines folded); with the
 * classifier returning false F's and P's left-out count is 0; without the paired
 * pin P fails (pinned 0, the option on, a different fingerprint); with the menu
 * greying removed D fails; with the exclude list reading the stored ShuffleSilver
 * key again (randomizer_check_objects.cpp) D fails (silver rows listed while keyed
 * On); with the LOGIC_* silver events reordered against the
 * RG_* items (or one removed) L fails for the items past the change.
 *
 * Every key, option, logic value and save field a leg changes is put back (in
 * memory: nothing here writes a config file).
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
#include "soh/Enhancements/randomizer/randomizer_check_objects.h"

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
// Shuffle Silver Rupees' lines, and whether that generation pinned it off.
extern "C" int OoT_Rando_LastSettingsFold(uint32_t* outTotalLines, uint32_t* outExcludeLines, int groupCap);
extern "C" int OoT_Rando_LastSettingsFoldSilver(uint32_t* outFolded, uint32_t* outSkipped, int* outPinned);

// THE APPEND-ONLY LOCKS: each id #1363 adds sits after the last id main had, so no
// id a world or a save already holds moves.
static_assert(RC_SHADOW_N_UNDER_BLADE_SILVER == RC_DEKU_TREE_QUEEN_GOHMA_GRASS_8 + 1,
              "the silver checks are appended after every check id main had (#1363)");
static_assert(RC_GANONS_CASTLE_MQ_MOVING_PLATFORM_SHADOW_SILVER - RC_SHADOW_N_UNDER_BLADE_SILVER + 1 == 154,
              "SoH b76e303ce adds 154 silver rupee checks, contiguous (#1363)");
static_assert(RG_SHADOW_SILVER_BLADES == RG_NUTS + 1, "the silver rupee items are appended after RG_NUTS (#1363)");
static_assert(RG_GANONS_CASTLE_MQ_SILVER_SHADOW - RG_SHADOW_SILVER_BLADES + 1 == 22,
              "SoH b76e303ce adds 22 silver rupee items, contiguous (#1363)");
static_assert(RAND_INF_RC_SHADOW_N_UNDER_BLADE_SILVER == RAND_INF_OBTAINED_SONG_OF_SOARING_OOT + 1,
              "the silver flags are appended after every flag main had (#1363)");
static_assert(RSK_SHUFFLE_SILVER == RSK_TIME_TRAVEL_REQUIRES_MASTER_SWORD + 1,
              "RSK_SHUFFLE_SILVER is appended after every setting key main had (#1363)");
static_assert(RO_SHUFFLE_SILVER_OFF == 0, "#1363: a world saved before the option reads its missing entry as 0, Off");

namespace {

int sFailures = 0;

void SrFail(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    printf("[TEST] oot-shuffle-silver: FAIL: ");
    vprintf(fmt, args);
    printf("\n");
    va_end(args);
    sFailures++;
}

#define SR_CHECK(cond, ...)      \
    do {                         \
        if (!(cond)) {           \
            SrFail(__VA_ARGS__); \
        }                        \
    } while (0)

const char* const kShuffleKey = CVAR_RANDOMIZER_SETTING("ShuffleSilver");

/** A key's stored state, put back by Restore. */
struct SavedKey {
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
        SrFail("M: needs OTRGlobals' randomizer (the flag map is its rcToRandomizerInf)");
        return;
    }
    if (Rando::StaticData::CheckFromActorMultimap.empty()) {
        Rando::StaticData::InitHashMaps();
    }
    int silverChecks = 0;
    int unmapped = 0;
    int shared = 0;
    int wrongFlag = 0;
    int noReverse = 0;
    int identifyTried = 0;
    int identifyWrong = 0;
    std::set<int> flags;
    PlayState* const priorPlay = OoT_gPlayState;
    OoT_gPlayState = nullptr; // ResourceMgr_IsGameMasterQuest() reads vanilla with no play state
    for (int c = RC_SHADOW_N_UNDER_BLADE_SILVER; c <= RC_GANONS_CASTLE_MQ_MOVING_PLATFORM_SHADOW_SILVER; c++) {
        const RandomizerCheck rc = (RandomizerCheck)c;
        Rando::Location* loc = Rando::StaticData::GetLocation(rc);
        if (loc->GetRandomizerCheck() != rc || loc->GetRCType() != RCTYPE_SILVER) {
            SrFail("M: check %d is not in the location table as a silver check (type %d)", c, (int)loc->GetRCType());
            continue;
        }
        silverChecks++;
        const RandomizerInf inf = randomizer->GetRandomizerInfFromCheck(rc);
        if (inf == RAND_INF_MAX || (int)inf < 0 || (int)inf > (int)RAND_INF_MAX) {
            printf("[TEST]   M: %s has no flag of its own (%d)\n", loc->GetName().c_str(), (int)inf);
            unmapped++;
            continue;
        }
        if (!flags.insert((int)inf).second) {
            printf("[TEST]   M: %s shares flag %d with another silver check\n", loc->GetName().c_str(), (int)inf);
            shared++;
        }
        const SpoilerCollectionCheck collection = loc->GetCollectionCheck();
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
            continue; // IdentifySilver keeps an MQ check only in an MQ scene
        }
        // The actor position the check is keyed by (TWO_ACTOR_PARAMS(x, z)); y = z
        // too, so the Shadow Temple's one (x, y)-keyed rupee is found either way.
        const int32_t params = loc->GetActorParams();
        Vec3f pos;
        pos.x = (float)(int16_t)((params >> 16) & 0xFFFF);
        pos.z = (float)(int16_t)(params & 0xFFFF);
        pos.y = pos.z;
        const CheckIdentity id = randomizer->IdentifySilver((s32)loc->GetScene(), pos);
        identifyTried++;
        if (id.randomizerCheck != rc || id.randomizerInf != inf) {
            printf("[TEST]   M: IdentifySilver at %s's position found check %d flag %d\n", loc->GetName().c_str(),
                   (int)id.randomizerCheck, (int)id.randomizerInf);
            identifyWrong++;
        }
    }
    OoT_gPlayState = priorPlay;
    printf("[TEST] M: %d silver check(s): %d without a flag, %d sharing one, %d disagreeing with their collection "
           "check, %d not mapping back; IdentifySilver %d of %d wrong\n",
           silverChecks, unmapped, shared, wrongFlag, noReverse, identifyWrong, identifyTried);
    SR_CHECK(silverChecks == 154, "M: %d of the 154 silver checks are in the location table", silverChecks);
    SR_CHECK(unmapped == 0, "M: %d silver check(s) have no RandomizerInf (they would share flag 0 or none)", unmapped);
    SR_CHECK(shared == 0, "M: %d silver check(s) share a RandomizerInf", shared);
    SR_CHECK(wrongFlag == 0, "M: %d silver check(s) map to a flag their collection check does not name", wrongFlag);
    SR_CHECK(noReverse == 0, "M: %d silver flag(s) do not map back to their check", noReverse);
    SR_CHECK(identifyTried > 0 && identifyWrong == 0,
             "M: IdentifySilver identified %d of %d vanilla silver checks wrongly", identifyWrong, identifyTried);
}

// ---- D: the option -------------------------------------------------------------------

void CheckOption() {
    auto settings = Rando::Settings::GetInstance();
    settings->CreateOptions(); // idempotent; what Rando_HeadlessSeedTest calls first
    SavedKey shuffle;
    shuffle.Save(kShuffleKey);
    CVarClear(kShuffleKey);
    Rando::Option& option = settings->GetOption(RSK_SHUFFLE_SILVER);
    const int shipped = option.GetOptionIndex();
    printf("[TEST] D: '%s' (key %s): %d with no key stored; greyed %d ('%s')\n", option.GetName().c_str(),
           option.GetCVarName().c_str(), shipped, option.IsDisabled() ? 1 : 0, option.GetDisabledText().c_str());
    SR_CHECK(option.GetName() == "Shuffle Silver Rupees", "D: the option is named '%s'", option.GetName().c_str());
    SR_CHECK(option.GetCVarName() == kShuffleKey, "D: the option is not keyed %s", kShuffleKey);
    SR_CHECK(shipped == RO_SHUFFLE_SILVER_OFF, "D: the shipped default is %d, not Off", shipped);
    SR_CHECK(option.IsDisabled() && !option.GetDisabledText().empty(),
             "D: the option is not greyed out with a reason, though every world here generates it off");

    // A stored On (an older build, a preset) must not list the silver checks under
    // Exclude Locations: the world is generated with the option off (leg P) and the
    // fold leaves those rows out (leg F). Vanilla-quest checks are counted, so the
    // default MQ setting (none) does not hide them for another reason.
    CVarSetInteger(kShuffleKey, RO_SHUFFLE_SILVER_ON);
    RandomizerCheckObjects::UpdateImGuiVisibility();
    int vanillaSilver = 0;
    int listedSilver = 0;
    for (auto& loc : Rando::StaticData::GetLocationTable()) {
        if (loc.GetRCType() != RCTYPE_SILVER || loc.GetQuest() != RCQUEST_VANILLA) {
            continue;
        }
        vanillaSilver++;
        if (Rando::Context::GetInstance()->GetItemLocation(loc.GetRandomizerCheck())->IsVisible()) {
            listedSilver++;
        }
    }
    printf("[TEST] D: keyed On, the exclude list shows %d of %d vanilla silver checks\n", listedSilver, vanillaSilver);
    SR_CHECK(vanillaSilver > 0, "D: the location table holds no vanilla silver check to look at");
    SR_CHECK(listedSilver == 0, "D: keyed On, the exclude list shows %d silver checks of a world generated off",
             listedSilver);
    shuffle.Restore();
    RandomizerCheckObjects::UpdateImGuiVisibility();
}

// ---- F and P: the fold -------------------------------------------------------------------

/** The fold lines #1363 added, counted from the option groups by pointer: what the
 *  fold must leave out while the option is off. Same walk as the fold (every
 *  Setting with options, in every group that is not a group of groups). */
uint32_t CountSilverLines() {
    auto settings = Rando::Settings::GetInstance();
    std::set<const Rando::Option*> excludeRows;
    for (auto& loc : Rando::StaticData::GetLocationTable()) {
        if (loc.GetRCType() == RCTYPE_SILVER) {
            excludeRows.insert(loc.GetExcludedOption());
        }
    }
    const Rando::Option* shuffle = &settings->GetOption(RSK_SHUFFLE_SILVER);
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
    printf("[TEST] the option groups hold %u #1363 line(s): %u silver exclude row(s) (%zu distinct options), %u "
           "row(s) of the option itself\n",
           (unsigned)(excludes + optionRows), (unsigned)excludes, excludeRows.size(), (unsigned)optionRows);
    // If either is 0, F and P below would be asserting nothing about it.
    SR_CHECK(excludes > 0, "the exclude groups hold none of the silver checks' rows");
    SR_CHECK(optionRows > 0, "no folded group holds the option's own row");
    return excludes + optionRows;
}

struct FoldRecord {
    bool generated;
    int runs;
    uint32_t total;
    uint32_t folded;
    uint32_t skipped;
    int pinned;
    uint32_t settingsHash;
    uint8_t shuffleOption;
    size_t locations;
    size_t silverLocations;
    int silverPlaced;
};

FoldRecord Generate(const char* leg) {
    FoldRecord r = {};
    const int rc = Rando_HeadlessSeedTest("RSBSUNIFIED1");
    r.generated = rc == 0;
    r.runs = OoT_Rando_LastSettingsFold(&r.total, nullptr, 0);
    (void)OoT_Rando_LastSettingsFoldSilver(&r.folded, &r.skipped, &r.pinned);
    r.settingsHash = gComboCtx.sharedRandoSettingsHash;
    auto world = Rando::Context::GetInstance();
    if (world != nullptr) {
        r.shuffleOption = world->GetOption(RSK_SHUFFLE_SILVER).Get();
        r.locations = world->allLocations.size();
        for (RandomizerCheck check : world->allLocations) {
            if (Rando::StaticData::GetLocation(check)->GetRCType() == RCTYPE_SILVER) {
                r.silverLocations++;
            }
        }
        for (int c = RC_UNKNOWN_CHECK + 1; c < RC_MAX; c++) {
            const RandomizerGet placed = world->GetItemLocation((RandomizerCheck)c)->GetPlacedRandomizerGet();
            if (placed >= RG_SHADOW_SILVER_BLADES && placed <= RG_GANONS_CASTLE_MQ_SILVER_SHADOW) {
                r.silverPlaced++;
            }
        }
    }
    printf("[TEST] %s: generation rc %d, fold run %d: %u line(s), #1363 lines folded %u / left out %u, pinned %d, "
           "settingsHash %08X; option %u; %zu location(s), %zu of them silver checks; %d silver rupee(s) placed\n",
           leg, rc, r.runs, (unsigned)r.total, (unsigned)r.folded, (unsigned)r.skipped, r.pinned,
           (unsigned)r.settingsHash, (unsigned)r.shuffleOption, r.locations, r.silverLocations, r.silverPlaced);
    return r;
}

void CheckOffWorld(const char* leg, const FoldRecord& r, uint32_t expectedSkipped) {
    SR_CHECK(r.generated && r.runs > 0, "%s: the generation failed or ran no settings fold", leg);
    SR_CHECK(r.folded == 0,
             "%s: the settings fold read %u line(s) #1363 added while the option is off: every default world's "
             "fingerprint, and so every placement, moves",
             leg, (unsigned)r.folded);
    SR_CHECK(r.skipped == expectedSkipped, "%s: the fold left out %u line(s), the option groups hold %u", leg,
             (unsigned)r.skipped, (unsigned)expectedSkipped);
    SR_CHECK(r.shuffleOption == RO_SHUFFLE_SILVER_OFF, "%s: the world was generated with Shuffle Silver Rupees %u", leg,
             (unsigned)r.shuffleOption);
    SR_CHECK(r.locations > 0 && r.silverLocations == 0,
             "%s: %zu of the world's %zu locations are silver checks, want none of a non-empty pool", leg,
             r.silverLocations, r.locations);
    SR_CHECK(r.silverPlaced == 0, "%s: %d silver rupee(s) were placed", leg, r.silverPlaced);
}

/** Legs F and P. Returns whether leg F's generation ran (L reads its world). */
bool CheckFold() {
    SavedKey shuffle;
    shuffle.Save(kShuffleKey);

    CVarClear(kShuffleKey);
    const FoldRecord f = Generate("F");
    const uint32_t expectedF = CountSilverLines();
    CheckOffWorld("F", f, expectedF);
    SR_CHECK(f.pinned == 0, "F: the generation reports pinning an option that was already off");

    // Never stored: a stored rando key poisons later rows (Restore below).
    CVarSetInteger(kShuffleKey, RO_SHUFFLE_SILVER_ON);
    const FoldRecord p = Generate("P");
    shuffle.Restore();
    const uint32_t expectedP = CountSilverLines();
    CheckOffWorld("P", p, expectedP);
    SR_CHECK(p.pinned == 1, "P: a paired generation with Shuffle Silver Rupees keyed On did not pin it off");
    SR_CHECK(f.generated && p.generated && p.settingsHash == f.settingsHash,
             "P: the pinned world's settings fingerprint %08X is not the default world's %08X",
             (unsigned)p.settingsHash, (unsigned)f.settingsHash);
    SR_CHECK(p.total == f.total, "P: the pinned world folded %u line(s), the default world %u", (unsigned)p.total,
             (unsigned)f.total);
    return f.generated || p.generated;
}

// ---- L: the logic fallback --------------------------------------------------------------

struct SilverLogic {
    RandomizerGet item;
    LogicVal event;
    const char* name;
};

// Spelled out by name, not derived: the offset mapping in logic.cpp is what L checks.
const SilverLogic kSilverLogic[] = {
    { RG_SHADOW_SILVER_BLADES, LOGIC_SHADOW_SILVER_BLADES, "Shadow blades" },
    { RG_SHADOW_SILVER_PIT, LOGIC_SHADOW_SILVER_PIT, "Shadow pit" },
    { RG_SHADOW_SILVER_SPIKES, LOGIC_SHADOW_SILVER_SPIKES, "Shadow spikes" },
    { RG_SPIRIT_SILVER_CHILD, LOGIC_SPIRIT_SILVER_CHILD, "Spirit child" },
    { RG_SPIRIT_SILVER_SUN, LOGIC_SPIRIT_SILVER_SUN, "Spirit sun" },
    { RG_SPIRIT_SILVER_BOULDERS, LOGIC_SPIRIT_SILVER_BOULDERS, "Spirit boulders" },
    { RG_BOTW_SILVER, LOGIC_BOTW_SILVER, "Bottom of the Well" },
    { RG_ICE_CAVERN_SILVER_BLADES, LOGIC_ICE_CAVERN_SILVER_BLADES, "Ice Cavern blades" },
    { RG_ICE_CAVERN_SILVER_BLOCK, LOGIC_ICE_CAVERN_SILVER_BLOCK, "Ice Cavern block" },
    { RG_GTG_SILVER_SLOPE, LOGIC_GTG_SILVER_SLOPE, "GTG slope" },
    { RG_GTG_SILVER_LAVA, LOGIC_GTG_SILVER_LAVA, "GTG lava" },
    { RG_GTG_SILVER_WATER, LOGIC_GTG_SILVER_WATER, "GTG water" },
    { RG_GANONS_CASTLE_SILVER_LIGHT, LOGIC_GANONS_CASTLE_SILVER_LIGHT, "Ganon's Castle light" },
    { RG_GANONS_CASTLE_SILVER_FOREST, LOGIC_GANONS_CASTLE_SILVER_FOREST, "Ganon's Castle forest" },
    { RG_GANONS_CASTLE_SILVER_FIRE, LOGIC_GANONS_CASTLE_SILVER_FIRE, "Ganon's Castle fire" },
    { RG_GANONS_CASTLE_SILVER_SPIRIT, LOGIC_GANONS_CASTLE_SILVER_SPIRIT, "Ganon's Castle spirit" },
    { RG_DODONGOS_CAVERN_MQ_SILVER, LOGIC_DODONGOS_CAVERN_MQ_SILVER, "MQ Dodongo's Cavern" },
    { RG_SHADOW_MQ_SILVER_INVISIBLE_BLADES, LOGIC_SHADOW_MQ_SILVER_INVISIBLE_BLADES, "MQ Shadow invisible blades" },
    { RG_SPIRIT_MQ_SILVER_LOBBY, LOGIC_SPIRIT_MQ_SILVER_LOBBY, "MQ Spirit lobby" },
    { RG_SPIRIT_MQ_SILVER_BIG_WALL, LOGIC_SPIRIT_MQ_SILVER_BIG_WALL, "MQ Spirit big wall" },
    { RG_GANONS_CASTLE_MQ_SILVER_WATER, LOGIC_GANONS_CASTLE_MQ_SILVER_WATER, "MQ Ganon's Castle water" },
    { RG_GANONS_CASTLE_MQ_SILVER_SHADOW, LOGIC_GANONS_CASTLE_MQ_SILVER_SHADOW, "MQ Ganon's Castle shadow" },
};
constexpr int kSilverItems = (int)(sizeof(kSilverLogic) / sizeof(kSilverLogic[0]));
static_assert(kSilverItems == RG_GANONS_CASTLE_MQ_SILVER_SHADOW - RG_SHADOW_SILVER_BLADES + 1,
              "L names every silver rupee item");

void CheckLogicFallback(Rando::Context& world) {
    std::shared_ptr<Rando::Logic> logic = world.GetLogic();
    if (logic == nullptr) {
        SrFail("L: the generated world has no logic");
        return;
    }
    const uint8_t priorOption = world.GetOption(RSK_SHUFFLE_SILVER).Get();
    world.GetOption(RSK_SHUFFLE_SILVER).Set(RO_SHUFFLE_SILVER_OFF);
    bool priorEvents[kSilverItems];
    for (int i = 0; i < kSilverItems; i++) {
        priorEvents[i] = logic->Get(kSilverLogic[i].event);
    }
    int wrong = 0;
    for (int k = 0; k < kSilverItems; k++) {
        for (int i = 0; i < kSilverItems; i++) {
            logic->Set(kSilverLogic[i].event, i == k);
        }
        for (int j = 0; j < kSilverItems; j++) {
            const bool has = logic->HasItem(kSilverLogic[j].item);
            if (has != (j == k)) {
                printf("[TEST]   L: with only the %s event set, HasItem(%s) answers %d\n", kSilverLogic[k].name,
                       kSilverLogic[j].name, has ? 1 : 0);
                wrong++;
            }
        }
    }
    for (int i = 0; i < kSilverItems; i++) {
        logic->Set(kSilverLogic[i].event, priorEvents[i]);
    }
    world.GetOption(RSK_SHUFFLE_SILVER).Set(priorOption);
    printf("[TEST] L: %d of %d silver item answers wrong with the option off\n", wrong, kSilverItems * kSilverItems);
    SR_CHECK(wrong == 0, "L: %d HasItem answer(s) read a silver rupee item's wrong LOGIC_* event", wrong);
}

} // namespace

/**
 * @return 0 when every leg holds, 1 otherwise.
 */
extern "C" int OoT_ShuffleSilverTest_Run(void) {
    printf("[TEST] oot-shuffle-silver: every silver check has its own flag, the option ships off and greyed out, "
           "default worlds fold none of its lines, a paired world generates it off, and logic reads each silver "
           "item's own event while it is off (#1363)\n");
    if (OTRGlobals::Instance == nullptr || OTRGlobals::Instance->gRandoContext == nullptr) {
        printf("[TEST] FAIL: oot-shuffle-silver needs OTRGlobals' randomizer context\n");
        return 1;
    }
    sFailures = 0;
    const GameId prevGame = Context_GetCurrentGame();
    Context_SetCurrentGame(GAME_OOT);
    std::unique_ptr<SaveContext> priorSave = std::make_unique<SaveContext>();
    std::memcpy(priorSave.get(), &gSaveContext, sizeof(SaveContext));

    CheckFlagMap();
    CheckOption();
    const bool generated = CheckFold();
    auto world = Rando::Context::GetInstance();
    if (generated && world != nullptr) {
        CheckLogicFallback(*world);
    } else {
        SrFail("L: no generated world to read the logic of");
    }

    std::memcpy(&gSaveContext, priorSave.get(), sizeof(SaveContext));
    Context_SetCurrentGame(prevGame);
    if (sFailures != 0) {
        printf("[TEST] FAIL (oot-shuffle-silver): %d failure(s)\n", sFailures);
        return 1;
    }
    printf("[TEST] PASS: oot-shuffle-silver\n");
    return 0;
}

#endif // RSBS_SINGLE_EXECUTABLE
