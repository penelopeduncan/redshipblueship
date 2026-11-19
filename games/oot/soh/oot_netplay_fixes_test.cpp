/**
 * @file oot_netplay_fixes_test.cpp
 * @brief ROM-free, display-free, connection-free checks for OoT netplay,
 *        Anchor, DummyPlayer, and GameInteractor fixes (#1751, #1750).
 *
 * Legs:
 *   (a) Anchor::HandlePacket_PlayerUpdate: a PLAYER_UPDATE payload missing keys
 *       (e.g. sceneNum, linkAge, jointTable) is handled without throwing and
 *       applies defaults, while a complete payload populates client fields.
 *   (b) DummyPlayer_Update: a client.unk_862 (draw id + 1) past this tree's
 *       draw table row count (GID_MAXIMUM, GID_MAXIMUM + 1) clamps to
 *       GID_STONE_OF_AGONY instead of indexing past OoT_sDrawItemTable; the
 *       last in-table value (GID_FISHING_POLE) and GID_OCARINA_TIME are kept.
 *   (c) GameInteractor::IsGameplayPaused (and IsPlayerInControl): a null
 *       OoT_gPlayState (and a PlayState with null GET_PLAYER) returns safely
 *       without dereferencing NULL.
 *   (d) VB_BE_ELIGIBLE_FOR_GREAT_FAIRY_REWARD in a randomizer file (magic and
 *       spell fountains): the TimeSavers handler sets the fountain's
 *       RandomizerInf flag and answers false, so the vanilla reward is not
 *       granted on top of the randomized check.
 *   (e) Anchor::OnIncomingJson: PLAYER_UPDATEs from one sender coalesce into
 *       one pending update (not the unbounded queue), and
 *       ProcessIncomingPacketQueue applies the newest one.
 */

#ifdef RSBS_SINGLE_EXECUTABLE

#include "soh/Network/Anchor/Anchor.h"
#include <libultraship/libultraship.h>
#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/ObjectExtension/ObjectExtension.h"
#include <nlohmann/json.hpp>

extern "C" {
#include "macros.h"
#include "variables.h"
#include "functions.h"
#include "src/overlays/actors/ovl_Bg_Dy_Yoseizo/z_bg_dy_yoseizo.h"
extern PlayState* OoT_gPlayState;
// z_draw.c: OoT_sDrawItemTable's row count.
s32 OoT_GetItem_DrawTableCount(void);
}

#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <memory>
#include <mutex>
#include <vector>

struct AnchorNetplayFixesTestAccess {
    static void HandlePlayerUpdate(Anchor& anchor, nlohmann::json payload) {
        anchor.HandlePacket_PlayerUpdate(std::move(payload));
    }

    static void SetDummyPlayerClientId(Anchor& anchor, const Actor* actor, uint32_t clientId) {
        anchor.SetDummyPlayerClientId(actor, clientId);
    }

    static bool ShouldRefreshActors(const Anchor& anchor) {
        return anchor.shouldRefreshActors;
    }

    static void ResetShouldRefreshActors(Anchor& anchor) {
        anchor.shouldRefreshActors = false;
    }

    static size_t PendingPlayerUpdateCount(Anchor& anchor) {
        std::lock_guard<std::mutex> lock(anchor.incomingPacketQueueMutex);
        return anchor.pendingPlayerUpdates.size();
    }

    static size_t QueuedPacketCount(Anchor& anchor) {
        std::lock_guard<std::mutex> lock(anchor.incomingPacketQueueMutex);
        return anchor.incomingPacketQueue.size();
    }
};

// timesaver_hook_handlers.cpp: registered on every file load by the TimeSavers init.
void TimeSaverOnVanillaBehaviorHandler(GIVanillaBehavior id, bool* should, va_list originalArgs);

namespace {

struct NetplayTestGuard {
    Anchor* prevAnchor = Anchor::Instance;
    PlayState* prevPlayState = OoT_gPlayState;
    const Actor* trackedActor = nullptr;

    ~NetplayTestGuard() {
        if (trackedActor != nullptr) {
            ObjectExtension::GetInstance().Free(trackedActor);
        }
        Anchor::Instance = prevAnchor;
        OoT_gPlayState = prevPlayState;
    }
};

} // namespace

#define NETPLAY_ASSERT(cond, msg)                                 \
    do {                                                          \
        if (!(cond)) {                                            \
            printf("[TEST] FAIL (oot-netplay-fixes): %s\n", msg); \
            return 1;                                             \
        }                                                         \
    } while (0)

namespace {

// Calls the TimeSavers vanilla-behaviour handler the way GameInteractor_Should's dispatch does.
bool NetplayTimeSaverShould(GIVanillaBehavior id, bool* should, ...) {
    va_list args;
    va_start(args, should);
    TimeSaverOnVanillaBehaviorHandler(id, should, args);
    va_end(args);
    return *should;
}

// One fountain in a randomizer file: the handler must set the fountain's RandomizerInf flag (which queues the
// randomized check) and answer false, so BgDyYoseizo_CheckMagicAcquired kills the fairy before
// BgDyYoseizo_ChooseType can grant the vanilla magic, double magic, double defence or spell.
int NetplayCheckRandoFountain(PlayState* play, s16 sceneNum, RandomizerInf flag, const char* label) {
    play->sceneNum = sceneNum;
    std::unique_ptr<BgDyYoseizo> fairy = std::make_unique<BgDyYoseizo>();
    fairy->fountainType = 0;
    bool should = true;
    NetplayTimeSaverShould(VB_BE_ELIGIBLE_FOR_GREAT_FAIRY_REWARD, &should, fairy.get());
    if (!Flags_GetRandomizerInf(flag)) {
        printf("[TEST] FAIL (oot-netplay-fixes): %s: rando Great Fairy did not set its RandomizerInf flag\n", label);
        return 1;
    }
    if (should) {
        printf("[TEST] FAIL (oot-netplay-fixes): %s: rando Great Fairy VB stayed true, so the vanilla reward is "
               "granted on top of the randomized check\n",
               label);
        return 1;
    }
    return 0;
}

int NetplayRunGreatFairyLeg(PlayState* play) {
    // Flags_SetRandomizerInf dispatches OnFlagSet through the instance (no hook is registered here).
    if (GameInteractor::Instance == nullptr) {
        GameInteractor::Instance = new GameInteractor();
    }
    SaveContext* savedSave = static_cast<SaveContext*>(malloc(sizeof(SaveContext)));
    if (savedSave == nullptr) {
        printf("[TEST] FAIL (oot-netplay-fixes): could not allocate SaveContext snapshot\n");
        return 1;
    }
    memcpy(savedSave, &gSaveContext, sizeof(SaveContext));
    memset(&gSaveContext, 0, sizeof(SaveContext));
    gSaveContext.ship.quest.id = QUEST_RANDOMIZER;
    PlayState* prevPlayState = OoT_gPlayState;
    OoT_gPlayState = play;

    int failed = NetplayCheckRandoFountain(play, SCENE_GREAT_FAIRYS_FOUNTAIN_MAGIC, RAND_INF_DMT_GREAT_FAIRY_REWARD,
                                           "magic fountain (DMT)");
    if (failed == 0) {
        failed = NetplayCheckRandoFountain(play, SCENE_GREAT_FAIRYS_FOUNTAIN_SPELLS, RAND_INF_ZF_GREAT_FAIRY_REWARD,
                                           "spell fountain (ZF)");
    }

    OoT_gPlayState = prevPlayState;
    memcpy(&gSaveContext, savedSave, sizeof(SaveContext));
    free(savedSave);
    return failed;
}

} // namespace

extern "C" int OoT_NetplayFixes_RunHeadless(void) {
    constexpr uint32_t kClientId = 42u;

    std::unique_ptr<Anchor> anchor = std::make_unique<Anchor>();
    std::unique_ptr<PlayState> play = std::make_unique<PlayState>();
    std::unique_ptr<Player> player = std::make_unique<Player>();
    NetplayTestGuard guard;

    Anchor::Instance = anchor.get();
    AnchorNetplayFixesTestAccess::SetDummyPlayerClientId(*anchor, &player->actor, kClientId);
    guard.trackedActor = &player->actor;

    // ---- (a) HandlePacket_PlayerUpdate missing key vs complete payload ----
    AnchorClient& client = anchor->clients[kClientId];
    client.clientId = kClientId;
    client.linkAge = LINK_AGE_CHILD;
    client.sceneNum = 0x12;
    client.unk_862 = 99;
    AnchorNetplayFixesTestAccess::ResetShouldRefreshActors(*anchor);

    try {
        AnchorNetplayFixesTestAccess::HandlePlayerUpdate(*anchor, nlohmann::json{ { "clientId", kClientId } });
    } catch (const std::exception& e) {
        printf("[TEST] FAIL (oot-netplay-fixes): HandlePacket_PlayerUpdate threw on partial payload: %s\n", e.what());
        return 1;
    }

    NETPLAY_ASSERT(client.sceneNum == (s16)SCENE_ID_MAX,
                   "partial PLAYER_UPDATE did not default missing sceneNum to SCENE_ID_MAX");
    NETPLAY_ASSERT(client.linkAge == (s32)LINK_AGE_ADULT,
                   "partial PLAYER_UPDATE did not default missing linkAge to LINK_AGE_ADULT");
    NETPLAY_ASSERT(client.unk_862 == 0, "partial PLAYER_UPDATE did not default missing unk_862 to 0");
    NETPLAY_ASSERT(AnchorNetplayFixesTestAccess::ShouldRefreshActors(*anchor),
                   "linkAge change in HandlePacket_PlayerUpdate did not flag shouldRefreshActors");

    std::vector<int> jointArray;
    jointArray.reserve(24 * 3);
    for (int i = 0; i < 24 * 3; i++) {
        jointArray.push_back(i + 1);
    }

    AnchorNetplayFixesTestAccess::ResetShouldRefreshActors(*anchor);
    nlohmann::json fullPayload = {
        { "clientId", kClientId },
        { "sceneNum", 0x55 },
        { "entranceIndex", 0x0123 },
        { "linkAge", LINK_AGE_CHILD },
        { "posRot",
          {
              { "pos", { { "x", 12.5f }, { "y", -3.0f }, { "z", 44.25f } } },
              { "rot", { { "x", 100 }, { "y", 200 }, { "z", 300 } } },
          } },
        { "jointTable", jointArray },
        { "prevTransl", { { "x", 7 }, { "y", 8 }, { "z", 9 } } },
        { "movementFlags", 5 },
        { "upperLimbRot", { { "x", 11 }, { "y", 22 }, { "z", 33 } } },
        { "currentBoots", 1 },
        { "currentShield", 2 },
        { "currentTunic", 3 },
        { "stateFlags1", 0x12345678u },
        { "stateFlags2", 0x00ABCDEFu },
        { "buttonItem0", 4 },
        { "itemAction", 6 },
        { "heldItemAction", 7 },
        { "modelGroup", 8 },
        { "invincibilityTimer", 9 },
        { "unk_862", (s16)GID_OCARINA_TIME },
        { "unk_85C", 2.5f },
        { "actionVar1", -3 },
    };

    try {
        AnchorNetplayFixesTestAccess::HandlePlayerUpdate(*anchor, std::move(fullPayload));
    } catch (const std::exception& e) {
        printf("[TEST] FAIL (oot-netplay-fixes): HandlePacket_PlayerUpdate threw on complete payload: %s\n", e.what());
        return 1;
    }

    NETPLAY_ASSERT(client.sceneNum == 0x55, "complete PLAYER_UPDATE did not update sceneNum");
    NETPLAY_ASSERT(client.entranceIndex == 0x0123, "complete PLAYER_UPDATE did not update entranceIndex");
    NETPLAY_ASSERT(client.linkAge == LINK_AGE_CHILD, "complete PLAYER_UPDATE did not update linkAge");
    NETPLAY_ASSERT(client.posRot.pos.x == 12.5f && client.posRot.pos.y == -3.0f && client.posRot.pos.z == 44.25f,
                   "complete PLAYER_UPDATE did not update posRot.pos");
    NETPLAY_ASSERT(client.posRot.rot.x == 100 && client.posRot.rot.y == 200 && client.posRot.rot.z == 300,
                   "complete PLAYER_UPDATE did not update posRot.rot");
    NETPLAY_ASSERT(client.jointTable[0].x == 1 && client.jointTable[0].y == 2 && client.jointTable[0].z == 3,
                   "complete PLAYER_UPDATE did not update jointTable[0]");
    NETPLAY_ASSERT(client.jointTable[23].x == 70 && client.jointTable[23].y == 71 && client.jointTable[23].z == 72,
                   "complete PLAYER_UPDATE did not update jointTable[23]");
    NETPLAY_ASSERT(client.movementFlags == 5, "complete PLAYER_UPDATE did not update movementFlags");
    NETPLAY_ASSERT(client.prevTransl.x == 7 && client.prevTransl.y == 8 && client.prevTransl.z == 9,
                   "complete PLAYER_UPDATE did not update prevTransl");
    NETPLAY_ASSERT(client.upperLimbRot.x == 11 && client.upperLimbRot.y == 22 && client.upperLimbRot.z == 33,
                   "complete PLAYER_UPDATE did not update upperLimbRot");
    NETPLAY_ASSERT(client.currentBoots == 1 && client.currentShield == 2 && client.currentTunic == 3,
                   "complete PLAYER_UPDATE did not update equips");
    NETPLAY_ASSERT(client.stateFlags1 == 0x12345678u && client.stateFlags2 == 0x00ABCDEFu,
                   "complete PLAYER_UPDATE did not update stateFlags");
    NETPLAY_ASSERT(client.buttonItem0 == 4 && client.itemAction == 6 && client.heldItemAction == 7 &&
                       client.modelGroup == 8 && client.invincibilityTimer == 9,
                   "complete PLAYER_UPDATE did not update action/model/timer fields");
    NETPLAY_ASSERT(client.unk_862 == (s16)GID_OCARINA_TIME && client.unk_85C == 2.5f && client.actionVar1 == -3,
                   "complete PLAYER_UPDATE did not update draw-id/actionVar1 fields");

    // ---- (b) DummyPlayer_Update unk_862 clamp ----
    play->sceneNum = 0x55;
    OoT_gPlayState = play.get();
    anchor->roomState.pvpMode = 0;

    client.online = true;
    client.isSaveLoaded = true;
    client.movementFlags = 0;
    client.modelGroup = 0;
    player->modelGroup = 0;

    // unk_862 is draw id + 1. This tree's draw table has no GID_FISHING_POLE row, so its row count is
    // GID_MAXIMUM - 1 and unk_862 == GID_MAXIMUM (a peer holding the randomizer Fishing Pole) is out of range.
    NETPLAY_ASSERT(OoT_GetItem_DrawTableCount() == (s32)GID_FISHING_POLE,
                   "premise: OoT_sDrawItemTable row count is no longer GID_FISHING_POLE; re-derive the clamp legs");

    client.unk_862 = (s16)(GID_MAXIMUM + 1);
    DummyPlayer_Update(&player->actor, play.get());
    NETPLAY_ASSERT(player->unk_862 == (s16)GID_STONE_OF_AGONY,
                   "DummyPlayer_Update did not clamp out-of-range unk_862 to GID_STONE_OF_AGONY");

    client.unk_862 = (s16)GID_MAXIMUM;
    DummyPlayer_Update(&player->actor, play.get());
    NETPLAY_ASSERT(player->unk_862 == (s16)GID_STONE_OF_AGONY,
                   "DummyPlayer_Update kept unk_862 == GID_MAXIMUM, one row past OoT_sDrawItemTable");

    client.unk_862 = (s16)GID_FISHING_POLE;
    DummyPlayer_Update(&player->actor, play.get());
    NETPLAY_ASSERT(player->unk_862 == (s16)GID_FISHING_POLE,
                   "DummyPlayer_Update clamped the last in-table unk_862 (GID_FISHING_POLE, draws row 0x7D)");

    client.unk_862 = (s16)GID_OCARINA_TIME;
    DummyPlayer_Update(&player->actor, play.get());
    NETPLAY_ASSERT(player->unk_862 == (s16)GID_OCARINA_TIME, "DummyPlayer_Update altered valid unk_862 draw id");

    // ---- (c) GameInteractor::IsGameplayPaused / IsPlayerInControl null PlayState ----
    OoT_gPlayState = NULL;
    NETPLAY_ASSERT(GameInteractor::IsGameplayPaused(),
                   "GameInteractor::IsGameplayPaused returned false on null OoT_gPlayState");
    NETPLAY_ASSERT(!GameInteractor::IsPlayerInControl(),
                   "GameInteractor::IsPlayerInControl returned true on null OoT_gPlayState");

    OoT_gPlayState = play.get();
    NETPLAY_ASSERT(GameInteractor::IsGameplayPaused(),
                   "GameInteractor::IsGameplayPaused returned false when GET_PLAYER(OoT_gPlayState) is NULL");
    NETPLAY_ASSERT(!GameInteractor::IsPlayerInControl(),
                   "GameInteractor::IsPlayerInControl returned true when GET_PLAYER(OoT_gPlayState) is NULL");

    // ---- (d) Great Fairy VB in a randomizer file (942134669 port) ----
    if (NetplayRunGreatFairyLeg(play.get()) != 0) {
        return 1;
    }

    // ---- (e) PLAYER_UPDATE coalescing on the receive side ----
    client.sceneNum = 0x12;
    AnchorNetplayFixesTestAccess::ResetShouldRefreshActors(*anchor);
    anchor->OnIncomingJson(nlohmann::json{ { "type", Anchor::PLAYER_UPDATE },
                                           { "clientId", kClientId },
                                           { "sceneNum", 0x10 },
                                           { "linkAge", LINK_AGE_CHILD },
                                           { "quiet", true } });
    anchor->OnIncomingJson(nlohmann::json{ { "type", Anchor::PLAYER_UPDATE },
                                           { "clientId", kClientId },
                                           { "sceneNum", 0x11 },
                                           { "linkAge", LINK_AGE_CHILD },
                                           { "quiet", true } });
    NETPLAY_ASSERT(AnchorNetplayFixesTestAccess::PendingPlayerUpdateCount(*anchor) == 1,
                   "two PLAYER_UPDATEs from one sender were not coalesced into one pending update");
    NETPLAY_ASSERT(AnchorNetplayFixesTestAccess::QueuedPacketCount(*anchor) == 0,
                   "a PLAYER_UPDATE was pushed onto the unbounded incoming packet queue");
    NETPLAY_ASSERT(client.sceneNum == 0x12, "PLAYER_UPDATE was applied on the receive side instead of being queued");
    anchor->ProcessIncomingPacketQueue();
    NETPLAY_ASSERT(client.sceneNum == 0x11, "ProcessIncomingPacketQueue did not apply the newest PLAYER_UPDATE");
    NETPLAY_ASSERT(AnchorNetplayFixesTestAccess::PendingPlayerUpdateCount(*anchor) == 0,
                   "ProcessIncomingPacketQueue left a pending PLAYER_UPDATE behind");

    printf("[TEST] PASS: oot-netplay-fixes - PlayerUpdate partial/full payload, DummyPlayer unk_862 clamp, null "
           "PlayState paused check, rando Great Fairy VB, and PLAYER_UPDATE coalescing\n");
    return 0;
}

#endif // RSBS_SINGLE_EXECUTABLE
