#include "2s2h/GameInteractor/GameInteractor.h"
#include "2s2h/ShipInit.hpp"

extern "C" {
#include "z64.h"
#include "functions.h"
#include "variables.h"
}

/**
 * The ocarina dive trick needs two ocarina presses on back-to-back frames: the first queues the
 * player-ocarina cutscene, the second consumes it before the promotion expires one frame later.
 * Console lag frames make that mash humanly possible; the port never lags, so instead we re-queue
 * the promoted cutscene for up to two extra frames, keeping it consumable at realistic mash
 * speeds. Re-queueing goes through the normal request path, so cutscene priority arbitration is
 * unaffected. bgCheckFlags and meleeWeaponState checks to identify hovering
 */
void RegisterOcarinaDive() {
    COND_HOOK(OnGameStateMainStart, true, []() {
        if (MM_gPlayState != NULL) {
            Player* player = GET_PLAYER(MM_gPlayState);
            static s32 ageFrames = 0;

            s16 ocarinaCsId = MM_gPlayState->playerCsIds[PLAYER_CS_ID_ITEM_OCARINA];

            // RSBS: `player != NULL` is single-exe hardening; this hook runs on every MM frame, and a play state
            // whose Player is not spawned yet would be dereferenced here otherwise.
            if ((ocarinaCsId > CS_ID_NONE) && (CutsceneManager_IsNext(ocarinaCsId) > 0) && (ageFrames < 2) &&
                (player != NULL) &&
                ((player->actor.bgCheckFlags & BGCHECKFLAG_GROUND_TOUCH) && (player->meleeWeaponState == 1))) {
                ageFrames++;
                CutsceneManager_Queue(ocarinaCsId);
            } else {
                ageFrames = 0;
            }
        }
    });
}

static RegisterShipInitFunc initFunc(RegisterOcarinaDive);
