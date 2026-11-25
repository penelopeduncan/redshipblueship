/**
 * @file mm_moon_crash_rules.h
 * @brief Majora's Mask's save and arrival rules that follow the file's frozen
 *        Moon Crash Behavior (#1728): where a New Cycle roof save comes back,
 *        and OoTMM's grace period on an arrival.
 *
 * ============================================================================
 * WHAT OoTMM DOES (OoTMM/OoTMM, packages/generator/src)
 * ============================================================================
 *
 * THE ROOF SAVE. OoTMM's pause-menu save (common/kaleido_scope.c, KaleidoScope_CanSave)
 * is refused on the Clock Tower roof unless the Moon is shuffled with the
 * dungeons or the moon crash is New Cycle:
 *
 *     case SCE_MM_CLOCK_TOWER_ROOFTOP:
 *         return !!Config_Flag(CFG_ER_MOON) || !!Config_Flag(CFG_MM_MOON_CRASH_CYCLE);
 *
 * A load starts in Ocarina of Time, and Majora's Mask's open forces its own
 * entrance to ENTR_MM_CLOCK_TOWN_FROM_CLOCK_TOWER (mm/save.c Sram_AfterOpenSave),
 * so a roof save never comes back on the roof. This build lets the pause save
 * and the autosave save on the roof whatever the setting (SavingEnhancements_CanSave
 * has no roof term, and a Last Save file keeps that), but with Remember Save
 * Location on it recorded the roof's own entrance, so the first arrival after a
 * load landed on the roof with a fresh five-minute timer.
 * Combo_MmRoofSaveLandsAtCrossing is the New Cycle rule that sends it to South
 * Clock Town instead, the crossing's own entrance.
 *
 * THE GRACE PERIOD. common/save.c gracePeriod, run from Save_OnLoad on every
 * open of Majora's Mask's save (and OoTMM's game switch saves and reboots, so
 * every arrival opens it): a clock within GRACE (0x1E0) of the moon crash is
 * pulled back to crash - GRACE, so the Moon cannot fall before the player can
 * act. Combo_MmGraceLinear is that arithmetic, exactly as this build's Last
 * Save reload already applied it (MM_Combo_ResetFromLastCommitOnMoonCrash);
 * Combo_MmArrivalTakesGrace says which arrivals take it.
 *
 * THE LAIR SAVE (#1744). 2Ship2Harkinian e5e9f3364 lets the pause save and
 * the autosave save on the Moon, and with Remember Save Location on a save in
 * Majora's lair records the lair's own entrance. The lair has no exit, so a
 * randomizer file without the Song of Time that reloads there may be stuck.
 * By the operator's ruling of 2026-11-19 ("Yes, Moon field") a save in the
 * lair records the Moon field's first entrance whatever Remember Save
 * Location says; Combo_MmLairSaveLandsOnMoonField is that rule. The four
 * trials and the Moon field keep 2Ship's choice.
 *
 * THE CRASH CUTSCENE (#1789). OoTMM's Interface_SkipMoonCrash (mm/interface.c)
 * replaces Interface_Update's call to Interface_StartMoonCrash and, under
 * CFG_MM_MOON_CRASH_CYCLE, runs Sram_SaveEndOfCycle on the crash frame and
 * sends the player through a white fade to its Clock Tower landing (0xC030,
 * which play.c rewrites to its initial entrance), so neither the Termina Field
 * moonfall nor the Fire Wall plays. This build keeps Interface_StartMoonCrash
 * and its white fade, and skips both cutscenes where the Story cutscene skip
 * already does (SkipMoonCrash.cpp, from Play_Init's func_800EDDB0): when the
 * arrival is the moon's fall (Combo_MmIsMoonFallArrival) and the file's frozen
 * behaviour is New Cycle (Combo_MmNewCycleSkipsCrashCutscene). Last Save keeps
 * both cutscenes, as in OoTMM.
 *
 * Game-header-free and pure: every input is an argument, so a ROM-free row
 * drives each rule without a PlayState, a CVar or a save.
 */
#ifndef RSBS_COMMON_MM_MOON_CRASH_RULES_H
#define RSBS_COMMON_MM_MOON_CRASH_RULES_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** OoTMM's GRACE (common/save.c), in units of the linear clock below. */
#define RSBS_MM_GRACE_PERIOD_LINEAR 0x1E0u

/**
 * OoTMM's Time_Game2Linear (common/time.c): day * 0x10000 + (time - 06:00),
 * the 16-bit subtraction wrapping, so the hours after midnight belong to the
 * day before them.
 */
uint32_t Combo_MmLinearClock(int32_t day, uint16_t time);

/**
 * OoTMM's grace period on the linear clock: when @p linear is at or within
 * RSBS_MM_GRACE_PERIOD_LINEAR of @p crashLinear (or past it), writes
 * crashLinear - RSBS_MM_GRACE_PERIOD_LINEAR to @p outLinear and returns 1;
 * otherwise writes @p linear and returns 0. @p outLinear may be NULL.
 */
int Combo_MmGraceLinear(uint32_t linear, uint32_t crashLinear, uint32_t* outLinear);

/**
 * Whether an arrival in Majora's Mask takes the grace period: 1 only when it
 * restored a frozen half (@p hadFrozenState) and that file's frozen Moon Crash
 * Behavior (@p frozenMoonCrash, MM_Rando_FrozenMoonCrash) is New Cycle. A Last
 * Save file keeps its clock as committed, exactly as before #1728; an arrival
 * with no frozen half spawns on the boot chain's new-file clock.
 */
int Combo_MmArrivalTakesGrace(int hadFrozenState, int frozenMoonCrash);

/**
 * Whether a pause save or an autosave taken in this scene records South Clock
 * Town as the place to come back to, whatever Remember Save Location says: 1
 * only on the Clock Tower roof (@p sceneIsRoof) of a paired file (@p paired,
 * Context_ComboSessionResident) whose frozen Moon Crash Behavior is New Cycle.
 * An unpaired session keeps 2Ship's choice, and a Last Save file keeps today's.
 */
int Combo_MmRoofSaveLandsAtCrossing(int sceneIsRoof, int paired, int frozenMoonCrash);

/**
 * Whether a pause save or an autosave taken in this scene records the Moon
 * field (ENTRANCE(THE_MOON, 0)) as the place to come back to, whatever Remember
 * Save Location says: 1 only in Majora's lair (@p sceneIsLair, SCENE_LAST_BS),
 * which has no exit (#1744, the ruling of 2026-11-19). Any file, paired or
 * not, under either Moon Crash Behavior. The four trials and the Moon field
 * keep 2Ship2Harkinian's choice (e5e9f3364).
 */
int Combo_MmLairSaveLandsOnMoonField(int sceneIsLair);

/**
 * ENTRANCE(TERMINA_FIELD, 12), where Interface_StartMoonCrash sends the moon's
 * fall (z_parameter.c) and OoTMM's Last Save branch writes the same literal
 * (mm/interface.c). SkipMoonCrash.cpp static-asserts the two agree.
 */
#define RSBS_MM_ENTRANCE_MOON_FALL 0x54C0u

/**
 * Whether the moon's fall skips the Termina Field and Fire Wall cutscenes
 * (#1789): 1 only when the file's frozen Moon Crash Behavior (@p frozenMoonCrash,
 * MM_Rando_FrozenMoonCrash, which already folds the Shared Song of Time and
 * Shuffle Ocarina forcing) is New Cycle, the same predicate under which
 * Sram_ResetSaveFromMoonCrash starts a new cycle. Last Save keeps both.
 */
int Combo_MmNewCycleSkipsCrashCutscene(int frozenMoonCrash);

/**
 * Whether Play_Init is arriving as the moon's fall: @p entrance is
 * RSBS_MM_ENTRANCE_MOON_FALL with cutscene index 0 (Interface_StartMoonCrash's
 * nextCutsceneIndex) and no respawn (@p respawnFlag 0: the telescope's
 * "fourth day" timing the Story skip's own guard keeps). The Giants' failure
 * from the Clock Tower roof (ENTRANCE(TERMINA_FIELD, 1), 0xFFF0) is not this
 * arrival, so its cutscene plays, as in OoTMM.
 */
int Combo_MmIsMoonFallArrival(uint16_t entrance, int32_t cutsceneIndex, int32_t respawnFlag);

#ifdef __cplusplus
}
#endif

#endif // RSBS_COMMON_MM_MOON_CRASH_RULES_H
