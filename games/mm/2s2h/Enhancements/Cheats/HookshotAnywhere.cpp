#include <libultraship/bridge/consolevariablebridge.h>
#include "2s2h/GameInteractor/GameInteractor.h"
#include "2s2h/ShipInit.hpp"
#ifdef RSBS_SINGLE_EXECUTABLE
#include "2s2h/Rando/Rando.h"
#include "combo_mm_hookshot_anywhere.h"

static_assert((int)RO_HOOKSHOT_ANYWHERE_OFF == (int)COMBO_MM_HOOKSHOT_ANYWHERE_OFF &&
                  (int)RO_HOOKSHOT_ANYWHERE_ON == (int)COMBO_MM_HOOKSHOT_ANYWHERE_ON &&
                  (int)RO_HOOKSHOT_ANYWHERE_LOGICAL == (int)COMBO_MM_HOOKSHOT_ANYWHERE_LOGICAL,
              "RandoOptionHookshotAnywhere and ComboMMHookshotAnywhereMode must agree (#1574)");
#endif

#define CVAR_NAME "gCheats.HookshotEverything"
#define CVAR CVarGetInteger(CVAR_NAME, 0)

void RegisterHookshotAnywhere() {
#ifdef RSBS_SINGLE_EXECUTABLE
    COND_VB_SHOULD(VB_BE_HOOKSHOT_SURFACE,
                   Combo_MMHookshotAnywhereSurfaces(IS_RANDO ? RANDO_SAVE_OPTIONS[RO_HOOKSHOT_ANYWHERE] : 0, CVAR),
                   { *should = true; });
#else
    COND_VB_SHOULD(VB_BE_HOOKSHOT_SURFACE, CVAR, { *should = true; });
#endif
}

static RegisterShipInitFunc initFunc(RegisterHookshotAnywhere, { CVAR_NAME, "IS_RANDO" });
