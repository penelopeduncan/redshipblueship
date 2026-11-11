#pragma once
/**
 * @file TimeTravelSword.h
 * @brief OoTMM's "Time Travel requires Master Sword" for Ocarina of Time's
 *        randomizer (#1434): RSK_TIME_TRAVEL_REQUIRES_MASTER_SWORD. The
 *        definitions and the reasoning are in TimeTravelSword.cpp; this header is
 *        what the pedestal's hook, the fill's time-travel step, the settings fold
 *        and the lock call, and the child pedestal check (#1484). C-compatible.
 */
#ifdef RSBS_SINGLE_EXECUTABLE

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/** THE RULE, pure. May Link travel through time at the Pedestal of Time?
 *  @param requiresSword the frozen RSK_TIME_TRAVEL_REQUIRES_MASTER_SWORD
 *  @param swordShuffled the frozen RSK_SHUFFLE_MASTER_SWORD: off, the pedestal
 *         itself holds the sword and pulling it hands it over (VB_GIVE_ITEM_MASTER_SWORD)
 *  @param ownsSword     Link owns the Master Sword (the equipment bit, not B) */
bool OoT_TimeTravelSword_Allows(bool requiresSword, bool swordShuffled, bool ownsSword);

/** 1 when the frozen world's settings can make the rule stop Link: the option on,
 *  the sword shuffled and not a starting item. Off this, the option changes
 *  nothing in the world, so the settings fold leaves it out (playthrough.cpp). */
bool OoT_TimeTravelSword_RuleCanBlock(void);

/** The pedestal's answer on the live save (hook_handlers.cpp,
 *  VB_BE_ABLE_TO_TIME_TRAVEL_AT_PEDESTAL). True on a vanilla file. */
bool OoT_TimeTravelSword_PedestalAllows(void);

/** The search's answer on logic's own save (3drando/fill.cpp, PropagateTimeTravel). */
bool OoT_TimeTravelSword_LogicAllows(void);

/** #1484, OoTMM's child pedestal check. 1 when the live save is a randomizer
 *  file whose frozen Shuffle Master Sword is on: then RC_TOT_MASTER_SWORD is a
 *  child check handed over as the child walks up, not at the pull. */
bool OoT_PedestalChildCheck_Applies(void);

/** #1484: the pedestal's update (hook_handlers.cpp, RandomizerOnActorUpdateHandler).
 *  A child in reach, outside a cutscene, whose pedestal check is not yet given,
 *  gets it: RAND_INF_TOT_MASTER_SWORD is set, and the flag-set hook queues the
 *  check. @p actor is any updated actor; only Bg_Toki_Swd is acted on. */
void OoT_PedestalChildCheck_OnPedestalUpdate(void* actor);

/** #1484: 1 while the pull must wait for the child's pedestal give (asked by
 *  VB_BE_ABLE_TO_TIME_TRAVEL_AT_PEDESTAL), so a child holding the sword cannot
 *  leave before the pedestal's item lands. @p giveInFlight: the randomizer's
 *  check queue holds a check or is giving one. */
bool OoT_PedestalChildCheck_HoldsPull(bool giveInFlight);

#ifdef __cplusplus
}
#endif

#endif // RSBS_SINGLE_EXECUTABLE
