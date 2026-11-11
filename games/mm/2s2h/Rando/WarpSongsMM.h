#pragma once
/**
 * @file WarpSongsMM.h
 * @brief Ocarina of Time's six warp songs as Majora's Mask items (#1169 lane XW5;
 *        OoTMM songMinuetMm .. songPreludeMm, "Minuet of Forest (MM)" ...). The
 *        definitions and the reasoning are in WarpSongsMM.cpp; this header is what
 *        the item pipeline (GiveItem, ConvertItem, GeneratePools, DrawItem,
 *        Items.cpp), the logic (Logic.h), the snapshot brackets and the locks call.
 *        C-compatible.
 */
#ifdef RSBS_SINGLE_EXECUTABLE

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** The extension rule (RSBS_XITEM2_SONG_MINUET_MM ...) that adds MM item `riId`
 *  (RI_SONG_MINUET .. RI_SONG_PRELUDE) to MM's pool; 0 for any other id. Pure. */
uint32_t MM_WarpSongMM_RuleBit(uint16_t riId);

/** The MM item of extension rule `ruleBit`; 0 (RI_UNKNOWN) for any other bit. Pure. */
uint16_t MM_WarpSongMM_ItemId(uint32_t ruleBit);

/** Is the frozen extension rule that adds `riId` on? False for any other id and
 *  while nothing is frozen. */
bool MM_WarpSongMM_RuleOn(uint16_t riId);

/** How many copies of `riId` the frozen world puts into MM's pool
 *  (Combo_MMWarpSongCopies over the frozen second rules word): 1 with its rule on
 *  and its shared song off, else 0. */
int MM_WarpSongMM_PooledCopies(uint16_t riId);

/** Does the pair own `riId` (its bit in gComboCtx.mmWarpSongsOwned)? The bit
 *  alone, whatever the rules say. */
bool MM_WarpSongMM_Owned(uint16_t riId);

/** Own `riId` (MM's give). No-op for any other id. */
void MM_WarpSongMM_Give(uint16_t riId);

/** OoTMM's has_song_tp_* in MM: the frozen rule holds the extension and the song
 *  is owned. What MM's logic reads (Logic.h, CAN_PLAY_OOT_WARP_SONG). */
bool MM_WarpSongMM_Usable(uint16_t riId);

/** The ownership byte, for a bracket that restores MM's save after a simulated
 *  playthrough gave items into it (the bag's MM engine, MM's crawl, its
 *  glitchless fill, the attempt ladder): read before, put back after. */
uint8_t MM_WarpSongsMM_SnapshotOwned(void);
void MM_WarpSongsMM_RestoreOwned(uint8_t owned);

/** The pause and tracker icon of `riId`: MM's own song note. NULL for any other id. */
const char* MM_WarpSongMM_IconPath(uint16_t riId);

/** Draw `riId`'s get-item model (DrawItem.cpp): Ocarina of Time's own song through
 *  the co-mounted archives (MM's host-native note in that song's colour), else
 *  MM's generic white note. */
void MM_WarpSongMM_DrawGetItem(uint16_t riId);

/* ---- #1169 lane XW7: PLAYING them (src/common/mm_warp_song_play.h) --------- */

struct PlayState;

/** The usable songs right now, bit i = song i in OoTMM's order: 0 unless one of
 *  the six rules is frozen on and the world is paired; then the songs whose rule
 *  is on and which are owned (MM_WarpSongMM_Usable). */
uint8_t MM_WarpSongMM_UsableMask(void);
/**
 * The free-play matcher (code_8019AF00.c, AudioOcarina_CheckSongsWithoutMusicStaff,
 * after the vanilla loop and only when it matched nothing on this note): the staff
 * state 0x80 + song when the buffer's tail is a usable song and MM_gPlayState's
 * ocarina action is free play, else 0. With every rule off it reads one word and
 * writes nothing.
 */
uint8_t MM_WarpSongMM_NoteMatched(const uint8_t* buf, uint8_t appendPos, const uint8_t* btnToPitch);
/** Whether @p state is a warp song's staff state (0x80..0x85). */
bool MM_WarpSongMM_IsStaffState(unsigned state);
/** The Termina wall's gates (AudioOcarina_TerminaWallGenerateNotes): OoTMM's patch
 *  (always the random-notes branch, never an OoT song) under any rule frozen on in
 *  a paired world, and the addition that refuses a random wall a usable song could
 *  complete before its last note. Both false with the rules off. */
bool MM_WarpSongMM_WallRandomOnly(void);
bool MM_WarpSongMM_WallRejected(const uint8_t* buttons, uint8_t n);
/** z_message.c's SETUP_DISPLAY_SONG_PLAYED: point MM's playback at OoT's own notes
 *  of the song of staff state @p state (OoTMM sOcarinaCustomSongNotes), after
 *  AudioOcarina_SetPlaybackSong(1, 1). */
void MM_WarpSongMM_PointPlaybackAtSong(unsigned state);
/** z_message.c's song effect: OoTMM's OCEFF_WIPE5 (params = song) or, for the
 *  Prelude, OCEFF_WIPE (params 1). True when it handled @p state. */
bool MM_WarpSongMM_SpawnSongEffect(struct PlayState* play, unsigned state);
/** z_message.c's DISPLAY_SONG_PLAYED_TEXT_BEGIN: arms "You played the <song>!" in
 *  0x1B78's box and returns the custom text id to continue to. */
uint16_t MM_WarpSongMM_PlayedTextId(unsigned state);
/** z_message.c's TEXT_CLOSING: the prompt or a refusal for the song of @p state. */
void MM_WarpSongMM_OnSongPlayed(struct PlayState* play, unsigned state);
/** Player's OCARINA_MODE_WARP arm: whether the prompt's song is pending AND is the
 *  song this ocarina session played (msgCtx.lastPlayedSong). */
bool MM_WarpSongMM_HasPending(struct PlayState* play);
/** Takes the pending song (and arms the one-shot departure). */
bool MM_WarpSongMM_TakePending(struct PlayState* play, uint8_t* outSong);
/** Player's OCARINA_MODE_END: "No", a refusal, B. */
void MM_WarpSongMM_ClearPending(void);
/** En_Test7's params for @p song (0x8000 | song). */
uint16_t MM_WarpSongMM_EnTest7Params(uint8_t song);
/** Whether En_Test7 @p params are a warp song's; the song is params & 0xFF. */
bool MM_WarpSongMM_IsEnTest7Params(int16_t params);
/**
 * The departure (En_Test7's final action, SkipSoaringCutscene, Player's fallback):
 * ONE-SHOT per TakePending. The first call asks the gates again, arms the one-shot
 * warp target (MM, the current entrance as marker, OoT, the song's vanilla pad,
 * COMBO_SWITCH_KIND_WARP_SONG) and starts the transition to the marker, which
 * Play_UpdateTransition hands to Combo_CheckEntranceSwitch; a refusal reloads the
 * current entrance with one stderr line. Every later call is a silent no-op.
 */
void MM_WarpSongMM_Depart(struct PlayState* play, int song);

#ifdef __cplusplus
}
#endif

#endif // RSBS_SINGLE_EXECUTABLE
