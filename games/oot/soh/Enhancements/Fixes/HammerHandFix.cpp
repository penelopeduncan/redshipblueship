#include "soh/Enhancements/game-interactor/GameInteractor_Hooks.h"
#include "soh/ResourceManagerHelpers.h"
#include "soh/ShipInit.hpp"
#ifdef RSBS_SINGLE_EXECUTABLE
#include "soh/Enhancements/cosmetics/OwedGfxPatches.h" // #1829: OoT_GfxPatches_RunOrOwe
#endif

extern "C" {
#include "macros.h"
#include "objects/object_link_boy/object_link_boy.h"
extern SaveContext gSaveContext;
}

static constexpr int32_t CVAR_HAMMER_HAND_DEFAULT = 0;
#define CVAR_HAMMER_HAND_NAME CVAR_ENHANCEMENT("FixHammerHand")
#define CVAR_HAMMER_HAND_VALUE CVarGetInteger(CVAR_HAMMER_HAND_NAME, CVAR_HAMMER_HAND_DEFAULT)

static void FixHammerHand() {
    if (LINK_IS_ADULT) {
        ResourceMgr_PatchGfxByName(gLinkAdultLeftHandHoldingHammerNearDL, "hammerHand1", 92,
                                   gsSPDisplayListOTRFilePath(gLinkAdultLeftHandClosedNearDL));
        ResourceMgr_PatchGfxByName(gLinkAdultLeftHandHoldingHammerNearDL, "hammerHand2", 93, gsSPEndDisplayList());
    }
}

static void ResetHammerHand() {
    ResourceMgr_UnpatchGfxByName(gLinkAdultLeftHandHoldingHammerNearDL, "hammerHand1");
    ResourceMgr_UnpatchGfxByName(gLinkAdultLeftHandHoldingHammerNearDL, "hammerHand2");
}

static void HammerHandFix_Pass(bool /*owed*/) {
    if (CVAR_HAMMER_HAND_VALUE) {
        FixHammerHand();
    } else {
        ResetHammerHand();
    }
}

static void RegisterHammerHandFix() {
#ifdef RSBS_SINGLE_EXECUTABLE
    OoT_GfxPatches_RunOrOwe("HammerHandFix", HammerHandFix_Pass, true); // #1829: OoT's owner only; owed while MM runs
#else
    HammerHandFix_Pass(false);
#endif

    COND_HOOK(OnSceneInit, CVAR_HAMMER_HAND_VALUE, [](int32_t) { FixHammerHand(); });
}

static RegisterShipInitFunc initFunc(RegisterHammerHandFix, { CVAR_HAMMER_HAND_NAME });
