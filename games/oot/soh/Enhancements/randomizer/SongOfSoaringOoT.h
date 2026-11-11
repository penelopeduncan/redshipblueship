#pragma once
/**
 * @file SongOfSoaringOoT.h
 * @brief Majora's Mask's Song of Soaring as an Ocarina of Time item (#1169 lane
 *        XW4; OoTMM songSoaringOot). The definitions and the reasoning are in
 *        SongOfSoaringOoT.cpp; this header is what the pool, the logic, the
 *        restricted song pass and the locks call.
 */
#ifdef RSBS_SINGLE_EXECUTABLE

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct PlayState;

#ifdef __cplusplus
extern "C" {
#endif

/** Is the frozen Song of Soaring (OoT) rule on (RSBS_XITEM2_SONG_SOARING_OOT)?
 *  False while nothing is frozen. OoT's logic credits the song only then. */
bool OoT_SongOfSoaringOoTOn(void);

/** How many separate Songs of Soaring (OoT) the frozen world puts into OoT's
 *  pool (Combo_SongSoaringOoTCopies over the frozen second rules word): 1 with
 *  the rule on and the shared song off, else 0. */
int OoT_SongOfSoaringOoTPooledCopies(void);

/** Does Link hold the Song of Soaring (OoT)? Its RAND_INF flag; lane XW10 widens
 *  this one predicate for the shared song. */
bool OoT_SoaringOoTOwned(void);

/* #1169 lane XW6: playing it (the gates are src/common/warp_song_soaring.h). */

/** THE MATCHER's question (code_800EC960.c, func_800ED200): 1 when the note
 *  buffer's tail is the Song of Soaring AND the frozen rule is on, the world is
 *  paired, Link owns the song, is a child and plays in free play; else 0. */
int32_t OoT_SoaringOoT_NoteMatched(const uint8_t* song, uint8_t appendPos);

/** The song's playback (z_message_PAL.c, SETUP_DISPLAY_SONG_PLAYED): MM's
 *  Soaring notes in OoT's format, without an index into sOcarinaSongs. */
void OoT_SoaringOoT_StartPlayback(void);

/** SONG_PLAYED_ACT for the song: the gates, then the owl chooser. Every refusal
 *  shows its text and leaves OCARINA_MODE_04. */
void OoT_SoaringOoT_OnSongPlayed(struct PlayState* play);

/** THE XW8 SEAM. Opens the owl chooser over @p owlMask (the interim one is a
 *  3-choice text cycle; lane XW8 replaces only this with OoTMM's pause map) and
 *  crosses to one owl (the gates again, then the one-shot warp target and an
 *  ordinary game switch). */
void OoT_SoaringOoT_BeginChooser(struct PlayState* play, uint16_t owlMask);
void OoT_SoaringOoT_Depart(struct PlayState* play, int owl);

#ifdef __cplusplus
}

#include <vector>

#include "randomizerTypes.h"

/**
 * THE GENERATE REFUSAL (OoTMM solve.ts placeSemiShuffled), asked by OoT's
 * restricted song pass (3drando/fill.cpp) with the songs it is about to place
 * (after the starting songs and the #1085 hold-back), the spots it would place
 * them on, and RSK_SHUFFLE_SONGS's value. 1 when the songs do not fit: the
 * refusal, beginning with OoTMM's "Not enough locations for songs/dungeon
 * rewards", is logged, written to stderr and recorded
 * (Combo_SongSpotsRecordRefusal), and the pass must end the generation. 0
 * otherwise, which is every world this build lets a player create.
 */
int OoT_SongSpots_RefuseOverflow(const std::vector<RandomizerGet>& songs, size_t spots, uint8_t shuffleSongs);
#endif

#endif // RSBS_SINGLE_EXECUTABLE
