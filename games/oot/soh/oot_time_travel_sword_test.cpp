/**
 * @file oot_time_travel_sword_test.cpp
 * @brief #1434: OoTMM's "Time Travel requires Master Sword" (timeTravelSword,
 *        packages/core/src/settings/data.ts:1240-1246, default true) in Ocarina of
 *        Time's randomizer: RSK_TIME_TRAVEL_REQUIRES_MASTER_SWORD
 *        (Enhancements/randomizer/TimeTravelSword.cpp). Row OoTTimeTravelSword
 *        (CTest label `rando`), dispatch `oot-time-travel-sword` in
 *        src/common/test_runner.cpp; the bring-up is master-sword-respawn's
 *        (OTRGlobals' randomizer context). No ROM, no generation, no display.
 *
 *  D  THE DEFAULT. With its key cleared, the option reads On, as OoTMM ships it;
 *     it is named, keyed, and drawn in Shuffle Items right after Shuffle Master
 *     Sword; Off is 0 (what a world saved before the option reads, leg S).
 *  R  THE RULE, pure: travel is refused exactly when the option is on, the sword
 *     is shuffled and Link does not own it.
 *  P  THE PEDESTAL: the real wait action of the Pedestal of Time
 *     (z_bg_toki_swd.c's func_808BAF40) on a stub frame, Link in reach and facing
 *     it, with the randomizer's real vanilla-behaviour handler registered as a
 *     randomizer load registers it.
 *       P1 child, option on, sword shuffled and not owned: the pull is NOT offered.
 *       P2 the same child owning the sword: offered.
 *       P3 adult, not owned: NOT offered (OoTMM's pedestal asks at either age).
 *       P4 the same adult owning it: offered.
 *       P5 the option off, not owned: offered (Ship of Harkinian's pedestal; also
 *          proves the stub frame reaches the offer, so P1 and P3 are not vacuous).
 *       P6 the option on, the sword NOT shuffled, not owned: offered (the pull
 *          hands the sword over, VB_GIVE_ITEM_MASTER_SWORD).
 *       P7 a vanilla file, the randomizer's handler gone: offered.
 *     P1 to P6 run with the pedestal's own check already obtained, so the child
 *     give below (and its hold on the pull) is out of their way.
 *  G  THE CHILD GIVE (#1484, OoTMM's child pedestal check): the pedestal's frame
 *     as z_actor.c runs it, the wait action and then the OnActorUpdate dispatch,
 *     with the randomizer's real vanilla-behaviour, actor-update and flag-set
 *     handlers registered as a randomizer load registers them, the pedestal
 *     holding an item.
 *       P8  child in reach, sword shuffled, no sword: RAND_INF_TOT_MASTER_SWORD is
 *           set, RC_TOT_MASTER_SWORD queued exactly once over three frames, and no
 *           pull offered.
 *       P9  adult in reach: no give.
 *       P10 a save that pulled before this change (the pull's EventChkInf set, the
 *           check SAVED): no give.
 *       P11 the flag maps: the pull's EventChkInf no longer resolves to the check,
 *           RAND_INF_TOT_MASTER_SWORD does.
 *       P12 the sword not shuffled: no approach give; the pull's give
 *           (VB_GIVE_ITEM_MASTER_SWORD) still hands the vanilla sword over, marks
 *           the check and sets RAND_INF_TOT_MASTER_SWORD, queueing nothing.
 *       P13 child in reach HOLDING the sword: the pull waits while the give is
 *           pending or queued, and is offered once the check is obtained.
 *       P14 a child out of reach (250 across): no give.
 *       C1  the adult start's creation flag (savefile.cpp,
 *           Randomizer_SetPedestalFlagAfterCreationGive) on a check the creation
 *           give marked SAVED: the flag is set, the check stays SAVED, nothing
 *           queued.
 *  L  THE LOGIC: OoT's real search (3drando/fill.cpp's ReachabilitySearch, whose
 *     PropagateTimeTravel is the one place an age reaches RR_ROOT through the
 *     pedestal), started in the Temple of Time with the Door of Time open, over
 *     an empty world (nothing placed, nothing to collect), on the search's own
 *     save holding the sword or not.
 *       L1 child start, option on, shuffled, no sword: no adult RR_ROOT, and the
 *          pedestal's check RC_TOT_MASTER_SWORD IS reached: with the sword
 *          shuffled it is a child check beyond the Door of Time (#1484).
 *       L2 the same holding the sword: adult, and the check reached.
 *       L3 the option off, no sword: adult, and the check reached (L1's control).
 *       L4 the option on, the sword not shuffled, no sword: adult.
 *       L5 adult start, option on, shuffled, no sword: no child RR_ROOT, so the
 *          child-only crossing to Termina (ComboLogicEngineOoT.cpp's crossingOpen,
 *          RR_MARKET_MASK_SHOP->Child()) waits for the sword.
 *       L6 the same holding the sword: child.
 *       L7 the option off, no sword: child (L5's control).
 *       L8 the row itself, evaluated at each age: the sword not shuffled, adult
 *          only (L4 passes under either row, the pull being free); L9 shuffled,
 *          child only.
 *  S  A STORED WORLD KEEPS ITS OWN PEDESTAL: the OoT world a save and a seed
 *     package carry (SaveManager's randomizer section) loads the option as it was
 *     stored (on), and the same world with its randoSettings array cut before the
 *     option, as every world saved before #1434 is, loads it off. Last: the load
 *     replaces the randomizer context.
 *
 * #1484 RED HALVES, derived, not run (this lane had no local build): on
 * origin/main L1 and L9 fail (the row is IsAdult), P8 fails (no give), P11 fails
 * (the location still names the pull's flag), P13 fails without the pull's hold,
 * P12 fails without the unshuffled pull's flag, and C1 fails without the
 * creation's status put back once the flag resolves to the check.
 *
 * RED HALVES, derived, not run (this lane had no local build): on origin/main the
 * row does not compile (no RSK_TIME_TRAVEL_REQUIRES_MASTER_SWORD); with the enum,
 * option and handler but without the pedestal's VB_BE_ABLE_TO_TIME_TRAVEL_AT_PEDESTAL
 * ask, P1 and P3 fail; without PropagateTimeTravel's ask, L1 and L5 fail; with the
 * option's default left Off, D fails.
 *
 * Every key, option and save field a leg changes is put back (in memory: nothing
 * here writes a config file), except leg S's context, which a load replaces.
 */
#ifdef RSBS_SINGLE_EXECUTABLE

#include <algorithm>
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include <libultraship/bridge.h>
#include <nlohmann/json.hpp>

#include "soh/OTRGlobals.h"
#include "soh/cvar_prefixes.h"
#include "soh/Enhancements/randomizer/randomizerTypes.h"
#include "soh/Enhancements/randomizer/static_data.h"
#include "soh/Enhancements/randomizer/location_access.h"
#include "soh/Enhancements/randomizer/SeedContext.h"
#include "soh/Enhancements/randomizer/logic.h"
#include "soh/Enhancements/randomizer/settings.h"
#include "soh/Enhancements/randomizer/TimeTravelSword.h"
#include "soh/Enhancements/randomizer/3drando/fill.hpp"               // ReachabilitySearch
#include "soh/Enhancements/randomizer/3drando/starting_inventory.hpp" // StartingInventory
#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/Enhancements/game-interactor/GameInteractor_Hooks.h"

#include "context.h" // src/common: Context_GetCurrentGame, Context_SetCurrentGame
#include "game.h"

// At file scope, not inside an extern "C" block (see oot_shared_kokiri_sword_test.cpp).
#include <z64.h>
extern "C" {
#include "macros.h"
#include "variables.h"
#include "src/overlays/actors/ovl_Bg_Toki_Swd/z_bg_toki_swd.h"
// z_bg_toki_swd.c: the pedestal's wait action (external linkage there), where it
// offers the pull.
void func_808BAF40(BgTokiSwd* thisx, PlayState* play);
extern PlayState* OoT_gPlayState;
// hook_handlers.cpp (#849, #1204 test bridges): empty the randomizer's check queue,
// writing up to `cap` of its checks to `out`; set or clear the give slot.
int OoT_Rando_TestTakeQueuedChecks(uint16_t* out, int cap);
void OoT_Rando_TestQueueGive(uint16_t rc, const GetItemEntry* entry);
// savefile.cpp (#1484): the adult start's pedestal flag after its creation give.
void Randomizer_SetPedestalFlagAfterCreationGive(void);
}

// hook_handlers.cpp: the handlers RandomizerRegisterHooks registers on a randomizer
// file load, and the flag -> check map the flag-set handler queues through.
void RandomizerOnVanillaBehaviorHandler(GIVanillaBehavior id, bool* should, va_list originalArgs);
void RandomizerOnActorUpdateHandler(void* refActor);
void RandomizerOnFlagSetHandler(int16_t flagType, int16_t flag);
RandomizerCheck GetRandomizerCheckFromFlag(int16_t flagType, int16_t flag);
// SaveManager.cpp (#1044): the OoT world a save and a seed package carry, written
// and read back by the functions a save uses.
std::string OoT_SeedPackage_SerializeOoTWorld();
bool OoT_SeedPackage_LoadOoTWorld(const std::string& sectionJson);

static_assert(RO_GENERIC_OFF == 0, "#1434: a world saved before the option reads its missing entry as 0, which must "
                                   "be Off, so the world keeps the pedestal it was generated with");

namespace {

int sFailures = 0;

void TtsFail(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    printf("[TEST] oot-time-travel-sword: FAIL: ");
    vprintf(fmt, args);
    printf("\n");
    va_end(args);
    sFailures++;
}

#define TTS_CHECK(cond, ...)      \
    do {                          \
        if (!(cond)) {            \
            TtsFail(__VA_ARGS__); \
        }                         \
    } while (0)

/** A hook type's registry set aside for a scope (oot_master_sword_respawn_test.cpp):
 *  the bring-up can leave vanilla-behaviour hooks registered that read a live play
 *  state this row does not have. */
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

const char* const kOptionKey = CVAR_RANDOMIZER_SETTING("TimeTravelRequiresMasterSword");

/** The world options the legs set, put back by RestoreOptions. */
const RandomizerSettingKey kTouched[] = {
    RSK_TIME_TRAVEL_REQUIRES_MASTER_SWORD,
    RSK_SHUFFLE_MASTER_SWORD,
    RSK_STARTING_MASTER_SWORD,
    RSK_SELECTED_STARTING_AGE,
    RSK_DOOR_OF_TIME,
    RSK_LOGIC_RULES,
};
constexpr int kTouchedCount = (int)(sizeof(kTouched) / sizeof(kTouched[0]));
uint8_t sSavedOptions[kTouchedCount];

void SaveOptions(Rando::Context& world) {
    for (int i = 0; i < kTouchedCount; i++) {
        sSavedOptions[i] = world.GetOption(kTouched[i]).Get();
    }
}

void RestoreOptions(Rando::Context& world) {
    for (int i = 0; i < kTouchedCount; i++) {
        world.GetOption(kTouched[i]).Set(sSavedOptions[i]);
    }
}

/** The pedestal's two frozen terms (the sword never a starting item here). */
void SetRule(Rando::Context& world, bool requiresSword, bool swordShuffled) {
    world.GetOption(RSK_TIME_TRAVEL_REQUIRES_MASTER_SWORD).Set(requiresSword ? RO_GENERIC_ON : RO_GENERIC_OFF);
    world.GetOption(RSK_SHUFFLE_MASTER_SWORD).Set(swordShuffled ? RO_GENERIC_ON : RO_GENERIC_OFF);
    world.GetOption(RSK_STARTING_MASTER_SWORD).Set(RO_GENERIC_OFF);
}

// ---- D: the default ---------------------------------------------------------------

int IndexInGroup(const std::vector<Rando::Option*>& options, const Rando::Option* option) {
    for (size_t i = 0; i < options.size(); i++) {
        if (options[i] == option) {
            return (int)i;
        }
    }
    return -1;
}

void CheckDefault() {
    auto settings = Rando::Settings::GetInstance();
    settings->CreateOptions(); // idempotent; what Rando_HeadlessSeedTest calls first
    const bool existed = CVarExists(kOptionKey);
    const int32_t stored = CVarGetInteger(kOptionKey, 0);
    CVarClear(kOptionKey);

    Rando::Option& option = settings->GetOption(RSK_TIME_TRAVEL_REQUIRES_MASTER_SWORD);
    const int shipped = option.GetOptionIndex();
    const auto& shuffles = settings->GetOptionGroup(RSG_MENU_SECTION_BASIC_SHUFFLES).GetOptions();
    const int at = IndexInGroup(shuffles, &option);
    const int swordAt = IndexInGroup(shuffles, &settings->GetOption(RSK_SHUFFLE_MASTER_SWORD));
    printf("[TEST] D: '%s' (key %s): %d with no key stored (On %d); Shuffle Items row %d, Shuffle Master Sword row "
           "%d\n",
           option.GetName().c_str(), option.GetCVarName().c_str(), shipped, (int)RO_GENERIC_ON, at, swordAt);
    TTS_CHECK(option.GetName() == "Time Travel Requires Master Sword",
              "D: the option is not named 'Time Travel Requires Master Sword' (named '%s')", option.GetName().c_str());
    TTS_CHECK(option.GetCVarName() == kOptionKey, "D: the option is not keyed %s", kOptionKey);
    TTS_CHECK(shipped == RO_GENERIC_ON, "D: the shipped default is %d, not On (OoTMM's timeTravelSword is true)",
              shipped);
    TTS_CHECK(swordAt >= 0 && at == swordAt + 1,
              "D: the option is not drawn in Shuffle Items right after Shuffle Master Sword (row %d, the sword's %d)",
              at, swordAt);

    if (existed) {
        CVarSetInteger(kOptionKey, stored);
    } else {
        CVarClear(kOptionKey);
    }
}

// ---- R: the rule ---------------------------------------------------------------------

void CheckRule() {
    int rows = 0;
    for (int option = 0; option < 2; option++) {
        for (int shuffled = 0; shuffled < 2; shuffled++) {
            for (int owns = 0; owns < 2; owns++) {
                const bool want = !(option && shuffled && !owns);
                const bool got = OoT_TimeTravelSword_Allows(option != 0, shuffled != 0, owns != 0);
                TTS_CHECK(got == want, "R: option %d, shuffled %d, owns %d: travel %s, want %s", option, shuffled, owns,
                          got ? "allowed" : "refused", want ? "allowed" : "refused");
                rows++;
            }
        }
    }
    printf("[TEST] R: %d rows of the rule checked\n", rows);
}

// ---- P: the pedestal -------------------------------------------------------------------

// File-static: PlayState and Player are large.
PlayState sPlay;
Player sPlayer;
BgTokiSwd sSwd;

/** Link (adult or child) on a file of @p questId, in reach of the pedestal and
 *  facing it, owning the Master Sword or not, after the chamber's first-visit
 *  cutscene. */
void ArmPedestal(uint8_t questId, bool adult, bool ownsSword) {
    std::memset(&gSaveContext, 0, sizeof(SaveContext));
    std::memset(gSaveContext.inventory.items, ITEM_NONE, sizeof(gSaveContext.inventory.items));
    gSaveContext.ship.quest.id = questId;
    gSaveContext.gameMode = GAMEMODE_NORMAL;
    gSaveContext.linkAge = adult ? LINK_AGE_ADULT : LINK_AGE_CHILD;
    SET_EVENTCHKINF(EVENTCHKINF_ENTERED_MASTER_SWORD_CHAMBER);
    if (ownsSword) {
        gSaveContext.inventory.equipment |= OWNED_EQUIP_FLAG(EQUIP_TYPE_SWORD, EQUIP_INV_SWORD_MASTER);
    }

    std::memset(&sPlay, 0, sizeof(PlayState));
    std::memset(&sPlayer, 0, sizeof(Player));
    std::memset(&sSwd, 0, sizeof(BgTokiSwd));
    sPlay.actorCtx.actorLists[ACTORCAT_PLAYER].head = &sPlayer.actor;
    sPlayer.actor.id = ACTOR_PLAYER;
    // OoT_Actor_OfferGetItem offers when getItemDirection (0) < |yawTowardsPlayer - Link's yaw|.
    sPlayer.actor.shape.rot.y = 0x4000;
    sSwd.actor.id = ACTOR_BG_TOKI_SWD;
    sSwd.actor.xzDistToPlayer = 20.0f; // inside OoT_Actor_OfferCarry's 50 x 10 reach
    sSwd.actor.yDistToPlayer = 0.0f;
    sSwd.actor.yawTowardsPlayer = 0; // facing Link: OoT_Actor_IsFacingPlayer(0x2000)
    sSwd.actor.shape.rot.y = 0;
}

/** One frame of the pedestal's wait action. Returns 1 when the pull was offered.
 *  Link's offer slot is cleared first, as his own update clears it each frame. */
int PedestalWaitFrame() {
    sPlayer.interactRangeActor = nullptr;
    sPlayer.getItemDirection = 0;
    func_808BAF40(&sSwd, &sPlay);
    return sPlayer.interactRangeActor == &sSwd.actor ? 1 : 0;
}

/** One frame of the pedestal's wait action on an armed Link. */
int PedestalOffers(uint8_t questId, bool adult, bool ownsSword) {
    ArmPedestal(questId, adult, ownsSword);
    return PedestalWaitFrame();
}

void PedestalLeg(Rando::Context& world, const char* leg, bool requiresSword, bool swordShuffled, bool adult,
                 bool ownsSword, int want) {
    SetRule(world, requiresSword, swordShuffled);
    const int got = PedestalOffers(QUEST_RANDOMIZER, adult, ownsSword);
    printf("[TEST] %s: option %d, shuffled %d, %s, owns the sword %d: the pull is %s\n", leg, requiresSword ? 1 : 0,
           swordShuffled ? 1 : 0, adult ? "adult" : "child", ownsSword ? 1 : 0, got ? "offered" : "not offered");
    TTS_CHECK(got == want, "%s: the pedestal %s the pull, want %s", leg, got ? "offered" : "did not offer",
              want ? "offered" : "not offered");
}

void CheckPedestal(Rando::Context& world) {
    if (GameInteractor::Instance == nullptr) {
        GameInteractor::Instance = new GameInteractor();
    }
    HooksAside<GameInteractor::OnVanillaBehavior> aside;
    const HOOK_ID vbHook = GameInteractor::Instance->RegisterGameHook<GameInteractor::OnVanillaBehavior>(
        RandomizerOnVanillaBehaviorHandler);
    // The pedestal's own check already given (#1484): the pull's hold for the child
    // give (legs G) is not what these legs measure.
    Rando::ItemLocation* pedestal = world.GetItemLocation(RC_TOT_MASTER_SWORD);
    const RandomizerCheckStatus priorStatus = pedestal->GetCheckStatus();
    pedestal->SetCheckStatus(RCSHOW_COLLECTED);

    PedestalLeg(world, "P1", true, true, false, false, 0);
    PedestalLeg(world, "P2", true, true, false, true, 1);
    PedestalLeg(world, "P3", true, true, true, false, 0);
    PedestalLeg(world, "P4", true, true, true, true, 1);
    PedestalLeg(world, "P5", false, true, false, false, 1);
    PedestalLeg(world, "P6", true, false, false, false, 1);

    // P7: a vanilla file, where a load leaves no randomizer hook (the unregister
    // takes effect at this frame's dispatch), the world's option still on.
    GameInteractor::Instance->UnregisterGameHook<GameInteractor::OnVanillaBehavior>(vbHook);
    SetRule(world, true, true);
    const int vanilla = PedestalOffers(QUEST_NORMAL, false, false);
    printf("[TEST] P7: a vanilla file, no sword: the pull is %s\n", vanilla ? "offered" : "not offered");
    TTS_CHECK(vanilla == 1, "P7: a vanilla file's pedestal did not offer the pull");
    pedestal->SetCheckStatus(priorStatus);
}

// ---- G: the child give (#1484) -----------------------------------------------------------

bool SaveHasRandInf(RandomizerInf flag) {
    return (gSaveContext.ship.randomizerInf[flag >> 4] & (1u << (flag & 0xF))) != 0;
}

/** One frame of the pedestal as z_actor.c's Actor_UpdateAll runs it: the actor's
 *  update (its wait action), then GameInteractor_ExecuteOnActorUpdate. Returns 1
 *  when the pull was offered. */
int PedestalFrame() {
    const int offered = PedestalWaitFrame();
    GameInteractor_ExecuteOnActorUpdate(&sSwd.actor);
    return offered;
}

/** Runs @p frames pedestal frames; returns how many offered the pull. */
int PedestalFrames(int frames) {
    int offered = 0;
    for (int i = 0; i < frames; i++) {
        offered += PedestalFrame();
    }
    return offered;
}

/** Empties the randomizer's check queue and give slot; returns how many checks
 *  were queued and whether RC_TOT_MASTER_SWORD was the only one. */
int TakeQueued(bool* onlyPedestal) {
    uint16_t queued[8] = {};
    const int n = OoT_Rando_TestTakeQueuedChecks(queued, 8);
    *onlyPedestal = n >= 1;
    for (int i = 0; i < n && i < 8; i++) {
        *onlyPedestal = *onlyPedestal && queued[i] == (uint16_t)RC_TOT_MASTER_SWORD;
    }
    OoT_Rando_TestQueueGive(0, nullptr);
    return n;
}

void CheckChildGive(Rando::Context& world) {
    if (GameInteractor::Instance == nullptr) {
        GameInteractor::Instance = new GameInteractor();
    }
    if (Rando::StaticData::GetLocation(RC_TOT_MASTER_SWORD)->GetName().empty()) {
        Rando::StaticData::InitLocationTable(); // the flag -> check map's table
    }
    HooksAside<GameInteractor::OnVanillaBehavior> vbAside;
    HooksAside<GameInteractor::OnActorUpdate> updateAside;
    HooksAside<GameInteractor::OnFlagSet> flagAside;
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnVanillaBehavior>(RandomizerOnVanillaBehaviorHandler);
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnActorUpdate>(RandomizerOnActorUpdateHandler);
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnFlagSet>(RandomizerOnFlagSetHandler);

    PlayState* const priorPlay = OoT_gPlayState;
    OoT_gPlayState = &sPlay; // the hook reads the live play state, as in play
    Rando::ItemLocation* pedestal = world.GetItemLocation(RC_TOT_MASTER_SWORD);
    const RandomizerGet priorItem = pedestal->GetPlacedRandomizerGet();
    const RandomizerCheckStatus priorStatus = pedestal->GetCheckStatus();
    // The flag-set handler queues only a location holding an item.
    pedestal->SetPlacedItem(RG_PIECE_OF_HEART);
    bool only = false;
    (void)TakeQueued(&only);

    // P8: a child in reach without the sword, the option on and the sword shuffled.
    SetRule(world, true, true);
    pedestal->SetCheckStatus(RCSHOW_UNCHECKED);
    ArmPedestal(QUEST_RANDOMIZER, false, false);
    int offered = PedestalFrames(3);
    bool given = SaveHasRandInf(RAND_INF_TOT_MASTER_SWORD);
    int queued = TakeQueued(&only);
    printf("[TEST] P8: child in reach, shuffled, no sword: flag %d, %d check(s) queued%s over 3 frames, the pull "
           "offered %d time(s)\n",
           given ? 1 : 0, queued, only ? " (the pedestal)" : "", offered);
    TTS_CHECK(given, "P8: the child in reach was not handed the pedestal's check (RAND_INF_TOT_MASTER_SWORD unset)");
    TTS_CHECK(queued == 1 && only, "P8: the pedestal's check was queued %d time(s), want exactly once", queued);
    TTS_CHECK(offered == 0, "P8: the pull was offered to a child without the sword");

    // P9: an adult in reach: the child check waits for a child.
    pedestal->SetCheckStatus(RCSHOW_UNCHECKED);
    ArmPedestal(QUEST_RANDOMIZER, true, false);
    (void)PedestalFrames(3);
    given = SaveHasRandInf(RAND_INF_TOT_MASTER_SWORD);
    queued = TakeQueued(&only);
    printf("[TEST] P9: adult in reach: flag %d, %d check(s) queued\n", given ? 1 : 0, queued);
    TTS_CHECK(!given && queued == 0, "P9: an adult was handed the child's pedestal check");

    // P10: a save that pulled before #1484 (the pull's flag gave the check then).
    pedestal->SetCheckStatus(RCSHOW_SAVED);
    ArmPedestal(QUEST_RANDOMIZER, false, false);
    SET_EVENTCHKINF(EVENTCHKINF_PULLED_MASTER_SWORD_FROM_PEDESTAL);
    (void)PedestalFrames(3);
    given = SaveHasRandInf(RAND_INF_TOT_MASTER_SWORD);
    queued = TakeQueued(&only);
    printf("[TEST] P10: pulled before the change: flag %d, %d check(s) queued, status %d\n", given ? 1 : 0, queued,
           (int)pedestal->GetCheckStatus());
    TTS_CHECK(!given && queued == 0, "P10: a save that already pulled was handed the pedestal's check again");

    // P14: a child out of reach (OoTMM's 200 across).
    pedestal->SetCheckStatus(RCSHOW_UNCHECKED);
    ArmPedestal(QUEST_RANDOMIZER, false, false);
    sSwd.actor.xzDistToPlayer = 250.0f;
    (void)PedestalFrames(3);
    given = SaveHasRandInf(RAND_INF_TOT_MASTER_SWORD);
    queued = TakeQueued(&only);
    printf("[TEST] P14: child 250 away: flag %d, %d check(s) queued\n", given ? 1 : 0, queued);
    TTS_CHECK(!given && queued == 0, "P14: a child out of reach was handed the pedestal's check");

    // P13: a child in reach holding the sword: the pull waits for the give.
    pedestal->SetCheckStatus(RCSHOW_UNCHECKED);
    ArmPedestal(QUEST_RANDOMIZER, false, true);
    const int firstFrame = PedestalFrame();  // not given yet: the give comes in this frame's update
    const int queuedFrame = PedestalFrame(); // given, still in the queue
    queued = TakeQueued(&only);
    pedestal->SetCheckStatus(RCSHOW_COLLECTED); // the give landed
    const int landedFrame = PedestalFrame();
    printf("[TEST] P13: child holding the sword: the pull offered %d before the give, %d while it is queued, %d once "
           "it landed (%d queued)\n",
           firstFrame, queuedFrame, landedFrame, queued);
    TTS_CHECK(firstFrame == 0 && queuedFrame == 0,
              "P13: the pull was offered before the pedestal's give landed (the child would take its item as adult)");
    TTS_CHECK(queued == 1 && only, "P13: the pedestal's check was queued %d time(s), want once", queued);
    TTS_CHECK(landedFrame == 1, "P13: the pull stayed withheld after the pedestal's give landed");

    // P11: the flag maps the flag-set handler and the load seam go through.
    const RandomizerCheck fromPull =
        GetRandomizerCheckFromFlag(FLAG_EVENT_CHECK_INF, EVENTCHKINF_PULLED_MASTER_SWORD_FROM_PEDESTAL);
    const RandomizerCheck fromGive = GetRandomizerCheckFromFlag(FLAG_RANDOMIZER_INF, RAND_INF_TOT_MASTER_SWORD);
    printf("[TEST] P11: the pull's flag resolves to check %d, RAND_INF_TOT_MASTER_SWORD to %d (pedestal %d)\n",
           (int)fromPull, (int)fromGive, (int)RC_TOT_MASTER_SWORD);
    TTS_CHECK(fromPull != RC_TOT_MASTER_SWORD, "P11: the pull's EventChkInf still resolves to the pedestal's check");
    TTS_CHECK(fromGive == RC_TOT_MASTER_SWORD, "P11: RAND_INF_TOT_MASTER_SWORD does not resolve to the pedestal");

    // P12: the sword not shuffled: the pull hands the vanilla sword over.
    SetRule(world, true, false);
    pedestal->SetCheckStatus(RCSHOW_UNCHECKED);
    ArmPedestal(QUEST_RANDOMIZER, false, false);
    (void)PedestalFrames(3);
    const bool approachGave = SaveHasRandInf(RAND_INF_TOT_MASTER_SWORD);
    const bool handsOver = GameInteractor_Should(VB_GIVE_ITEM_MASTER_SWORD, true);
    given = SaveHasRandInf(RAND_INF_TOT_MASTER_SWORD);
    queued = TakeQueued(&only);
    const RandomizerCheckStatus pulled = pedestal->GetCheckStatus();
    printf("[TEST] P12: unshuffled: approach flag %d; the pull hands the sword over %d, flag %d, status %d, %d "
           "queued\n",
           approachGave ? 1 : 0, handsOver ? 1 : 0, given ? 1 : 0, (int)pulled, queued);
    TTS_CHECK(!approachGave, "P12: an unshuffled pedestal handed its check over on approach");
    TTS_CHECK(handsOver, "P12: the unshuffled pull no longer hands the vanilla sword over");
    TTS_CHECK(pulled == RCSHOW_COLLECTED && given,
              "P12: the unshuffled pull did not mark the check and set RAND_INF_TOT_MASTER_SWORD (the load seam "
              "follows that flag)");
    TTS_CHECK(queued == 0, "P12: the unshuffled pull queued the pedestal's check (%d), a second give", queued);

    // C1: the adult start's creation give (savefile.cpp): StartingItemGive marked
    // the check SAVED, then the flag is set with the flag-set handler registered.
    SetRule(world, true, true);
    std::memset(&gSaveContext, 0, sizeof(SaveContext));
    gSaveContext.ship.quest.id = QUEST_RANDOMIZER;
    gSaveContext.linkAge = LINK_AGE_ADULT;
    pedestal->SetCheckStatus(RCSHOW_SAVED);
    Randomizer_SetPedestalFlagAfterCreationGive();
    given = SaveHasRandInf(RAND_INF_TOT_MASTER_SWORD);
    queued = TakeQueued(&only);
    const RandomizerCheckStatus created = pedestal->GetCheckStatus();
    printf("[TEST] C1: adult-start creation: flag %d, status %d (SAVED %d), %d queued\n", given ? 1 : 0, (int)created,
           (int)RCSHOW_SAVED, queued);
    TTS_CHECK(given, "C1: the adult start's creation did not set RAND_INF_TOT_MASTER_SWORD");
    TTS_CHECK(created == RCSHOW_SAVED, "C1: the creation give's SAVED status was changed to %d", (int)created);
    TTS_CHECK(queued == 0, "C1: the adult start's creation queued the pedestal's check (%d), a second give", queued);

    pedestal->SetPlacedItem(priorItem);
    pedestal->SetCheckStatus(priorStatus);
    OoT_gPlayState = priorPlay;
}

// ---- L: the logic ------------------------------------------------------------------------

struct SearchResult {
    bool rootAdult;
    bool rootChild;
    bool pedestalCheck;
};

/** OoT's search from the Temple of Time (Door of Time open) at the starting age's
 *  day, on a fresh logic save holding the Master Sword or not. */
SearchResult SearchFromTemple(Rando::Context& world, bool adultStart, bool ownsSword) {
    world.GetOption(RSK_SELECTED_STARTING_AGE).Set(adultStart ? RO_AGE_ADULT : RO_AGE_CHILD);
    logic->Reset(true);
    logic->CurrentCheckKey = RC_UNKNOWN_CHECK;
    logic->CalculatingAvailableChecks = false; // Item::ApplyEffect grants nothing while it is set
    if (ownsSword) {
        Rando::StaticData::RetrieveItem(RG_MASTER_SWORD).ApplyEffect();
    }
    const std::vector<RandomizerCheck> reached = ReachabilitySearch(
        { RC_TOT_MASTER_SWORD }, RG_NONE, false, RR_TEMPLE_OF_TIME, adultStart ? RAT_ADULT_DAY : RAT_CHILD_DAY);
    SearchResult r;
    r.rootAdult = RegionTable(RR_ROOT)->Adult();
    r.rootChild = RegionTable(RR_ROOT)->Child();
    r.pedestalCheck = std::find(reached.begin(), reached.end(), RC_TOT_MASTER_SWORD) != reached.end();
    return r;
}

void LogicLeg(Rando::Context& world, const char* leg, bool adultStart, bool requiresSword, bool swordShuffled,
              bool ownsSword, bool wantOtherAge) {
    SetRule(world, requiresSword, swordShuffled);
    const SearchResult r = SearchFromTemple(world, adultStart, ownsSword);
    const bool otherAge = adultStart ? r.rootChild : r.rootAdult;
    printf("[TEST] %s: %s start, option %d, shuffled %d, holds the sword %d: RR_ROOT as %s %d; the pedestal check "
           "%d\n",
           leg, adultStart ? "adult" : "child", requiresSword ? 1 : 0, swordShuffled ? 1 : 0, ownsSword ? 1 : 0,
           adultStart ? "child" : "adult", otherAge ? 1 : 0, r.pedestalCheck ? 1 : 0);
    TTS_CHECK(otherAge == wantOtherAge, "%s: logic %s Link through time, want %s", leg, otherAge ? "sends" : "keeps",
              wantOtherAge ? "sent" : "kept");
    if (!adultStart) {
        // #1484: with the sword shuffled RC_TOT_MASTER_SWORD is a CHILD check beyond
        // the Door of Time (open here, so the child is beyond it): always reached.
        // Unshuffled it is the adult check of the pull (and a root check only for an
        // adult start): reached exactly when the adult side is.
        const bool wantPedestal = swordShuffled || wantOtherAge;
        TTS_CHECK(r.pedestalCheck == wantPedestal, "%s: the pedestal's check is %s, want %s", leg,
                  r.pedestalCheck ? "reached" : "not reached", wantPedestal ? "reached" : "not reached");
    }
}

/** The pedestal's row beyond the Door of Time, evaluated at one age (day) the way
 *  LocationAccess::ConditionsMet asks it. -1 when the row is missing. */
int PedestalRowAt(bool adult) {
    for (const LocationAccess& row : RegionTable(RR_TOT_BEYOND_DOOR_OF_TIME)->locations) {
        if (row.GetLocation() == RC_TOT_MASTER_SWORD) {
            return row.CheckConditionAtAgeTime(adult ? logic->IsAdult : logic->IsChild, logic->AtDay) ? 1 : 0;
        }
    }
    return -1;
}

void RowLeg(Rando::Context& world, const char* leg, bool swordShuffled, int wantChild, int wantAdult) {
    SetRule(world, true, swordShuffled);
    const int child = PedestalRowAt(false);
    const int adult = PedestalRowAt(true);
    printf("[TEST] %s: shuffled %d: the pedestal's row holds for a child %d, for an adult %d\n", leg,
           swordShuffled ? 1 : 0, child, adult);
    TTS_CHECK(child == wantChild && adult == wantAdult,
              "%s: the pedestal's row is child %d / adult %d, want child %d / adult %d", leg, child, adult, wantChild,
              wantAdult);
}

void CheckLogic(Rando::Context& world) {
    world.GetOption(RSK_LOGIC_RULES).Set(RO_LOGIC_GLITCHLESS);
    world.GetOption(RSK_DOOR_OF_TIME).Set(RO_DOOROFTIME_OPEN);
    // The world graph, bound to this context's logic (location_access.cpp).
    RegionTable_Init();
    if (logic == nullptr) {
        TtsFail("L: RegionTable_Init bound no logic object");
        return;
    }
    // An empty world: nothing placed, so the search collects nothing, and no
    // starting item (ApplyStartingInventory runs at the search's start).
    if (world.GetItemLocation(RC_TOT_MASTER_SWORD)->GetPlacedRandomizerGet() != RG_NONE) {
        TtsFail("L: RC_TOT_MASTER_SWORD holds an item, so the search could not report reaching it");
        return;
    }
    for (int rc = RC_UNKNOWN_CHECK + 1; rc < RC_MAX; rc++) {
        if (world.GetItemLocation((RandomizerCheck)rc)->GetPlacedRandomizerGet() == RG_MASTER_SWORD) {
            TtsFail("L: check %d holds the Master Sword, so a search could find it", rc);
            return;
        }
    }
    const std::vector<RandomizerGet> savedStarting = StartingInventory;
    StartingInventory.clear();

    LogicLeg(world, "L1", false, true, true, false, false);
    LogicLeg(world, "L2", false, true, true, true, true);
    LogicLeg(world, "L3", false, false, true, false, true);
    LogicLeg(world, "L4", false, true, false, false, true);
    LogicLeg(world, "L5", true, true, true, false, false);
    LogicLeg(world, "L6", true, true, true, true, true);
    LogicLeg(world, "L7", true, false, true, false, true);
    RowLeg(world, "L8", false, 0, 1);
    RowLeg(world, "L9", true, 1, 0);

    StartingInventory = savedStarting;
}

// ---- S: a stored world keeps its own pedestal ------------------------------------------------

int LoadedOption(const nlohmann::json& section) {
    if (!OoT_SeedPackage_LoadOoTWorld(section.dump())) {
        return -1;
    }
    auto loaded = Rando::Context::GetInstance();
    return loaded != nullptr ? (int)loaded->GetOption(RSK_TIME_TRAVEL_REQUIRES_MASTER_SWORD).Get() : -1;
}

void CheckStoredWorld(Rando::Context& world) {
    SetRule(world, true, true);
    nlohmann::json section;
    try {
        section = nlohmann::json::parse(OoT_SeedPackage_SerializeOoTWorld());
    } catch (...) {
        TtsFail("S: the serialized OoT world is not JSON");
        return;
    }
    if (!section.contains("randoSettings") || !section["randoSettings"].is_array() ||
        section["randoSettings"].size() != (size_t)RSK_MAX) {
        TtsFail("S: the serialized OoT world carries no randoSettings array of RSK_MAX entries");
        return;
    }
    const int fileValue = section["randoSettings"][(size_t)RSK_TIME_TRAVEL_REQUIRES_MASTER_SWORD].get<int>();
    TTS_CHECK(fileValue == RO_GENERIC_ON, "S: the file carries the option as %d, not the world's On", fileValue);

    const int asStored = LoadedOption(section);
    // A world saved before #1434: its array ends before the option.
    nlohmann::json older = section;
    nlohmann::json& settings = older["randoSettings"];
    settings.erase(settings.begin() + (std::ptrdiff_t)RSK_TIME_TRAVEL_REQUIRES_MASTER_SWORD, settings.end());
    const int fromOlder = LoadedOption(older);
    printf("[TEST] S: the stored world loads the option as %d (want On %d); cut before it, as %d (want Off %d)\n",
           asStored, (int)RO_GENERIC_ON, fromOlder, (int)RO_GENERIC_OFF);
    TTS_CHECK(asStored == RO_GENERIC_ON, "S1: a stored world did not load its own option back");
    TTS_CHECK(fromOlder == RO_GENERIC_OFF,
              "S2: a world saved before the option loaded it as %d, not Off: its pedestal would change under it",
              fromOlder);
    // The search's globals follow the loaded context, not the one it replaced.
    RegionTable_Init();
}

} // namespace

/**
 * @return 0 when every leg holds, 1 otherwise.
 */
extern "C" int OoT_TimeTravelSwordTest_Run(void) {
    printf("[TEST] oot-time-travel-sword: Time Travel Requires Master Sword ships on, the pedestal and logic need "
           "the sword when it is shuffled, and a stored world keeps its own pedestal (#1434)\n");
    if (OTRGlobals::Instance == nullptr || OTRGlobals::Instance->gRandoContext == nullptr) {
        printf("[TEST] FAIL: oot-time-travel-sword needs OTRGlobals' randomizer context\n");
        return 1;
    }
    sFailures = 0;
    const GameId prevGame = Context_GetCurrentGame();
    // The GameInteractor dispatch is gated off only under GAME_MM.
    Context_SetCurrentGame(GAME_OOT);
    const std::shared_ptr<Rando::Context> world = Rando::Context::GetInstance();
    std::unique_ptr<SaveContext> priorSave = std::make_unique<SaveContext>();
    std::memcpy(priorSave.get(), &gSaveContext, sizeof(SaveContext));
    SaveOptions(*world);

    CheckDefault();
    CheckRule();
    CheckPedestal(*world);
    CheckChildGive(*world);
    CheckLogic(*world);
    RestoreOptions(*world);
    std::memcpy(&gSaveContext, priorSave.get(), sizeof(SaveContext));
    CheckStoredWorld(*world); // last: the load replaces the randomizer context

    std::memcpy(&gSaveContext, priorSave.get(), sizeof(SaveContext));
    Context_SetCurrentGame(prevGame);
    if (sFailures != 0) {
        printf("[TEST] FAIL (oot-time-travel-sword): %d failure(s)\n", sFailures);
        return 1;
    }
    printf("[TEST] PASS: oot-time-travel-sword\n");
    return 0;
}

#endif // RSBS_SINGLE_EXECUTABLE
