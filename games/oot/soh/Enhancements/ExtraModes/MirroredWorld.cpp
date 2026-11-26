#include "soh/Enhancements/cosmetics/authenticGfxPatches.h"
#include "soh/Enhancements/game-interactor/GameInteractor_Hooks.h"
#include "soh/ShipUtils.h"
#include "soh/Enhancements/randomizer/SeedContext.h"
#include "soh/Enhancements/enhancementTypes.h"
#include "soh/ResourceManagerHelpers.h"
#include "soh/ShipInit.hpp"
#ifdef RSBS_SINGLE_EXECUTABLE
#include "soh/Enhancements/cosmetics/OwedGfxPatches.h" // #1829: OoT_GfxPatches_RunOrOwe
#endif

extern "C" {
extern SaveContext gSaveContext;
extern PlayState* OoT_gPlayState;
}

static constexpr MirroredWorldMode CVAR_MIRRORED_WORLD_DEFAULT = MIRRORED_WORLD_OFF;
#define CVAR_MIRRORED_WORLD_NAME CVAR_ENHANCEMENT("MirroredWorld")
#define CVAR_MIRRORED_WORLD_MODE_NAME CVAR_ENHANCEMENT("MirroredWorldMode")
#define CVAR_MIRRORED_WORLD_MODE_VALUE CVarGetInteger(CVAR_MIRRORED_WORLD_MODE_NAME, CVAR_MIRRORED_WORLD_DEFAULT)

static bool prevMirroredWorld = false;

static bool MirroredWorld_IsInDungeon(int32_t sceneNum) {
    return (sceneNum >= SCENE_DEKU_TREE && sceneNum <= SCENE_INSIDE_GANONS_CASTLE_COLLAPSE &&
            sceneNum != SCENE_THIEVES_HIDEOUT) ||
           (sceneNum >= SCENE_DEKU_TREE_BOSS && sceneNum <= SCENE_GANONS_TOWER_COLLAPSE_EXTERIOR) ||
           (sceneNum == SCENE_GANON_BOSS);
}

static void MirroredWorld_InitRandomSeed(int32_t sceneNum, uint64_t* randState) {
    uint32_t seed =
        sceneNum + (IS_RANDO ? Rando::Context::GetInstance()->GetSeed() : gSaveContext.ship.stats.fileCreatedAt);
    ShipUtils::RandInit(seed, randState);
}

static bool MirroredWorld_ShouldApply(int32_t sceneNum) {
    uint64_t randState = 0;
    switch (CVAR_MIRRORED_WORLD_MODE_VALUE) {
        case MIRRORED_WORLD_ALWAYS:
            return true;
        case MIRRORED_WORLD_RANDOM_SEEDED:
            MirroredWorld_InitRandomSeed(sceneNum, &randState);
            return ShipUtils::Random(0, 2, &randState) == 0;
        case MIRRORED_WORLD_RANDOM:
            return ShipUtils::Random(0, 2) == 0;
        case MIRRORED_WORLD_DUNGEONS_ALL:
            return MirroredWorld_IsInDungeon(sceneNum);
        case MIRRORED_WORLD_DUNGEONS_VANILLA:
            return MirroredWorld_IsInDungeon(sceneNum) && !ResourceMgr_IsSceneMasterQuest(sceneNum);
        case MIRRORED_WORLD_DUNGEONS_MQ:
            return MirroredWorld_IsInDungeon(sceneNum) && ResourceMgr_IsSceneMasterQuest(sceneNum);
        case MIRRORED_WORLD_DUNGEONS_RANDOM_SEEDED:
            MirroredWorld_InitRandomSeed(sceneNum, &randState);
            return MirroredWorld_IsInDungeon(sceneNum) && ShipUtils::Random(0, 2, &randState) == 0;
        case MIRRORED_WORLD_DUNGEONS_RANDOM:
            return MirroredWorld_IsInDungeon(sceneNum) && ShipUtils::Random(0, 2) == 0;
        default:
            return false;
    }
}

static void UpdateMirrorModeState(int32_t sceneNum) {
    bool nextMirroredWorld = MirroredWorld_ShouldApply(sceneNum);

    if (prevMirroredWorld == nextMirroredWorld) {
        return;
    }
    prevMirroredWorld = nextMirroredWorld;

    if (nextMirroredWorld) {
        CVarSetInteger(CVAR_MIRRORED_WORLD_NAME, 1);
    } else {
        CVarClear(CVAR_MIRRORED_WORLD_NAME);
    }
    ShipInit::Init(CVAR_MIRRORED_WORLD_NAME);

    ApplyMirrorWorldGfxPatches();
}

// RSBS (#1829): the registrar's body. A pass owed while MM ran is paid on OoT's first frame, before any Play_Init (no
// play state). With the mode off no scene mirrors (MirroredWorld_ShouldApply's default), so the owed pass makes the
// transition itself: the hook is already off, and nothing else would clear the mirror CVar and unpatch under OoT's
// owner. The sync covers a mirror CVar saved by a shutdown with this pass still owed (prevMirroredWorld starts false at
// boot); with the mode off that CVar must be 0. Every other mode keeps upstream's first-scene transition (OnSceneInit).
static void MirroredWorld_Pass([[maybe_unused]] bool owed) {
    if (OoT_gPlayState != NULL) {
        UpdateMirrorModeState(OoT_gPlayState->sceneNum);
#ifdef RSBS_SINGLE_EXECUTABLE
    } else if (owed && CVAR_MIRRORED_WORLD_MODE_VALUE == MIRRORED_WORLD_OFF) {
        prevMirroredWorld = prevMirroredWorld || CVarGetInteger(CVAR_MIRRORED_WORLD_NAME, 0) != 0; // #1829 sync
        UpdateMirrorModeState(SCENE_ID_MAX);
#endif
    }
}

static void RegisterMirroredWorld() {
#ifdef RSBS_SINGLE_EXECUTABLE
    OoT_GfxPatches_RunOrOwe("MirroredWorld", MirroredWorld_Pass, false); // #1829: OoT's owner only; owed while MM runs
#else
    MirroredWorld_Pass(false);
#endif

    COND_HOOK(OnSceneInit, CVAR_MIRRORED_WORLD_MODE_VALUE, UpdateMirrorModeState);
}

#ifdef RSBS_SINGLE_EXECUTABLE
// #1829: the MMShipInitDriver row's seams. ShipInit::Init on the mode key would also run MM's own mirror registrar.
extern "C" void OoT_MirroredWorldTest_RunRegistrar(void) {
    RegisterMirroredWorld();
}

extern "C" void OoT_MirroredWorldTest_SetPrevMirrored(int prev) {
    prevMirroredWorld = prev != 0;
}

extern "C" int OoT_MirroredWorldTest_PrevMirrored(void) {
    return prevMirroredWorld ? 1 : 0;
}
#endif

static RegisterShipInitFunc initFunc(RegisterMirroredWorld, { CVAR_MIRRORED_WORLD_MODE_NAME });
