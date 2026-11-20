/**
 * @file mm_moon_crash_rules.c
 * @brief Majora's Mask's Moon Crash Behavior rules for saves and arrivals
 *        (#1728), and where a save in Majora's lair comes back (#1744). The
 *        model and OoTMM's source are in mm_moon_crash_rules.h.
 */
#include "mm_moon_crash_rules.h"

#include "combo_mm_options_view.h" // RSBS_MM_MOON_CRASH_CYCLE

#include <stddef.h>

uint32_t Combo_MmLinearClock(int32_t day, uint16_t time) {
    return (uint32_t)day * 0x10000u + (uint16_t)(time - 0x4000u);
}

int Combo_MmGraceLinear(uint32_t linear, uint32_t crashLinear, uint32_t* outLinear) {
    const int moved = crashLinear >= RSBS_MM_GRACE_PERIOD_LINEAR && linear + RSBS_MM_GRACE_PERIOD_LINEAR >= crashLinear;
    if (outLinear != NULL) {
        *outLinear = moved ? crashLinear - RSBS_MM_GRACE_PERIOD_LINEAR : linear;
    }
    return moved;
}

int Combo_MmArrivalTakesGrace(int hadFrozenState, int frozenMoonCrash) {
    return hadFrozenState != 0 && frozenMoonCrash == RSBS_MM_MOON_CRASH_CYCLE;
}

int Combo_MmRoofSaveLandsAtCrossing(int sceneIsRoof, int paired, int frozenMoonCrash) {
    return sceneIsRoof != 0 && paired != 0 && frozenMoonCrash == RSBS_MM_MOON_CRASH_CYCLE;
}

int Combo_MmLairSaveLandsOnMoonField(int sceneIsLair) {
    return sceneIsLair != 0;
}
