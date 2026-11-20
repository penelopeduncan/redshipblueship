/**
 * @file mm_small_options_1755_test.cpp
 * ROM-free, display-free lock for #1755: five small Majora's Mask options backported from 2Ship.
 * CTest label "redship", row MMSmallOptions1755 in CMake/SingleExecutable.cmake,
 * dispatch "mm-small-options-1755" in src/common/test_runner.cpp.
 *
 * Legs (driving production registrars solely through CVarSetInteger / CVarClear + ShipInit::Init; nothing defined by
 * any of the five TUs is named here so link-elision gates stay honest):
 *   1. PictoBoxOnCUp (2Ship #1558): off preserves vanilla; on + null MM_gPlayState cancels VB_FIRST_PERSON_CAMERA
 *      without touching play state; on with ITEM_PICTOGRAPH_BOX owned (and QUEST_PICTOGRAPH clear) sets
 *      PLAYER_UNKAA5_2; without Picto Box (or with QUEST_PICTOGRAPH set) sets PLAYER_UNKAA5_1.
 *   2. AlwaysShowShrineFeathers (2Ship #1594, #1905): with SkipMiscInteractions off, OnActorInit[EN_OWL] guards null
 *      MM_gPlayState and null setupPathList, spawns ENOWL_GET_TYPE_30 feathers along setupPathList points with a live
 *      OBJECT_OWL slot, OnActorUpdate[EN_OWL] locks unk_3DC = 0x12C, and VB_OWL_SPAWN_FEATHER / VB_OWL_DESPAWN_FEATHER
 *      return false while on and vanilla while off.
 *   3. DashAfterRoll (2Ship #1938): OnPassPlayerInputs guards null MM_gPlayState; holding BTN_A across a non-bonked
 *      roll (Player_Action_26, av2 == 0) through the 1-frame MM_Player_Action_Idle transition and Player_Action_13
 *      sets VB_CONSIDER_BUNNY_HOOD_EQUIPPED in On mode and scales walk speed by 1.5x in Stack mode; releasing BTN_A or
 *      bonking (av2 != 0) clears dash.
 *   4. AlwaysFindRockSirloin (2Ship #1944): VB_FIND_ROCK_SIRLOIN forces true when on, keeps vanilla false when off.
 *   5. FishermanJumpingGame (2Ship #1947): TorchTimeLimit Normal / Double (<= 200 vs > 200) / Infinity; TargetScore
 *      shifts VB_FAIL_FISHERMAN_JUMPING_GAME and rewrites 0x109A text ("20" -> TARGET_CVAR) via S2H::CustomMessage;
 *      EarlyWin sets VB_JUMPING_GAME_END and zeroes TIMER_ID_MINIGAME_2 once minigameScore reaches target.
 */

#include "global.h"

#include <cstdio>

#if !defined(RSBS_SINGLE_EXECUTABLE)
extern "C" int MM_SmallOptions1755_RunHeadless(void) {
    printf("[TEST] mm-small-options-1755: PASS (not applicable outside RSBS_SINGLE_EXECUTABLE)\n");
    return 0;
}
#else

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include <libultraship/bridge/consolevariablebridge.h>

#include "2s2h/Enhancements/Enhancements.h"
#include "2s2h/GameInteractor/GameInteractor.h"
#include "2s2h/ShipInit.hpp"
#include "mm_game_hooks.h"

extern "C" {
#include "variables.h"
#include "functions.h"
#include "overlays/actors/ovl_En_Owl/z_en_owl.h"
#include "objects/object_owl/object_owl.h"
#include <message_data_static.h>
extern Input* sPlayerControlInput;
extern void MM_Player_Action_Idle(Player* player, PlayState* play);
extern void Player_Action_13(Player* player, PlayState* play);
extern void Player_Action_26(Player* player, PlayState* play);
extern void GameInteractor_ExecuteOnPassPlayerInputs(Input* input);
extern void MM_GameHooks_ExecuteOnActorUpdate(Actor* actor);
}

namespace {

#define SO1755_ASSERT(cond, code, msg)                                                  \
    do {                                                                                \
        if (!(cond)) {                                                                  \
            printf("[TEST] FAIL(%d): %s (%s:%d)\n", (code), (msg), __FILE__, __LINE__); \
            return (code);                                                              \
        }                                                                               \
    } while (0)

constexpr const char* kPictoBoxKey = "gEnhancements.Items.PictoBoxOnCUp";
constexpr const char* kFeathersKey = "gEnhancements.Timesavers.AlwaysShowShrineFeathers";
constexpr const char* kSkipMiscKey = "gEnhancements.TimeSavers.SkipMiscInteractions";
constexpr const char* kDashAfterRollKey = "gEnhancements.Player.DashAfterRoll";
constexpr const char* kDebugCamKey = "gEnhancements.Camera.DebugCam.Enable";
constexpr const char* kSpeedModKey = "gSettings.SpeedModifier.Enable";
constexpr const char* kPersistentHoodKey = "gEnhancements.Masks.PersistentBunnyHood.Enabled";
constexpr const char* kRockSirloinKey = "gEnhancements.DifficultyOptions.AlwaysFindRockSirloin";
constexpr const char* kTorchLimitKey = "gEnhancements.Minigames.FishermanJumpingGame.TorchTimeLimit";
constexpr const char* kTargetScoreKey = "gEnhancements.Minigames.FishermanJumpingGame.TargetScore";
constexpr const char* kEarlyWinKey = "gEnhancements.Minigames.FishermanJumpingGame.EarlyWin";

struct SavedKey {
    const char* key;
    bool existed;
    int32_t value;

    explicit SavedKey(const char* k) : key(k), existed(CVarExists(k)), value(CVarGetInteger(k, 0)) {
    }

    void Restore() const {
        if (existed) {
            CVarSetInteger(key, value);
        } else {
            CVarClear(key);
        }
        ShipInit::Init(key);
    }
};

void DriveKey(const char* key, int32_t value) {
    CVarSetInteger(key, value);
    ShipInit::Init(key);
}

void ClearKey(const char* key) {
    CVarClear(key);
    ShipInit::Init(key);
}

void FreeSpawnedOwls(PlayState* play) {
    if (play == nullptr) {
        return;
    }
    while (play->actorCtx.actorLists[ACTORCAT_NPC].first != nullptr) {
        Actor* actor = play->actorCtx.actorLists[ACTORCAT_NPC].first;
        MM_Actor_Delete(&play->actorCtx, actor, play);
    }
}

struct ScopedStateRestore {
    PlayState* savedPlay;
    Input* savedPlayerInput;
    std::unique_ptr<uint8_t[]> savedSave;
    s8 savedOwlLoaded;
    bool arenaInitialized = false;
    PlayState* testPlay = nullptr;
    Player* testPlayer = nullptr;
    Camera* testCamera = nullptr;
    Input* testInput = nullptr;
    std::vector<SavedKey> keys;

    ScopedStateRestore()
        : savedPlay(MM_gPlayState), savedPlayerInput(sPlayerControlInput),
          savedSave(std::make_unique<uint8_t[]>(sizeof(gSaveContext))),
          savedOwlLoaded(gActorOverlayTable[ACTOR_EN_OWL].numLoaded) {
        memcpy(savedSave.get(), &gSaveContext, sizeof(gSaveContext));
        const char* allKeys[] = { kPictoBoxKey, kFeathersKey, kSkipMiscKey, kDashAfterRollKey, kDebugCamKey,
                                  kSpeedModKey, kPersistentHoodKey, kRockSirloinKey, kTorchLimitKey, kTargetScoreKey,
                                  kEarlyWinKey };
        for (const char* k : allKeys) {
            keys.emplace_back(k);
        }
    }

    ~ScopedStateRestore() {
        if (testPlay != nullptr) {
            FreeSpawnedOwls(testPlay);
        }
        if (arenaInitialized) {
            MM_ZeldaArena_Cleanup();
        }
        gActorOverlayTable[ACTOR_EN_OWL].numLoaded = savedOwlLoaded;

        // Neutralize DashAfterRoll's static state before disarming keys.
        if (testPlay != nullptr && testPlayer != nullptr && testInput != nullptr) {
            MM_gPlayState = testPlay;
            testPlay->actorCtx.actorLists[ACTORCAT_PLAYER].first = &testPlayer->actor;
            testPlayer->actionFunc = MM_Player_Action_Idle;
            testInput->cur.button = 0;
            DriveKey(kDashAfterRollKey, 1);
            GameInteractor_ExecuteOnPassPlayerInputs(testInput);
        }

        MM_gPlayState = savedPlay;
        sPlayerControlInput = savedPlayerInput;
        memcpy(&gSaveContext, savedSave.get(), sizeof(gSaveContext));

        for (const auto& k : keys) {
            k.Restore();
        }
        S2H::GameHooks::FlushPendingUnregistrations<GameInteractor::ShouldVanillaBehavior>();
        S2H::GameHooks::FlushPendingUnregistrations<GameInteractor::OnActorInit>();
        S2H::GameHooks::FlushPendingUnregistrations<GameInteractor::OnActorUpdate>();
        S2H::GameHooks::FlushPendingUnregistrations<GameInteractor::OnPassPlayerInputs>();
        S2H::GameHooks::FlushPendingUnregistrations<GameInteractor::OnOpenText>();

        free(testInput);
        free(testCamera);
        free(testPlayer);
        free(testPlay);
    }
};

static uint8_t sTestArenaBuffer[64 * 1024];

int RunLegs(ScopedStateRestore& guard) {
    guard.testPlay = static_cast<PlayState*>(calloc(1, sizeof(PlayState)));
    guard.testPlayer = static_cast<Player*>(calloc(1, sizeof(Player)));
    guard.testCamera = static_cast<Camera*>(calloc(1, sizeof(Camera)));
    guard.testInput = static_cast<Input*>(calloc(1, sizeof(Input)));
    SO1755_ASSERT(guard.testPlay != nullptr && guard.testPlayer != nullptr && guard.testCamera != nullptr &&
                      guard.testInput != nullptr,
                  1, "calloc failed for stand-in play state structs");

    PlayState* play = guard.testPlay;
    Player* player = guard.testPlayer;
    Camera* cam = guard.testCamera;
    Input* input = guard.testInput;

    play->actorCtx.actorLists[ACTORCAT_PLAYER].first = &player->actor;
    player->actor.id = ACTOR_PLAYER;
    cam->setting = CAM_SET_NORMAL0;
    cam->mode = CAM_MODE_NORMAL;
    play->cameraPtrs[CAM_ID_MAIN] = cam;
    sPlayerControlInput = input;

    // ---- Leg 1: PictoBoxOnCUp (2Ship #1558) -----------------------------------------------------------------------
    DriveKey(kPictoBoxKey, 0);
    MM_gPlayState = play;
    player->actor.bgCheckFlags = BGCHECKFLAG_GROUND;
    player->unk_AA5 = PLAYER_UNKAA5_0;
    input->press.button = BTN_CUP;
    SO1755_ASSERT(GameInteractor_Should(VB_FIRST_PERSON_CAMERA, true), 10,
                  "PictoBoxOnCUp off changed VB_FIRST_PERSON_CAMERA verdict");

    DriveKey(kPictoBoxKey, 1);
    MM_gPlayState = NULL;
    SO1755_ASSERT(!GameInteractor_Should(VB_FIRST_PERSON_CAMERA, true), 11,
                  "PictoBoxOnCUp did not clear VB_FIRST_PERSON_CAMERA when MM_gPlayState == NULL");

    MM_gPlayState = play;
    gSaveContext.save.saveInfo.inventory.items[SLOT_PICTOGRAPH_BOX] = ITEM_NONE;
    REMOVE_QUEST_ITEM(QUEST_PICTOGRAPH);
    player->unk_AA5 = PLAYER_UNKAA5_0;
    SO1755_ASSERT(!GameInteractor_Should(VB_FIRST_PERSON_CAMERA, true), 12,
                  "PictoBoxOnCUp did not cancel vanilla error beep without Picto Box");
    SO1755_ASSERT(player->unk_AA5 == PLAYER_UNKAA5_1, 13,
                  "PictoBoxOnCUp did not fall back to PLAYER_UNKAA5_1 without Picto Box owned");

    gSaveContext.save.saveInfo.inventory.items[SLOT_PICTOGRAPH_BOX] = ITEM_PICTOGRAPH_BOX;
    player->unk_AA5 = PLAYER_UNKAA5_0;
    SO1755_ASSERT(!GameInteractor_Should(VB_FIRST_PERSON_CAMERA, true), 14,
                  "PictoBoxOnCUp did not handle C-Up with Picto Box owned");
    SO1755_ASSERT(player->unk_AA5 == PLAYER_UNKAA5_2, 15,
                  "PictoBoxOnCUp did not set PLAYER_UNKAA5_2 when Picto Box owned and QUEST_PICTOGRAPH unset");

    SET_QUEST_ITEM(QUEST_PICTOGRAPH);
    player->unk_AA5 = PLAYER_UNKAA5_0;
    SO1755_ASSERT(!GameInteractor_Should(VB_FIRST_PERSON_CAMERA, true), 16,
                  "PictoBoxOnCUp failed when QUEST_PICTOGRAPH was already set");
    SO1755_ASSERT(player->unk_AA5 == PLAYER_UNKAA5_1, 17,
                  "PictoBoxOnCUp used PLAYER_UNKAA5_2 even with QUEST_PICTOGRAPH set");
    DriveKey(kPictoBoxKey, 0);
    printf("[TEST] mm-small-options-1755: leg 1 (PictoBoxOnCUp) passed\n");

    // ---- Leg 2: AlwaysShowShrineFeathers (2Ship #1594, #1905) -----------------------------------------------------
    DriveKey(kSkipMiscKey, 0);
    DriveKey(kFeathersKey, 0);
    EnOwl type2Owl{};
    type2Owl.actor.id = ACTOR_EN_OWL;
    type2Owl.actor.params = (ENOWL_GET_TYPE_2 << 7); // pathIndex = 0
    SO1755_ASSERT(GameInteractor_Should(VB_OWL_SPAWN_FEATHER, true, &type2Owl), 20,
                  "AlwaysShowShrineFeathers off cancelled VB_OWL_SPAWN_FEATHER");
    SO1755_ASSERT(GameInteractor_Should(VB_OWL_DESPAWN_FEATHER, true, &type2Owl), 21,
                  "AlwaysShowShrineFeathers off cancelled VB_OWL_DESPAWN_FEATHER");

    DriveKey(kFeathersKey, 1);
    SO1755_ASSERT(!GameInteractor_Should(VB_OWL_SPAWN_FEATHER, true, &type2Owl), 22,
                  "AlwaysShowShrineFeathers on allowed VB_OWL_SPAWN_FEATHER");
    SO1755_ASSERT(!GameInteractor_Should(VB_OWL_DESPAWN_FEATHER, true, &type2Owl), 23,
                  "AlwaysShowShrineFeathers on allowed VB_OWL_DESPAWN_FEATHER");

    // Null playState and null setupPathList guards must hold without crashing.
    MM_gPlayState = NULL;
    GameInteractor_ExecuteOnActorInit(&type2Owl.actor);
    MM_gPlayState = play;
    play->setupPathList = NULL;
    GameInteractor_ExecuteOnActorInit(&type2Owl.actor);

    memset(sTestArenaBuffer, 0, sizeof(sTestArenaBuffer));
    MM_ZeldaArena_Init(sTestArenaBuffer, sizeof(sTestArenaBuffer));
    guard.arenaInitialized = true;

    Vec3s pathPoints[2] = { { 12, 34, 56 }, { 78, 90, 120 } };
    Path pathEntry{};
    pathEntry.count = 2;
    pathEntry.points = pathPoints;
    play->setupPathList = &pathEntry;
    play->objectCtx.numEntries = 1;
    play->objectCtx.slots[0].id = OBJECT_OWL;

    GameInteractor_ExecuteOnActorInit(&type2Owl.actor);
    SO1755_ASSERT(play->actorCtx.actorLists[ACTORCAT_NPC].length == 2, 24,
                  "AlwaysShowShrineFeathers did not spawn one feather per path point");

    Actor* spawned = play->actorCtx.actorLists[ACTORCAT_NPC].first;
    SO1755_ASSERT(spawned != nullptr && ENOWL_GET_TYPE(spawned) == ENOWL_GET_TYPE_30, 25,
                  "Spawned Shrine feather did not carry ENOWL_GET_TYPE_30");
    EnOwl* feather = reinterpret_cast<EnOwl*>(spawned);
    feather->unk_3DC = 10;
    MM_GameHooks_ExecuteOnActorUpdate(&feather->actor);
    SO1755_ASSERT(feather->unk_3DC == 0x12C, 26,
                  "AlwaysShowShrineFeathers OnActorUpdate did not lock unk_3DC to 0x12C");
    type2Owl.unk_3DC = 10;
    MM_GameHooks_ExecuteOnActorUpdate(&type2Owl.actor);
    SO1755_ASSERT(type2Owl.unk_3DC == 10, 27,
                  "AlwaysShowShrineFeathers OnActorUpdate modified a type-2 owl");

    FreeSpawnedOwls(play);
    play->setupPathList = NULL;
    DriveKey(kFeathersKey, 0);
    printf("[TEST] mm-small-options-1755: leg 2 (AlwaysShowShrineFeathers) passed\n");

    // ---- Leg 3: DashAfterRoll (2Ship #1938) -----------------------------------------------------------------------
    DriveKey(kDebugCamKey, 0);
    DriveKey(kSpeedModKey, 0);
    DriveKey(kPersistentHoodKey, 0);
    DriveKey(kDashAfterRollKey, 1);

    // Null play state must return safely.
    MM_gPlayState = NULL;
    input->cur.button = BTN_A;
    GameInteractor_ExecuteOnPassPlayerInputs(input);

    MM_gPlayState = play;
    player->actionFunc = Player_Action_26;
    player->av2.actionVar2 = 0;
    input->cur.button = BTN_A;
    GameInteractor_ExecuteOnPassPlayerInputs(input);
    SO1755_ASSERT(GameInteractor_Should(VB_CONSIDER_BUNNY_HOOD_EQUIPPED, false), 30,
                  "DashAfterRoll On did not set VB_CONSIDER_BUNNY_HOOD_EQUIPPED during roll");

    // 1-frame Idle bounce between roll and run preserves dash.
    player->actionFunc = MM_Player_Action_Idle;
    GameInteractor_ExecuteOnPassPlayerInputs(input);
    SO1755_ASSERT(GameInteractor_Should(VB_CONSIDER_BUNNY_HOOD_EQUIPPED, false), 31,
                  "DashAfterRoll did not preserve dash across 1-frame MM_Player_Action_Idle after roll");

    player->actionFunc = Player_Action_13;
    GameInteractor_ExecuteOnPassPlayerInputs(input);
    SO1755_ASSERT(GameInteractor_Should(VB_CONSIDER_BUNNY_HOOD_EQUIPPED, false), 32,
                  "DashAfterRoll dropped dash while running (Player_Action_13) with A held");

    // Bonking a roll (actionVar2 != 0) must not start dash.
    player->actionFunc = Player_Action_26;
    player->av2.actionVar2 = 1;
    GameInteractor_ExecuteOnPassPlayerInputs(input);
    SO1755_ASSERT(!GameInteractor_Should(VB_CONSIDER_BUNNY_HOOD_EQUIPPED, false), 33,
                  "DashAfterRoll started dash on a bonked roll (av2.actionVar2 != 0)");

    // Releasing A stops dash.
    player->actionFunc = Player_Action_26;
    player->av2.actionVar2 = 0;
    GameInteractor_ExecuteOnPassPlayerInputs(input);
    input->cur.button = 0;
    player->actionFunc = Player_Action_13;
    GameInteractor_ExecuteOnPassPlayerInputs(input);
    SO1755_ASSERT(!GameInteractor_Should(VB_CONSIDER_BUNNY_HOOD_EQUIPPED, false), 34,
                  "DashAfterRoll stayed dashing after BTN_A release");

    // Stack mode multiplies VB_SPEED_MODIFIER_WALK by 1.5f and leaves Bunny Hood VB alone.
    DriveKey(kDashAfterRollKey, 2);
    player->actionFunc = Player_Action_26;
    player->av2.actionVar2 = 0;
    input->cur.button = BTN_A;
    GameInteractor_ExecuteOnPassPlayerInputs(input);
    f32 speedTarget = 10.0f;
    (void)GameInteractor_Should(VB_SPEED_MODIFIER_WALK, true, &speedTarget);
    SO1755_ASSERT(speedTarget == 15.0f, 35,
                  "DashAfterRoll Stack mode did not scale speedTarget by 1.5x");
    SO1755_ASSERT(!GameInteractor_Should(VB_CONSIDER_BUNNY_HOOD_EQUIPPED, false), 36,
                  "DashAfterRoll Stack mode should not arm VB_CONSIDER_BUNNY_HOOD_EQUIPPED");
    DriveKey(kDashAfterRollKey, 0);
    printf("[TEST] mm-small-options-1755: leg 3 (DashAfterRoll) passed\n");

    // ---- Leg 4: AlwaysFindRockSirloin (2Ship #1944) ---------------------------------------------------------------
    DriveKey(kRockSirloinKey, 0);
    SO1755_ASSERT(!GameInteractor_Should(VB_FIND_ROCK_SIRLOIN, false), 40,
                  "AlwaysFindRockSirloin off altered false VB_FIND_ROCK_SIRLOIN");
    SO1755_ASSERT(GameInteractor_Should(VB_FIND_ROCK_SIRLOIN, true), 41,
                  "AlwaysFindRockSirloin off altered true VB_FIND_ROCK_SIRLOIN");
    DriveKey(kRockSirloinKey, 1);
    SO1755_ASSERT(GameInteractor_Should(VB_FIND_ROCK_SIRLOIN, false), 42,
                  "AlwaysFindRockSirloin on did not force VB_FIND_ROCK_SIRLOIN to true");
    DriveKey(kRockSirloinKey, 0);
    printf("[TEST] mm-small-options-1755: leg 4 (AlwaysFindRockSirloin) passed\n");

    // ---- Leg 5: FishermanJumpingGame (2Ship #1947) ----------------------------------------------------------------
    DriveKey(kTorchLimitKey, TORCH_TIME_LIMIT_NORMAL);
    SO1755_ASSERT(GameInteractor_Should(VB_JUMPING_GAME_TORCH_RUN_OUT, true, 150), 50,
                  "TorchTimeLimit Normal cancelled VB_JUMPING_GAME_TORCH_RUN_OUT");

    DriveKey(kTorchLimitKey, TORCH_TIME_LIMIT_DOUBLE);
    SO1755_ASSERT(!GameInteractor_Should(VB_JUMPING_GAME_TORCH_RUN_OUT, true, 150), 51,
                  "TorchTimeLimit Double let torch burn out at 150");
    SO1755_ASSERT(!GameInteractor_Should(VB_JUMPING_GAME_TORCH_RUN_OUT, true, 200), 52,
                  "TorchTimeLimit Double let torch burn out at boundary 200");
    SO1755_ASSERT(GameInteractor_Should(VB_JUMPING_GAME_TORCH_RUN_OUT, false, 201), 53,
                  "TorchTimeLimit Double failed to burn out past 200");

    DriveKey(kTorchLimitKey, TORCH_TIME_LIMIT_INFINITY);
    SO1755_ASSERT(!GameInteractor_Should(VB_JUMPING_GAME_TORCH_RUN_OUT, true, 9999), 54,
                  "TorchTimeLimit Infinity allowed torch to burn out");
    DriveKey(kTorchLimitKey, TORCH_TIME_LIMIT_NORMAL);

    // TargetScore: score comparison + 0x109A custom message replacement.
    DriveKey(kTargetScoreKey, 10);
    gSaveContext.minigameScore = 9;
    SO1755_ASSERT(GameInteractor_Should(VB_FAIL_FISHERMAN_JUMPING_GAME, false), 55,
                  "TargetScore 10 did not fail at minigameScore == 9");
    gSaveContext.minigameScore = 10;
    SO1755_ASSERT(!GameInteractor_Should(VB_FAIL_FISHERMAN_JUMPING_GAME, true), 56,
                  "TargetScore 10 still failed at minigameScore == 10");

    char rawEntryBytes[MESSAGE_HEADER_SIZE + 8] = {};
    rawEntryBytes[9] = '\xFF';
    rawEntryBytes[10] = '\xFF';
    memcpy(rawEntryBytes + MESSAGE_HEADER_SIZE, "\x01" "20 pts\xBF", 8);
    MessageTableEntry fakeTable[2]{};
    fakeTable[0].textId = 0x109A;
    fakeTable[0].segment = rawEntryBytes;
    fakeTable[0].msgSize = sizeof(rawEntryBytes);
    fakeTable[1].textId = 0xFFFF;
    play->msgCtx.messageTableNES = fakeTable;
    MM_gPlayState = play;

    u16 textId = 0x109A;
    bool loadFromTable = true;
    GameInteractor_ExecuteOnOpenText(&textId, &loadFromTable);
    SO1755_ASSERT(!loadFromTable, 57, "TargetScore OnOpenText[0x109A] did not clear loadFromMessageTable");
    SO1755_ASSERT(play->msgCtx.msgLength > MESSAGE_HEADER_SIZE, 58,
                  "TargetScore OnOpenText[0x109A] produced empty message buffer");
    const std::string rendered(reinterpret_cast<const char*>(play->msgCtx.font.msgBuf.schar + MESSAGE_HEADER_SIZE),
                               play->msgCtx.msgLength - MESSAGE_HEADER_SIZE);
    SO1755_ASSERT(rendered.find("\x01" "10") != std::string::npos && rendered.find("20") == std::string::npos, 59,
                  "TargetScore OnOpenText[0x109A] did not replace vanilla 20 with target score");
    play->msgCtx.messageTableNES = nullptr;

    // EarlyWin zeroes TIMER_ID_MINIGAME_2 and ends game once score reaches TargetScore.
    DriveKey(kEarlyWinKey, 1);
    gSaveContext.minigameScore = 9;
    gSaveContext.timerCurTimes[TIMER_ID_MINIGAME_2] = SECONDS_TO_TIMER(30);
    SO1755_ASSERT(!GameInteractor_Should(VB_JUMPING_GAME_END, false), 60,
                  "EarlyWin ended jumping game below target score");
    SO1755_ASSERT(gSaveContext.timerCurTimes[TIMER_ID_MINIGAME_2] == SECONDS_TO_TIMER(30), 61,
                  "EarlyWin modified timer below target score");

    gSaveContext.minigameScore = 10;
    SO1755_ASSERT(GameInteractor_Should(VB_JUMPING_GAME_END, false), 62,
                  "EarlyWin did not end jumping game at target score");
    SO1755_ASSERT(gSaveContext.timerCurTimes[TIMER_ID_MINIGAME_2] == SECONDS_TO_TIMER(0), 63,
                  "EarlyWin did not zero TIMER_ID_MINIGAME_2 upon reaching target score");

    DriveKey(kEarlyWinKey, 0);
    ClearKey(kTargetScoreKey);
    printf("[TEST] mm-small-options-1755: leg 5 (FishermanJumpingGame) passed\n");

    return 0;
}

} // namespace

extern "C" int MM_SmallOptions1755_RunHeadless(void) {
    ScopedStateRestore guard;
    const int rc = RunLegs(guard);
    if (rc == 0) {
        printf("[TEST] mm-small-options-1755: PASS\n");
    }
    return rc;
}

#endif // RSBS_SINGLE_EXECUTABLE
