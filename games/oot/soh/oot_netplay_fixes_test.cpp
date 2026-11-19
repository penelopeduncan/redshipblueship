/**
 * @file oot_netplay_fixes_test.cpp
 * @brief ROM-free, display-free, connection-free checks for OoT netplay,
 *        Anchor, DummyPlayer, and GameInteractor fixes (#1751, #1750).
 *
 * Legs:
 *   (a) Anchor::HandlePacket_PlayerUpdate: a PLAYER_UPDATE payload missing keys
 *       (e.g. sceneNum, linkAge, jointTable) is handled without throwing and
 *       applies defaults, while a complete payload populates client fields.
 *   (b) DummyPlayer_Update: an out-of-range client.unk_862 past GID_MAXIMUM
 *       clamps to GID_STONE_OF_AGONY instead of indexing past the draw table;
 *       valid draw ids (at GID_MAXIMUM and GID_OCARINA_TIME) are preserved.
 *   (c) GameInteractor::IsGameplayPaused (and IsPlayerInControl): a null
 *       OoT_gPlayState (and a PlayState with null GET_PLAYER) returns safely
 *       without dereferencing NULL.
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
extern PlayState* OoT_gPlayState;
}

#include <cstdio>
#include <exception>
#include <memory>
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
};

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

#define NETPLAY_ASSERT(cond, msg)                                  \
    do {                                                           \
        if (!(cond)) {                                             \
            printf("[TEST] FAIL (oot-netplay-fixes): %s\n", msg);  \
            return 1;                                              \
        }                                                          \
    } while (0)

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

    client.unk_862 = (s16)(GID_MAXIMUM + 1);
    DummyPlayer_Update(&player->actor, play.get());
    NETPLAY_ASSERT(player->unk_862 == (s16)GID_STONE_OF_AGONY,
                   "DummyPlayer_Update did not clamp out-of-range unk_862 to GID_STONE_OF_AGONY");

    client.unk_862 = (s16)GID_MAXIMUM;
    DummyPlayer_Update(&player->actor, play.get());
    NETPLAY_ASSERT(player->unk_862 == (s16)GID_MAXIMUM,
                   "DummyPlayer_Update incorrectly clamped GID_MAXIMUM boundary");

    client.unk_862 = (s16)GID_OCARINA_TIME;
    DummyPlayer_Update(&player->actor, play.get());
    NETPLAY_ASSERT(player->unk_862 == (s16)GID_OCARINA_TIME,
                   "DummyPlayer_Update altered valid unk_862 draw id");

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

    printf("[TEST] PASS: oot-netplay-fixes - PlayerUpdate partial/full payload, DummyPlayer unk_862 clamp, and null PlayState paused check\n");
    return 0;
}

#endif // RSBS_SINGLE_EXECUTABLE
