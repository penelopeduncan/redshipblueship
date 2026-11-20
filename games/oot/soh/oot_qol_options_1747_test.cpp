/**
 * @file oot_qol_options_1747_test.cpp
 * ROM-free, display-free lock for the #1747 QoL options backport (SoH
 * 6606eefbf #6138, ef014a35e #6663, 3250cc27f #6569, 0ed782ddc #7023,
 * a629af2e9 #6604). CTest label "redship", row OoTQolOptions1747 in
 * CMake/SingleExecutable.cmake, dispatch "oot-qol-options-1747" in
 * src/common/test_runner.cpp.
 *
 * WHAT IT DRIVES. For each of the five new registrars
 * (Graphics/DisableLinkSpinWithGoronPot.cpp, DisableHeatHaze.cpp,
 * Graphics/SariaGestureFriendsForever.cpp, DropThrowOnlyActors.cpp,
 * TimeSavers/ImprovedRoll.cpp): find it in ShipInit's map by its registration
 * site under "*" and under each of its keys (so a registrar the linker dropped,
 * or a key left off the list, fails the row), set the key, re-run it the way a
 * CVar change does, then call the REAL GameInteractor_Should with the
 * arguments the game passes. OoT_gPlayState is a zeroed static PlayState.
 *
 * THE LEGS.
 *   (a) Goron Pot: off -> true; on -> false (Actor_Find finds no basket).
 *   (b) Heat haze: off -> true; on -> false.
 *   (c) Saria, with a zeroed EnSa: in SCENE_SARIAS_HOUSE (vanilla false)
 *       off -> false, on -> true; in SCENE_KOKIRI_FOREST, on -> true.
 *   (d) Throw-only, with a fake ACTOR_OBJ_TSUBO carrying ACTOR_FLAG_THROW_ONLY:
 *       off -> false and the flag is kept; on -> true and the flag is cleared.
 *       The renamed z_player.c function OoT_Player_CanThrowCarriedActor with a
 *       standing fake Player answers throw (true) off and drop (false) on.
 *       ACTOR_EN_BOM_CHU, on -> the vanilla value, both ways.
 *   (e) Roll chain: on, curFrame 20, A pressed, floor type 7 -> false,
 *       Player_SetupRoll not called (no action set) and one handler
 *       registered; off -> false and no handler registered. A chain that
 *       starts is not checked here (it needs a ROM animation).
 *   (f) Roll steer, with gGameInfo pointed at a static GameInfo with
 *       R_UPDATE_RATE = 3: both keys on, Z clear, rot.y 0, target 0x1000 ->
 *       rot.y 0x300 and false. Z held -> rot.y 0. Steering off -> rot.y 0.
 *       Improved Roll off with steering on -> rot.y 0.
 *
 * Each key is cleared after its leg and never stored; on the way out every key
 * is put back (cleared when it was unset), the five registrars are re-run to
 * match, and OoT_gPlayState, gGameInfo and the current game are restored. The
 * row touches no save memory.
 */

#ifdef RSBS_SINGLE_EXECUTABLE

// At file scope, not inside an extern "C" block: z64.h includes <memory>
// under C++ (see oot_shared_kokiri_sword_test.cpp).
#include <z64.h>
#include "variables.h"
#include "macros.h"

#include "context.h"
#include "soh/ShipInit.hpp"
#include "soh/cvar_prefixes.h"
#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/Enhancements/game-interactor/GameInteractor_Hooks.h"

#include <cstdio>
#include <cstring>
#include <string>

extern "C" {
#include "src/overlays/actors/ovl_En_Sa/z_en_sa.h"
extern PlayState* OoT_gPlayState;
// z_player.c (0ed782ddc's Player_CanThrowCarriedActor, OoT_-prefixed here).
s32 OoT_Player_CanThrowCarriedActor(Player* player, Actor* actor);
}

namespace {

int sFailures = 0;

#define QOL_CHECK(cond, ...)                                \
    do {                                                    \
        if (!(cond)) {                                      \
            printf("[TEST] FAIL (oot-qol-options-1747): "); \
            printf(__VA_ARGS__);                            \
            printf(" (%s:%d)\n", __FILE__, __LINE__);       \
            sFailures++;                                    \
        }                                                   \
    } while (0)

const char* const kGoronPotKey = CVAR_ENHANCEMENT("DisableLinkSpinWithGoronPot");
const char* const kHeatHazeKey = CVAR_SETTING("A11yNoHeatHaze");
const char* const kSariaKey = CVAR_ENHANCEMENT("SariaGestureFriendsForever");
const char* const kThrowOnlyKey = CVAR_ENHANCEMENT("DropThrowOnlyObjects");
const char* const kRollKey = CVAR_ENHANCEMENT("ImprovedRoll");
const char* const kSteerKey = CVAR_ENHANCEMENT("ImprovedRollSteering");
const char* const kRowKeys[] = { kGoronPotKey, kHeatHazeKey, kSariaKey, kThrowOnlyKey, kRollKey, kSteerKey };
constexpr int kRowKeyCount = 6;

const char* const kGoronPotFile = "DisableLinkSpinWithGoronPot.cpp";
const char* const kHeatHazeFile = "DisableHeatHaze.cpp";
const char* const kSariaFile = "SariaGestureFriendsForever.cpp";
const char* const kThrowOnlyFile = "DropThrowOnlyActors.cpp";
const char* const kRollFile = "ImprovedRoll.cpp";
const char* const kRowFiles[] = { kGoronPotFile, kHeatHazeFile, kSariaFile, kThrowOnlyFile, kRollFile };
constexpr int kRowFileCount = 5;

// File-static: PlayState, Player and EnSa are large; every leg re-zeroes them.
PlayState sPlay;
Player sPlayer;
EnSa sSaria;
Actor sCarried;
Input sInput;
GameInfo sGameInfo;

bool IsSite(const ShipInitEntry& entry, const char* want) {
    const std::string file = entry.registeredAt.file_name();
    const size_t n = strlen(want);
    if (file.size() < n || file.compare(file.size() - n, n, want) != 0) {
        return false;
    }
    // A whole basename: preceded by a path separator (or nothing).
    return file.size() == n || file[file.size() - n - 1] == '/' || file[file.size() - n - 1] == '\\';
}

int CountRegistrars(const char* key, const char* file) {
    int found = 0;
    for (const auto& entry : ShipInit::GetAll()[key]) {
        if (IsSite(entry, file)) {
            found++;
        }
    }
    return found;
}

// Re-runs @p file's registrar under @p key, and only that one (the other
// registrars under "*" are not this row's business).
void RunRegistrar(const char* key, const char* file) {
    for (const auto& entry : ShipInit::GetAll()[key]) {
        if (IsSite(entry, file)) {
            entry.initFunc();
        }
    }
}

void SetKey(const char* key, bool on) {
    if (on) {
        CVarSetInteger(key, 1);
    } else {
        CVarClear(key);
    }
}

void ResetState(void) {
    memset(&sPlay, 0, sizeof(sPlay));
    memset(&sPlayer, 0, sizeof(sPlayer));
    memset(&sSaria, 0, sizeof(sSaria));
    memset(&sCarried, 0, sizeof(sCarried));
    memset(&sInput, 0, sizeof(sInput));
}

void LegGoronPot(void) {
    ResetState();
    SetKey(kGoronPotKey, false);
    RunRegistrar(kGoronPotKey, kGoronPotFile);
    QOL_CHECK(GameInteractor_Should(VB_LINK_SPIN_WITH_GORON_POT, true), "(a) off: Link no longer spins");
    SetKey(kGoronPotKey, true);
    RunRegistrar(kGoronPotKey, kGoronPotFile);
    QOL_CHECK(!GameInteractor_Should(VB_LINK_SPIN_WITH_GORON_POT, true), "(a) on: Link still spins");
    SetKey(kGoronPotKey, false);
    RunRegistrar(kGoronPotKey, kGoronPotFile);
}

void LegHeatHaze(void) {
    ResetState();
    SetKey(kHeatHazeKey, false);
    RunRegistrar(kHeatHazeKey, kHeatHazeFile);
    QOL_CHECK(GameInteractor_Should(VB_HOT_ROOM_DISTORTION, true), "(b) off: the hot-room distortion is refused");
    SetKey(kHeatHazeKey, true);
    RunRegistrar(kHeatHazeKey, kHeatHazeFile);
    QOL_CHECK(!GameInteractor_Should(VB_HOT_ROOM_DISTORTION, true), "(b) on: the hot-room distortion still applies");
    SetKey(kHeatHazeKey, false);
    RunRegistrar(kHeatHazeKey, kHeatHazeFile);
}

void LegSaria(void) {
    ResetState();
    sPlay.sceneNum = SCENE_SARIAS_HOUSE;
    SetKey(kSariaKey, false);
    RunRegistrar(kSariaKey, kSariaFile);
    QOL_CHECK(!GameInteractor_Should(VB_SARIA_GESTURE, false, &sSaria), "(c) off: Saria gestures in her house");
    SetKey(kSariaKey, true);
    RunRegistrar(kSariaKey, kSariaFile);
    QOL_CHECK(GameInteractor_Should(VB_SARIA_GESTURE, false, &sSaria), "(c) on: Saria does not gesture in her house");
    sPlay.sceneNum = SCENE_KOKIRI_FOREST;
    QOL_CHECK(GameInteractor_Should(VB_SARIA_GESTURE, true, &sSaria),
              "(c) on: Saria no longer gestures in Kokiri Forest");
    SetKey(kSariaKey, false);
    RunRegistrar(kSariaKey, kSariaFile);
}

bool ThrowOnlyShould(void) {
    return GameInteractor_Should(VB_ON_ACTOR_THROW_ONLY_CHECK, !(sCarried.flags & ACTOR_FLAG_THROW_ONLY), &sCarried);
}

void LegThrowOnly(void) {
    ResetState();
    // A standing Link (linearVelocity 0) carrying a pot.
    sCarried.id = ACTOR_OBJ_TSUBO;
    sCarried.flags = ACTOR_FLAG_THROW_ONLY;
    SetKey(kThrowOnlyKey, false);
    RunRegistrar(kThrowOnlyKey, kThrowOnlyFile);
    QOL_CHECK(!ThrowOnlyShould(), "(d) off: a pot may be dropped");
    QOL_CHECK((sCarried.flags & ACTOR_FLAG_THROW_ONLY) != 0, "(d) off: the pot lost ACTOR_FLAG_THROW_ONLY");
    QOL_CHECK(OoT_Player_CanThrowCarriedActor(&sPlayer, &sCarried),
              "(d) off: z_player.c lets a standing Link drop a pot");

    SetKey(kThrowOnlyKey, true);
    RunRegistrar(kThrowOnlyKey, kThrowOnlyFile);
    QOL_CHECK(ThrowOnlyShould(), "(d) on: a pot may not be dropped");
    QOL_CHECK((sCarried.flags & ACTOR_FLAG_THROW_ONLY) == 0, "(d) on: the pot kept ACTOR_FLAG_THROW_ONLY");
    sCarried.flags = ACTOR_FLAG_THROW_ONLY;
    QOL_CHECK(!OoT_Player_CanThrowCarriedActor(&sPlayer, &sCarried),
              "(d) on: z_player.c does not let a standing Link drop a pot");

    // A Bombchu is not one of the four actors: the vanilla value either way.
    sCarried.id = ACTOR_EN_BOM_CHU;
    sCarried.flags = 0;
    QOL_CHECK(ThrowOnlyShould(), "(d) on: a droppable Bombchu became throw-only");
    sCarried.flags = ACTOR_FLAG_THROW_ONLY;
    QOL_CHECK(!ThrowOnlyShould(), "(d) on: a throw-only Bombchu became droppable");
    QOL_CHECK((sCarried.flags & ACTOR_FLAG_THROW_ONLY) != 0, "(d) on: the Bombchu's flag was changed");

    SetKey(kThrowOnlyKey, false);
    RunRegistrar(kThrowOnlyKey, kThrowOnlyFile);
}

bool RollChainShould(void) {
    return GameInteractor_Should(VB_PLAYER_ROLL_CHAIN, false, &sPlayer, &sPlay, &sInput, (s32)7);
}

// Handlers registered under @p vb. An unregistration is applied lazily, on the
// next dispatch of that id, so read this only after a GameInteractor_Should.
size_t VbCount(GIVanillaBehavior vb) {
    auto& buckets = GameInteractor::RegisteredGameHooks<GameInteractor::OnVanillaBehavior>::functionsForID;
    auto it = buckets.find((int32_t)vb);
    return it == buckets.end() ? 0u : it->second.size();
}

// A chain that actually starts needs Player_SetupRoll's animation load (a ROM),
// so this leg locks the floor-type-7 guard and whether the handler is
// registered at all; only a person in the game can see a chained roll.
void LegRollChain(void) {
    ResetState();
    sPlayer.skelAnime.curFrame = 20.0f;
    sInput.press.button = BTN_A;
    SetKey(kRollKey, true);
    RunRegistrar(kRollKey, kRollFile);
    QOL_CHECK(!RollChainShould(), "(e) on: a roll chained on floor type 7");
    QOL_CHECK(sPlayer.actionFunc == nullptr, "(e) on: Player_SetupRoll ran on floor type 7");
    QOL_CHECK(VbCount(VB_PLAYER_ROLL_CHAIN) == 1, "(e) on: %zu roll-chain handler(s) registered, want 1",
              VbCount(VB_PLAYER_ROLL_CHAIN));
    SetKey(kRollKey, false);
    RunRegistrar(kRollKey, kRollFile);
    QOL_CHECK(!RollChainShould(), "(e) off: the roll-chain behaviour is not vanilla");
    QOL_CHECK(VbCount(VB_PLAYER_ROLL_CHAIN) == 0, "(e) off: %zu roll-chain handler(s) still registered, want 0",
              VbCount(VB_PLAYER_ROLL_CHAIN));
}

s16 Steer(u16 buttons, bool* outShould) {
    sPlayer.actor.shape.rot.y = 0;
    sPlay.state.input[0].cur.button = buttons;
    *outShould = GameInteractor_Should(VB_PLAYER_ROLL_STEER, false, &sPlayer, &sPlay, (s16)0x1000);
    return sPlayer.actor.shape.rot.y;
}

void LegRollSteer(void) {
    ResetState();
    bool should = true;
    SetKey(kRollKey, true);
    SetKey(kSteerKey, true);
    RunRegistrar(kSteerKey, kRollFile);
    s16 rot = Steer(0, &should);
    QOL_CHECK(rot == 0x300 && !should, "(f) both on, Z clear: rot.y 0x%X should %d, want 0x300 and 0", (u16)rot,
              should);
    rot = Steer(BTN_Z, &should);
    QOL_CHECK(rot == 0 && !should, "(f) both on, Z held: rot.y 0x%X should %d, want 0 and 0", (u16)rot, should);
    SetKey(kSteerKey, false);
    RunRegistrar(kSteerKey, kRollFile);
    rot = Steer(0, &should);
    QOL_CHECK(rot == 0 && !should, "(f) steering off: rot.y 0x%X should %d, want 0 and 0", (u16)rot, should);
    // Steering is a sub-option: with Improved Roll itself off it does nothing.
    SetKey(kSteerKey, true);
    SetKey(kRollKey, false);
    RunRegistrar(kRollKey, kRollFile);
    rot = Steer(0, &should);
    QOL_CHECK(rot == 0 && !should, "(f) roll off, steering on: rot.y 0x%X should %d, want 0 and 0", (u16)rot, should);
    SetKey(kSteerKey, false);
    RunRegistrar(kSteerKey, kRollFile);
}

void CheckRegistered(const char* file, const char* key) {
    QOL_CHECK(CountRegistrars("*", file) == 1, "%s's boot registrar is not in ShipInit's map exactly once", file);
    QOL_CHECK(CountRegistrars(key, file) == 1, "%s's registrar does not re-run on %s", file, key);
}

void RunLegs(void) {
    CheckRegistered(kGoronPotFile, kGoronPotKey);
    CheckRegistered(kHeatHazeFile, kHeatHazeKey);
    CheckRegistered(kSariaFile, kSariaKey);
    CheckRegistered(kThrowOnlyFile, kThrowOnlyKey);
    CheckRegistered(kRollFile, kRollKey);
    CheckRegistered(kRollFile, kSteerKey);
    if (sFailures != 0) {
        return;
    }
    LegGoronPot();
    LegHeatHaze();
    LegSaria();
    LegThrowOnly();
    LegRollChain();
    LegRollSteer();
}

} // namespace

extern "C" int OoT_QolOptions1747_RunHeadless(void) {
    if (GameInteractor::Instance == nullptr) {
        GameInteractor::Instance = new GameInteractor();
    }
    const GameId prevGame = Context_GetCurrentGame();
    PlayState* const prevPlay = OoT_gPlayState;
    GameInfo* const prevGameInfo = gGameInfo;
    int saved[kRowKeyCount];
    for (int i = 0; i < kRowKeyCount; i++) {
        saved[i] = CVarGetInteger(kRowKeys[i], 0);
    }

    // The GameInteractor dispatch is gated off only under GAME_MM.
    Context_SetCurrentGame(GAME_OOT);
    OoT_gPlayState = &sPlay;
    // OoT_Math_ScaledStepToS (leg f) scales its step by R_UPDATE_RATE.
    memset(&sGameInfo, 0, sizeof(sGameInfo));
    gGameInfo = &sGameInfo;
    R_UPDATE_RATE = 3;
    sFailures = 0;

    RunLegs();

    // Leave global state as it was, on pass or fail: the keys (cleared, never
    // stored, when they were unset), then the registrars to match them.
    for (int i = 0; i < kRowKeyCount; i++) {
        if (saved[i] != 0) {
            CVarSetInteger(kRowKeys[i], saved[i]);
        } else {
            CVarClear(kRowKeys[i]);
        }
    }
    for (int i = 0; i < kRowFileCount; i++) {
        RunRegistrar("*", kRowFiles[i]);
    }
    OoT_gPlayState = prevPlay;
    gGameInfo = prevGameInfo;
    Context_SetCurrentGame(prevGame);

    if (sFailures != 0) {
        printf("[TEST] FAIL (oot-qol-options-1747): %d failure(s)\n", sFailures);
        return 1;
    }
    printf("[TEST] oot-qol-options-1747: PASS\n");
    return 0;
}

#endif // RSBS_SINGLE_EXECUTABLE
