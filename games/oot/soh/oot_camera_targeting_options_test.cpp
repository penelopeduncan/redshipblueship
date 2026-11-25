/**
 * @file oot_camera_targeting_options_test.cpp
 * ROM-free, display-free lock for the Reworked Targeting backport (SoH 2af5d2112, #6322). CTest label "redship", row
 * OoTCameraTargetingOptions in CMake/SingleExecutable.cmake, dispatch "oot-camera-targeting-options" in
 * src/common/test_runner.cpp. The hook is games/oot/soh/Enhancements/ReworkedTargeting.cpp; the two vanilla
 * behaviours it answers are the lock-on press and the "is this a new target" test in Player_UpdateZTargeting
 * (z_player.c).
 *
 * WHAT IT DRIVES. ReworkedTargeting.cpp's own registrar, found in ShipInit's map by its registration site under "*"
 * and under its CVar key (so a registrar the linker dropped, or a key left off the list, fails the row), then the REAL
 * GameInteractor_Should dispatch of VB_TOGGLE_Z_TARGET_SWITCH_TARGETS and VB_TOGGLE_Z_TARGET_SWITCH_DIRECTION, with a
 * fake OoT_gPlayState that holds the player in actorLists[ACTORCAT_PLAYER].
 *
 * THE LEGS (the hook keeps a static "was the combo down last frame" flag and a static "this switch came from the
 * combo" flag, so the order matters).
 *   (a) setting off, the player has a focus actor: SWITCH_TARGETS given true stays true, DIRECTION given false stays
 *       false.
 *   (b) setting on, focus set, no combo: SWITCH_TARGETS is false (pressing Z on a locked target un-targets).
 *   (c) setting on, combo = R, R in press.button, targetCtx.unk_94 set: DIRECTION is true (a new target is chosen),
 *       then SWITCH_TARGETS stays true (the combo switch is allowed through).
 *   (c2) R still down the next frame, unk_94 still set: DIRECTION is false (the combo fires on its rising edge only).
 *   (d) the same with unk_94 NULL, after one frame with R released: DIRECTION is false (nothing to switch to); then
 *       one released frame, so the "was the combo down" static is false before the hooks are disarmed.
 *   (e) setting off again: both are vanilla, with R still down and unk_94 set.
 */

#ifdef RSBS_SINGLE_EXECUTABLE

// At file scope, not inside an extern "C" block: z64.h includes <memory> under C++ (see
// oot_shared_kokiri_sword_test.cpp).
#include <z64.h>

#include "context.h"
#include "soh/ShipInit.hpp"
#include "soh/cvar_prefixes.h"
#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/Enhancements/game-interactor/GameInteractor_Hooks.h"

#include <cstdio>
#include <cstring>
#include <string>

extern "C" {
extern PlayState* OoT_gPlayState;
}

namespace {

#define CTO_ASSERT(cond, msg)                                             \
    do {                                                                  \
        if (!(cond)) {                                                    \
            printf("[TEST] FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__); \
            return 1;                                                     \
        }                                                                 \
    } while (0)

const char* const kEnabledKey = CVAR_ENHANCEMENT("ReworkedTargeting.Enabled");
const char* const kBtnKey = CVAR_ENHANCEMENT("ReworkedTargeting.Btn");

// Static so every run starts from zeroed state.
PlayState sPlay;
Player sPlayer;
Actor sFocus;
Actor sNextTarget;

bool IsReworkedTargetingSite(const ShipInitEntry& entry) {
    const std::string file = entry.registeredAt.file_name();
    const std::string want = "ReworkedTargeting.cpp";
    return file.size() >= want.size() && file.compare(file.size() - want.size(), want.size(), want) == 0;
}

int CountRegistrars(const char* key) {
    int found = 0;
    for (const auto& entry : ShipInit::GetAll()[key]) {
        if (IsReworkedTargetingSite(entry)) {
            found++;
        }
    }
    return found;
}

// Re-arms the hooks the way a CVar change does: ReworkedTargeting.cpp's entry under @p key, and only that one.
void RunRegistrar(const char* key) {
    for (const auto& entry : ShipInit::GetAll()[key]) {
        if (IsReworkedTargetingSite(entry)) {
            entry.initFunc();
        }
    }
}

void ResetWorld(void) {
    memset(&sPlay, 0, sizeof(sPlay));
    memset(&sPlayer, 0, sizeof(sPlayer));
    memset(&sFocus, 0, sizeof(sFocus));
    memset(&sNextTarget, 0, sizeof(sNextTarget));
    sPlayer.actor.id = ACTOR_PLAYER;
    sPlayer.actor.category = ACTORCAT_PLAYER;
    sPlay.actorCtx.actorLists[ACTORCAT_PLAYER].head = &sPlayer.actor;
    sPlay.actorCtx.actorLists[ACTORCAT_PLAYER].length = 1;
    sPlayer.focusActor = &sFocus;
}

// The two lines Player_UpdateZTargeting wraps, through the real dispatch.
bool ShouldSwitchTargets(bool vanilla) {
    return GameInteractor_Should(VB_TOGGLE_Z_TARGET_SWITCH_TARGETS, vanilla);
}

bool ShouldSwitchDirection(bool vanilla) {
    return GameInteractor_Should(VB_TOGGLE_Z_TARGET_SWITCH_DIRECTION, vanilla);
}

void SetPress(u16 buttons) {
    sPlay.state.input[0].press.button = buttons;
}

int RunLegs(void) {
    CTO_ASSERT(CountRegistrars("*") == 1,
               "ReworkedTargeting.cpp's boot registrar is not in ShipInit's map exactly once");
    CTO_ASSERT(CountRegistrars(kEnabledKey) == 1,
               "ReworkedTargeting.cpp's registrar does not re-run on the Reworked Targeting CVar");

    ResetWorld();
    OoT_gPlayState = &sPlay;

    // (a) setting off, focus set: both vanilla.
    CVarClear(kEnabledKey);
    CVarClear(kBtnKey);
    RunRegistrar("*");
    SetPress(0);
    bool switchTargets = ShouldSwitchTargets(true);
    bool direction = ShouldSwitchDirection(false);
    printf("[TEST] (a) off: switch targets %d direction %d\n", switchTargets, direction);
    CTO_ASSERT(switchTargets, "(a): SWITCH_TARGETS was refused with the setting off");
    CTO_ASSERT(!direction, "(a): DIRECTION was forced on with the setting off");

    // (b) on, focus set, no combo: pressing Z on a locked target un-targets.
    CVarSetInteger(kEnabledKey, 1);
    RunRegistrar(kEnabledKey);
    switchTargets = ShouldSwitchTargets(true);
    direction = ShouldSwitchDirection(false);
    printf("[TEST] (b) on, no combo: switch targets %d direction %d\n", switchTargets, direction);
    CTO_ASSERT(!switchTargets, "(b): SWITCH_TARGETS was not refused with a focus actor and no combo");
    CTO_ASSERT(!direction, "(b): DIRECTION was forced on with no combo configured");

    // (c) on, combo R pressed, a next target exists: the switch goes through.
    CVarSetInteger(kBtnKey, BTN_R);
    SetPress(BTN_R);
    sPlay.actorCtx.targetCtx.unk_94 = &sNextTarget;
    direction = ShouldSwitchDirection(false);
    switchTargets = ShouldSwitchTargets(true);
    printf("[TEST] (c) on, combo R, next target: direction %d switch targets %d\n", direction, switchTargets);
    CTO_ASSERT(direction, "(c): DIRECTION was not forced on by the combo with a next target");
    CTO_ASSERT(switchTargets, "(c): SWITCH_TARGETS was refused for a switch the combo triggered");

    // (c2) the combo still down the next frame, a next target still there: the switch fires on the rising edge only.
    direction = ShouldSwitchDirection(false);
    printf("[TEST] (c2) on, combo R still down: direction %d\n", direction);
    CTO_ASSERT(!direction, "(c2): DIRECTION was forced on again with the combo held from the previous frame");

    // (d) one frame with R released, then R again with no next target: nothing to switch to.
    SetPress(0);
    direction = ShouldSwitchDirection(false);
    CTO_ASSERT(!direction, "(d): DIRECTION was forced on in a frame with the combo released");
    SetPress(BTN_R);
    sPlay.actorCtx.targetCtx.unk_94 = nullptr;
    direction = ShouldSwitchDirection(false);
    printf("[TEST] (d) on, combo R, no next target: direction %d\n", direction);
    CTO_ASSERT(!direction, "(d): DIRECTION was forced on by the combo with no next target");
    // One released frame while the setting is still on, so the hook's "combo down last frame" static is false again
    // before the hooks are disarmed.
    SetPress(0);
    CTO_ASSERT(!ShouldSwitchDirection(false), "(d): DIRECTION was forced on with the combo released");

    // (e) off again: both vanilla, even with R down and a next target.
    CVarClear(kEnabledKey);
    RunRegistrar(kEnabledKey);
    SetPress(BTN_R);
    sPlay.actorCtx.targetCtx.unk_94 = &sNextTarget;
    switchTargets = ShouldSwitchTargets(true);
    direction = ShouldSwitchDirection(false);
    printf("[TEST] (e) off again: switch targets %d direction %d\n", switchTargets, direction);
    CTO_ASSERT(switchTargets, "(e): SWITCH_TARGETS stayed refused after the setting went off");
    CTO_ASSERT(!direction, "(e): DIRECTION stayed forced after the setting went off");
    return 0;
}

} // namespace

extern "C" int OoT_CameraTargetingOptions_RunHeadless(void) {
    if (GameInteractor::Instance == nullptr) {
        GameInteractor::Instance = new GameInteractor();
    }
    const GameId prevGame = Context_GetCurrentGame();
    // The GameInteractor dispatch is gated off only under GAME_MM.
    Context_SetCurrentGame(GAME_OOT);
    PlayState* const prevPlay = OoT_gPlayState;
    const int savedEnabled = CVarGetInteger(kEnabledKey, 0);
    const int savedBtn = CVarGetInteger(kBtnKey, 0);

    const int rc = RunLegs();

    // Leave global state as it was, on pass or fail: the CVars (cleared, never stored, when they were unset), then
    // the hooks to match them, then the play state and the current game.
    if (savedEnabled != 0) {
        CVarSetInteger(kEnabledKey, savedEnabled);
    } else {
        CVarClear(kEnabledKey);
    }
    if (savedBtn != 0) {
        CVarSetInteger(kBtnKey, savedBtn);
    } else {
        CVarClear(kBtnKey);
    }
    RunRegistrar("*");
    OoT_gPlayState = prevPlay;
    Context_SetCurrentGame(prevGame);

    if (rc == 0) {
        printf("[TEST] oot-camera-targeting-options: PASS\n");
    }
    return rc;
}

#endif // RSBS_SINGLE_EXECUTABLE
