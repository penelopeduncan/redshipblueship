#include <libultraship/bridge/consolevariablebridge.h>
#include "2s2h/ShipInit.hpp"
#include "SavingEnhancements.h"

extern "C" PlayState* MM_gPlayState;

#define CVAR_NAME "gEnhancements.Saving.PauseSave"
// RSBS (#1728): on by default here; the one read of the key is SavingEnhancements_PauseSaveOn.
#define CVAR SavingEnhancements_PauseSaveOn()

void RegisterPauseSave() {
    COND_VB_SHOULD(VB_SAVE_ON_B_BUTTON_IN_PAUSE_MENU, CVAR, {
        // W6-TIMESAVERS (#1236): linked in the single exe now, and the body reads MM_gPlayState; a missing play state
        // leaves the vanilla behavior untouched.
        RSBS_SINGLE_EXE_REQUIRE(MM_gPlayState != NULL);
        Input* input = CONTROLLER1(&MM_gPlayState->state);
        *should = SavingEnhancements_CanSave() && CHECK_BTN_ALL(input->press.button, BTN_B);
    });
}

static RegisterShipInitFunc initFunc(RegisterPauseSave, { CVAR_NAME });
