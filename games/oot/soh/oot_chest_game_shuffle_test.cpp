/**
 * @file oot_chest_game_shuffle_test.cpp
 * @brief #1364: Ship of Harkinian's Shuffle Treasure Chest Game (SoH 6706b49d1 and
 *        4868b0712) in Ocarina of Time's randomizer
 *        (Enhancements/randomizer/ShuffleTreasureChestGame.cpp). Row
 *        OoTChestGameShuffle (CTest label `rando`), dispatch `oot-chest-game-shuffle`
 *        in src/common/test_runner.cpp; the bring-up is oot-time-travel-sword's
 *        (OTRGlobals' randomizer context). No ROM: the generations run in whatever
 *        archive set the build directory holds, as rando-settings-fold-excludes'.
 *
 *  D  THE OPTIONS. Shuffle Chest Minigame is named and keyed as SoH names it, reads
 *     Off with its key cleared, is greyed out in this build (every world here is a
 *     paired creation, which generates it off: leg P), and still sits in Shuffle
 *     Items, the group the settings fold read it from before #1364, where its Off
 *     text is the one that fold folded ("Off"). Key Rings keeps its name, which
 *     spoilers and seed packages key it by (SoH renamed it "Keyrings").
 *  F  THE FOLD AT DEFAULT SETTINGS. A real generation with both new options' keys
 *     cleared: the settings fold reads none of the lines #1364 added (the Chest
 *     Minigame Keyring rows in Keyrings and Shuffle Dungeon Items, the option's row
 *     in the Basic Shuffles menu section, the room's RCTYPE_CHEST_GAME exclude
 *     rows), and leaves out exactly as many as the option groups hold, counted here
 *     by pointer from the groups and the static location table. The counts are
 *     taken inside the fold loop at the line itself (playthrough.cpp), so a skip
 *     deleted at the call site shows up as a folded line.
 *  P  A PAIRED WORLD GENERATES IT OFF. The same generation with Shuffle Chest
 *     Minigame and the Chest Minigame Keyring keyed on, as a player could set them
 *     before the option was greyed out (or a spoiler import could): the generation
 *     pins both off, the fold again reads none of their lines, the settings
 *     fingerprint is leg F's, and the room's checks are not locations.
 *  H  THE SHOPKEEPER GIVES ONCE (SoH 4868b0712). The shopkeeper's real wait action
 *     (z_en_takara_man.c's func_80B17A6C) on a stub frame, Link in reach, with the
 *     real hooks (RegisterShuffleTreasureChestGame, the option on) and the
 *     randomizer's real flag-set handler registered as a randomizer load registers
 *     it, over leg P's world with an item placed at the shopkeeper's check.
 *       H0 control: setting the check's flag while the check is unchecked queues
 *          the check (the flag handler maps the flag to it), so H1's "nothing
 *          queued" is not vacuous.
 *       H1 first frame: the flag is set, the check is collected, NOTHING is queued
 *          (the paid item is not given a second time by the queue drain), and the
 *          seed's item is offered.
 *       H2 the next frame, Link has not taken it: the item is offered again and
 *          still nothing is queued.
 *       H3 Link has taken it: the shopkeeper moves on and offers nothing.
 *  S  THE SENTINEL HAS NO BIT. RAND_INF_MAX (kept by unidentified crates, grass,
 *     pots and cows) is no flag: setting it writes nothing, and it reads unset (and
 *     unsetting it clears nothing) with its own word all ones. (Before save format
 *     6 that word was past randomizerInf, over foreignMask; it is spare capacity now.)
 *
 * RED HALVES, derived, not run (this lane had no build): with the fold's skip
 * removed at its call site F and P fail (lines folded); with the classifier
 * returning false F's and P's left-out count is 0; without the paired pin P fails
 * (pinned 0, the option on, a different fingerprint, the room's checks in the
 * pool); with the check marked collected after its flag (or not at all), H1 and H2
 * see the check queued; with the give put back behind the flag guard (the shape
 * this PR first had), H2 sees no offer; with the menu greying removed or Key Rings
 * renamed, D fails; with the bound removed from Flags_SetRandomizerInf S1 fails
 * (foreignMask written), and from Flags_GetRandomizerInf S2 fails (it reads 1).
 * Flags_UnsetRandomizerInf's own bound is not separately observable: it writes only
 * after Flags_GetRandomizerInf reads the bit set, which that bound already stops.
 *
 * Every key, option, flag and save field a leg changes is put back (in memory:
 * nothing here writes a config file).
 */
#ifdef RSBS_SINGLE_EXECUTABLE

#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include <libultraship/bridge.h>

#include "soh/OTRGlobals.h"
#include "soh/cvar_prefixes.h"
#include "soh/Enhancements/randomizer/randomizerTypes.h"
#include "soh/Enhancements/randomizer/static_data.h"
#include "soh/Enhancements/randomizer/SeedContext.h"
#include "soh/Enhancements/randomizer/settings.h"
#include "soh/Enhancements/randomizer/item_location.h"
#include "soh/Enhancements/game-interactor/GameInteractor.h"

#include "context.h" // src/common: gComboCtx, Context_GetCurrentGame, Context_SetCurrentGame
#include "game.h"

// At file scope, not inside an extern "C" block (see oot_shared_kokiri_sword_test.cpp).
#include <z64.h>
extern "C" {
#include "src/overlays/actors/ovl_En_Takara_Man/z_en_takara_man.h"
extern SaveContext gSaveContext;
extern PlayState* OoT_gPlayState;
// z_en_takara_man.c: the shopkeeper's wait-for-the-take action and the one after it
// (external linkage there).
void func_80B17A6C(EnTakaraMan* thisx, PlayState* play);
void func_80B17AC4(EnTakaraMan* thisx, PlayState* play);
// hook_handlers.cpp: the test bridge that empties the check queue the flag-set
// handler feeds.
int OoT_Rando_TestTakeQueuedChecks(uint16_t* out, int cap);
}

// 3drando/menu.cpp: the headless generation bridge every rando row uses.
extern "C" int Rando_HeadlessSeedTest(const char* seedStr);
// 3drando/playthrough.cpp: what the last settings fold folded, in total and of
// Shuffle Chest Minigame's lines, and whether that generation pinned it off.
extern "C" int OoT_Rando_LastSettingsFold(uint32_t* outTotalLines, uint32_t* outExcludeLines, int groupCap);
extern "C" int OoT_Rando_LastSettingsFoldChestGame(uint32_t* outFolded, uint32_t* outSkipped, int* outPinned);
// ShuffleTreasureChestGame.cpp: the registrar ShipInit runs on a randomizer load.
void RegisterShuffleTreasureChestGame();
// hook_handlers.cpp: the flag-set handler RandomizerRegisterHooks registers on a
// randomizer file load.
void RandomizerOnFlagSetHandler(int16_t flagType, int16_t flag);

// THE APPEND-ONLY LOCKS: each id #1364 adds sits after the last id main had, so no
// id a world or a save already holds moves.
static_assert(RC_MARKET_TREASURE_CHEST_GAME_SHOPKEEPER > RC_DEKU_TREE_QUEEN_GOHMA_GRASS_8,
              "RC_MARKET_TREASURE_CHEST_GAME_SHOPKEEPER is appended after every check id main had (#1364)");
static_assert(RHT_MARKET_TREASURE_CHEST_GAME_SHOPKEEPER > RHT_SONG_OF_SOARING_OOT,
              "RHT_MARKET_TREASURE_CHEST_GAME_SHOPKEEPER is appended after every hint text id main had (#1364)");
static_assert(RSK_KEYRINGS_CHEST_GAME > RSK_TIME_TRAVEL_REQUIRES_MASTER_SWORD,
              "RSK_KEYRINGS_CHEST_GAME is appended after every setting key main had (#1364)");
static_assert(RAND_INF_MARKET_TREASURE_CHEST_GAME_SHOPKEEPER > RAND_INF_OBTAINED_SONG_OF_SOARING_OOT,
              "RAND_INF_MARKET_TREASURE_CHEST_GAME_SHOPKEEPER is appended after every flag main had (#1364)");
static_assert(RO_GENERIC_OFF == 0 && RO_KEYRING_FOR_DUNGEON_OFF == 0,
              "#1364: a world saved before the options reads their missing entries as 0, which must be Off");

namespace {

int sFailures = 0;

void CgFail(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    printf("[TEST] oot-chest-game-shuffle: FAIL: ");
    vprintf(fmt, args);
    printf("\n");
    va_end(args);
    sFailures++;
}

#define CG_CHECK(cond, ...)      \
    do {                         \
        if (!(cond)) {           \
            CgFail(__VA_ARGS__); \
        }                        \
    } while (0)

/** A hook type's registry set aside for a scope (oot_time_travel_sword_test.cpp):
 *  the bring-up can leave hooks registered that read a live play state this row
 *  does not have. The hooks registered for an id stay. */
template <typename H> struct HooksAside {
    decltype(GameInteractor::RegisteredGameHooks<H>::functions) functions;
    decltype(GameInteractor::RegisteredGameHooks<H>::functionsForFilter) filters;
    HooksAside() {
        functions.swap(GameInteractor::RegisteredGameHooks<H>::functions);
        filters.swap(GameInteractor::RegisteredGameHooks<H>::functionsForFilter);
    }
    ~HooksAside() {
        functions.swap(GameInteractor::RegisteredGameHooks<H>::functions);
        filters.swap(GameInteractor::RegisteredGameHooks<H>::functionsForFilter);
    }
};

const char* const kShuffleKey = CVAR_RANDOMIZER_SETTING("ShuffleChestMinigame");
const char* const kKeyringKey = CVAR_RANDOMIZER_SETTING("ShuffleKeyRingsChestGame");

/** A key's stored state, put back by Restore. */
struct CgSavedKey {
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

int IndexInGroup(const std::vector<Rando::Option*>& options, const Rando::Option* option) {
    for (size_t i = 0; i < options.size(); i++) {
        if (options[i] == option) {
            return (int)i;
        }
    }
    return -1;
}

// ---- D: the options ----------------------------------------------------------------

void CheckOptions() {
    auto settings = Rando::Settings::GetInstance();
    settings->CreateOptions(); // idempotent; what Rando_HeadlessSeedTest calls first
    CgSavedKey shuffle;
    shuffle.Save(kShuffleKey);
    CVarClear(kShuffleKey);

    Rando::Option& option = settings->GetOption(RSK_SHUFFLE_CHEST_MINIGAME);
    const int shipped = option.GetOptionIndex();
    const int inShuffleSettings = IndexInGroup(settings->GetOptionGroup(RSG_SHUFFLE).GetOptions(), &option);
    printf("[TEST] D: '%s' (key %s): %d with no key stored; greyed %d ('%s'); Shuffle Settings row %d, Off text '%s'\n",
           option.GetName().c_str(), option.GetCVarName().c_str(), shipped, option.IsDisabled() ? 1 : 0,
           option.GetDisabledText().c_str(), inShuffleSettings, option.GetOptionText(RO_GENERIC_OFF).c_str());
    CG_CHECK(option.GetName() == "Shuffle Chest Minigame", "D: the option is named '%s', not 'Shuffle Chest Minigame'",
             option.GetName().c_str());
    CG_CHECK(option.GetCVarName() == kShuffleKey, "D: the option is not keyed %s", kShuffleKey);
    CG_CHECK(shipped == RO_GENERIC_OFF, "D: the shipped default is %d, not Off", shipped);
    CG_CHECK(option.IsDisabled() && !option.GetDisabledText().empty(),
             "D: the option is not greyed out with a reason, though every world here generates it off");
    // Main folded this option once, from Shuffle Settings, as "Off" (an OPT_U8 whose
    // first entry was "Off"); that line still folds, so it must still say so.
    CG_CHECK(inShuffleSettings >= 0, "D: the option left Shuffle Settings, the group the settings fold read it from");
    CG_CHECK(option.GetOptionText(RO_GENERIC_OFF) == "Off",
             "D: the option's Off text is '%s', not the 'Off' every world's settings string folded",
             option.GetOptionText(RO_GENERIC_OFF).c_str());

    Rando::Option& keyRings = settings->GetOption(RSK_KEYRINGS);
    Rando::Option& chestKeyring = settings->GetOption(RSK_KEYRINGS_CHEST_GAME);
    const bool keyRingsByName = Rando::StaticData::optionNameToEnum.contains("Key Rings") &&
                                Rando::StaticData::optionNameToEnum["Key Rings"] == RSK_KEYRINGS;
    printf("[TEST] D: '%s' (spoiler name maps back %d); '%s' (key %s)\n", keyRings.GetName().c_str(),
           keyRingsByName ? 1 : 0, chestKeyring.GetName().c_str(), chestKeyring.GetCVarName().c_str());
    CG_CHECK(keyRings.GetName() == "Key Rings" && keyRingsByName,
             "D: Key Rings is named '%s': a spoiler or seed package written before #1364 names it 'Key Rings', so "
             "its import would drop the setting",
             keyRings.GetName().c_str());
    CG_CHECK(chestKeyring.GetName() == "Chest Minigame Keyring" && chestKeyring.GetCVarName() == kKeyringKey,
             "D: the chest game's keyring is '%s' keyed %s, not 'Chest Minigame Keyring' keyed %s",
             chestKeyring.GetName().c_str(), chestKeyring.GetCVarName().c_str(), kKeyringKey);

    shuffle.Restore();
}

// ---- F and P: the fold -----------------------------------------------------------------

/** The fold lines #1364 added, counted from the option groups by pointer: what the
 *  fold must leave out while the option is off. Same walk as the fold (every
 *  Setting with options, in every group that is not a group of groups). */
uint32_t CountChestGameLines() {
    auto settings = Rando::Settings::GetInstance();
    std::set<const Rando::Option*> excludeRows;
    for (auto& loc : Rando::StaticData::GetLocationTable()) {
        if (loc.GetRCType() == RCTYPE_CHEST_GAME) {
            excludeRows.insert(loc.GetExcludedOption());
        }
    }
    const Rando::Option* keyring = &settings->GetOption(RSK_KEYRINGS_CHEST_GAME);
    const Rando::Option* shuffle = &settings->GetOption(RSK_SHUFFLE_CHEST_MINIGAME);
    uint32_t lines = 0;
    uint32_t excludes = 0;
    uint32_t keyrings = 0;
    uint32_t menuRows = 0;
    const auto& groups = settings->GetOptionGroups();
    for (size_t g = 0; g < groups.size(); g++) {
        if (groups[g].GetContainsType() == Rando::OptionGroupType::SUBGROUP) {
            continue;
        }
        for (const Rando::Option* option : groups[g].GetOptions()) {
            if (!option->IsCategory(Rando::OptionCategory::Setting) || option->GetOptionCount() == 0) {
                continue;
            }
            const bool isExclude =
                g >= RSG_EXCLUDES_KOKIRI_FOREST && g <= RSG_EXCLUDES_GANONS_CASTLE && excludeRows.count(option) != 0;
            const bool isKeyring = option == keyring;
            const bool isMenuRow = option == shuffle && g == RSG_MENU_SECTION_BASIC_SHUFFLES;
            excludes += isExclude ? 1 : 0;
            keyrings += isKeyring ? 1 : 0;
            menuRows += isMenuRow ? 1 : 0;
            lines += (isExclude || isKeyring || isMenuRow) ? 1 : 0;
        }
    }
    printf("[TEST] the option groups hold %u #1364 line(s): %u exclude row(s) of %zu chest-game check(s), %u Chest "
           "Minigame Keyring row(s), %u Basic Shuffles row(s)\n",
           (unsigned)lines, (unsigned)excludes, excludeRows.size(), (unsigned)keyrings, (unsigned)menuRows);
    // Every chest-game check has its own exclude row (their names are distinct),
    // and the keyring sits in Keyrings and Shuffle Dungeon Items: if one of these
    // is 0, F and P below would be asserting nothing about it.
    CG_CHECK(excludes == excludeRows.size() && excludes > 0,
             "the exclude groups hold %u of the %zu chest-game checks' rows", (unsigned)excludes, excludeRows.size());
    CG_CHECK(keyrings == 2, "the Chest Minigame Keyring sits in %u folded group(s), not 2", (unsigned)keyrings);
    CG_CHECK(menuRows == 1, "the option sits in the Basic Shuffles menu section %u time(s), not once",
             (unsigned)menuRows);
    return lines;
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
    uint8_t keyringOption;
    size_t locations;
    size_t chestGameLocations;
    int chestGameKeysPlaced;
};

FoldRecord CgGenerate(const char* leg) {
    FoldRecord r = {};
    const int rc = Rando_HeadlessSeedTest("RSBSUNIFIED1");
    r.generated = rc == 0;
    r.runs = OoT_Rando_LastSettingsFold(&r.total, nullptr, 0);
    (void)OoT_Rando_LastSettingsFoldChestGame(&r.folded, &r.skipped, &r.pinned);
    r.settingsHash = gComboCtx.sharedRandoSettingsHash;
    auto world = Rando::Context::GetInstance();
    if (world != nullptr) {
        r.shuffleOption = world->GetOption(RSK_SHUFFLE_CHEST_MINIGAME).Get();
        r.keyringOption = world->GetOption(RSK_KEYRINGS_CHEST_GAME).Get();
        r.locations = world->allLocations.size();
        for (RandomizerCheck check : world->allLocations) {
            if (Rando::StaticData::GetLocation(check)->GetRCType() == RCTYPE_CHEST_GAME) {
                r.chestGameLocations++;
            }
        }
        for (int c = RC_UNKNOWN_CHECK + 1; c < RC_MAX; c++) {
            const RandomizerGet placed = world->GetItemLocation((RandomizerCheck)c)->GetPlacedRandomizerGet();
            if (placed == RG_TREASURE_GAME_SMALL_KEY || placed == RG_TREASURE_GAME_KEY_RING) {
                r.chestGameKeysPlaced++;
            }
        }
    }
    printf("[TEST] %s: generation rc %d, fold run %d: %u line(s), #1364 lines folded %u / left out %u, pinned %d, "
           "settingsHash %08X; option %u, keyring %u; %zu location(s), %zu of them the chest game's; %d chest-game "
           "key(s) placed in Ocarina of Time\n",
           leg, rc, r.runs, (unsigned)r.total, (unsigned)r.folded, (unsigned)r.skipped, r.pinned,
           (unsigned)r.settingsHash, (unsigned)r.shuffleOption, (unsigned)r.keyringOption, r.locations,
           r.chestGameLocations, r.chestGameKeysPlaced);
    return r;
}

void CheckOffWorld(const char* leg, const FoldRecord& r, uint32_t expectedSkipped) {
    CG_CHECK(r.generated && r.runs > 0, "%s: the generation failed or ran no settings fold", leg);
    CG_CHECK(r.folded == 0,
             "%s: the settings fold read %u line(s) #1364 added while the option is off: every default world's "
             "fingerprint, and so every placement, moves",
             leg, (unsigned)r.folded);
    CG_CHECK(r.skipped == expectedSkipped, "%s: the fold left out %u line(s), the option groups hold %u", leg,
             (unsigned)r.skipped, (unsigned)expectedSkipped);
    CG_CHECK(r.shuffleOption == RO_GENERIC_OFF, "%s: the world was generated with Shuffle Chest Minigame %u", leg,
             (unsigned)r.shuffleOption);
    CG_CHECK(r.locations > 0 && r.chestGameLocations == 0,
             "%s: %zu of the world's %zu locations are chest-game checks, want none of a non-empty pool", leg,
             r.chestGameLocations, r.locations);
    CG_CHECK(r.chestGameKeysPlaced == 0, "%s: %d chest-game key(s) were placed", leg, r.chestGameKeysPlaced);
}

/** Legs F and P. Returns leg P's generation result (H runs over its world). */
bool CheckFold() {
    CgSavedKey shuffle;
    CgSavedKey keyring;
    shuffle.Save(kShuffleKey);
    keyring.Save(kKeyringKey);

    CVarClear(kShuffleKey);
    CVarClear(kKeyringKey);
    const FoldRecord f = CgGenerate("F");
    const uint32_t expectedF = CountChestGameLines();
    CheckOffWorld("F", f, expectedF);
    CG_CHECK(f.pinned == 0, "F: the generation reports pinning an option that was already off");

    // Never stored: a stored rando key poisons later rows (CVarClear below).
    CVarSetInteger(kShuffleKey, RO_GENERIC_ON);
    CVarSetInteger(kKeyringKey, RO_KEYRING_FOR_DUNGEON_ON);
    const FoldRecord p = CgGenerate("P");
    shuffle.Restore();
    keyring.Restore();
    const uint32_t expectedP = CountChestGameLines();
    CheckOffWorld("P", p, expectedP);
    CG_CHECK(p.pinned == 1, "P: a paired generation with Shuffle Chest Minigame keyed on did not pin it off");
    CG_CHECK(p.keyringOption == RO_KEYRING_FOR_DUNGEON_OFF, "P: the Chest Minigame Keyring was generated as %u",
             (unsigned)p.keyringOption);
    CG_CHECK(f.generated && p.generated && p.settingsHash == f.settingsHash,
             "P: the pinned world's settings fingerprint %08X is not the default world's %08X",
             (unsigned)p.settingsHash, (unsigned)f.settingsHash);
    CG_CHECK(p.total == f.total, "P: the pinned world folded %u line(s), the default world %u", (unsigned)p.total,
             (unsigned)f.total);
    return p.generated;
}

// ---- H: the shopkeeper gives once ---------------------------------------------------------

struct Stage {
    PlayState* play;
    Player* player;
    EnTakaraMan* takaraMan;
};

/** The shopkeeper waiting for Link to take what he was paid for, Link in reach and
 *  holding nothing. @p taken: Link has taken it (the shopkeeper's parent is set). */
void ArmFrame(Stage& s, bool taken) {
    std::memset(s.play, 0, sizeof(PlayState));
    std::memset(s.player, 0, sizeof(Player));
    s.play->sceneNum = SCENE_TREASURE_BOX_SHOP;
    s.play->actorCtx.actorLists[ACTORCAT_PLAYER].head = &s.player->actor;
    s.player->actor.id = ACTOR_PLAYER;
    s.player->getItemId = GI_NONE;
    s.takaraMan->actor.id = ACTOR_EN_TAKARA_MAN;
    s.takaraMan->actor.xzDistToPlayer = 20.0f; // inside the hook's 2000 x 1000 reach
    s.takaraMan->actor.yDistToPlayer = 0.0f;
    s.takaraMan->actor.parent = taken ? &s.player->actor : nullptr;
    s.takaraMan->actionFunc = func_80B17A6C;
}

struct Frame {
    bool offered;  // the hook armed the give for the shopkeeper
    bool seedItem; // and it is the seed's item, not the vanilla key
    bool flagSet;
    bool collected;
    int queued;
    uint16_t firstQueued;
    bool movedOn; // the shopkeeper went on to func_80B17AC4
};

Frame RunFrame(Stage& s, Rando::Context& world, const GetItemEntry& seedEntry) {
    Frame f = {};
    func_80B17A6C(s.takaraMan, s.play);
    f.offered = s.player->interactRangeActor == &s.takaraMan->actor;
    f.seedItem = s.player->getItemEntry.modIndex == seedEntry.modIndex &&
                 s.player->getItemEntry.itemId == seedEntry.itemId &&
                 s.player->getItemEntry.getItemId == seedEntry.getItemId;
    f.flagSet = Flags_GetRandomizerInf(RAND_INF_MARKET_TREASURE_CHEST_GAME_SHOPKEEPER) != 0;
    f.collected = world.GetItemLocation(RC_MARKET_TREASURE_CHEST_GAME_SHOPKEEPER)->HasObtained();
    uint16_t queued[4] = {};
    f.queued = OoT_Rando_TestTakeQueuedChecks(queued, 4);
    f.firstQueued = queued[0];
    f.movedOn = s.takaraMan->actionFunc == func_80B17AC4;
    return f;
}

void PrintFrame(const char* leg, const Frame& f) {
    printf("[TEST] %s: offered %d (the seed's item %d), flag %d, collected %d, queued %d (first %u), moved on %d\n",
           leg, f.offered ? 1 : 0, f.seedItem ? 1 : 0, f.flagSet ? 1 : 0, f.collected ? 1 : 0, f.queued,
           (unsigned)f.firstQueued, f.movedOn ? 1 : 0);
}

void CheckShopkeeper(Rando::Context& world) {
    if (OTRGlobals::Instance == nullptr || OTRGlobals::Instance->gRandomizer == nullptr) {
        CgFail("H: needs OTRGlobals' randomizer (the hook's give reads the item's obtainability)");
        return;
    }
    if (GameInteractor::Instance == nullptr) {
        GameInteractor::Instance = new GameInteractor();
    }
    Stage s = {};
    s.play = (PlayState*)calloc(1, sizeof(PlayState));
    s.player = (Player*)calloc(1, sizeof(Player));
    s.takaraMan = (EnTakaraMan*)calloc(1, sizeof(EnTakaraMan));
    if (s.play == nullptr || s.player == nullptr || s.takaraMan == nullptr) {
        CgFail("H: could not allocate the stub play state");
        free(s.play);
        free(s.player);
        free(s.takaraMan);
        return;
    }

    Rando::ItemLocation* shopkeeper = world.GetItemLocation(RC_MARKET_TREASURE_CHEST_GAME_SHOPKEEPER);
    const RandomizerGet priorItem = shopkeeper->GetPlacedRandomizerGet();
    const uint16_t priorPrice = shopkeeper->GetPrice();
    const RandomizerCheckStatus priorStatus = shopkeeper->GetCheckStatus();
    const uint8_t priorOption = world.GetOption(RSK_SHUFFLE_CHEST_MINIGAME).Get();
    PlayState* const priorPlay = OoT_gPlayState;

    std::memset(&gSaveContext, 0, sizeof(SaveContext));
    gSaveContext.ship.quest.id = QUEST_RANDOMIZER; // IS_RANDO, as a randomizer load leaves it
    shopkeeper->SetPlacedItem(RG_PIECE_OF_HEART);
    shopkeeper->SetCheckStatus(RCSHOW_UNCHECKED);
    world.GetOption(RSK_SHUFFLE_CHEST_MINIGAME).Set(RO_GENERIC_ON); // the pin lifted, as upstream plays it
    const GetItemEntry seedEntry = world.GetFinalGIEntry(RC_MARKET_TREASURE_CHEST_GAME_SHOPKEEPER, true, GI_DOOR_KEY);
    OoT_gPlayState = s.play;
    (void)OoT_Rando_TestTakeQueuedChecks(nullptr, 0);
    {
        HooksAside<GameInteractor::OnVanillaBehavior> asideVb;
        HooksAside<GameInteractor::OnFlagSet> asideFlags;
        HooksAside<GameInteractor::OnRandoSetCheckStatus> asideStatus;
        RegisterShuffleTreasureChestGame();
        const HOOK_ID flagHook =
            GameInteractor::Instance->RegisterGameHook<GameInteractor::OnFlagSet>(RandomizerOnFlagSetHandler);

        // H0: the flag alone, the check unchecked.
        Flags_SetRandomizerInf(RAND_INF_MARKET_TREASURE_CHEST_GAME_SHOPKEEPER);
        uint16_t queued[4] = {};
        const int controlQueued = OoT_Rando_TestTakeQueuedChecks(queued, 4);
        printf("[TEST] H0: the shopkeeper's flag set on an unchecked check queues %d check(s) (first %u)\n",
               controlQueued, (unsigned)queued[0]);
        CG_CHECK(controlQueued == 1 && queued[0] == (uint16_t)RC_MARKET_TREASURE_CHEST_GAME_SHOPKEEPER,
                 "H0: setting the shopkeeper's flag queued %d check(s), first %u; want the shopkeeper's check, or "
                 "H1's 'nothing queued' proves nothing",
                 controlQueued, (unsigned)queued[0]);
        Flags_UnsetRandomizerInf(RAND_INF_MARKET_TREASURE_CHEST_GAME_SHOPKEEPER);
        shopkeeper->SetCheckStatus(RCSHOW_UNCHECKED);

        // H1: the first frame the shopkeeper asks.
        ArmFrame(s, false);
        const Frame h1 = RunFrame(s, world, seedEntry);
        PrintFrame("H1", h1);
        CG_CHECK(h1.flagSet && h1.collected, "H1: the paid item's check was not marked collected and flagged");
        CG_CHECK(h1.queued == 0,
                 "H1: the flag queued %d check(s) (first %u): the queue drain gives the paid item a second time "
                 "(SoH 4868b0712 marks the check collected before setting its flag)",
                 h1.queued, (unsigned)h1.firstQueued);
        CG_CHECK(h1.offered && h1.seedItem, "H1: the shopkeeper did not offer the seed's item");

        // H2: the next frame, the item not taken yet.
        ArmFrame(s, false);
        const Frame h2 = RunFrame(s, world, seedEntry);
        PrintFrame("H2", h2);
        CG_CHECK(h2.offered && h2.seedItem,
                 "H2: the shopkeeper stopped offering the seed's item before Link took it (the give only arms the "
                 "offer, so it must be offered on every frame, as SoH does)");
        CG_CHECK(h2.queued == 0 && h2.collected, "H2: a second frame queued %d check(s)", h2.queued);

        // H3: Link has taken it.
        ArmFrame(s, true);
        const Frame h3 = RunFrame(s, world, seedEntry);
        PrintFrame("H3", h3);
        CG_CHECK(h3.movedOn && !h3.offered && h3.queued == 0,
                 "H3: once Link took the item the shopkeeper did not move on without offering it again");

        // Unregister: the registrar with the option off registers nothing and drops
        // what it registered above.
        GameInteractor::Instance->UnregisterGameHook<GameInteractor::OnFlagSet>(flagHook);
        world.GetOption(RSK_SHUFFLE_CHEST_MINIGAME).Set(RO_GENERIC_OFF);
        RegisterShuffleTreasureChestGame();
    }
    (void)OoT_Rando_TestTakeQueuedChecks(nullptr, 0);
    OoT_gPlayState = priorPlay;
    world.GetOption(RSK_SHUFFLE_CHEST_MINIGAME).Set(priorOption);
    shopkeeper->SetPlacedItem(priorItem);
    shopkeeper->SetPrice(priorPrice);
    shopkeeper->SetCheckStatus(priorStatus);
    free(s.play);
    free(s.player);
    free(s.takaraMan);
}

// S: RAND_INF_MAX (the sentinel unidentified crates, grass, pots and cows keep) is
// no flag. Since save format 6 its word is inside randomizerInf's fixed capacity
// (spare, never a flag), so a guard-less accessor would set and read a real bit
// there. The accessors must ignore it.
void CheckSentinel() {
    static_assert((size_t)(RAND_INF_MAX >> 4) < sizeof(gSaveContext.ship.randomizerInf) / sizeof(u16),
                  "S assumes the sentinel's word is inside randomizerInf's saved capacity (save format 6)");
    std::unique_ptr<SaveContext> before = std::make_unique<SaveContext>();
    HooksAside<GameInteractor::OnFlagSet> asideSet;
    HooksAside<GameInteractor::OnFlagUnset> asideUnset;

    // S1: a set of the sentinel writes nothing (the field after the array is clear,
    // so a write there shows).
    std::memset(&gSaveContext, 0, sizeof(SaveContext));
    gSaveContext.ship.quest.id = QUEST_RANDOMIZER;
    std::memcpy(before.get(), &gSaveContext, sizeof(SaveContext));
    Flags_SetRandomizerInf(RAND_INF_MAX);
    const bool setWrote = std::memcmp(before.get(), &gSaveContext, sizeof(SaveContext)) != 0;
    printf("[TEST] S1: setting RAND_INF_MAX %s the save\n", setWrote ? "changed" : "left");
    CG_CHECK(!setWrote, "S1: Flags_SetRandomizerInf(RAND_INF_MAX) wrote the save (its word %04X)",
             (unsigned)gSaveContext.ship.randomizerInf[RAND_INF_MAX >> 4]);

    // S2: with every bit of the sentinel's own word set, the sentinel still reads
    // unset and an unset of it clears nothing.
    gSaveContext.ship.randomizerInf[RAND_INF_MAX >> 4] = 0xFFFF;
    std::memcpy(before.get(), &gSaveContext, sizeof(SaveContext));
    const s32 read = Flags_GetRandomizerInf(RAND_INF_MAX);
    Flags_UnsetRandomizerInf(RAND_INF_MAX);
    const bool unsetWrote = std::memcmp(before.get(), &gSaveContext, sizeof(SaveContext)) != 0;
    printf("[TEST] S2: RAND_INF_MAX reads %d over its own word all ones; unsetting it %s the save\n", (int)read,
           unsetWrote ? "changed" : "left");
    CG_CHECK(read == 0, "S2: Flags_GetRandomizerInf(RAND_INF_MAX) read %d from its spare word", (int)read);
    CG_CHECK(!unsetWrote, "S2: Flags_UnsetRandomizerInf(RAND_INF_MAX) cleared its spare word");
}

} // namespace

/**
 * @return 0 when every leg holds, 1 otherwise.
 */
extern "C" int OoT_ChestGameShuffleTest_Run(void) {
    printf("[TEST] oot-chest-game-shuffle: Shuffle Chest Minigame ships off and greyed out, default worlds fold none "
           "of its lines, a paired world generates it off, and the shopkeeper gives his item once (#1364)\n");
    if (OTRGlobals::Instance == nullptr || OTRGlobals::Instance->gRandoContext == nullptr) {
        printf("[TEST] FAIL: oot-chest-game-shuffle needs OTRGlobals' randomizer context\n");
        return 1;
    }
    sFailures = 0;
    const GameId prevGame = Context_GetCurrentGame();
    // The GameInteractor dispatch is gated off only under GAME_MM.
    Context_SetCurrentGame(GAME_OOT);
    std::unique_ptr<SaveContext> priorSave = std::make_unique<SaveContext>();
    std::memcpy(priorSave.get(), &gSaveContext, sizeof(SaveContext));

    CheckOptions();
    const bool generated = CheckFold();
    auto world = Rando::Context::GetInstance();
    if (generated && world != nullptr) {
        CheckShopkeeper(*world);
    } else {
        CgFail("H: no generated world to run the shopkeeper over");
    }
    CheckSentinel();

    std::memcpy(&gSaveContext, priorSave.get(), sizeof(SaveContext));
    Context_SetCurrentGame(prevGame);
    if (sFailures != 0) {
        printf("[TEST] FAIL (oot-chest-game-shuffle): %d failure(s)\n", sFailures);
        return 1;
    }
    printf("[TEST] PASS: oot-chest-game-shuffle\n");
    return 0;
}

#endif // RSBS_SINGLE_EXECUTABLE
