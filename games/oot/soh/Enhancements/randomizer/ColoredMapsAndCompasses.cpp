#include <libultraship/bridge.h>
#include "soh/ResourceManagerHelpers.h"
#include "soh/ShipInit.hpp"
#ifdef RSBS_SINGLE_EXECUTABLE
#include "soh/Enhancements/cosmetics/OwedGfxPatches.h" // #1829: OoT_GfxPatches_RunOrOwe
#endif
#include "z64save.h"
#include "objects/object_gi_compass/object_gi_compass.h"
#include "objects/object_gi_map/object_gi_map.h"
#include "soh/OTRGlobals.h"
#include "soh/Enhancements/randomizer/SeedContext.h"

extern "C" {
#include "variables.h"
#include "macros.h"
}

#define CVAR_COLORED_MAPS_AND_COMPASSES_NAME CVAR_RANDOMIZER_ENHANCEMENT("ColoredMapsAndCompasses")
#define CVAR_COLORED_MAPS_AND_COMPASSES_DEFAULT 1
#define CVAR_COLORED_MAPS_AND_COMPASSES_VALUE \
    CVarGetInteger(CVAR_COLORED_MAPS_AND_COMPASSES_NAME, CVAR_COLORED_MAPS_AND_COMPASSES_DEFAULT)

static void ColoredMapsAndCompasses_Pass(bool /*owed*/) {
    s8 mapsAndCompassesCanBeOutsideDungeon =
        IS_RANDO && DUNGEON_ITEMS_CAN_BE_OUTSIDE_DUNGEON(RSK_SHUFFLE_MAPANDCOMPASS);
    s8 isColoredMapsAndCompassesEnabled = mapsAndCompassesCanBeOutsideDungeon && CVAR_COLORED_MAPS_AND_COMPASSES_VALUE;
    if (isColoredMapsAndCompassesEnabled) {
        ResourceMgr_PatchGfxByName(gGiDungeonMapDL, "Map_PrimColor", 5, gsDPNoOp());
        ResourceMgr_PatchGfxByName(gGiDungeonMapDL, "Map_EnvColor", 6, gsDPNoOp());
        ResourceMgr_PatchGfxByName(gGiCompassDL, "Compass_PrimColor", 5, gsDPNoOp());
        ResourceMgr_PatchGfxByName(gGiCompassDL, "Compass_EnvColor", 6, gsDPNoOp());
    } else {
        ResourceMgr_UnpatchGfxByName(gGiDungeonMapDL, "Map_PrimColor");
        ResourceMgr_UnpatchGfxByName(gGiDungeonMapDL, "Map_EnvColor");
        ResourceMgr_UnpatchGfxByName(gGiCompassDL, "Compass_PrimColor");
        ResourceMgr_UnpatchGfxByName(gGiCompassDL, "Compass_EnvColor");
    }
}

void RegisterColoredMapsAndCompasses() {
#ifdef RSBS_SINGLE_EXECUTABLE
    // #1829: OoT's owner only; owed while MM runs, and paid in play, where IS_RANDO and the seed are OoT's own
    OoT_GfxPatches_RunOrOwe("ColoredMapsAndCompasses", ColoredMapsAndCompasses_Pass, true);
#else
    ColoredMapsAndCompasses_Pass(false);
#endif
}

static RegisterShipInitFunc initFunc(RegisterColoredMapsAndCompasses,
                                     { "IS_RANDO", CVAR_COLORED_MAPS_AND_COMPASSES_NAME });
