#pragma once
/**
 * @file warp_song_family.h
 * @brief The seven SHARED warp songs (#1169 lane XW10; OoTMM sharedSongSoaring,
 *        sharedSongMinuet .. sharedSongPrelude): which game hosts each song, and
 *        the one question every player-facing reader asks, "does Link hold this
 *        song in this game?". Game-header-free.
 *
 * THE MODEL (OoTMM packages/logic/src/world/transform.ts, the Soaring branch and
 * the six warp songs' branch). A shared song REPLACES the host game's native copy
 * with one SHARED item and adds NO copy to the other game: `replaceItem(MM_SONG_SOARING,
 * SHARED_SONG_SOARING)` for the Song of Soaring (hosted by Majora's Mask), and
 * `replaceItem(OOT_SONG_TP_FOREST, SHARED_SONG_TP_FOREST)` .. for the six (hosted by
 * Ocarina of Time). Here the host game's native copy becomes the shared family's
 * row (combo_single_bag.c pairs it; the composer turns a one-sided family into the
 * shared row and trims nothing), the added copy stays out of the pool
 * (Combo_SongSoaringOoTCopies / Combo_MMWarpSongCopies answer 0 under the shared
 * rule), and both games' copies are the family's two members
 * (Combo_RegisterSharedFamily2Member), owned in gComboCtx.sharedFamilyOwned2.
 *
 * WHO HOLDS IT. OoTMM's `has_song_X = has(SONG_X) || has(SHARED_SONG_X)`
 * (data/macros/macros_common.yml): under the shared rule one item is the song in
 * both games. Combo_WarpSongHeld answers that for the players' readers (the
 * matchers, the prompt, the trackers): the game's own copy, or, under the shared
 * rule, the family owned or the other game's copy held. It is right in a session
 * that booted either game first and after a reload, because the family word rides
 * Tier-1 and the running game's own copy is live.
 *
 * NEVER FOR LOGIC. A logic engine must not read the family word: at Generate the
 * session fields may still hold an earlier file's values. Each engine credits its
 * own half (the host through `place`, the far engine through the coordinator's
 * exchange gate), and MM_WarpSongMM_Usable stays own-copy only.
 */

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** The seven warp songs, by their EXTENSION rule's bit index in the second rules
 *  word (RSBS_XITEM2_SONG_SOARING_OOT is bit 0, RSBS_XITEM2_SONG_MINUET_MM bit 1,
 *  ... RSBS_XITEM2_SONG_PRELUDE_MM bit 6). The shared rule of song s is bit s + 7. */
typedef enum {
    RSBS_WARP_SONG_SOARING = 0,
    RSBS_WARP_SONG_MINUET = 1,
    RSBS_WARP_SONG_BOLERO = 2,
    RSBS_WARP_SONG_SERENADE = 3,
    RSBS_WARP_SONG_REQUIEM = 4,
    RSBS_WARP_SONG_NOCTURNE = 5,
    RSBS_WARP_SONG_PRELUDE = 6,
    RSBS_WARP_SONG_COUNT = 7
} RsbsWarpSong;

/** The extension rule of @p song (RSBS_XITEM2_SONG_*), 0 for a bad index. Pure. */
uint32_t Combo_WarpSongExtensionBit(int song);

/** The shared rule of @p song (RSBS_XITEM2_SHARED_SONG_*), 0 for a bad index. Pure. */
uint32_t Combo_WarpSongSharedBit(int song);

/** The game whose NATIVE copy is the song (the shared family's row): Majora's Mask
 *  for the Song of Soaring, Ocarina of Time for the six. GAME_NONE for a bad index.
 *  The other game is the one the extension ADDS the song to. Pure. */
int Combo_WarpSongHost(int song);

/**
 * What a reader knows about one moment (all passed in, so the answer is pure):
 *
 *   rules2  the FROZEN second rules word (Combo_ComboCrossItemFlags2).
 *   owned2  the second family word (gComboCtx.sharedFamilyOwned2).
 *   heldOoT bit s: OoT's own copy of song s, as OoT can read it NOW: bit 0 is
 *           RAND_INF_OBTAINED_SONG_OF_SOARING_OOT, bits 1-6 QUEST_SONG_MINUET ..
 *           QUEST_SONG_PRELUDE. Readable only while OoT runs: pass 0 otherwise.
 *   heldMM  bit s: MM's own copy of song s: bit 0 is MM's QUEST_SONG_SOARING
 *           (readable only while MM runs, else 0), bits 1-6 the ownership byte
 *           gComboCtx.mmWarpSongsOwned shifted UP by one (Minuet is 0x01 there,
 *           bit 1 here), which is always readable: Combo_WarpSongHeldMMBits.
 */
typedef struct {
    uint32_t rules2;
    uint32_t owned2;
    uint8_t heldOoT;
    uint8_t heldMM;
} ComboWarpSongHoldings;

/** heldMM for @p mmWarpSongsOwned (the ownership byte, Minuet 0x01 .. Prelude 0x20)
 *  and MM's own Song of Soaring: `(owned << 1) | soaring`, allocated bits only. Pure. */
uint8_t Combo_WarpSongHeldMMBits(uint8_t mmWarpSongsOwned, bool mmSoaring);

/**
 * Does Link hold @p song in @p game (GAME_OOT or GAME_MM)?
 *   1. false for a bad game or song;
 *   2. false when @p game is the one the extension ADDS the song to and the
 *      extension is not frozen on (that game has no such song);
 *   3. true when @p game's own copy is held;
 *   4. else, under the song's shared rule: the family owned (owned2) or the OTHER
 *      game's copy held;
 *   5. else false.
 * With the shared rule off this is exactly "the game's own copy" (gated by rule 2),
 * the answer every reader gave before XW10. Pure.
 */
bool Combo_WarpSongHeld(const ComboWarpSongHoldings* h, int game, int song);

#ifdef __cplusplus
}
#endif
