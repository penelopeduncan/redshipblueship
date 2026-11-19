#include "soh/Enhancements/game-interactor/GameInteractor_Hooks.h"
#include "soh/ShipInit.hpp"
#include "src/overlays/actors/ovl_Fishing/z_fishing.h"

extern "C" {
#include "macros.h"
#include "variables.h"
#include "functions.h"

extern SaveContext gSaveContext;
extern PlayState* OoT_gPlayState;
f32 Fishing_GetMinimumRequiredScore();
}

// Ship of Harkinian 172b373fb: outside the randomizer, the Customize Fishing minimum weights also set the weight that
// wins the Piece of Heart and the Golden Scale (the randomizer gives its own prize through VB_GIVE_RANDO_FISHING_PRIZE).
void RegisterFishingPrizeThreshold() {
    COND_VB_SHOULD(
        VB_SHOULD_GIVE_VANILLA_FISHING_PRIZE, (!IS_RANDO && CVarGetInteger(CVAR_ENHANCEMENT("CustomizeFishing"), 0)), {
            VBFishingData* fishData = va_arg(args, VBFishingData*);
            *should = fishData->fishWeight >= Fishing_GetMinimumRequiredScore() &&
                      !(HIGH_SCORE(HS_FISHING) & (LINK_IS_CHILD ? HS_FISH_PRIZE_CHILD : HS_FISH_PRIZE_ADULT));
        });
}

// Ship of Harkinian a089fa60b: Hover Fishing restores NTSC 1.0's cast, which ran only the rod-in-wall check and kept
// Link's momentum, instead of skipping every check.
void RegisterHoverFishing() {
    COND_VB_SHOULD(VB_NOT_CAST_FISHING, (CVarGetInteger(CVAR_ENHANCEMENT("HoverFishing"), false)), {
        Vec3f* rodCheckPos = va_arg(args, Vec3f*);
        *should = false;
        // Run only original NTSC 1.0 check before cast
        if (OoT_BgCheck_SphVsFirstPoly(&OoT_gPlayState->colCtx, rodCheckPos, 20.0f)) {
            *should = true;
        }
    });

    COND_VB_SHOULD(VB_FISHING_ZERO_XZ, (CVarGetInteger(CVAR_ENHANCEMENT("HoverFishing"), false)), {
        *should = false; // NTSC 1.0
    });
}

// Vanilla bug: Not possible to fish with blank B because blank B item value 0xFF is saved
// as temp B = disabled B -> fishing pole is unequipped.
// Fix: If fishing, disregard disabled B and on B press set used item to fishing pole.
void RegisterAllowFishingBlankB() {
    COND_VB_SHOULD(VB_PUTAWAY_BECAUSE_DISABLED_ITEM_BUTTONS,
                   (IS_RANDO || CVarGetInteger(CVAR_ENHANCEMENT("FishingBlankB"), IS_RANDO)), {
                       if (OoT_gPlayState->interfaceCtx.unk_260 != 0 &&
                           gSaveContext.equips.buttonItems[0] == ITEM_FISHING_POLE) {
                           *should = false;
                       }
                   });

    COND_VB_SHOULD(
        VB_OVERRIDE_BUTTON_ITEM_USED, (IS_RANDO || CVarGetInteger(CVAR_ENHANCEMENT("FishingBlankB"), IS_RANDO)), {
            s32* i = va_arg(args, s32*);
            Player* player = va_arg(args, Player*);
            s32* item = va_arg(args, s32*);
            if (OoT_gPlayState->interfaceCtx.unk_260 != 0 && *i == 0 && player->itemAction == PLAYER_IA_FISHING_POLE) {
                *item = ITEM_FISHING_POLE;
            }
        });
}

static RegisterShipInitFunc initFishingPrizeThreshold(RegisterFishingPrizeThreshold,
                                                      { CVAR_ENHANCEMENT("CustomizeFishing"), "IS_RANDO" });
static RegisterShipInitFunc initHoverFishing(RegisterHoverFishing, { CVAR_ENHANCEMENT("HoverFishing") });
static RegisterShipInitFunc initAllowFishingBlankB(RegisterAllowFishingBlankB,
                                                   { CVAR_ENHANCEMENT("FishingBlankB"), "IS_RANDO" });
