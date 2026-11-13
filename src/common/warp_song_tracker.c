/**
 * @file warp_song_tracker.c
 * @brief The check trackers' cross-game warp-song travel facts from the frozen
 *        world (#1169 lane XW11). The reasoning is in warp_song_tracker.h.
 */

#include "warp_song_tracker.h"

#include "combo_logic.h"       // Combo_Logic_TravelGate, RSBS_COMBO_TRAVEL_INTO_*
#include "context.h"           // Context_HasFrozenState, the frozen halves, gComboCtx
#include "foreign_items.h"     // RSBS_XITEM2_*, Combo_ComboCrossItemFlags2, Combo_ForeignPairingActive
#include "game.h"              // GAME_OOT, GAME_MM, the halves' capacities
#include "mm_warp_song_play.h" // the frozen record, OoT's frozen age, RSBS_MMWP_SONG_COUNT
#include "shared_resources.h"  // Combo_SharedFamilies2HeldNow: the shared songs held now (#1169 XW14)
#include "warp_song_items.h"   // Combo_AddedSongHeld: the one ownership question
#include "warp_songs_mm.h"     // Combo_MMWarpSongsOwned

#include <stddef.h>

RSBS_CTX_STATIC_ASSERT(RSBS_TRACKER_TRAVEL_SONG_COUNT == RSBS_MMWP_SONG_COUNT &&
                           RSBS_TRACKER_TRAVEL_SONG_COUNT == (int)RSBS_MM_WARP_SONG_COUNT,
                       "the tracker's songs are XW5's and XW7's six, in OoTMM's order");
RSBS_CTX_STATIC_ASSERT((RSBS_XITEM2_SONG_MINUET_MM << 5) == RSBS_XITEM2_SONG_PRELUDE_MM &&
                           RSBS_COMBO_TRAVEL_INTO_OOT ==
                               (RSBS_XITEM2_WARP_SONG_EXTENSIONS & ~RSBS_XITEM2_SONG_SOARING_OOT),
                       "song i's rule bit and its travel bit are both 0x02 << i");

uint32_t Combo_TrackerTravel_IntoOoT(uint32_t rules2, uint8_t mmOwned, uint32_t mmPlayableMask, int paired,
                                     int crossOoT) {
    return Combo_TrackerTravel_IntoOoTEx(rules2, mmOwned, mmPlayableMask, paired, crossOoT, 0u);
}

uint32_t Combo_TrackerTravel_IntoOoTEx(uint32_t rules2, uint8_t mmOwned, uint32_t mmPlayableMask, int paired,
                                       int crossOoT, uint32_t families2Held) {
    if (paired == 0) {
        return 0u;
    }
    uint32_t factsMM = 0u;
    for (int song = 0; song < RSBS_TRACKER_TRAVEL_SONG_COUNT; song++) {
        const uint32_t extBit = 0x02u << song; // RSBS_XITEM2_SONG_MINUET_MM .. _PRELUDE_MM
        if ((rules2 & extBit) == 0u) {
            continue;
        }
        const bool separateOwned = (mmOwned & (1u << song)) != 0u;
        const bool sharedOwned = (families2Held & (extBit << RSBS_XITEM2_WARP_SONG_SHARED_SHIFT)) != 0u;
        if (!Combo_AddedSongHeld(rules2, extBit, separateOwned, sharedOwned)) {
            continue; // neither MM's own copy nor, under its shared rule, the family
        }
        if ((mmPlayableMask & (1u << song)) == 0u) {
            continue;
        }
        factsMM |= extBit; // the travel bit is the rule bit (static-asserted above)
    }
    uint32_t toOoT = 0u;
    Combo_Logic_TravelGate(0u, factsMM, crossOoT, 0, &toOoT, NULL);
    return toOoT;
}

uint32_t Combo_TrackerTravel_IntoMM(uint32_t rules2, bool soaringOwned, bool soaringPlayable, int ootAge, int paired,
                                    int crossMM) {
    return Combo_TrackerTravel_IntoMMEx(rules2, soaringOwned, soaringPlayable, ootAge, paired, crossMM, 0u);
}

uint32_t Combo_TrackerTravel_IntoMMEx(uint32_t rules2, bool soaringOwned, bool soaringPlayable, int ootAge, int paired,
                                      int crossMM, uint32_t families2Held) {
    if (paired == 0 || (rules2 & RSBS_XITEM2_SONG_SOARING_OOT) == 0u || ootAge != RSBS_MMWP_AGE_CHILD ||
        !soaringPlayable) {
        return 0u;
    }
    const bool sharedOwned = (families2Held & RSBS_XITEM2_SHARED_SONG_SOARING) != 0u;
    if (!Combo_AddedSongHeld(rules2, RSBS_XITEM2_SONG_SOARING_OOT, soaringOwned, sharedOwned)) {
        return 0u; // neither OoT's own copy nor, under the shared rule, the family
    }
    uint32_t toMM = 0u;
    // OoT's crossing is open: MM's tracker runs while the player is in MM.
    Combo_Logic_TravelGate(RSBS_COMBO_TRAVEL_INTO_MM, 0u, 1, crossMM, NULL, &toMM);
    return toMM;
}

static ComboTrackerMMPlayableFn sTrackerTravelMMReader = NULL;
static ComboTrackerOoTSoaringFn sTrackerTravelOoTReader = NULL;

void Combo_TrackerTravel_RegisterMMReader(ComboTrackerMMPlayableFn fn) {
    sTrackerTravelMMReader = fn;
}

void Combo_TrackerTravel_RegisterOoTReader(ComboTrackerOoTSoaringFn fn) {
    sTrackerTravelOoTReader = fn;
}

uint32_t Combo_TrackerTravel_OoTFactsNow(int crossOoT) {
    const uint32_t rules2 = Combo_ComboCrossItemFlags2();
    if ((rules2 & RSBS_COMBO_TRAVEL_INTO_OOT) == 0u) {
        return 0u; // every MM warp-song rule off: today's tracker
    }
    uint32_t playable = 0u;
    if (sTrackerTravelMMReader != NULL && Context_HasFrozenState(GAME_MM) &&
        Context_FrozenStateIsLiveFile(GAME_MM) != 0) {
        const void* mm = Context_GetMMSaveContext();
        if (mm != NULL) {
            playable = sTrackerTravelMMReader(mm, (size_t)MM_SAVE_CONTEXT_SIZE);
        }
    }
    return Combo_TrackerTravel_IntoOoTEx(rules2, Combo_MMWarpSongsOwned(), playable,
                                         Combo_ForeignPairingActive() ? 1 : 0, crossOoT,
                                         Combo_SharedFamilies2HeldNow());
}

uint32_t Combo_TrackerTravel_MMFactNow(int crossMM) {
    const uint32_t rules2 = Combo_ComboCrossItemFlags2();
    if ((rules2 & RSBS_XITEM2_SONG_SOARING_OOT) == 0u) {
        return 0u; // the Soaring rule off: today's tracker
    }
    uint8_t owned = 0u;
    uint8_t playable = 0u;
    if (sTrackerTravelOoTReader != NULL && Context_HasFrozenState(GAME_OOT) &&
        Context_FrozenStateIsLiveFile(GAME_OOT) != 0) {
        const void* oot = Context_GetOoTSaveContext();
        if (oot == NULL || sTrackerTravelOoTReader(oot, (size_t)OOT_SAVE_CONTEXT_SIZE, &owned, &playable) == 0) {
            owned = 0u;
            playable = 0u;
        }
    }
    return Combo_TrackerTravel_IntoMMEx(rules2, owned != 0u, playable != 0u, Combo_MMWarpPlay_OoTFrozenAge(),
                                        Combo_ForeignPairingActive() ? 1 : 0, crossMM, Combo_SharedFamilies2HeldNow());
}

uint16_t Combo_TrackerTravel_OoTDest(int song) {
    if (song < 0 || song >= RSBS_TRACKER_TRAVEL_SONG_COUNT) {
        return 0u;
    }
    if (Combo_OoTWarpRecord_Status() != (int)COMBO_OOT_WARP_REC_PRESENT) {
        return 0u; // a file from before XW7 (UNSET) or a malformed record: credit nothing
    }
    return Combo_OoTWarpRecord_Dest(song);
}
