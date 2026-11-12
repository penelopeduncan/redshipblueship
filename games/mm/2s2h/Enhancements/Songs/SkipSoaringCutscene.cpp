#include <libultraship/bridge/consolevariablebridge.h>
#include "2s2h/GameInteractor/GameInteractor.h"
#include "2s2h/ShipInit.hpp"
#include "2s2h/ShipUtils.h"
#ifdef RSBS_SINGLE_EXECUTABLE
#include "2s2h/Rando/WarpSongsMM.h" // #1169 XW7: an OoT warp song's departure
#endif

extern "C" {
#include "variables.h"
#include "overlays/actors/ovl_En_Test7/z_en_test7.h"
}

#define CVAR_NAME "gEnhancements.Songs.SkipSoaringCutscene"
#define CVAR CVarGetInteger(CVAR_NAME, 0)

static void SkipSoaringCutscene(Actor* actor, bool* should) {
    // W6-MASKS-SONGS (#1236): this TU is in the single-exe link now and the body reads MM_gPlayState; a missing
    // play state leaves the vanilla behavior untouched.
    RSBS_SINGLE_EXE_REQUIRE(MM_gPlayState != NULL);
    s16 ocarinaMode = OWL_WARP_CS_GET_OCARINA_MODE(actor);
    if (ocarinaMode == ENTEST7_ARRIVE) {
        return;
    }

#ifdef RSBS_SINGLE_EXECUTABLE
    // #1169 XW7: an OoT warp song (params 0x8000 | song) departs through the same
    // one-shot as En_Test7's final action, never through the owl table below,
    // which its params would index past.
    if (MM_WarpSongMM_IsEnTest7Params(ocarinaMode)) {
        *should = false;
        MM_WarpSongMM_Depart(MM_gPlayState, ocarinaMode & 0xFF);
        return;
    }
#endif

    *should = false;

    if (MM_gPlayState->sceneId == SCENE_SECOM) {
        MM_gPlayState->nextEntrance = ENTRANCE(IKANA_CANYON, 6);
    } else if (ocarinaMode == OCARINA_MODE_WARP_TO_ENTRANCE) {
        func_80169F78(MM_gPlayState);
        gSaveContext.respawn[RESPAWN_MODE_TOP].playerParams =
            PLAYER_PARAMS(gSaveContext.respawn[RESPAWN_MODE_TOP].playerParams, PLAYER_START_MODE_OWL);
        gSaveContext.respawnFlag = -6;
    } else {
        MM_gPlayState->nextEntrance = sOwlWarpEntrancesForMods[ocarinaMode - OCARINA_MODE_WARP_TO_GREAT_BAY_COAST];
    }

    MM_gPlayState->transitionTrigger = TRANS_TRIGGER_START;
    MM_gPlayState->transitionType = TRANS_TYPE_FADE_BLACK;
}

static void RegisterSkipSoaringCutscene() {
    COND_ID_HOOK(ShouldActorInit, ACTOR_EN_TEST7, CVAR, SkipSoaringCutscene);
}

static RegisterShipInitFunc initFunc(RegisterSkipSoaringCutscene, { CVAR_NAME });
