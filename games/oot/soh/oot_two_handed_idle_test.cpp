/**
 * @file oot_two_handed_idle_test.cpp
 * ROM-free, display-free lock for SoH 8584ced40b (#1742): the Fix Two-Handed Idle Animations option. CTest label
 * "redship", row OoTTwoHandedIdleAnim in CMake/SingleExecutable.cmake, dispatch "oot-two-handed-idle-anim" in
 * src/common/test_runner.cpp. The fix is games/oot/soh/Enhancements/Fixes/FixTwoHandedIdleAnim.cpp plus the
 * VB_SET_IDLE_ANIM wrap in OoT_Player_ChooseNextIdleAnim (z_player.c).
 *
 * WHY. The sword-swing fidget needed the shield right hand, and a two-handed weapon has the closed hand, so the
 * two-handed swing the option was meant to pick could never be reached.
 *
 * WHAT IT DRIVES. The registrar, found in ShipInit's map by its registration site under "*" and under the option's
 * key, then the REAL GameInteractor_Should dispatch of VB_SET_IDLE_ANIM with a fake Link, for each common fidget type
 * (0 sword swing, 1 adjust tunic, 2 tap feet, 3 adjust shield).
 *
 * THE LEGS.
 *   (a) option off, Biggoron's Sword then the Deku Stick, closed hand: every type passes the vanilla result through.
 *   (b) option on, the same two weapons: the swing is accepted (the fix), the shield adjust is refused, tunic and
 *       feet are accepted.
 *   (c) option on, Kokiri Sword with the shield hand: swing and shield adjust are both accepted (unchanged).
 *   (d) option off again: the hook is unregistered (vanilla passes through).
 */

#ifdef RSBS_SINGLE_EXECUTABLE

// At file scope, not inside an extern "C" block: z64.h includes <memory> under C++.
#include <z64.h>

#include "context.h"
#include "soh/ShipInit.hpp"
#include "soh/cvar_prefixes.h"
#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/Enhancements/game-interactor/GameInteractor_Hooks.h"

#include <cstdio>
#include <cstring>
#include <string>

namespace {

#define THI_ASSERT(cond, msg)                                             \
    do {                                                                  \
        if (!(cond)) {                                                    \
            printf("[TEST] FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__); \
            return 1;                                                     \
        }                                                                 \
    } while (0)

const char* const kTwoHandedIdleKey = CVAR_ENHANCEMENT("TwoHandedIdle");

Player sTwoHandedIdleLink;

bool IsTwoHandedIdleSite(const ShipInitEntry& entry) {
    const std::string file = entry.registeredAt.file_name();
    const std::string want = "FixTwoHandedIdleAnim.cpp";
    return file.size() >= want.size() && file.compare(file.size() - want.size(), want.size(), want) == 0;
}

int CountTwoHandedIdleRegistrars(const char* key) {
    int found = 0;
    for (const auto& entry : ShipInit::GetAll()[key]) {
        if (IsTwoHandedIdleSite(entry)) {
            found++;
        }
    }
    return found;
}

void RunTwoHandedIdleRegistrar(const char* key) {
    for (const auto& entry : ShipInit::GetAll()[key]) {
        if (IsTwoHandedIdleSite(entry)) {
            entry.initFunc();
        }
    }
}

// The vanilla condition of OoT_Player_ChooseNextIdleAnim, for the fake Link.
bool TwoHandedIdleVanilla(const Player* p, s32 t) {
    const s32 swing = 0;
    const s32 shield = 3;
    const bool meleeHeld2 =
        p->heldItemAction > PLAYER_IA_FISHING_POLE && p->heldItemAction < PLAYER_IA_FISHING_POLE + 6;
    return (t != swing && t != shield) ||
           (p->rightHandType == PLAYER_MODELTYPE_RH_SHIELD && (t == shield || meleeHeld2));
}

void SetLink(s8 itemAction, u8 rightHandType) {
    memset(&sTwoHandedIdleLink, 0, sizeof(sTwoHandedIdleLink));
    sTwoHandedIdleLink.actor.id = ACTOR_PLAYER;
    sTwoHandedIdleLink.heldItemAction = itemAction;
    sTwoHandedIdleLink.rightHandType = rightHandType;
}

bool Accepts(s32 t) {
    return GameInteractor_Should(VB_SET_IDLE_ANIM, TwoHandedIdleVanilla(&sTwoHandedIdleLink, t), &sTwoHandedIdleLink,
                                 t);
}

int CheckPassThrough(const char* leg) {
    for (s32 t = 0; t < 4; t++) {
        const bool want = TwoHandedIdleVanilla(&sTwoHandedIdleLink, t);
        const bool got = Accepts(t);
        if (got != want) {
            printf("[TEST] FAIL: %s: fidget type %d gave %d, vanilla %d\n", leg, (int)t, got, want);
            return 1;
        }
    }
    return 0;
}

int RunTwoHandedIdleLegs(void) {
    THI_ASSERT(CountTwoHandedIdleRegistrars("*") == 1,
               "FixTwoHandedIdleAnim.cpp's boot registrar is not in ShipInit's map exactly once");
    THI_ASSERT(CountTwoHandedIdleRegistrars(kTwoHandedIdleKey) == 1,
               "FixTwoHandedIdleAnim.cpp's registrar does not re-run on the TwoHandedIdle CVar");

    const s8 twoHanded[] = { PLAYER_IA_SWORD_BIGGORON, PLAYER_IA_DEKU_STICK };
    const char* const names[] = { "Biggoron's Sword", "Deku Stick" };

    // (a) option off: vanilla passes through, so the swing stays refused for a two-handed weapon.
    CVarClear(kTwoHandedIdleKey);
    RunTwoHandedIdleRegistrar(kTwoHandedIdleKey);
    for (int w = 0; w < 2; w++) {
        SetLink(twoHanded[w], PLAYER_MODELTYPE_RH_CLOSED);
        THI_ASSERT(CheckPassThrough(names[w]) == 0, "(a): the option off did not pass the vanilla result through");
        THI_ASSERT(!Accepts(0), "(a): the swing fidget was accepted for a two-handed weapon with the option off");
    }

    // (b) option on.
    CVarSetInteger(kTwoHandedIdleKey, 1);
    RunTwoHandedIdleRegistrar(kTwoHandedIdleKey);
    for (int w = 0; w < 2; w++) {
        SetLink(twoHanded[w], PLAYER_MODELTYPE_RH_CLOSED);
        printf("[TEST] (b) %s: swing %d tunic %d feet %d shield %d\n", names[w], Accepts(0), Accepts(1), Accepts(2),
               Accepts(3));
        THI_ASSERT(Accepts(0), "(b): the swing fidget is still refused for a two-handed weapon with the option on");
        THI_ASSERT(Accepts(1) && Accepts(2), "(b): the tunic or feet fidget was refused");
        THI_ASSERT(!Accepts(3), "(b): the shield fidget was accepted without a shield in hand");
    }

    // (c) option on, one-handed sword and the shield hand: unchanged.
    SetLink(PLAYER_IA_SWORD_KOKIRI, PLAYER_MODELTYPE_RH_SHIELD);
    THI_ASSERT(Accepts(0) && Accepts(3), "(c): a one-handed sword lost the swing or shield fidget");

    // (d) option off again: the hook is gone.
    CVarClear(kTwoHandedIdleKey);
    RunTwoHandedIdleRegistrar(kTwoHandedIdleKey);
    SetLink(PLAYER_IA_SWORD_BIGGORON, PLAYER_MODELTYPE_RH_CLOSED);
    THI_ASSERT(!Accepts(0), "(d): the hook stayed registered after the option went off");
    return 0;
}

} // namespace

extern "C" int OoT_TwoHandedIdleAnim_RunHeadless(void) {
    if (GameInteractor::Instance == nullptr) {
        GameInteractor::Instance = new GameInteractor();
    }
    const GameId prevGame = Context_GetCurrentGame();
    // The GameInteractor dispatch is gated off only under GAME_MM.
    Context_SetCurrentGame(GAME_OOT);
    const int saved = CVarGetInteger(kTwoHandedIdleKey, 0);

    const int rc = RunTwoHandedIdleLegs();

    // Restore: the CVar (cleared, never stored, when it was unset), then the hook to match it.
    if (saved != 0) {
        CVarSetInteger(kTwoHandedIdleKey, saved);
    } else {
        CVarClear(kTwoHandedIdleKey);
    }
    RunTwoHandedIdleRegistrar(kTwoHandedIdleKey);
    Context_SetCurrentGame(prevGame);

    if (rc == 0) {
        printf("[TEST] oot-two-handed-idle-anim: PASS\n");
    }
    return rc;
}

#endif // RSBS_SINGLE_EXECUTABLE
