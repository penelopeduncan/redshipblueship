/**
 * @file oot_dark_link_fixes_test.cpp
 * ROM-free, display-free lock for the O16 backport (SoH 0cf2b3701, #7054, and
 * faed544b2, #7272). CTest label "redship", row OoTDarkLinkFixes in
 * CMake/SingleExecutable.cmake, dispatch "oot-dark-link-fixes" in
 * src/common/test_runner.cpp. The fix is games/oot/soh/Enhancements/Fixes/
 * DarkLinkFixes.cpp.
 *
 * WHY. Dark Link runs the player update function, so a Dark Link that voids
 * out, takes fall damage or stands on an ice floor does it to Link. The enemy
 * randomizer carried six VB_SHOULD guards against that, armed only while the
 * enemy randomizer was on, but Crowd Control and Sail can spawn Dark Link
 * too. Upstream moved the guards to DarkLinkFixes.cpp and arms them when any
 * of the three is on; CrowdControl / Sail re-run the registrar from Enable()
 * and Disable().
 *
 * WHAT IT DRIVES. DarkLinkFixes.cpp's own registrar, found in ShipInit's map
 * by its registration site under "*" and under each of the three CVar keys
 * (so a registrar the linker dropped, or a key left off the list, fails the
 * row), then the REAL GameInteractor_Should dispatch of each of the six
 * behaviours with a fake Dark Link (ACTOR_EN_TORCH2, an enemy) and a fake
 * Link (ACTOR_PLAYER, the player category).
 *
 * THE LEGS.
 *   (a) everything off: all six behaviours are vanilla for Dark Link (the
 *       default world plays as before).
 *   (b) Crowd Control on, enemy randomizer off: all six guards hold.
 *   (c) Sail on: the same.
 *   (d) enemy randomizer on: the same (the move lost nothing).
 *   (e) everything off again: the guards are unregistered.
 *   In every leg Link himself is never refused.
 *   (f) the rename: the enemy randomizer's menu label for the
 *       RandomizedEnemyList.GerudoFighter key (the key is unchanged) and the
 *       ActorDB description of ACTOR_EN_GELDB both read "Gerudo Thief".
 */

#ifdef RSBS_SINGLE_EXECUTABLE

// At file scope, not inside an extern "C" block: z64.h includes <memory> under
// C++ (see oot_shared_kokiri_sword_test.cpp).
#include <z64.h>

#include "context.h"
#include "soh/ActorDB.h"
#include "soh/ShipInit.hpp"
#include "soh/cvar_prefixes.h"
#include "soh/Enhancements/enemyrandomizer.h"
#include "soh/Enhancements/enhancementTypes.h"
#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/Enhancements/game-interactor/GameInteractor_Hooks.h"

#include <cstdio>
#include <cstring>
#include <string>

extern "C" {
#include "src/overlays/actors/ovl_En_Rr/z_en_rr.h"
}

namespace {

#define DLF_ASSERT(cond, msg)                                             \
    do {                                                                  \
        if (!(cond)) {                                                    \
            printf("[TEST] FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__); \
            return 1;                                                     \
        }                                                                 \
    } while (0)

const char* const kEnemyRandoKey = CVAR_ENHANCEMENT("RandomizedEnemies");
const char* const kCrowdControlKey = CVAR_REMOTE_CROWD_CONTROL("Enabled");
const char* const kSailKey = CVAR_REMOTE_SAIL("Enabled");
const char* const kRowKeys[] = { kEnemyRandoKey, kCrowdControlKey, kSailKey };
constexpr int kRowKeyCount = 3;

// Static so every leg starts from zeroed actors.
Player sDarkLink;
Player sLink;
EnRr sLikeLike;

void DummyUpdate(Actor*, PlayState*) {
}

bool IsDarkLinkFixesSite(const ShipInitEntry& entry) {
    const std::string file = entry.registeredAt.file_name();
    const std::string want = "DarkLinkFixes.cpp";
    return file.size() >= want.size() && file.compare(file.size() - want.size(), want.size(), want) == 0;
}

int CountRegistrars(const char* key) {
    int found = 0;
    for (const auto& entry : ShipInit::GetAll()[key]) {
        if (IsDarkLinkFixesSite(entry)) {
            found++;
        }
    }
    return found;
}

// Re-arms the guards the way a CVar change does: DarkLinkFixes.cpp's entry
// under @p key, and only that one (the other registrars under the same keys
// are not this row's business).
void RunRegistrar(const char* key) {
    for (const auto& entry : ShipInit::GetAll()[key]) {
        if (IsDarkLinkFixesSite(entry)) {
            entry.initFunc();
        }
    }
}

void SetOnly(const char* key) {
    for (int i = 0; i < kRowKeyCount; i++) {
        CVarClear(kRowKeys[i]);
    }
    if (key == kEnemyRandoKey) {
        CVarSetInteger(key, ENEMY_RANDOMIZER_RANDOM);
    } else if (key != nullptr) {
        CVarSetInteger(key, 1);
    }
}

void ResetActors(void) {
    memset(&sDarkLink, 0, sizeof(sDarkLink));
    sDarkLink.actor.id = ACTOR_EN_TORCH2;
    sDarkLink.actor.category = ACTORCAT_ENEMY;
    sDarkLink.actor.update = DummyUpdate;
    sDarkLink.actor.draw = nullptr;
    sDarkLink.actor.xzDistToPlayer = 500.0f;
    memset(&sLink, 0, sizeof(sLink));
    sLink.actor.id = ACTOR_PLAYER;
    sLink.actor.category = ACTORCAT_PLAYER;
    sLink.actor.update = DummyUpdate;
    sLink.actor.xzDistToPlayer = 500.0f;
    memset(&sLikeLike, 0, sizeof(sLikeLike));
}

struct Outcome {
    bool voidout;
    bool killed;
    bool fallDamage;
    bool clank;
    bool prevFloor;
    bool floor;
    bool likeLikeGrab;
};

// Dark Link's six behaviours, through the real dispatch, each with vanilla
// result true.
Outcome DarkLinkOutcome(void) {
    ResetActors();
    Outcome out = {};
    out.voidout = GameInteractor_Should(VB_TRIGGER_VOIDOUT, true, &sDarkLink.actor);
    out.killed = sDarkLink.actor.update == nullptr;
    out.fallDamage = GameInteractor_Should(VB_RECIEVE_FALL_DAMAGE, true, &sDarkLink.actor);
    out.clank = GameInteractor_Should(VB_TORCH2_HANDLE_CLANKING, true, &sDarkLink.actor);
    out.prevFloor = GameInteractor_Should(VB_SET_STATIC_PREV_FLOOR_TYPE, true, &sDarkLink);
    out.floor = GameInteractor_Should(VB_SET_STATIC_FLOOR_TYPE, true, &sDarkLink);
    sLikeLike.collider1.base.oc = &sDarkLink.actor;
    out.likeLikeGrab = GameInteractor_Should(VB_LIKE_LIKE_GRAB_PLAYER, true, &sLikeLike);
    return out;
}

void PrintOutcome(const char* leg, const Outcome& o) {
    printf("[TEST] %s: Dark Link voidout %d killed %d fall damage %d clank %d prev floor %d floor %d like-like grab "
           "%d\n",
           leg, o.voidout, o.killed, o.fallDamage, o.clank, o.prevFloor, o.floor, o.likeLikeGrab);
}

int CheckVanilla(const char* leg) {
    Outcome o = DarkLinkOutcome();
    PrintOutcome(leg, o);
    if (!o.voidout || o.killed || !o.fallDamage || !o.clank || !o.prevFloor || !o.floor || !o.likeLikeGrab) {
        printf("[TEST] FAIL: %s: a Dark Link guard is armed with every trigger off\n", leg);
        return 1;
    }
    return 0;
}

int CheckGuarded(const char* leg) {
    Outcome o = DarkLinkOutcome();
    PrintOutcome(leg, o);
    if (o.voidout || !o.killed || o.fallDamage || o.clank || o.prevFloor || o.floor || o.likeLikeGrab) {
        printf("[TEST] FAIL: %s: a Dark Link guard does not hold\n", leg);
        return 1;
    }
    // Inside 100 units Dark Link still clanks; that is vanilla upstream too.
    ResetActors();
    sDarkLink.actor.xzDistToPlayer = 50.0f;
    if (!GameInteractor_Should(VB_TORCH2_HANDLE_CLANKING, true, &sDarkLink.actor)) {
        printf("[TEST] FAIL: %s: the clank guard refused a Dark Link within 100 units\n", leg);
        return 1;
    }
    return 0;
}

// Link himself is never refused, whatever is armed.
int CheckLinkUntouched(const char* leg) {
    ResetActors();
    bool voidout = GameInteractor_Should(VB_TRIGGER_VOIDOUT, true, &sLink.actor);
    bool alive = sLink.actor.update != nullptr;
    bool fallDamage = GameInteractor_Should(VB_RECIEVE_FALL_DAMAGE, true, &sLink.actor);
    bool prevFloor = GameInteractor_Should(VB_SET_STATIC_PREV_FLOOR_TYPE, true, &sLink);
    bool floor = GameInteractor_Should(VB_SET_STATIC_FLOOR_TYPE, true, &sLink);
    sLikeLike.collider2.base.oc = &sLink.actor;
    bool grab = GameInteractor_Should(VB_LIKE_LIKE_GRAB_PLAYER, true, &sLikeLike);
    if (!voidout || !alive || !fallDamage || !prevFloor || !floor || !grab) {
        printf("[TEST] FAIL: %s: Link: voidout %d alive %d fall damage %d prev floor %d floor %d like-like grab %d\n",
               leg, voidout, alive, fallDamage, prevFloor, floor, grab);
        return 1;
    }
    return 0;
}

int RunLegs(void) {
    DLF_ASSERT(CountRegistrars("*") == 1, "DarkLinkFixes.cpp's boot registrar is not in ShipInit's map exactly once");
    DLF_ASSERT(CountRegistrars(kEnemyRandoKey) == 1,
               "DarkLinkFixes.cpp's registrar does not re-run on the enemy randomizer CVar");
    DLF_ASSERT(CountRegistrars(kCrowdControlKey) == 1,
               "DarkLinkFixes.cpp's registrar does not re-run on the Crowd Control CVar");
    DLF_ASSERT(CountRegistrars(kSailKey) == 1, "DarkLinkFixes.cpp's registrar does not re-run on the Sail CVar");

    // (a) everything off.
    SetOnly(nullptr);
    RunRegistrar("*");
    DLF_ASSERT(CheckVanilla("(a) all off") == 0, "(a): Dark Link is not vanilla with every trigger off");
    DLF_ASSERT(CheckLinkUntouched("(a) all off") == 0, "(a): Link was refused");

    // (b) Crowd Control on, enemy randomizer off.
    SetOnly(kCrowdControlKey);
    RunRegistrar(kCrowdControlKey);
    DLF_ASSERT(CheckGuarded("(b) Crowd Control") == 0, "(b): the guards do not hold under Crowd Control");
    DLF_ASSERT(CheckLinkUntouched("(b) Crowd Control") == 0, "(b): Link was refused");

    // (c) Sail on.
    SetOnly(kSailKey);
    RunRegistrar(kSailKey);
    DLF_ASSERT(CheckGuarded("(c) Sail") == 0, "(c): the guards do not hold under Sail");
    DLF_ASSERT(CheckLinkUntouched("(c) Sail") == 0, "(c): Link was refused");

    // (d) enemy randomizer on.
    SetOnly(kEnemyRandoKey);
    RunRegistrar(kEnemyRandoKey);
    DLF_ASSERT(CheckGuarded("(d) enemy randomizer") == 0, "(d): the guards do not hold under the enemy randomizer");
    DLF_ASSERT(CheckLinkUntouched("(d) enemy randomizer") == 0, "(d): Link was refused");

    // (e) everything off again: the change re-run unregisters the guards.
    SetOnly(nullptr);
    RunRegistrar(kEnemyRandoKey);
    DLF_ASSERT(CheckVanilla("(e) all off again") == 0, "(e): a guard stayed armed after every trigger went off");

    // (f) the rename.
    int geldbRow = -1;
    for (int i = 0; i < RANDOMIZED_ENEMY_SPAWN_TABLE_SIZE; i++) {
        if (strcmp(enemyCVarList[i], CVAR_ENHANCEMENT("RandomizedEnemyList.GerudoFighter")) == 0) {
            geldbRow = i;
        }
    }
    DLF_ASSERT(geldbRow >= 0, "(f): the RandomizedEnemyList.GerudoFighter key is gone");
    printf("[TEST] (f) enemy list label: \"%s\", ActorDB: \"%s\"\n", enemyNameList[geldbRow],
           ActorDB::Instance->RetrieveEntry(ACTOR_EN_GELDB).desc.c_str());
    DLF_ASSERT(strcmp(enemyNameList[geldbRow], "Gerudo Thief") == 0, "(f): the enemy list label is not Gerudo Thief");
    DLF_ASSERT(ActorDB::Instance->RetrieveEntry(ACTOR_EN_GELDB).desc == "Gerudo Thief",
               "(f): the ActorDB description of ACTOR_EN_GELDB is not Gerudo Thief");
    return 0;
}

} // namespace

extern "C" int OoT_DarkLinkFixes_RunHeadless(void) {
    if (GameInteractor::Instance == nullptr) {
        GameInteractor::Instance = new GameInteractor();
    }
    bool ownsActorDB = false;
    if (ActorDB::Instance == nullptr) {
        // The headless tier never runs InitOTR; the real table is pure data.
        ActorDB::Instance = new ActorDB();
        ownsActorDB = true;
    }
    const GameId prevGame = Context_GetCurrentGame();
    // The GameInteractor dispatch is gated off only under GAME_MM.
    Context_SetCurrentGame(GAME_OOT);
    int saved[kRowKeyCount];
    for (int i = 0; i < kRowKeyCount; i++) {
        saved[i] = CVarGetInteger(kRowKeys[i], 0);
    }

    const int rc = RunLegs();

    // Leave global state as it was, on pass or fail: the CVars (cleared, never
    // stored, when they were unset), then the guards to match them.
    for (int i = 0; i < kRowKeyCount; i++) {
        if (saved[i] != 0) {
            CVarSetInteger(kRowKeys[i], saved[i]);
        } else {
            CVarClear(kRowKeys[i]);
        }
    }
    RunRegistrar("*");
    if (ownsActorDB) {
        delete ActorDB::Instance;
        ActorDB::Instance = nullptr;
    }
    Context_SetCurrentGame(prevGame);

    if (rc == 0) {
        printf("[TEST] oot-dark-link-fixes: PASS\n");
    }
    return rc;
}

#endif // RSBS_SINGLE_EXECUTABLE
