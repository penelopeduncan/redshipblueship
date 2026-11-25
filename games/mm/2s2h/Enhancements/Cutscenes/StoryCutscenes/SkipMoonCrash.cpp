#include <libultraship/bridge/consolevariablebridge.h>
#include "2s2h/GameInteractor/GameInteractor.h"
#include "2s2h/ShipInit.hpp"

#ifdef RSBS_SINGLE_EXECUTABLE
#include <cstdio>

#include "mm_moon_crash_rules.h" // src/common: Combo_MmIsMoonFallArrival, Combo_MmNewCycleSkipsCrashCutscene (#1789)
#endif

extern "C" {
#include "variables.h"
#ifdef RSBS_SINGLE_EXECUTABLE
u16 MM_Play_MoonCrashArrivalEntrance(u16 entrance); // games/mm/2s2h/GameExports_SingleExe.cpp
int MM_Rando_FrozenMoonCrash(void);                 // games/mm/2s2h/Rando/Foreign.cpp
#endif
}

// RSBS (#1278): a paired file lands where the Song of Time does, not in the
// salesman's cycle-reset cutscene (OoTMM's rewrite of 0xC030). A macro because
// the landing is written inside the COND_VB_SHOULD body, where #ifdef cannot go.
#ifdef RSBS_SINGLE_EXECUTABLE
#define RSBS_MOON_CRASH_LANDING(entrance) MM_Play_MoonCrashArrivalEntrance(entrance)
#else
#define RSBS_MOON_CRASH_LANDING(entrance) (entrance)
#endif

// RSBS (#1789): the moon's fall as Play_Init sees it, shared by the Story skip
// below and the New Cycle skip (src/common/mm_moon_crash_rules.h).
#ifdef RSBS_SINGLE_EXECUTABLE
static_assert(ENTRANCE(TERMINA_FIELD, 12) == RSBS_MM_ENTRANCE_MOON_FALL,
              "Interface_StartMoonCrash's Termina Field spawn 12 is the moon's fall the rule matches");
#define RSBS_IS_MOON_FALL_ARRIVAL()                                                                         \
    Combo_MmIsMoonFallArrival((uint16_t)gSaveContext.save.entrance, (int32_t)gSaveContext.save.cutsceneIndex, \
                              (int32_t)gSaveContext.respawnFlag)
#else
#define RSBS_IS_MOON_FALL_ARRIVAL()                                                                     \
    (gSaveContext.save.cutsceneIndex == 0x0 && gSaveContext.save.entrance == ENTRANCE(TERMINA_FIELD, 12) && \
     gSaveContext.respawnFlag == 0)
#endif

#define CVAR_NAME "gEnhancements.TimeSavers.SkipCutscene.Story"
#define CVAR CVarGetInteger(CVAR_NAME, 0)

/**
 * The moon-crash reset at the moon's fall, in place of the Termina Field and
 * Fire Wall cutscenes: when Play_Init is arriving in Termina Field spawn 12 with
 * cutscene index 0 and no respawn, runs the reset the Fire Wall's
 * CS_MISC_RESET_SAVE_FROM_MOON_CRASH would have run and writes the landing its
 * CS_DESTINATION_DEFAULT would have taken. Returns 1 when it acted, 0 otherwise.
 * Called by the Story skip below and (#1789) by MM_Play_SkipMoonCrashUnderNewCycle.
 */
extern "C" int MM_SkipMoonCrash_ResetAtFall(void) {
    // The respawn flag check here is so that we don't skip if the player is trying to pull off the 4th day
    // glitch from the telescope
    if (!RSBS_IS_MOON_FALL_ARRIVAL()) {
        return 0;
    }
    // This cutscene command would otherwise be run as part of the Fire Wall cutscene that gets skipped.
    // IT MUST BE CALLED HERE IF THAT CUTSCENE IS SKIPPED OR ELSE THE GAME STATE WILL NOT RESET CORRECTLY!
    Sram_ResetSaveFromMoonCrash(&MM_gPlayState->sramCtx);

    // RSBS (#1278): through RSBS_MOON_CRASH_LANDING, above.
    gSaveContext.save.entrance = RSBS_MOON_CRASH_LANDING(ENTRANCE(CLOCK_TOWER_INTERIOR, 3));
    return 1;
}

#ifdef RSBS_SINGLE_EXECUTABLE
/**
 * RSBS (#1789): under Moon Crash Behavior: New Cycle the moon's fall skips the
 * Termina Field and Fire Wall cutscenes, as OoTMM does (its
 * Interface_SkipMoonCrash, mm/interface.c): the white fade
 * Interface_StartMoonCrash already starts, then the Dawn of the First Day and
 * the landing. Called directly by z_demo.c's func_800EDDB0, before Play_Init's
 * VB_PLAY_TRANSITION_CS dispatch (so no hook count moves, and the Story skip's
 * hook then sees the landing, not the fall, and does nothing). The predicate is
 * the one Sram_ResetSaveFromMoonCrash starts a new cycle under
 * (MM_Rando_FrozenMoonCrash, the Shared Song of Time and Shuffle Ocarina
 * forcing included); Last Save keeps both cutscenes. Returns 1 when it acted.
 */
extern "C" int MM_Play_SkipMoonCrashUnderNewCycle(void) {
    if (MM_gPlayState == NULL || !Combo_MmNewCycleSkipsCrashCutscene(MM_Rando_FrozenMoonCrash())) {
        return 0;
    }
    if (MM_SkipMoonCrash_ResetAtFall() == 0) {
        return 0;
    }
    fprintf(stderr,
            "[MM] moon crash: New Cycle skips the Termina Field and Fire Wall cutscenes (#1789): lands on 0x%04X\n",
            (unsigned)(u16)gSaveContext.save.entrance);
    fflush(stderr);
    return 1;
}
#endif

/**
 *  Skips only the visible cutscenes (Clock Tower destruction and Link's terrible fate with the Fire Wall).
 *  Note: There is a cutscene command SPECIFICALLY for resetting game state related to the moon crash,
 *        CS_MISC_RESET_SAVE_FROM_MOON_CRASH, that MUST NOT be skipped, or else
 *        it would break the vanilla functionality of a Moon Crash reset.
 */
void RegisterSkipMoonCrash() {
    // Skips initial cutscene in Termina Field where the moon falls, goes straight to clock tower cutscene
    COND_VB_SHOULD(VB_PLAY_TRANSITION_CS, CVAR, {
        // #1018-#1022, per PR #673's criterion: this TU is in the single-exe link now and the body reads MM_gPlayState;
        // a missing play state leaves the vanilla verdict untouched.
        RSBS_SINGLE_EXE_REQUIRE(MM_gPlayState != NULL);
        // RSBS (#1789): the body is MM_SkipMoonCrash_ResetAtFall, above.
        MM_SkipMoonCrash_ResetAtFall();
    });
}

static RegisterShipInitFunc initFunc(RegisterSkipMoonCrash, { CVAR_NAME });
