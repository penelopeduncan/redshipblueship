/**
 * @file oot_diving_zora_talon_cucco_test.cpp
 * ROM-free, display-free lock for #1800: the two game-side hunks of Ship of
 * Harkinian 63597ec63 (SoH #6324) whose logic hunk #1178 ported. CTest label
 * "redship", row OoTDivingZoraTalonCucco in CMake/SingleExecutable.cmake,
 * dispatch "oot-diving-zora-talon-cucco" in src/common/test_runner.cpp. No
 * generation: the row authors one Rando::Context option, as
 * oot_deku_theater_prize_test.cpp does.
 *
 * Z  THE DIVING-GAME ZORA (ShuffleSpeak.cpp, `case ACTOR_EN_DIVING_GAME:`). The
 *    REAL registrar RegisterShuffleSpeak, run on a randomizer file with Shuffle
 *    Jabber Nuts (RSK_SHUFFLE_SPEAK) on as ShipInit runs it on a file load,
 *    answers the question Player asks before accepting a talk offer
 *    (GameInteractor_Should(VB_SPEAK, ...), z_player.c) for a stub Player whose
 *    talkActor is the diving-game Zora (ACTOR_EN_DIVING_GAME, ACTORCAT_NPC, no
 *    ACTOR_FLAG_TALK_OFFER_AUTO_ACCEPTED, as z_en_diving_game.c's profile has it):
 *    Z1 false with every jabber nut but the Zora one; Z2 true with the Zora one
 *    alone; Z3 an NPC in no case of the switch is still asked true (control);
 *    Z4 with Shuffle Jabber Nuts off no VB_SPEAK hook is registered and the Zora
 *    is asked true. RED: with the case line removed Z1 answers true; with it
 *    moved under another language Z1 or Z2 fails.
 *
 * C  TALON'S CUCCO GAME (z_en_ta.c, EnTa_RunCuccoGame, the VB_PREVENT_STRENGTH
 *    wrap). The REAL EnTa_RunCuccoGame runs one frame with one super cucco whose
 *    found-countdown (unk_2C4[0]) is 3 and the player carrying it, the
 *    randomizer's REAL vanilla-behaviour handler registered as
 *    RandomizerRegisterHooks registers it. The countdown is the observable: a
 *    cucco counted as held goes to 2, one not held is reset to 7, and neither
 *    path reaches the animation or text code (the found branch needs 0).
 *    C1 a randomizer file without the Grab ability (RAND_INF_CAN_GRAB): 7, and
 *    the handler ran (it clears PLAYER_STATE2_MOVING_DYNAPOLY); C2 with Grab: 2;
 *    C3 with Grab and not carrying: 7 (control: 2 depends on the carry); C4 a
 *    vanilla file, where a load registers no randomizer handler: 2.
 *    RED: with the hunk reverted to `if (func_80B150AC(this, play, i))` C1
 *    reads 2.
 *
 * Every hook, option, flag and save field the row changes is put back.
 */

#ifdef RSBS_SINGLE_EXECUTABLE

// At file scope, not inside an extern "C" block: z64.h includes <memory> under
// C++ (see oot_shared_kokiri_sword_test.cpp).
#include <z64.h>

#include "context.h"
#include "game.h"

#include "soh/Enhancements/randomizer/randomizerTypes.h"
#include "soh/Enhancements/randomizer/SeedContext.h"
#include "soh/Enhancements/randomizer/logic.h" // Rando::Logic::SetContext
#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/Enhancements/game-interactor/GameInteractor_Hooks.h" // GameInteractor_Should

#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>

extern "C" {
#include "src/overlays/actors/ovl_En_Ta/z_en_ta.h"
extern SaveContext gSaveContext;
extern PlayState* OoT_gPlayState;
// z_en_ta.c: the cucco game's per-frame action (external linkage there).
void EnTa_RunCuccoGame(EnTa* thisx, PlayState* play);
}

// ShuffleSpeak.cpp: its ShipInit registrar ("IS_RANDO").
void RegisterShuffleSpeak();
// hook_handlers.cpp: the handler RandomizerRegisterHooks registers on a
// randomizer file load.
void RandomizerOnVanillaBehaviorHandler(GIVanillaBehavior id, bool* should, va_list originalArgs);

namespace {

int sDzcFailures = 0;

void DzcFail(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    printf("[TEST] oot-diving-zora-talon-cucco: FAIL: ");
    vprintf(fmt, args);
    printf("\n");
    va_end(args);
    sDzcFailures++;
}

#define DZC_CHECK(cond, ...)      \
    do {                          \
        if (!(cond)) {            \
            DzcFail(__VA_ARGS__); \
        }                         \
    } while (0)

/** Sets every vanilla-behaviour hook already registered aside for the row's
 *  lifetime, so only the hooks a leg registers answer. The pending-unregister
 *  queues go aside too: ExecuteHooks and ExecuteHooksForFilter clear their queue
 *  against whatever map is current, so a generic unregistration queued before
 *  the row would otherwise be dropped against the row's empty map and its hook
 *  would come back live. Whatever the row leaves queued is discarded with the
 *  row's own maps. */
struct DzcVbHooksAside {
    using Hooks = GameInteractor::RegisteredGameHooks<GameInteractor::OnVanillaBehavior>;
    decltype(Hooks::functions) functions;
    decltype(Hooks::functionsForID) forId;
    decltype(Hooks::functionsForPtr) forPtr;
    decltype(Hooks::functionsForFilter) filters;
    using Pending = GameInteractor::HooksToUnregister<GameInteractor::OnVanillaBehavior>;
    decltype(Pending::hooks) pending;
    decltype(Pending::hooksForID) pendingForId;
    decltype(Pending::hooksForPtr) pendingForPtr;
    decltype(Pending::hooksForFilter) pendingFilters;
    DzcVbHooksAside() {
        Swap();
    }
    ~DzcVbHooksAside() {
        Swap();
    }
    void Swap() {
        functions.swap(Hooks::functions);
        forId.swap(Hooks::functionsForID);
        forPtr.swap(Hooks::functionsForPtr);
        filters.swap(Hooks::functionsForFilter);
        pending.swap(Pending::hooks);
        pendingForId.swap(Pending::hooksForID);
        pendingForPtr.swap(Pending::hooksForPtr);
        pendingFilters.swap(Pending::hooksForFilter);
    }
};

size_t DzcVbCount(GIVanillaBehavior vb) {
    auto& buckets = GameInteractor::RegisteredGameHooks<GameInteractor::OnVanillaBehavior>::functionsForID;
    auto it = buckets.find((int32_t)vb);
    return it == buckets.end() ? 0u : it->second.size();
}

// The bit Flags_GetRandomizerInf reads, set without Flags_SetRandomizerInf's
// OnFlagSet dispatch.
void DzcSetInf(RandomizerInf flag) {
    gSaveContext.ship.randomizerInf[flag >> 4] |= (u16)(1 << (flag & 0xF));
}

struct DzcStage {
    PlayState* play;
    Player* player;
    Actor* npc;
    EnTa* talon;
    EnNiw* cucco;
};

void DzcArmSave(u8 questId) {
    memset(&gSaveContext, 0, sizeof(SaveContext));
    gSaveContext.ship.quest.id = questId;
    gSaveContext.linkAge = LINK_AGE_CHILD;
}

void DzcArmPlay(DzcStage& s) {
    memset(s.play, 0, sizeof(PlayState));
    memset(s.player, 0, sizeof(Player));
    s.play->actorCtx.actorLists[ACTORCAT_PLAYER].head = &s.player->actor;
    s.player->actor.id = ACTOR_PLAYER;
    s.player->actor.category = ACTORCAT_PLAYER;
}

// ---- Z: the diving-game Zora ---------------------------------------------------------

// Player's talk offer from @p actorId: an NPC with no auto-accept flag (the
// diving-game Zora's FLAGS, z_en_diving_game.c).
bool DzcAskSpeak(DzcStage& s, s16 actorId) {
    memset(s.npc, 0, sizeof(Actor));
    s.npc->id = actorId;
    s.npc->category = ACTORCAT_NPC;
    s.npc->flags = ACTOR_FLAG_ATTENTION_ENABLED | ACTOR_FLAG_FRIENDLY | ACTOR_FLAG_UPDATE_CULLING_DISABLED;
    s.player->talkActor = s.npc;
    return GameInteractor_Should(VB_SPEAK, true);
}

void DzcRegisterSpeak(const std::shared_ptr<Rando::Context>& world, u8 questId, bool speakOn) {
    DzcArmSave(questId);
    world->GetOption(RSK_SHUFFLE_SPEAK).Set(speakOn ? RO_GENERIC_ON : RO_GENERIC_OFF);
    RegisterShuffleSpeak();
}

void DzcZoraLegs(DzcStage& s, const std::shared_ptr<Rando::Context>& world) {
    DzcArmPlay(s);

    DzcRegisterSpeak(world, QUEST_RANDOMIZER, true);
    DZC_CHECK(DzcVbCount(VB_SPEAK) == 1, "Z: with Shuffle Jabber Nuts on, %zu VB_SPEAK hook(s) are registered, want 1",
              DzcVbCount(VB_SPEAK));

    // Z1: every jabber nut but the Zora one.
    DzcSetInf(RAND_INF_CAN_SPEAK_DEKU);
    DzcSetInf(RAND_INF_CAN_SPEAK_GERUDO);
    DzcSetInf(RAND_INF_CAN_SPEAK_GORON);
    DzcSetInf(RAND_INF_CAN_SPEAK_HYLIAN);
    DzcSetInf(RAND_INF_CAN_SPEAK_KOKIRI);
    const bool z1 = DzcAskSpeak(s, ACTOR_EN_DIVING_GAME);
    printf("[TEST] oot-diving-zora-talon-cucco: Z1 diving Zora, no Zora jabber nut: should=%d\n", z1 ? 1 : 0);
    DZC_CHECK(!z1, "Z1: the diving-game Zora accepted a talk without the Zora Jabber Nut (SoH 63597ec63's "
                   "ShuffleSpeak.cpp hunk)");

    // Z2: the Zora jabber nut alone.
    DzcArmSave(QUEST_RANDOMIZER);
    DzcSetInf(RAND_INF_CAN_SPEAK_ZORA);
    const bool z2 = DzcAskSpeak(s, ACTOR_EN_DIVING_GAME);
    printf("[TEST] oot-diving-zora-talon-cucco: Z2 diving Zora, Zora jabber nut only: should=%d\n", z2 ? 1 : 0);
    DZC_CHECK(z2, "Z2: the diving-game Zora refused a talk with the Zora Jabber Nut (keyed to the wrong language)");

    // Z3: control, an NPC no case of the switch names (a cucco), with no jabber nut.
    DzcArmSave(QUEST_RANDOMIZER);
    DZC_CHECK(DzcAskSpeak(s, ACTOR_EN_NIW), "Z3: control: an NPC in no case of the switch was refused a talk");

    // Z4: Shuffle Jabber Nuts off: no hook, the vanilla answer.
    // An unregistration is queued and carried out by the next dispatch of that
    // id (GameInteractor::ExecuteHooksForID), so count after asking; the scrub
    // dispatch flushes the registrar's other hook the same way.
    DzcRegisterSpeak(world, QUEST_RANDOMIZER, false);
    DZC_CHECK(DzcAskSpeak(s, ACTOR_EN_DIVING_GAME),
              "Z4: with Shuffle Jabber Nuts off the diving-game Zora was refused a talk");
    (void)GameInteractor_Should(VB_BUSINESS_SCRUB_SPEAK, true);
    DZC_CHECK(DzcVbCount(VB_SPEAK) == 0, "Z4: with Shuffle Jabber Nuts off, %zu VB_SPEAK hook(s) are registered",
              DzcVbCount(VB_SPEAK));

    // Unregister: a non-randomizer save turns the registrar's condition off, so
    // its static hook ids read 0 when the set-aside hooks come back.
    DzcRegisterSpeak(world, QUEST_NORMAL, true);
    s.player->talkActor = nullptr;
}

// ---- C: Talon's cucco game -----------------------------------------------------------

/** @return the cucco's found-countdown after one EnTa_RunCuccoGame frame: 2 when
 *  the frame counted it as held, 7 when it did not. */
int DzcRunCuccoFrame(DzcStage& s, u8 questId, bool canGrab, bool carrying, bool* outDynapolyCleared) {
    DzcArmSave(questId);
    gSaveContext.timerSeconds = 30; // neither the 10-second tempo nor the time-up branch
    if (canGrab) {
        DzcSetInf(RAND_INF_CAN_GRAB);
    }
    DzcArmPlay(s);
    memset(s.talon, 0, sizeof(EnTa));
    memset(s.cucco, 0, sizeof(EnNiw));
    s.cucco->actor.id = ACTOR_EN_NIW;
    s.cucco->actor.gravity = -3.0f; // past the fall ramp
    s.talon->actor.id = ACTOR_EN_TA;
    s.talon->superCuccos[0] = s.cucco;
    s.talon->unk_2C4[0] = 3;
    s.player->interactRangeActor = &s.cucco->actor;
    s.player->stateFlags1 = carrying ? PLAYER_STATE1_CARRYING_ACTOR : 0;
    s.player->stateFlags2 = PLAYER_STATE2_MOVING_DYNAPOLY; // the handler clears it when it fires
    s.talon->actionFunc = EnTa_RunCuccoGame;

    EnTa_RunCuccoGame(s.talon, s.play);

    if (outDynapolyCleared != nullptr) {
        *outDynapolyCleared = (s.player->stateFlags2 & PLAYER_STATE2_MOVING_DYNAPOLY) == 0;
    }
    DZC_CHECK(s.talon->actionFunc == EnTa_RunCuccoGame,
              "C: the frame left the cucco game (countdown %d): the stub reached the found branch",
              (int)s.talon->unk_2C4[0]);
    return s.talon->unk_2C4[0];
}

void DzcCuccoLegs(DzcStage& s) {
    const HOOK_ID vbHook = GameInteractor::Instance->RegisterGameHook<GameInteractor::OnVanillaBehavior>(
        RandomizerOnVanillaBehaviorHandler);

    bool cleared = false;
    const int c1 = DzcRunCuccoFrame(s, QUEST_RANDOMIZER, false, true, &cleared);
    printf("[TEST] oot-diving-zora-talon-cucco: C1 no Grab, carrying: countdown %d, handler fired %d\n", c1,
           cleared ? 1 : 0);
    DZC_CHECK(cleared, "C1: the VB_PREVENT_STRENGTH handler did not fire without the Grab ability");
    DZC_CHECK(c1 == 7,
              "C1: without the Grab ability a carried super cucco was counted as found (countdown %d, want 7; SoH "
              "63597ec63's z_en_ta.c hunk)",
              c1);

    const int c2 = DzcRunCuccoFrame(s, QUEST_RANDOMIZER, true, true, &cleared);
    printf("[TEST] oot-diving-zora-talon-cucco: C2 Grab, carrying: countdown %d\n", c2);
    DZC_CHECK(c2 == 2, "C2: with the Grab ability a carried super cucco was not counted (countdown %d, want 2)", c2);
    DZC_CHECK(!cleared, "C2: with the Grab ability the VB_PREVENT_STRENGTH handler still fired");

    const int c3 = DzcRunCuccoFrame(s, QUEST_RANDOMIZER, true, false, nullptr);
    DZC_CHECK(c3 == 7, "C3: control: a super cucco nobody carries was counted (countdown %d, want 7)", c3);

    GameInteractor::Instance->UnregisterGameHook<GameInteractor::OnVanillaBehavior>(vbHook);

    const int c4 = DzcRunCuccoFrame(s, QUEST_NORMAL, false, true, nullptr);
    printf("[TEST] oot-diving-zora-talon-cucco: C4 vanilla file, carrying: countdown %d\n", c4);
    DZC_CHECK(c4 == 2, "C4: on a vanilla file a carried super cucco was not counted (countdown %d, want 2)", c4);
}

} // namespace

extern "C" int OoT_DivingZoraTalonCucco_RunHeadless(void) {
    printf("[TEST] oot-diving-zora-talon-cucco: the diving-game Zora needs the Zora Jabber Nut and a super cucco "
           "needs the Grab ability, as SoH 63597ec63 has it (#1800)\n");
    sDzcFailures = 0;
    if (GameInteractor::Instance == nullptr) {
        GameInteractor::Instance = new GameInteractor();
    }

    const bool hadWorld = Rando::Context::GetInstance() != nullptr;
    std::shared_ptr<Rando::Context> world = Rando::Context::CreateInstance();
    if (world == nullptr) {
        printf("[TEST] FAIL (oot-diving-zora-talon-cucco): could not create a Rando::Context\n");
        return 1;
    }
    const uint8_t priorSpeak = world->GetOption(RSK_SHUFFLE_SPEAK).Get();

    DzcStage s = {};
    s.play = (PlayState*)calloc(1, sizeof(PlayState));
    s.player = (Player*)calloc(1, sizeof(Player));
    s.npc = (Actor*)calloc(1, sizeof(Actor));
    s.talon = (EnTa*)calloc(1, sizeof(EnTa));
    s.cucco = (EnNiw*)calloc(1, sizeof(EnNiw));
    std::unique_ptr<SaveContext> priorSave = std::make_unique<SaveContext>();
    std::memcpy(priorSave.get(), &gSaveContext, sizeof(SaveContext));
    PlayState* const priorPlay = OoT_gPlayState;
    const GameId priorGame = Context_GetCurrentGame();

    int setup = 0;
    if (s.play == nullptr || s.player == nullptr || s.npc == nullptr || s.talon == nullptr || s.cucco == nullptr) {
        printf("[TEST] FAIL (oot-diving-zora-talon-cucco): could not allocate the stub play state\n");
        setup = 1;
    } else {
        // The GameInteractor dispatch is gated off only under GAME_MM.
        Context_SetCurrentGame(GAME_OOT);
        // Both hooks read the player through OoT_gPlayState.
        OoT_gPlayState = s.play;
        {
            DzcVbHooksAside aside;
            DzcZoraLegs(s, world);
            DzcCuccoLegs(s);
        }
    }

    // Leave process state as it was, on pass or fail.
    world->GetOption(RSK_SHUFFLE_SPEAK).Set(priorSpeak);
    std::memcpy(&gSaveContext, priorSave.get(), sizeof(SaveContext));
    OoT_gPlayState = priorPlay;
    Context_SetCurrentGame(priorGame);
    if (!hadWorld) {
        // Context and its Logic hold each other by shared_ptr; cut the back edge
        // of the instance this row made (oot_deku_theater_prize_test.cpp).
        world->GetLogic()->SetContext(nullptr);
    }
    world.reset();
    free(s.play);
    free(s.player);
    free(s.npc);
    free(s.talon);
    free(s.cucco);

    if (setup != 0 || sDzcFailures != 0) {
        printf("[TEST] FAIL (oot-diving-zora-talon-cucco): %d failure(s)\n", sDzcFailures);
        return 1;
    }
    printf("[TEST] PASS: oot-diving-zora-talon-cucco\n");
    return 0;
}

#else

extern "C" int OoT_DivingZoraTalonCucco_RunHeadless(void) {
    return 0;
}

#endif // RSBS_SINGLE_EXECUTABLE
