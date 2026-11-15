/**
 * @file oot_grotto_void_test.cpp
 * Lock for the SoH backport bundle O7 (grotto void-outs, grotto entry timing and
 * the entrance tracker's Ganon's Tower rows). Rando-tier row `rando-grotto-void`
 * (CTest RandoGrottoVoid), dispatch in src/common/test_runner.cpp, same bring-up
 * as rando-warp-arrival (OTRGlobals' randomizer context).
 *
 * THE LEGS. Every grotto leg runs on the Hyrule Field tektite grotto (content
 * 0xE1, load ENTR_GROTTOS_11, return ENTR_HYRULE_FIELD_CENTER_EXIT) with Shuffle
 * Entrances and Shuffle Grotto Entrances on, the grotto lists reset to identity
 * (Grotto_InitExitAndLoadLists, as a file with no grotto override), a zeroed
 * play state as OoT_gPlayState and a randomizer save whose fileNum is the 0xFF
 * sentinel (the entrance-discovery save is a no-op there).
 *
 *   G1 (SoH 849928689, #6721) A NORMAL GROTTO EXIT with Mixed Grotto Entrances
 *      and Decoupled Entrances off: Link falls in through the hole (the row writes
 *      the pre-grotto return point Door_Ana's OoT_Play_SetupRespawnPoint leaves,
 *      then runs the real Grotto_OverrideActorEntrance) and walks out
 *      (Grotto_OverrideSpecialEntrance(ENTR_RETURN_GROTTO)). The exit lands on the
 *      pre-grotto entrance and leaves both the return and the down respawn as
 *      they were, so a later void-out returns to the last real entrance.
 *   G1b THE CONTROL: the same trip with Mixed Grotto Entrances on lands on the
 *      grotto's return entrance and writes the grotto's return point.
 *   G1c A GROTTO ENTERED WITHOUT THE HOLE (a warp song or spawn onto the grotto
 *      load point) with Mixed off still sets the grotto's own return data on the
 *      way out: the return point Door_Ana would have left is stale there.
 *   G2 (SoH 69681e608, #6529) A SWIM VOID-OUT INSIDE A GROTTO without Swim
 *      (RandomizerOnPlayerUpdateHandler, a player in water): the grotto reloads
 *      (next entrance the current one, respawnFlag 0, fade black) instead of a
 *      void-out to RESPAWN_MODE_DOWN, which grotto entry does not refresh.
 *   G2b (SoH d1d6a564e, #6854) NO SWIM VOID-OUT MID-TRANSITION: with a transition
 *      already started the handler leaves the transition and the respawn alone.
 *   G2c (SoH d1d6a564e) A SWIM VOID-OUT OUTSIDE A GROTTO after a grotto return
 *      runs the forced grotto return: respawnFlag 2 and the grotto's own return
 *      point with its temp flags cleared, not the stale point across the boulder.
 *   G3 (SoH aa25eb4d5, #7276) THE ENTRANCE TRACKER'S GANON'S TOWER ROWS are
 *      oneExit, both ways.
 *   G4 (SoH 9eafd15fe, #7271) A GROTTO HOLE WAITS FOR THE FADE-IN under the Bunny
 *      Hood speed-up: with the Bunny Hood Effect on, VB_DOOR_ANA_GRAB_PLAYER is
 *      vetoed while a transition runs and passes once it is off; with the effect
 *      off (when no shared rule forces it on) nothing vetoes it.
 *
 * Not covered here: the entrance tracker's blue-warp filter (SoH 9d42aedc9) is
 * inside an ImGui draw, and the grotto start jump's water speed factor (SoH
 * d1d6a564e, z_player.c) is a file-static player mode.
 *
 * Everything the row changes is put back: the options, the grotto lists (and
 * the entrance table if it was installed), the Bunny Hood CVar and its
 * registrars, the save, OoT_gPlayState, the current game and the hook registries.
 *
 * RED WITHOUT THE PORT: without the normalGrottoExit gate G1 lands on the
 * grotto's return; without the grotto branch G2 voids out to the down respawn;
 * without the transition gate G2b voids out; without the forced grotto return
 * G2c keeps respawnFlag 1; without the oneExit column G3 reads 0; without the
 * VB and its Bunny Hood hook G4's grab passes mid-transition.
 */

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <initializer_list>
#include <memory>
#include <string>

#include <libultraship/bridge.h>

#include "soh/OTRGlobals.h"
#include "soh/cvar_prefixes.h"
#include "soh/ShipInit.hpp"
#include "soh/Enhancements/enhancementTypes.h"
#include "soh/Enhancements/randomizer/randomizerTypes.h"
#include "soh/Enhancements/randomizer/SeedContext.h"
#include "soh/Enhancements/randomizer/randomizer_entrance_tracker.h"
#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/Enhancements/game-interactor/GameInteractor_Hooks.h" // GameInteractor_Should

#include "context.h" // src/common: Context_GetCurrentGame, Context_SetCurrentGame

// At file scope, not inside an extern "C" block (see oot_shared_kokiri_sword_test.cpp).
#include <z64.h>
#include "variables.h"
#include "soh/Enhancements/randomizer/randomizer_entrance.h"
#include "soh/Enhancements/randomizer/randomizer_grotto.h"

extern "C" PlayState* OoT_gPlayState;
// games/oot/soh/Enhancements/randomizer/hook_handlers.cpp: the randomizer's
// OnPlayerUpdate handler (registered there, file-local declaration).
void RandomizerOnPlayerUpdateHandler();

namespace {

/** A hook type's registry set aside for a scope: a grotto discovery dispatches
 *  OnRandoEntranceDiscovered. */
template <typename H> struct GrottoVoidHooksAside {
    decltype(GameInteractor::RegisteredGameHooks<H>::functions) functions;
    decltype(GameInteractor::RegisteredGameHooks<H>::functionsForFilter) filters;
    GrottoVoidHooksAside() {
        functions.swap(GameInteractor::RegisteredGameHooks<H>::functions);
        filters.swap(GameInteractor::RegisteredGameHooks<H>::functionsForFilter);
    }
    ~GrottoVoidHooksAside() {
        functions.swap(GameInteractor::RegisteredGameHooks<H>::functions);
        filters.swap(GameInteractor::RegisteredGameHooks<H>::functionsForFilter);
    }
};

int sGrottoVoidFailures = 0;

#define GV_CHECK(cond, ...)                                             \
    do {                                                                \
        if (!(cond)) {                                                  \
            std::printf("[TEST] FAIL (rando-grotto-void): ");           \
            std::printf(__VA_ARGS__);                                   \
            std::printf("  [%s] (%s:%d)\n", #cond, __FILE__, __LINE__); \
            sGrottoVoidFailures++;                                      \
        }                                                               \
    } while (0)

// randomizer_grotto.c's grottoLoadTable / grottoReturnTable rows for the Hyrule
// Field tektite grotto.
constexpr s16 kTektiteContent = 0x00E1;
constexpr s16 kTektiteLoadEntrance = ENTR_GROTTOS_11;
constexpr s16 kTektiteLoadPoint = ENTRANCE_GROTTO_LOAD_START + GROTTO_HF_TEKTITE_OFFSET;
constexpr s16 kTektiteExitPoint = ENTRANCE_GROTTO_EXIT_START + GROTTO_HF_TEKTITE_OFFSET;
constexpr s16 kTektiteReturnEntrance = ENTR_HYRULE_FIELD_CENTER_EXIT;
constexpr float kTektiteReturnX = -4949.58f;
constexpr float kTektiteReturnZ = 2837.59f;

// The pre-grotto return point Door_Ana leaves (any entrance distinct from the
// grotto's return), and a down respawn the grotto must not touch.
constexpr s16 kPreGrottoEntrance = ENTR_LON_LON_RANCH_ENTRANCE;
constexpr float kPreGrottoX = 111.0f;
constexpr s16 kDownEntrance = ENTR_KAKARIKO_VILLAGE_FRONT_GATE;
constexpr float kDownX = 222.0f;
// G2c: a return point and temp flags a previous scene left.
constexpr float kStaleReturnX = 333.0f;
constexpr u32 kStaleTempSwchFlags = 0x00A50000;

constexpr const char* kBunnyHoodCVar = CVAR_ENHANCEMENT("MMBunnyHood");

void ResetSave() {
    std::memset(&gSaveContext, 0, sizeof(SaveContext));
    gSaveContext.ship.quest.id = QUEST_RANDOMIZER;
    gSaveContext.fileNum = 0xFF; // the entrance-discovery Save_SaveSection is a no-op
    gSaveContext.gameMode = GAMEMODE_NORMAL;
    gSaveContext.linkAge = LINK_AGE_CHILD;
    // The grotto's load and exit points read as discovered, so the trips below
    // never reach the discovery's hook and check-tracker recalculation (the fake
    // play state would make IsSaveLoaded true and run the logic on a blank save).
    for (const u16 point : { (u16)kTektiteLoadPoint, (u16)kTektiteExitPoint }) {
        gSaveContext.ship.stats.entrancesDiscovered[point / 32] |= 1u << (point % 32);
    }
}

void ResetPlay(PlayState* play, Player* player, s16 scene) {
    std::memset(play, 0, sizeof(PlayState));
    std::memset(player, 0, sizeof(Player));
    play->sceneNum = scene;
    play->transitionTrigger = TRANS_TRIGGER_OFF;
    play->actorCtx.actorLists[ACTORCAT_PLAYER].head = &player->actor;
}

/** The return point Door_Ana's OoT_Play_SetupRespawnPoint leaves on the way in,
 *  and an unrelated down respawn. */
void ArmPreGrottoRespawns() {
    gSaveContext.respawn[RESPAWN_MODE_RETURN].entranceIndex = kPreGrottoEntrance;
    gSaveContext.respawn[RESPAWN_MODE_RETURN].pos.x = kPreGrottoX;
    gSaveContext.respawn[RESPAWN_MODE_RETURN].playerParams = 0x04FF;
    gSaveContext.respawn[RESPAWN_MODE_DOWN].entranceIndex = kDownEntrance;
    gSaveContext.respawn[RESPAWN_MODE_DOWN].pos.x = kDownX;
}

/** Falls into the tektite grotto through its hole (Door_Ana's path), then walks
 *  out of it. Returns the exit's next entrance. */
s16 HoleTrip(PlayState* play, Player* player) {
    ResetPlay(play, player, SCENE_HYRULE_FIELD);
    ArmPreGrottoRespawns();
    Actor hole;
    std::memset(&hole, 0, sizeof(hole));
    hole.params = kTektiteContent;
    play->nextEntranceIndex = ENTR_HYRULE_FIELD_CENTER_EXIT;
    Grotto_OverrideActorEntrance(&hole);
    GV_CHECK(play->nextEntranceIndex == kTektiteLoadEntrance,
             "control: the hole did not lead into the tektite grotto (0x%04X)\n",
             (unsigned)(u16)play->nextEntranceIndex);

    play->sceneNum = SCENE_GROTTOS;
    play->nextEntranceIndex = ENTR_RETURN_GROTTO;
    gSaveContext.respawnFlag = 0;
    return Grotto_OverrideSpecialEntrance(ENTR_RETURN_GROTTO);
}

void RunGrottoExitLegs(Rando::Context& ctx, PlayState* play, Player* player) {
    // G1. A normal grotto exit keeps the pre-grotto respawn.
    ctx.GetOption(RSK_MIX_GROTTO_ENTRANCES).Set(RO_GENERIC_OFF);
    ctx.GetOption(RSK_DECOUPLED_ENTRANCES).Set(RO_GENERIC_OFF);
    Grotto_InitExitAndLoadLists();
    ResetSave();
    const s16 normal = HoleTrip(play, player);
    const RespawnData& ret = gSaveContext.respawn[RESPAWN_MODE_RETURN];
    const RespawnData& down = gSaveContext.respawn[RESPAWN_MODE_DOWN];
    std::printf("[TEST] rando-grotto-void G1: normal exit lands on 0x%04X, return 0x%04X (x %.2f), down 0x%04X "
                "(x %.2f), respawnFlag %d\n",
                (unsigned)(u16)normal, (unsigned)(u16)ret.entranceIndex, ret.pos.x, (unsigned)(u16)down.entranceIndex,
                down.pos.x, (int)gSaveContext.respawnFlag);
    GV_CHECK(normal == kPreGrottoEntrance,
             "a normal grotto exit (Mixed and Decoupled off) must land on the pre-grotto return entrance\n");
    GV_CHECK(ret.entranceIndex == kPreGrottoEntrance && std::fabs(ret.pos.x - kPreGrottoX) < 0.01f,
             "a normal grotto exit must leave Door_Ana's pre-grotto return point\n");
    GV_CHECK(down.entranceIndex == kDownEntrance && std::fabs(down.pos.x - kDownX) < 0.01f,
             "a normal grotto exit must leave the down respawn alone\n");
    GV_CHECK(gSaveContext.respawnFlag == 2, "a grotto exit must still set respawnFlag 2\n");

    // G1b. The control: Mixed Grotto Entrances on writes the grotto's return point.
    ctx.GetOption(RSK_MIX_GROTTO_ENTRANCES).Set(RO_GENERIC_ON);
    ResetSave();
    const s16 mixed = HoleTrip(play, player);
    std::printf("[TEST] rando-grotto-void G1b: mixed exit lands on 0x%04X, return 0x%04X (%.2f, %.2f)\n",
                (unsigned)(u16)mixed, (unsigned)(u16)ret.entranceIndex, ret.pos.x, ret.pos.z);
    GV_CHECK(mixed == kTektiteReturnEntrance, "with Mixed Grotto Entrances on the exit must land on the grotto's "
                                              "return entrance\n");
    GV_CHECK(ret.entranceIndex == kTektiteReturnEntrance && std::fabs(ret.pos.x - kTektiteReturnX) < 0.01f &&
                 std::fabs(ret.pos.z - kTektiteReturnZ) < 0.01f,
             "with Mixed Grotto Entrances on the exit must write the grotto's return point\n");
    ctx.GetOption(RSK_MIX_GROTTO_ENTRANCES).Set(RO_GENERIC_OFF);

    // G1c. Entered without the hole (a warp onto the load point): the exit sets
    // the grotto's own return data even with Mixed off.
    ResetSave();
    ResetPlay(play, player, SCENE_HYRULE_FIELD);
    ArmPreGrottoRespawns();
    play->nextEntranceIndex = kTektiteLoadPoint;
    const s16 warpedIn = Grotto_OverrideSpecialEntrance(kTektiteLoadPoint);
    play->sceneNum = SCENE_GROTTOS;
    play->nextEntranceIndex = ENTR_RETURN_GROTTO;
    const s16 warpedOut = Grotto_OverrideSpecialEntrance(ENTR_RETURN_GROTTO);
    std::printf("[TEST] rando-grotto-void G1c: warped in to 0x%04X, out to 0x%04X, return 0x%04X (x %.2f)\n",
                (unsigned)(u16)warpedIn, (unsigned)(u16)warpedOut, (unsigned)(u16)ret.entranceIndex, ret.pos.x);
    GV_CHECK(warpedIn == kTektiteLoadEntrance, "control: the load point did not lead into the tektite grotto\n");
    GV_CHECK(warpedOut == kTektiteReturnEntrance && ret.entranceIndex == kTektiteReturnEntrance &&
                 std::fabs(ret.pos.x - kTektiteReturnX) < 0.01f,
             "a grotto entered without its hole must set the grotto's own return data on the way out\n");
}

/** A player in water without Swim or the Iron Boots, in @p scene. */
void ArmSwimVoid(PlayState* play, Player* player, s16 scene, s16 entrance) {
    ResetPlay(play, player, scene);
    player->stateFlags1 = PLAYER_STATE1_IN_WATER;
    gSaveContext.entranceIndex = entrance;
    gSaveContext.respawn[RESPAWN_MODE_DOWN].entranceIndex = kDownEntrance;
    gSaveContext.respawnFlag = 0;
    play->nextEntranceIndex = entrance;
}

void RunSwimVoidLegs(PlayState* play, Player* player) {
    // G2. A swim void-out inside a grotto reloads the grotto.
    ResetSave();
    ArmSwimVoid(play, player, SCENE_GROTTOS, kTektiteLoadEntrance);
    RandomizerOnPlayerUpdateHandler();
    std::printf("[TEST] rando-grotto-void G2: in-grotto swim void next 0x%04X, trigger %d, type %d, respawnFlag %d\n",
                (unsigned)(u16)play->nextEntranceIndex, (int)play->transitionTrigger, (int)play->transitionType,
                (int)gSaveContext.respawnFlag);
    GV_CHECK(play->nextEntranceIndex == kTektiteLoadEntrance && play->transitionTrigger == TRANS_TRIGGER_START &&
                 play->transitionType == TRANS_TYPE_FADE_BLACK && gSaveContext.respawnFlag == 0,
             "a swim void-out inside a grotto must reload the grotto, not void out to the down respawn\n");

    // G2b. No swim void-out while a transition already runs.
    ResetSave();
    ArmSwimVoid(play, player, SCENE_HYRULE_FIELD, kTektiteReturnEntrance);
    play->transitionTrigger = TRANS_TRIGGER_START;
    play->nextEntranceIndex = kPreGrottoEntrance;
    RandomizerOnPlayerUpdateHandler();
    std::printf("[TEST] rando-grotto-void G2b: mid-transition next 0x%04X, respawnFlag %d\n",
                (unsigned)(u16)play->nextEntranceIndex, (int)gSaveContext.respawnFlag);
    GV_CHECK(play->nextEntranceIndex == kPreGrottoEntrance && gSaveContext.respawnFlag == 0,
             "a swim void-out must not fire while a transition already runs\n");

    // G2c. A swim void-out after a grotto return runs the forced grotto return.
    // A warp onto the tektite grotto's exit point sets the grotto return up
    // afresh (lastEntranceType GROTTO_RETURN, grottoId the tektite grotto).
    ResetSave();
    ResetPlay(play, player, SCENE_HYRULE_FIELD);
    play->nextEntranceIndex = kTektiteExitPoint;
    const s16 returned = Grotto_OverrideSpecialEntrance(kTektiteExitPoint);
    GV_CHECK(returned == kTektiteReturnEntrance, "control: the exit point did not lead to the grotto's return\n");
    ArmSwimVoid(play, player, SCENE_HYRULE_FIELD, kTektiteReturnEntrance);
    gSaveContext.respawn[RESPAWN_MODE_RETURN].pos.x = kStaleReturnX;
    gSaveContext.respawn[RESPAWN_MODE_RETURN].tempSwchFlags = kStaleTempSwchFlags;
    RandomizerOnPlayerUpdateHandler();
    const RespawnData& ret = gSaveContext.respawn[RESPAWN_MODE_RETURN];
    std::printf("[TEST] rando-grotto-void G2c: void-out next 0x%04X, respawnFlag %d, return params 0x%04X (x %.2f), "
                "temp switch flags 0x%08X\n",
                (unsigned)(u16)play->nextEntranceIndex, (int)gSaveContext.respawnFlag, (unsigned)(u16)ret.playerParams,
                ret.pos.x, (unsigned)ret.tempSwchFlags);
    GV_CHECK(play->transitionTrigger == TRANS_TRIGGER_START && play->nextEntranceIndex == kDownEntrance,
             "control: a swim void-out outside a grotto is a void-out to the down respawn\n");
    GV_CHECK(gSaveContext.respawnFlag == 2 && ret.playerParams == 0x04FF &&
                 std::fabs(ret.pos.x - kTektiteReturnX) < 0.01f && ret.tempSwchFlags == 0,
             "a swim void-out after a grotto return must run the forced grotto return\n");
}

void RunTrackerLegs() {
    // G3. The Ganon's Tower rows are oneExit both ways.
    const EntranceData* toTower = EntranceTracker::GetEntranceData(ENTR_INSIDE_GANONS_CASTLE_1);
    const EntranceData* fromTower = EntranceTracker::GetEntranceData(ENTR_GANONS_TOWER_0);
    std::printf("[TEST] rando-grotto-void G3: Ganon's Tower entrance oneExit %d, exit oneExit %d\n",
                toTower != nullptr ? (int)toTower->oneExit : -1, fromTower != nullptr ? (int)fromTower->oneExit : -1);
    GV_CHECK(toTower != nullptr && toTower->oneExit == 1, "the Ganon's Tower entrance row must be oneExit\n");
    GV_CHECK(fromTower != nullptr && fromTower->oneExit == 1, "the Ganon's Tower exit row must be oneExit\n");
}

void RunBunnyHoodRegistrars() {
    auto& all = ShipInit::GetAll();
    auto it = all.find(kBunnyHoodCVar);
    if (it == all.end()) {
        return;
    }
    for (const auto& entry : it->second) {
        entry.initFunc();
    }
}

void RunDoorAnaLegs(PlayState* play, Player* player) {
    // G4. The grab waits for the fade-in under the Bunny Hood speed-up.
    const int priorBunny = CVarGetInteger(kBunnyHoodCVar, BUNNY_HOOD_VANILLA);
    const GameId priorGame = Context_GetCurrentGame();
    Context_SetCurrentGame(GAME_OOT);
    ResetPlay(play, player, SCENE_HYRULE_FIELD);

    CVarSetInteger(kBunnyHoodCVar, BUNNY_HOOD_FAST);
    RunBunnyHoodRegistrars();
    play->transitionTrigger = TRANS_TRIGGER_START;
    const bool fading = GameInteractor_Should(VB_DOOR_ANA_GRAB_PLAYER, true);
    play->transitionTrigger = TRANS_TRIGGER_OFF;
    const bool settled = GameInteractor_Should(VB_DOOR_ANA_GRAB_PLAYER, true);

    CVarClear(kBunnyHoodCVar);
    RunBunnyHoodRegistrars();
    const bool effectOff = OoT_BunnyHoodEffectMode() == BUNNY_HOOD_VANILLA;
    play->transitionTrigger = TRANS_TRIGGER_START;
    const bool vanillaFading = GameInteractor_Should(VB_DOOR_ANA_GRAB_PLAYER, true);
    play->transitionTrigger = TRANS_TRIGGER_OFF;

    std::printf("[TEST] rando-grotto-void G4: bunny hood on: grab during fade %d, after %d; effect off %d: grab during "
                "fade %d\n",
                fading ? 1 : 0, settled ? 1 : 0, effectOff ? 1 : 0, vanillaFading ? 1 : 0);
    GV_CHECK(!fading, "with the Bunny Hood Effect on a grotto hole must not grab Link while a transition runs\n");
    GV_CHECK(settled, "with the Bunny Hood Effect on a grotto hole must grab Link once the transition is off\n");
    if (effectOff) {
        GV_CHECK(vanillaFading, "control: with the Bunny Hood Effect off nothing vetoes the grotto hole's grab\n");
    }

    if (priorBunny != BUNNY_HOOD_VANILLA) {
        CVarSetInteger(kBunnyHoodCVar, priorBunny);
        RunBunnyHoodRegistrars();
    }
    Context_SetCurrentGame(priorGame);
}

} // namespace

/**
 * @return 0 when every leg holds, 1 otherwise.
 */
extern "C" int OoT_GrottoVoidTest_Run(void) {
    if (OTRGlobals::Instance == nullptr || OTRGlobals::Instance->gRandoContext == nullptr) {
        std::printf("[TEST] FAIL: rando-grotto-void needs OTRGlobals' randomizer context\n");
        return 1;
    }
    if (GameInteractor::Instance == nullptr) {
        GameInteractor::Instance = new GameInteractor();
    }
    const std::shared_ptr<Rando::Context> ctx = OTRGlobals::Instance->gRandoContext;
    const RandomizerSettingKey kKeys[] = { RSK_SHUFFLE_ENTRANCES, RSK_SHUFFLE_GROTTO_ENTRANCES,
                                           RSK_MIX_GROTTO_ENTRANCES, RSK_DECOUPLED_ENTRANCES, RSK_TRIFORCE_HUNT };
    uint8_t priorOptions[sizeof(kKeys) / sizeof(kKeys[0])];
    for (size_t i = 0; i < sizeof(kKeys) / sizeof(kKeys[0]); i++) {
        priorOptions[i] = ctx->GetOption(kKeys[i]).Get();
    }
    const bool priorReady = Entrance_WarpArrivalOverridesReady() != 0;
    std::unique_ptr<SaveContext> priorSave = std::make_unique<SaveContext>();
    std::memcpy(priorSave.get(), &gSaveContext, sizeof(SaveContext));
    PlayState* const priorPlay = OoT_gPlayState;

    std::unique_ptr<PlayState> play = std::make_unique<PlayState>();
    std::unique_ptr<Player> player = std::make_unique<Player>();

    ctx->GetOption(RSK_SHUFFLE_ENTRANCES).Set(RO_GENERIC_ON);
    ctx->GetOption(RSK_SHUFFLE_GROTTO_ENTRANCES).Set(RO_GENERIC_ON);
    ctx->GetOption(RSK_TRIFORCE_HUNT).Set(RO_TRIFORCE_HUNT_OFF);

    sGrottoVoidFailures = 0;
    OoT_gPlayState = play.get();
    {
        GrottoVoidHooksAside<GameInteractor::OnRandoEntranceDiscovered> discovered;
        RunGrottoExitLegs(*ctx, play.get(), player.get());
        RunSwimVoidLegs(play.get(), player.get());
        RunTrackerLegs();
        RunDoorAnaLegs(play.get(), player.get());
    }
    OoT_gPlayState = priorPlay;

    for (size_t i = 0; i < sizeof(kKeys) / sizeof(kKeys[0]); i++) {
        ctx->GetOption(kKeys[i]).Set(priorOptions[i]);
    }
    Grotto_InitExitAndLoadLists();
    if (priorReady) {
        Entrance_Init();
    }
    std::memcpy(&gSaveContext, priorSave.get(), sizeof(SaveContext));

    if (sGrottoVoidFailures != 0) {
        std::printf("[TEST] FAIL (rando-grotto-void): %d failure(s)\n", sGrottoVoidFailures);
        return 1;
    }
    std::printf("[TEST] PASS: rando-grotto-void\n");
    return 0;
}
