/**
 * @file mm_cutscene_skips_test.cpp
 * ROM-free, display-free lock for #1755: Majora's Mask cutscene skips ported from 2Ship2Harkinian.
 * CTest label "redship", row MMCutsceneSkips in CMake/SingleExecutable.cmake,
 * dispatch "mm-cutscene-skips" in src/common/test_runner.cpp.
 *
 * LEGS.
 *   1. SkipOnePointCutscenes (VB_START_CUTSCENE, gEnhancements.TimeSavers.SkipCutscene.OnePoint):
 *      - setting off: bell (ACTOR_EN_CHA), owl statue (ACTOR_OBJ_WARPSTONE) and bean patch (ACTOR_OBJ_BEAN) all keep
 *        the vanilla verdict true, and the owl statue stays unactivated;
 *      - setting on: bell skips (*should = false), owl statue skips and calls Sram_ActivateOwl right away
 *        (GET_OWL_STATUE_ACTIVATED becomes non-zero), bean subtype ENOBJBEAN_GET_C000_0 skips with csId = -1, and
 *        excluded bean subtypes ENOBJBEAN_GET_C000_1 / ENOBJBEAN_GET_C000_2 keep the vanilla verdict and csId;
 *      - missing MM_gPlayState leaves vanilla untouched; setting back off disarms cleanly.
 *   2. SkipSakonsHideoutCutscenes Ikana Canyon arm (VB_START_CUTSCENE, gEnhancements.TimeSavers.SkipMiscInteractions):
 *      - setting off: SCENE_IKANA csIds 14 and 34 keep true;
 *      - setting on: SCENE_IKANA csIds 14 and 34 skip, csId 15 and SCENE_TOWN csId 14 stay true;
 *      - missing MM_gPlayState leaves vanilla untouched.
 *   3. SkipSakonsHideoutCutscenes Sakon's Hideout arm (VB_START_CUTSCENE, gEnhancements.TimeSavers.SkipMiscInteractions):
 *      - setting off: SCENE_SECOM csId 21 keeps true;
 *      - setting on: SCENE_SECOM csId 21 skips, csId 22 stays true;
 *      - setting back off disarms cleanly.
 *   4. SkipCouplesMaskCutscene (VB_PLAY_COUPLES_MASK_CS, gEnhancements.TimeSavers.SkipMiscInteractions):
 *      - setting off: incoming true returns true and leaves phase at 0;
 *      - setting on over a zeroed PlayState (SubS_FindActor returns NULL cleanly): incoming true returns false and sets
 *        phase = 2; incoming false leaves phase at 0;
 *      - missing MM_gPlayState leaves true and phase at 0; setting back off disarms cleanly.
 *
 * WHAT CAN ONLY BE SEEN IN THE RUNNING GAME.
 *   - Actor call sites invoking CutsceneManager_Start (En_Cha, Obj_Warpstone, Obj_Bean, Bg_Iknv_Obj, En_Test3
 *     func_80A3FE20, Sakon's Hideout conveyor) and En_Test3 func_80A3FFD0 calling VB_PLAY_COUPLES_MASK_CS.
 *   - SkipCouplesMaskCutscene's post-SubS_FindActor body when Anju (ACTOR_EN_AN) is spawned: positioning Anju/Kafei,
 *     MM_Player_StartTalking(0x2955), LoadVanillaMessageTableEntry(0x85) and draining GIEventGiveItem (or rando's
 *     VB_GIVE_ITEM_FROM_DMCHAR05 hand-off).
 *
 * Names no symbol defined by the three enhancement TUs so it never pulls them into the link on its own.
 */

#include "global.h"

#include <cstdio>

#if !defined(RSBS_SINGLE_EXECUTABLE)
extern "C" int MM_CutsceneSkips_RunHeadless(void) {
    printf("[TEST] mm-cutscene-skips: PASS (not applicable outside RSBS_SINGLE_EXECUTABLE)\n");
    return 0;
}
#else

#include <cstring>
#include <memory>

#include <libultraship/bridge/consolevariablebridge.h>
#include <ship/Context.h>

#include "2s2h/GameInteractor/GameInteractor.h"
#include "2s2h/ShipInit.hpp"
#include "mm_game_hooks.h"

extern "C" {
#include "variables.h"
#include "functions.h"
#include "overlays/actors/ovl_Obj_Bean/z_obj_bean.h"
#include "src/overlays/actors/ovl_En_Test3/z_en_test3.h"
}

namespace {

#define CS_ASSERT(cond, code, msg)                                                      \
    do {                                                                                \
        if (!(cond)) {                                                                  \
            printf("[TEST] FAIL(%d): %s (%s:%d)\n", (code), (msg), __FILE__, __LINE__); \
            return (code);                                                              \
        }                                                                               \
    } while (0)

constexpr const char* kOnePointCVar = "gEnhancements.TimeSavers.SkipCutscene.OnePoint";
constexpr const char* kMiscCVar = "gEnhancements.TimeSavers.SkipMiscInteractions";

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
    S2H::GameHooks::FlushPendingUnregistrations<GameInteractor::ShouldVanillaBehavior>();
}

bool StartCutsceneForActor(PlayState* play, s16 sceneId, Actor* actor, s16 csId) {
    if (play != nullptr) {
        play->sceneId = sceneId;
    }
    s16 cs = csId;
    return GameInteractor_Should(VB_START_CUTSCENE, true, &cs, actor);
}

Actor MakeActor(s16 id, s16 params, s8 csId) {
    Actor actor{};
    actor.id = id;
    actor.params = params;
    actor.csId = csId;
    return actor;
}

int RunOnePointLeg(PlayState* play) {
    DriveKey(kMiscCVar, 0);
    DriveKey(kOnePointCVar, 0);
    MM_gPlayState = play;
    memset(&gSaveContext, 0, sizeof(gSaveContext));
    gSaveContext.save.shipSaveInfo.saveType = SAVETYPE_VANILLA;
    gSaveContext.save.saveInfo.playerData.owlWarpId = OWL_WARP_NONE;

    Actor bellOff = MakeActor(ACTOR_EN_CHA, 0, 10);
    Actor owlOff = MakeActor(ACTOR_OBJ_WARPSTONE, OWL_WARP_CLOCK_TOWN, 10);
    Actor bean0Off = MakeActor(ACTOR_OBJ_BEAN, 0, 10);
    Actor bean1Off = MakeActor(ACTOR_OBJ_BEAN, static_cast<s16>(ENOBJBEAN_GET_C000_1 << 0xE), 10);

    CS_ASSERT(StartCutsceneForActor(play, SCENE_ALLEY, &bellOff, 10), 10,
              "bell cutscene already skipped with OnePoint off");
    CS_ASSERT(StartCutsceneForActor(play, SCENE_TOWN, &owlOff, 10), 10,
              "owl statue cutscene already skipped with OnePoint off");
    CS_ASSERT(GET_OWL_STATUE_ACTIVATED(OWL_WARP_CLOCK_TOWN) == 0, 10,
              "owl statue flag set with OnePoint off");
    CS_ASSERT(StartCutsceneForActor(play, SCENE_22DEKUCITY, &bean0Off, 10) && bean0Off.csId == 10, 10,
              "bean plant cutscene already skipped with OnePoint off");
    CS_ASSERT(StartCutsceneForActor(play, SCENE_22DEKUCITY, &bean1Off, 10) && bean1Off.csId == 10, 10,
              "soil patch cutscene already skipped with OnePoint off");

    DriveKey(kOnePointCVar, 1);

    Actor bellOn = MakeActor(ACTOR_EN_CHA, 0, 10);
    Actor owlOn = MakeActor(ACTOR_OBJ_WARPSTONE, OWL_WARP_CLOCK_TOWN, 10);
    Actor bean0On = MakeActor(ACTOR_OBJ_BEAN, 0, 10);
    Actor bean1On = MakeActor(ACTOR_OBJ_BEAN, static_cast<s16>(ENOBJBEAN_GET_C000_1 << 0xE), 10);
    Actor bean2On = MakeActor(ACTOR_OBJ_BEAN, static_cast<s16>(ENOBJBEAN_GET_C000_2 << 0xE), 10);

    CS_ASSERT(!StartCutsceneForActor(play, SCENE_ALLEY, &bellOn, 10), 11,
              "Laundry Pool bell (ACTOR_EN_CHA) cutscene was not skipped with OnePoint on");
    CS_ASSERT(!StartCutsceneForActor(play, SCENE_TOWN, &owlOn, 10), 12,
              "owl statue (ACTOR_OBJ_WARPSTONE) cutscene was not skipped with OnePoint on");
    CS_ASSERT(GET_OWL_STATUE_ACTIVATED(OWL_WARP_CLOCK_TOWN) != 0, 13,
              "owl statue flag was not activated immediately on skip");
    CS_ASSERT(!StartCutsceneForActor(play, SCENE_22DEKUCITY, &bean0On, 10) && bean0On.csId == -1, 14,
              "bean plant (ENOBJBEAN_GET_C000_0) cutscene was not skipped or csId not cleared");
    CS_ASSERT(StartCutsceneForActor(play, SCENE_22DEKUCITY, &bean1On, 10) && bean1On.csId == 10, 15,
              "excluded bean subtype ENOBJBEAN_GET_C000_1 was still skipped");
    CS_ASSERT(StartCutsceneForActor(play, SCENE_22DEKUCITY, &bean2On, 10) && bean2On.csId == 10, 15,
              "excluded bean subtype ENOBJBEAN_GET_C000_2 was still skipped");

    MM_gPlayState = nullptr;
    Actor bellNoPlay = MakeActor(ACTOR_EN_CHA, 0, 10);
    CS_ASSERT(StartCutsceneForActor(nullptr, 0, &bellNoPlay, 10), 16,
              "SkipOnePointCutscenes skipped without a play state");

    MM_gPlayState = play;
    DriveKey(kOnePointCVar, 0);
    Actor bellAfter = MakeActor(ACTOR_EN_CHA, 0, 10);
    CS_ASSERT(StartCutsceneForActor(play, SCENE_ALLEY, &bellAfter, 10), 17,
              "SkipOnePointCutscenes remained armed after OnePoint returned to 0");
    return 0;
}

int RunSakonsHideoutLegs(PlayState* play) {
    DriveKey(kOnePointCVar, 0);
    DriveKey(kMiscCVar, 0);
    MM_gPlayState = play;

    // Leg 2: Ikana Canyon arm (csIds 14 and 34)
    CS_ASSERT(StartCutsceneForActor(play, SCENE_IKANA, nullptr, 14), 20,
              "SCENE_IKANA csId 14 skipped with SkipMiscInteractions off");
    CS_ASSERT(StartCutsceneForActor(play, SCENE_IKANA, nullptr, 34), 20,
              "SCENE_IKANA csId 34 skipped with SkipMiscInteractions off");

    // Leg 3 baseline: Sakon's Hideout conveyor arm (csId 21)
    CS_ASSERT(StartCutsceneForActor(play, SCENE_SECOM, nullptr, 21), 30,
              "SCENE_SECOM csId 21 skipped with SkipMiscInteractions off");

    DriveKey(kMiscCVar, 1);

    // Leg 2: Ikana Canyon arm on
    CS_ASSERT(!StartCutsceneForActor(play, SCENE_IKANA, nullptr, 14), 21,
              "SCENE_IKANA csId 14 not skipped with SkipMiscInteractions on");
    CS_ASSERT(!StartCutsceneForActor(play, SCENE_IKANA, nullptr, 34), 22,
              "SCENE_IKANA csId 34 not skipped with SkipMiscInteractions on");
    CS_ASSERT(StartCutsceneForActor(play, SCENE_IKANA, nullptr, 15), 23,
              "unrelated SCENE_IKANA csId 15 skipped");
    CS_ASSERT(StartCutsceneForActor(play, SCENE_TOWN, nullptr, 14), 23,
              "SCENE_TOWN csId 14 skipped by Sakon's Hideout hook");

    MM_gPlayState = nullptr;
    CS_ASSERT(StartCutsceneForActor(nullptr, 0, nullptr, 14), 24,
              "SkipSakonsHideoutCutscenes skipped without a play state");

    // Leg 3: Sakon's Hideout conveyor arm on
    MM_gPlayState = play;
    CS_ASSERT(!StartCutsceneForActor(play, SCENE_SECOM, nullptr, 21), 31,
              "SCENE_SECOM csId 21 not skipped with SkipMiscInteractions on");
    CS_ASSERT(StartCutsceneForActor(play, SCENE_SECOM, nullptr, 22), 32,
              "unrelated SCENE_SECOM csId 22 skipped");

    DriveKey(kMiscCVar, 0);
    CS_ASSERT(StartCutsceneForActor(play, SCENE_IKANA, nullptr, 14), 25,
              "SkipSakonsHideoutCutscenes remained armed for SCENE_IKANA after key off");
    CS_ASSERT(StartCutsceneForActor(play, SCENE_SECOM, nullptr, 21), 33,
              "SkipSakonsHideoutCutscenes remained armed for SCENE_SECOM after key off");
    return 0;
}

int RunCouplesMaskLeg(PlayState* play) {
    auto kafei = std::make_unique<EnTest3>();
    s32 phase = 0;

    DriveKey(kOnePointCVar, 0);
    DriveKey(kMiscCVar, 0);
    MM_gPlayState = play;

    CS_ASSERT(GameInteractor_Should(VB_PLAY_COUPLES_MASK_CS, true, kafei.get(), &phase) && phase == 0, 40,
              "VB_PLAY_COUPLES_MASK_CS skipped with SkipMiscInteractions off");

    DriveKey(kMiscCVar, 1);

    phase = 0;
    CS_ASSERT(!GameInteractor_Should(VB_PLAY_COUPLES_MASK_CS, true, kafei.get(), &phase) && phase == 2, 41,
              "VB_PLAY_COUPLES_MASK_CS did not skip or advance phase to 2 with SkipMiscInteractions on");

    phase = 0;
    CS_ASSERT(!GameInteractor_Should(VB_PLAY_COUPLES_MASK_CS, false, kafei.get(), &phase) && phase == 0, 42,
              "VB_PLAY_COUPLES_MASK_CS mutated phase when vanilla condition was false");

    MM_gPlayState = nullptr;
    phase = 0;
    CS_ASSERT(GameInteractor_Should(VB_PLAY_COUPLES_MASK_CS, true, kafei.get(), &phase) && phase == 0, 43,
              "VB_PLAY_COUPLES_MASK_CS skipped without a play state");

    MM_gPlayState = play;
    DriveKey(kMiscCVar, 0);
    phase = 0;
    CS_ASSERT(GameInteractor_Should(VB_PLAY_COUPLES_MASK_CS, true, kafei.get(), &phase) && phase == 0, 44,
              "VB_PLAY_COUPLES_MASK_CS remained armed after SkipMiscInteractions returned to 0");
    return 0;
}

} // namespace

extern "C" int MM_CutsceneSkips_RunHeadless(void) {
    printf("[TEST] mm-cutscene-skips: OnePoint and MiscInteractions cutscene skips (#1755)\n");

    auto ctx = Ship::Context::GetInstance();
    CS_ASSERT(ctx != nullptr, 1, "Ship::Context singleton missing - run the shared bring-up first");
    CS_ASSERT(ctx->GetConsoleVariables() != nullptr, 1, "ConsoleVariables missing - the registrars read CVars");

    auto savedSave = std::make_unique<uint8_t[]>(sizeof(gSaveContext));
    memcpy(savedSave.get(), &gSaveContext, sizeof(gSaveContext));
    PlayState* const savedPlay = MM_gPlayState;
    const SavedKey savedOnePoint(kOnePointCVar);
    const SavedKey savedMisc(kMiscCVar);

    struct RestoreGuard {
        uint8_t* save;
        PlayState* play;
        SavedKey onePoint;
        SavedKey misc;
        ~RestoreGuard() {
            MM_gPlayState = play;
            memcpy(&gSaveContext, save, sizeof(gSaveContext));
            onePoint.Restore();
            misc.Restore();
            S2H::GameHooks::FlushPendingUnregistrations<GameInteractor::ShouldVanillaBehavior>();
        }
    } guard{ savedSave.get(), savedPlay, savedOnePoint, savedMisc };

    auto play = std::make_unique<PlayState>();

    int rc = RunOnePointLeg(play.get());
    if (rc == 0) {
        rc = RunSakonsHideoutLegs(play.get());
    }
    if (rc == 0) {
        rc = RunCouplesMaskLeg(play.get());
    }
    if (rc == 0) {
        printf("[TEST] mm-cutscene-skips: PASS (OnePoint bell/owl/bean-subtype, SakonsHideout, CouplesMask)\n");
    }
    return rc;
}

#endif // RSBS_SINGLE_EXECUTABLE
