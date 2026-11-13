#pragma once
/**
 * @file warp_song_tracker.h
 * @brief The check trackers' cross-game warp-song travel facts, read from the
 *        FROZEN world (#1169 lane XW11). Game-header-free.
 *
 * THE GAP XW9 LEFT (its decision D6). OoT's root warp-song exits
 * (location_access/root.cpp, OoT_ComboLogic_MMWarpTravel) and MM's root owl
 * landings (Logic.cpp, CAN_OOT_SOAR_TO_OWL through MM_ComboLogic_OoTSoaringTravel)
 * open for the other game's warp song only inside a coordinator round. Each
 * check tracker evaluates its own game's logic alone, outside any round, so it
 * never credited that travel.
 *
 * WHAT THE TRACKERS READ NOW. The other game's facts from that game's FROZEN
 * copy (its suspended save, Context_GetOoTSaveContext / Context_GetMMSaveContext),
 * never its live heap (XW7's ruling: MM never reads OoT's suspended Rando heap),
 * plus the shared combo state:
 *   - INTO OoT (OoT's tracker): song i counts when its frozen rule (0x02 << i) is
 *     on, MM owns it (gComboCtx.mmWarpSongsOwned through Combo_AddedSongHeld), the
 *     pair is active, MM's frozen save holds the Ocarina of Time and the song's
 *     buttons (the registered MM reader), and OoT's crossing is open in the
 *     tracker's own first search (the caller's @p crossOoT). The destination is
 *     the frozen warp record's (Combo_TrackerTravel_OoTDest), never the graph's.
 *   - INTO MM (MM's tracker): the frozen rule 0x1 is on, OoT owns its Song of
 *     Soaring and can play it in OoT's frozen save (the registered OoT reader),
 *     OoT's frozen age is CHILD (XW6's play gate), the pair is active, and MM's
 *     crossing (the tower interior) is reached in the tracker's first crawl
 *     (@p crossMM). OoT's crossing is implied: the player is in MM.
 * Both end in Combo_Logic_TravelGate, the coordinator's one gate rule.
 *
 * SHOW LESS WHEN UNSURE. Any fact that cannot be derived (no frozen copy, a copy
 * frozen as not a live file, OoT adult or unknown, a reader not registered, a
 * record not PRESENT, the crossing not reached) is 0, and the tracker credits
 * exactly what it credited before XW11. Under-crediting never strands a player;
 * over-crediting misleads.
 *
 * SCOPE. Only the two tracker entry points arm these facts (OoT's
 * ComputeAvailableChecks, MM's ComputeChecksInLogic), for a second search that
 * runs only when a fact is nonzero. OoT's fill, hints, engine searches and MM's
 * generation crawl never see them, and a file with every rule off never runs
 * the second search.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** How many OoT warp songs MM can host (RSBS_MM_WARP_SONG_COUNT, RSBS_MMWP_SONG_COUNT). */
#define RSBS_TRACKER_TRAVEL_SONG_COUNT 6

/**
 * The facts INTO OoT OoT's check tracker may credit (bit 0x02 << song, the
 * RSBS_COMBO_TRAVEL_INTO_OOT bits): for each song, the frozen word @p rules2 has
 * its extension on, Combo_AddedSongHeld over @p mmOwned's bit (bit i song i,
 * gComboCtx.mmWarpSongsOwned's layout), bit i of @p mmPlayableMask (MM can play
 * it), @p paired, and then Combo_Logic_TravelGate with @p crossOoT. Pure.
 */
uint32_t Combo_TrackerTravel_IntoOoT(uint32_t rules2, uint8_t mmOwned, uint32_t mmPlayableMask, int paired,
                                     int crossOoT);

/**
 * Combo_TrackerTravel_IntoOoT with the shared songs (#1169 XW14): song i also
 * counts as MM's when @p families2Held (Combo_SharedFamilies2HeldNow's bits) has
 * its shared bit, (0x02 << i) << RSBS_XITEM2_WARP_SONG_SHARED_SHIFT, and the frozen
 * @p rules2 has that shared rule on (Combo_AddedSongHeld's fourth argument).
 * Combo_TrackerTravel_IntoOoT is this with 0. Pure.
 */
uint32_t Combo_TrackerTravel_IntoOoTEx(uint32_t rules2, uint8_t mmOwned, uint32_t mmPlayableMask, int paired,
                                       int crossOoT, uint32_t families2Held);

/**
 * The fact INTO MM MM's check tracker may credit (RSBS_COMBO_TRAVEL_INTO_MM or
 * 0): the frozen rule RSBS_XITEM2_SONG_SOARING_OOT on, Combo_AddedSongHeld over
 * @p soaringOwned, @p soaringPlayable, @p ootAge == RSBS_MMWP_AGE_CHILD,
 * @p paired, and then Combo_Logic_TravelGate with OoT's crossing open (the
 * player is in MM) and @p crossMM. Pure.
 */
uint32_t Combo_TrackerTravel_IntoMM(uint32_t rules2, bool soaringOwned, bool soaringPlayable, int ootAge, int paired,
                                    int crossMM);

/**
 * Combo_TrackerTravel_IntoMM with the shared Song of Soaring (#1169 XW14): OoT's
 * copy also counts as owned when @p families2Held has
 * RSBS_XITEM2_SHARED_SONG_SOARING and the frozen @p rules2 has that rule on.
 * Combo_TrackerTravel_IntoMM is this with 0. Pure.
 */
uint32_t Combo_TrackerTravel_IntoMMEx(uint32_t rules2, bool soaringOwned, bool soaringPlayable, int ootAge, int paired,
                                      int crossMM, uint32_t families2Held);

/* ---- The frozen copies' readers, registered by each game's TU --------------- */

/** MM's reader (games/mm/2s2h/Rando/ItemAdapterSingleExe.cpp): over an MM save of
 *  @p size bytes, the songs (bit i song i, OoTMM's order) whose notes MM can play
 *  there: the Ocarina of Time in the inventory and the song's buttons, read
 *  straight from the save's randoInf. 0 for a blob that is not a created file. */
typedef uint32_t (*ComboTrackerMMPlayableFn)(const void* mmSave, size_t size);
/** OoT's reader (games/oot/soh/Enhancements/randomizer/ItemAdapterSingleExe.cpp):
 *  over an OoT save of @p size bytes, whether it owns the Song of Soaring (OoT)
 *  and can play it (an ocarina and C-down, C-up, C-left). Returns 0 for a blob
 *  that is not a created file (both outputs then 0). */
typedef int (*ComboTrackerOoTSoaringFn)(const void* ootSave, size_t size, uint8_t* owned, uint8_t* playable);

/** Register MM's reader; NULL unregisters (every INTO OoT fact is then 0). */
void Combo_TrackerTravel_RegisterMMReader(ComboTrackerMMPlayableFn fn);
/** Register OoT's reader; NULL unregisters (the INTO MM fact is then 0). */
void Combo_TrackerTravel_RegisterOoTReader(ComboTrackerOoTSoaringFn fn);

/* ---- The facts now, from the frozen world ------------------------------------ */

/**
 * OoT's tracker's facts now: Combo_TrackerTravel_IntoOoTEx over the frozen second
 * word, the ownership byte, the shared songs held now (Combo_SharedFamilies2HeldNow,
 * #1169 XW14), the pairing, and MM's FROZEN save through MM's reader
 * (0 when MM has no frozen copy, or one frozen as not a live file, or no reader).
 * With every MM warp-song rule off it reads the word and returns 0.
 */
uint32_t Combo_TrackerTravel_OoTFactsNow(int crossOoT);

/**
 * MM's tracker's fact now: Combo_TrackerTravel_IntoMMEx over the frozen second
 * word, OoT's FROZEN save through OoT's reader, the shared songs held now
 * (Combo_SharedFamilies2HeldNow, #1169 XW14), Combo_MMWarpPlay_OoTFrozenAge and
 * the pairing (0 when OoT has no frozen copy, or one frozen as not a live file,
 * or no reader). With the Soaring rule off it reads the word and returns 0.
 */
uint32_t Combo_TrackerTravel_MMFactNow(int crossMM);

/** The destination OoT's tracker credits for @p song: the frozen warp record's
 *  (Combo_OoTWarpRecord_Dest) when the record is PRESENT, else 0, which no logic
 *  credits. Never the entrance graph's: a loaded file's tracker graph carries no
 *  replacements, so the graph would answer the vanilla pad. */
uint16_t Combo_TrackerTravel_OoTDest(int song);

#ifdef __cplusplus
}
#endif
