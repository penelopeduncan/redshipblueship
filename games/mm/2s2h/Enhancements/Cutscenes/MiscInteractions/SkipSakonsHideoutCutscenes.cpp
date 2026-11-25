#include <libultraship/bridge/consolevariablebridge.h>
#include "2s2h/GameInteractor/GameInteractor.h"
#include "2s2h/ShipInit.hpp"

#define CVAR_NAME "gEnhancements.TimeSavers.SkipMiscInteractions"
#define CVAR CVarGetInteger(CVAR_NAME, 0)

static void RegisterSkipHideoutCutscenes() {
    COND_VB_SHOULD(VB_START_CUTSCENE, CVAR, {
        // #1030-#1034, per PR #673's criterion: this TU is in the single-exe link now and the body reads MM_gPlayState;
        // a missing play state leaves the vanilla behavior untouched.
        RSBS_SINGLE_EXE_REQUIRE(MM_gPlayState != NULL);
        s16* csId = va_arg(args, s16*);
        if (MM_gPlayState->sceneId == SCENE_IKANA) {
            if (*csId == 14 || *csId == 34) {
                *should = false;
            }
        } else if (MM_gPlayState->sceneId == SCENE_SECOM) {
            if (*csId == 21) {
                *should = false;
            }
        }
    });
}

static RegisterShipInitFunc initFunc(RegisterSkipHideoutCutscenes, { CVAR_NAME });
