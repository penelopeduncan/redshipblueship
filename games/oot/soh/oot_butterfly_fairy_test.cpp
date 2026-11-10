/**
 * @file oot_butterfly_fairy_test.cpp
 * @brief #1407: Ship of Harkinian's Shuffle Butterfly Fairies and Easy Butterfly
 *        Fairies (Shipwright#6833) in Ocarina of Time's randomizer. Two rows, CTest
 *        label `rando`, dispatch in src/common/test_runner.cpp. No ROM: the
 *        generations run in whatever archive set the build directory holds, as
 *        rando-settings-fold-excludes' do.
 *
 * oot-butterfly-fairy-shuffle (OoTButterflyFairyShuffle), real generations:
 *  D  THE OPTION. Shuffle Butterfly Fairies is named and keyed as SoH names it,
 *     reads Off with its key cleared, and is not greyed out: in a paired world it
 *     is handled by the paths the other four fairy shuffles use (see the PR's
 *     paired-world notes and ComboLogicEngineOoT.cpp's OoT_ComboLogic_HostAcceptsForeign,
 *     which refuses an ACTOR_EN_BUTTE host as it refuses every non-chest fairy).
 *  F  THE FOLD AT DEFAULT SETTINGS. A real generation with the key cleared: the
 *     settings fold reads none of the lines #1407 added (the option's rows in
 *     Shuffle Settings and the Basic Shuffles menu section, and the 20 butterfly
 *     checks' exclude rows), and leaves out exactly as many as the option groups
 *     hold, counted here by pointer from the groups and the static location table.
 *     The counts are taken inside the fold loop at the line itself
 *     (playthrough.cpp), so a skip deleted at the call site shows up here as a
 *     folded line. No butterfly check is a location.
 *  N  THE OPTION ON. The same generation with the key on: every one of those lines
 *     folds and none is left out, the settings fingerprint differs from F's, and
 *     all 20 butterfly checks are locations.
 *
 * oot-butterfly-fairy-hooks (OoTButterflyFairyHooks), the real registrars
 * (RegisterShuffleFairies, EasyButterflyFairies_Register) on a stub frame, through
 * the real GameInteractor_Should dispatch z_en_butte.c makes:
 *  O  OFF MEANS UNCHANGED. A randomizer save with Shuffle Butterfly Fairies off and
 *     Easy Butterfly Fairies' key cleared registers no VB_SPAWN_BUTTERFLY_FAIRY or
 *     VB_SPAWN_BUTTERFLY_FAIRY_EASY hook; both dispatches return the value they
 *     are given; a butterfly beside Link's held Deku Stick keeps its action.
 *  A  ON. Exactly one hook each; the easy hook starts the transformation of a
 *     butterfly within 60 units of a held Deku Stick (EnButte_TransformIntoFairy,
 *     timer 9) and answers false, and leaves one alone when Link holds no stick
 *     (control); the spawn hook leaves the vanilla fairy to a butterfly whose check
 *     is already collected, and leaves a frame that is not the spawn frame alone.
 *  I  IDENTITY. ShuffleFairies_GetFairyIdentity, given each butterfly location's own
 *     scene and params and ACTOR_EN_BUTTE, resolves to that check and its
 *     RandomizerInf; given ACTOR_EN_ELF (what the function always asked for
 *     before #1407) it does not.
 *
 * RED HALVES, derived, not run (this lane had no build): with the fold's skip
 * removed at its call site F fails (22 lines folded); with the classifier returning
 * false F's left-out count is 0; with the classifier's exclude branch removed F
 * counts 2, not 22, left out; with RCTYPE_BUTTERFLY_FAIRY dropped from
 * Context::GenerateLocationPool's filter F sees butterfly locations; with the spawn
 * hook registered on `shouldRegister` instead of `shouldRegisterButterfly`, or the
 * easy hook armed on the randomizer save alone, O fails (a registrant); with the
 * easy hook's stick or distance test removed A's control fails; with
 * GetCheckObjectFromActor asked for ACTOR_EN_ELF again I fails.
 *
 * Every key, option, flag and save field a leg changes is put back (in memory:
 * nothing here writes a config file).
 */
#ifdef RSBS_SINGLE_EXECUTABLE

#include <cstdarg>
#include <cstdint>
#include <cstdio>
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
#include "soh/Enhancements/randomizer/location.h"
#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/Enhancements/game-interactor/GameInteractor_Hooks.h" // GameInteractor_Should

#include "context.h" // src/common: gComboCtx, Context_GetCurrentGame, Context_SetCurrentGame
#include "game.h"

// At file scope, not inside an extern "C" block (see oot_shared_kokiri_sword_test.cpp).
#include <z64.h>
extern "C" {
#include "src/overlays/actors/ovl_En_Butte/z_en_butte.h"
extern SaveContext gSaveContext;
extern PlayState* OoT_gPlayState;
// z_en_butte.c: the transformation action EnButte_SetupTransformIntoFairy hands
// the butterfly (external linkage there).
void EnButte_TransformIntoFairy(EnButte* thisx, PlayState* play);
}

// 3drando/menu.cpp: the headless generation bridge every rando row uses.
extern "C" int Rando_HeadlessSeedTest(const char* seedStr);
// 3drando/playthrough.cpp: what the last settings fold folded, in total and of
// Shuffle Butterfly Fairies' lines.
extern "C" int OoT_Rando_LastSettingsFold(uint32_t* outTotalLines, uint32_t* outExcludeLines, int groupCap);
extern "C" int OoT_Rando_LastSettingsFoldButterfly(uint32_t* outFolded, uint32_t* outSkipped);
// ShuffleFairies.cpp: the ShipInit registrar ("IS_RANDO") for every fairy hook,
// and the identity a spawned fairy carries.
void RegisterShuffleFairies();
CheckIdentity ShuffleFairies_GetFairyIdentity(int32_t params, ActorID id);
// QoL/EasyButterflyFairies.cpp: its ShipInit registrar.
void EasyButterflyFairies_Register();

// THE APPEND-ONLY LOCKS: each id #1407 adds sits after the last id main had, so no
// id a world or a save already holds moves.
static_assert(RC_HC_NEAR_WALL_BUTTERFLY_FAIRY > RC_DEKU_TREE_QUEEN_GOHMA_GRASS_8,
              "the butterfly fairy checks are appended after every check id main had (#1407)");
static_assert(RHT_BUTTERFLY_FAIRY_HYRULE_CASTLE > RHT_SONG_OF_SOARING_OOT,
              "the butterfly fairy hint texts are appended after every hint text id main had (#1407)");
static_assert(RSK_SHUFFLE_BUTTERFLY_FAIRIES > RSK_TIME_TRAVEL_REQUIRES_MASTER_SWORD,
              "RSK_SHUFFLE_BUTTERFLY_FAIRIES is appended after every setting key main had (#1407)");
static_assert(RAND_INF_HC_NEAR_WALL_BUTTERFLY_FAIRY > RAND_INF_OBTAINED_SONG_OF_SOARING_OOT,
              "the butterfly fairy flags are appended after every flag main had (#1407)");
static_assert(RO_GENERIC_OFF == 0, "#1407: a world saved before the option reads its missing entry as 0, Off");

namespace {

constexpr int kButterflyChecks = 20;

int sFailures = 0;
const char* sRow = "";

void BfFail(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    printf("[TEST] %s: FAIL: ", sRow);
    vprintf(fmt, args);
    printf("\n");
    va_end(args);
    sFailures++;
}

#define BF_CHECK(cond, ...)      \
    do {                         \
        if (!(cond)) {           \
            BfFail(__VA_ARGS__); \
        }                        \
    } while (0)

const char* const kShuffleKey = CVAR_RANDOMIZER_SETTING("ShuffleButterflyFairies");
const char* const kEasyKey = CVAR_ENHANCEMENT("EasyButterflyFairies");

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

std::vector<RandomizerCheck> ButterflyChecks() {
    std::vector<RandomizerCheck> checks;
    for (auto& loc : Rando::StaticData::GetLocationTable()) {
        if (loc.GetRandomizerCheck() != RC_UNKNOWN_CHECK && loc.GetRCType() == RCTYPE_BUTTERFLY_FAIRY) {
            checks.push_back(loc.GetRandomizerCheck());
        }
    }
    return checks;
}

// ---- D: the option -------------------------------------------------------------------

void CheckOption() {
    auto settings = Rando::Settings::GetInstance();
    settings->CreateOptions(); // idempotent; what Rando_HeadlessSeedTest calls first
    SavedKey shuffle;
    shuffle.Save(kShuffleKey);
    CVarClear(kShuffleKey);
    Rando::Option& option = settings->GetOption(RSK_SHUFFLE_BUTTERFLY_FAIRIES);
    const int shipped = option.GetOptionIndex();
    printf("[TEST] D: '%s' (key %s): %d with no key stored; greyed %d\n", option.GetName().c_str(),
           option.GetCVarName().c_str(), shipped, option.IsDisabled() ? 1 : 0);
    BF_CHECK(option.GetName() == "Shuffle Butterfly Fairies",
             "D: the option is named '%s', not 'Shuffle Butterfly Fairies'", option.GetName().c_str());
    BF_CHECK(option.GetCVarName() == kShuffleKey, "D: the option is not keyed %s", kShuffleKey);
    BF_CHECK(shipped == RO_GENERIC_OFF, "D: the shipped default is %d, not Off", shipped);
    BF_CHECK(!option.IsDisabled(), "D: the option is greyed out ('%s'), though a paired world generates it as set",
             option.GetDisabledText().c_str());
    BF_CHECK((int)ButterflyChecks().size() == kButterflyChecks,
             "D: the location table holds %zu butterfly checks, not %d", ButterflyChecks().size(), kButterflyChecks);
    shuffle.Restore();
}

// ---- F and N: the fold -----------------------------------------------------------------

/** The fold lines #1407 added, counted from the option groups by pointer: what the
 *  fold must leave out while the option is off. Same walk as the fold (every
 *  Setting with options, in every group that is not a group of groups). */
uint32_t CountButterflyLines() {
    auto settings = Rando::Settings::GetInstance();
    std::set<const Rando::Option*> excludeRows;
    for (RandomizerCheck rc : ButterflyChecks()) {
        excludeRows.insert(Rando::StaticData::GetLocation(rc)->GetExcludedOption());
    }
    const Rando::Option* shuffle = &settings->GetOption(RSK_SHUFFLE_BUTTERFLY_FAIRIES);
    uint32_t excludes = 0;
    uint32_t optionRows = 0;
    const auto& groups = settings->GetOptionGroups();
    for (size_t g = 0; g < groups.size(); g++) {
        if (groups[g].GetContainsType() == Rando::OptionGroupType::SUBGROUP) {
            continue;
        }
        for (const Rando::Option* option : groups[g].GetOptions()) {
            if (!option->IsCategory(Rando::OptionCategory::Setting) || option->GetOptionCount() == 0) {
                continue;
            }
            const bool isExclude = g >= RSG_EXCLUDES_KOKIRI_FOREST && g <= RSG_EXCLUDES_GANONS_CASTLE;
            if (isExclude && excludeRows.count(option) != 0) {
                excludes++;
            } else if (!isExclude && g != RSG_TRICKS && option == shuffle) {
                optionRows++;
            }
        }
    }
    printf("[TEST] the option groups hold %u #1407 line(s): %u exclude row(s) of %zu butterfly check(s), %u option "
           "row(s)\n",
           (unsigned)(excludes + optionRows), (unsigned)excludes, excludeRows.size(), (unsigned)optionRows);
    // Every butterfly check has its own exclude row (their names are distinct within
    // each area), and the option sits in Shuffle Settings and the Basic Shuffles
    // menu section: if one of these is 0, F and N would be asserting nothing about it.
    BF_CHECK(excludes == (uint32_t)kButterflyChecks, "the exclude groups hold %u of the %d butterfly checks' rows",
             (unsigned)excludes, kButterflyChecks);
    BF_CHECK(optionRows == 2, "the option sits in %u folded group(s), not 2", (unsigned)optionRows);
    return excludes + optionRows;
}

struct FoldRecord {
    bool generated;
    int runs;
    uint32_t total;
    uint32_t folded;
    uint32_t skipped;
    uint32_t settingsHash;
    uint8_t option;
    size_t locations;
    size_t butterflyLocations;
};

FoldRecord Generate(const char* leg) {
    FoldRecord r = {};
    const int rc = Rando_HeadlessSeedTest("RSBSUNIFIED1");
    r.generated = rc == 0;
    r.runs = OoT_Rando_LastSettingsFold(&r.total, nullptr, 0);
    (void)OoT_Rando_LastSettingsFoldButterfly(&r.folded, &r.skipped);
    r.settingsHash = gComboCtx.sharedRandoSettingsHash;
    auto world = Rando::Context::GetInstance();
    if (world != nullptr) {
        r.option = world->GetOption(RSK_SHUFFLE_BUTTERFLY_FAIRIES).Get();
        r.locations = world->allLocations.size();
        for (RandomizerCheck check : world->allLocations) {
            if (Rando::StaticData::GetLocation(check)->GetRCType() == RCTYPE_BUTTERFLY_FAIRY) {
                r.butterflyLocations++;
            }
        }
    }
    printf("[TEST] %s: generation rc %d, fold run %d: %u line(s), #1407 lines folded %u / left out %u, settingsHash "
           "%08X; option %u; %zu location(s), %zu of them butterfly fairies\n",
           leg, rc, r.runs, (unsigned)r.total, (unsigned)r.folded, (unsigned)r.skipped, (unsigned)r.settingsHash,
           (unsigned)r.option, r.locations, r.butterflyLocations);
    return r;
}

void CheckFold() {
    SavedKey shuffle;
    shuffle.Save(kShuffleKey);

    CVarClear(kShuffleKey);
    const FoldRecord f = Generate("F");
    const uint32_t expectedLines = CountButterflyLines();
    BF_CHECK(f.generated && f.runs > 0, "F: the generation failed or ran no settings fold");
    BF_CHECK(f.folded == 0,
             "F: the settings fold read %u line(s) #1407 added while the option is off: every default world's "
             "fingerprint, and so every placement, moves",
             (unsigned)f.folded);
    BF_CHECK(f.skipped == expectedLines, "F: the fold left out %u line(s), the option groups hold %u",
             (unsigned)f.skipped, (unsigned)expectedLines);
    BF_CHECK(f.option == RO_GENERIC_OFF, "F: the world was generated with Shuffle Butterfly Fairies %u",
             (unsigned)f.option);
    BF_CHECK(f.locations > 0 && f.butterflyLocations == 0,
             "F: %zu of the world's %zu locations are butterfly fairies, want none of a non-empty pool",
             f.butterflyLocations, f.locations);

    // Never stored: a stored rando key poisons later rows (Restore below).
    CVarSetInteger(kShuffleKey, RO_GENERIC_ON);
    const FoldRecord n = Generate("N");
    shuffle.Restore();
    BF_CHECK(n.generated && n.runs > f.runs, "N: the generation with the option on failed or ran no new fold");
    BF_CHECK(n.option == RO_GENERIC_ON, "N: the world was generated with Shuffle Butterfly Fairies %u",
             (unsigned)n.option);
    BF_CHECK(n.folded == expectedLines && n.skipped == 0,
             "N: with the option on the fold read %u and left out %u of the %u #1407 line(s)", (unsigned)n.folded,
             (unsigned)n.skipped, (unsigned)expectedLines);
    BF_CHECK(n.total == f.total + expectedLines, "N: the fold read %u line(s), F's %u plus %u expected",
             (unsigned)n.total, (unsigned)f.total, (unsigned)expectedLines);
    BF_CHECK(n.settingsHash != f.settingsHash, "N: the option on left the settings fingerprint at F's %08X",
             (unsigned)f.settingsHash);
    BF_CHECK(n.butterflyLocations == (size_t)kButterflyChecks,
             "N: %zu of the %d butterfly checks are locations with the option on", n.butterflyLocations,
             kButterflyChecks);
}

// ---- O, A and I: the hooks ---------------------------------------------------------------

/** A hook type's registry set aside for a scope (oot_shuffled_fairy_test.cpp): the
 *  bring-up can leave hooks registered that read a live play state this row does
 *  not have. */
template <typename H> struct HooksAside {
    decltype(GameInteractor::RegisteredGameHooks<H>::functions) functions;
    decltype(GameInteractor::RegisteredGameHooks<H>::functionsForID) forId;
    decltype(GameInteractor::RegisteredGameHooks<H>::functionsForPtr) forPtr;
    decltype(GameInteractor::RegisteredGameHooks<H>::functionsForFilter) filters;
    HooksAside() {
        Swap();
    }
    ~HooksAside() {
        Swap();
    }
    void Swap() {
        functions.swap(GameInteractor::RegisteredGameHooks<H>::functions);
        forId.swap(GameInteractor::RegisteredGameHooks<H>::functionsForID);
        forPtr.swap(GameInteractor::RegisteredGameHooks<H>::functionsForPtr);
        filters.swap(GameInteractor::RegisteredGameHooks<H>::functionsForFilter);
    }
};

// File-static: PlayState and Player are large.
PlayState sPlay;
Player sPlayer;
EnButte sButte;

size_t VbCount(GIVanillaBehavior vb) {
    auto& buckets = GameInteractor::RegisteredGameHooks<GameInteractor::OnVanillaBehavior>::functionsForID;
    auto it = buckets.find((int32_t)vb);
    return it == buckets.end() ? 0u : it->second.size();
}

/** A randomizer save; Link at the origin of Hyrule Castle holding a Deku Stick (or
 *  nothing); a butterfly 30 units away in its follow action, whose identity is
 *  RC_HC_NEAR_WALL_BUTTERFLY_FAIRY's (params 1, home 1476 high). */
void ArmFrame(bool holdingStick) {
    std::memset(&gSaveContext, 0, sizeof(SaveContext));
    gSaveContext.ship.quest.id = QUEST_RANDOMIZER;
    gSaveContext.gameMode = GAMEMODE_NORMAL;
    std::memset(&sPlay, 0, sizeof(PlayState));
    std::memset(&sPlayer, 0, sizeof(Player));
    std::memset(&sButte, 0, sizeof(EnButte));
    sPlay.sceneNum = SCENE_HYRULE_CASTLE;
    sPlay.actorCtx.actorLists[ACTORCAT_PLAYER].head = &sPlayer.actor;
    sPlayer.actor.id = ACTOR_PLAYER;
    sPlayer.actor.category = ACTORCAT_PLAYER;
    sPlayer.heldItemAction = holdingStick ? PLAYER_IA_DEKU_STICK : PLAYER_IA_NONE;
    sButte.actor.id = ACTOR_EN_BUTTE;
    sButte.actor.params = 1;
    sButte.actor.home.pos.y = 1476.0f;
    sButte.actor.xzDistToPlayer = 30.0f;
    sButte.actionFunc = nullptr; // a sentinel: only the easy hook may change it
}

void Register(bool shuffleOn) {
    Rando::Context::GetInstance()
        ->GetOption(RSK_SHUFFLE_BUTTERFLY_FAIRIES)
        .Set(shuffleOn ? RO_GENERIC_ON : RO_GENERIC_OFF);
    RegisterShuffleFairies();
    EasyButterflyFairies_Register();
}

void OffLeg() {
    ArmFrame(true);
    Register(false);
    BF_CHECK(VbCount(VB_SPAWN_BUTTERFLY_FAIRY) == 0 && VbCount(VB_SPAWN_BUTTERFLY_FAIRY_EASY) == 0,
             "O: with both off, %zu spawn and %zu easy hook(s) are registered", VbCount(VB_SPAWN_BUTTERFLY_FAIRY),
             VbCount(VB_SPAWN_BUTTERFLY_FAIRY_EASY));
    const bool easy = GameInteractor_Should(VB_SPAWN_BUTTERFLY_FAIRY_EASY, true, &sButte);
    BF_CHECK(easy && sButte.actionFunc == nullptr && sButte.timer == 0,
             "O: with both off the easy dispatch answered %d and changed the butterfly beside the stick", easy ? 1 : 0);
    BF_CHECK(GameInteractor_Should(VB_SPAWN_BUTTERFLY_FAIRY, true, &sButte),
             "O: with the shuffle off the spawn frame's vanilla fairy was vetoed");
    BF_CHECK(!GameInteractor_Should(VB_SPAWN_BUTTERFLY_FAIRY, false, &sButte),
             "O: with the shuffle off a frame that is not the spawn frame spawned a fairy");
}

void OnLeg() {
    ArmFrame(true);
    Register(true);
    BF_CHECK(VbCount(VB_SPAWN_BUTTERFLY_FAIRY) == 1 && VbCount(VB_SPAWN_BUTTERFLY_FAIRY_EASY) == 1,
             "A: with the shuffle on, %zu spawn and %zu easy hook(s) are registered, want 1 each",
             VbCount(VB_SPAWN_BUTTERFLY_FAIRY), VbCount(VB_SPAWN_BUTTERFLY_FAIRY_EASY));
    const bool easy = GameInteractor_Should(VB_SPAWN_BUTTERFLY_FAIRY_EASY, true, &sButte);
    printf("[TEST] A: stick beside the butterfly: should=%d action=%s timer=%d\n", easy ? 1 : 0,
           sButte.actionFunc == EnButte_TransformIntoFairy ? "transform" : "other", (int)sButte.timer);
    BF_CHECK(!easy && sButte.actionFunc == EnButte_TransformIntoFairy && sButte.timer == 9,
             "A: the easy hook did not turn the butterfly beside the held stick into a fairy");

    ArmFrame(false);
    const bool noStick = GameInteractor_Should(VB_SPAWN_BUTTERFLY_FAIRY_EASY, true, &sButte);
    BF_CHECK(noStick && sButte.actionFunc == nullptr, "A: control: with no stick held the easy hook still fired");

    ArmFrame(true);
    const CheckIdentity identity = ShuffleFairies_GetFairyIdentity(
        TWO_ACTOR_PARAMS(1, 1476), ACTOR_EN_BUTTE); // what the spawn hook asks, OoT_gPlayState = &sPlay
    BF_CHECK(identity.randomizerCheck == RC_HC_NEAR_WALL_BUTTERFLY_FAIRY,
             "A: the frame's butterfly resolves to check %d, not RC_HC_NEAR_WALL_BUTTERFLY_FAIRY",
             (int)identity.randomizerCheck);
    if (identity.randomizerInf != RAND_INF_MAX) {
        // Collected: SpawnFairy declines, so the vanilla fairy spawns (no actor
        // spawn here: OoT_Actor_Spawn needs a live play state).
        Flags_SetRandomizerInf(identity.randomizerInf);
        BF_CHECK(GameInteractor_Should(VB_SPAWN_BUTTERFLY_FAIRY, true, &sButte),
                 "A: a butterfly whose check is collected did not leave the vanilla fairy alone");
    }
    BF_CHECK(!GameInteractor_Should(VB_SPAWN_BUTTERFLY_FAIRY, false, &sButte),
             "A: a frame that is not the spawn frame was turned into one");
}

void IdentityLeg() {
    int resolved = 0;
    for (RandomizerCheck rc : ButterflyChecks()) {
        Rando::Location* loc = Rando::StaticData::GetLocation(rc);
        sPlay.sceneNum = loc->GetScene();
        const CheckIdentity butte = ShuffleFairies_GetFairyIdentity(loc->GetActorParams(), ACTOR_EN_BUTTE);
        const CheckIdentity elf = ShuffleFairies_GetFairyIdentity(loc->GetActorParams(), ACTOR_EN_ELF);
        const RandomizerInf inf = static_cast<RandomizerInf>(loc->GetCollectionCheck().flag);
        BF_CHECK(butte.randomizerCheck == rc && butte.randomizerInf == inf,
                 "I: %s resolves to check %d / flag %d through ACTOR_EN_BUTTE, want %d / %d", loc->GetName().c_str(),
                 (int)butte.randomizerCheck, (int)butte.randomizerInf, (int)rc, (int)inf);
        BF_CHECK(elf.randomizerCheck != rc, "I: %s also resolves through ACTOR_EN_ELF", loc->GetName().c_str());
        resolved += butte.randomizerCheck == rc ? 1 : 0;
    }
    printf("[TEST] I: %d of %d butterfly locations resolve through ACTOR_EN_BUTTE\n", resolved, kButterflyChecks);
}

void CheckHooks() {
    if (GameInteractor::Instance == nullptr) {
        GameInteractor::Instance = new GameInteractor();
    }
    if (Rando::StaticData::GetLocation(RC_HC_NEAR_WALL_BUTTERFLY_FAIRY)->GetName().empty()) {
        Rando::StaticData::InitLocationTable();
    }
    if (Rando::StaticData::CheckFromActorMultimap.empty()) {
        Rando::StaticData::InitHashMaps();
    }
    const std::shared_ptr<Rando::Context> ctx = Rando::Context::GetInstance();
    const uint8_t priorOption = ctx->GetOption(RSK_SHUFFLE_BUTTERFLY_FAIRIES).Get();
    SavedKey easyKey;
    easyKey.Save(kEasyKey);
    CVarClear(kEasyKey);
    std::unique_ptr<SaveContext> priorSave = std::make_unique<SaveContext>();
    std::memcpy(priorSave.get(), &gSaveContext, sizeof(SaveContext));
    PlayState* const priorPlay = OoT_gPlayState;
    const GameId priorGame = Context_GetCurrentGame();

    // The GameInteractor dispatch is gated off only under GAME_MM.
    Context_SetCurrentGame(GAME_OOT);
    OoT_gPlayState = &sPlay;
    {
        HooksAside<GameInteractor::OnVanillaBehavior> vb;
        OffLeg();
        OnLeg();
        IdentityLeg();
        // Unregister them again: a non-randomizer save turns every condition off.
        gSaveContext.ship.quest.id = QUEST_NORMAL;
        RegisterShuffleFairies();
        EasyButterflyFairies_Register();
    }
    ctx->GetOption(RSK_SHUFFLE_BUTTERFLY_FAIRIES).Set(priorOption);
    easyKey.Restore();
    std::memcpy(&gSaveContext, priorSave.get(), sizeof(SaveContext));
    OoT_gPlayState = priorPlay;
    Context_SetCurrentGame(priorGame);
}

int Finish(const char* row) {
    if (sFailures != 0) {
        printf("[TEST] FAIL (%s): %d failure(s)\n", row, sFailures);
        return 1;
    }
    printf("[TEST] PASS: %s\n", row);
    return 0;
}

bool HaveContext(const char* row) {
    if (OTRGlobals::Instance == nullptr || Rando::Context::GetInstance() == nullptr) {
        printf("[TEST] FAIL: %s needs OTRGlobals' randomizer context\n", row);
        return false;
    }
    return true;
}

} // namespace

/** @return 0 when every leg holds, 1 otherwise. */
extern "C" int OoT_ButterflyFairyShuffleTest_Run(void) {
    sRow = "oot-butterfly-fairy-shuffle";
    sFailures = 0;
    if (!HaveContext(sRow)) {
        return 1;
    }
    CheckOption();
    CheckFold();
    return Finish(sRow);
}

/** @return 0 when every leg holds, 1 otherwise. */
extern "C" int OoT_ButterflyFairyHooksTest_Run(void) {
    sRow = "oot-butterfly-fairy-hooks";
    sFailures = 0;
    if (!HaveContext(sRow)) {
        return 1;
    }
    CheckHooks();
    return Finish(sRow);
}

#else

extern "C" int OoT_ButterflyFairyShuffleTest_Run(void) {
    return 0;
}
extern "C" int OoT_ButterflyFairyHooksTest_Run(void) {
    return 0;
}

#endif
