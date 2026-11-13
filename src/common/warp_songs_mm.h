#pragma once
/**
 * @file warp_songs_mm.h
 * @brief Ocarina of Time's six warp songs as Majora's Mask items (#1169 lane
 *        XW5; OoTMM songMinuetMm, songBoleroMm, songSerenadeMm, songRequiemMm,
 *        songNocturneMm, songPreludeMm): the game-neutral rules and the ownership
 *        byte. Game-header-free.
 *
 * THE ITEMS. Under its frozen second-word rule (RSBS_XITEM2_SONG_MINUET_MM ...,
 * foreign_items.h) Majora's Mask's pool gains one copy of that song, an MM item
 * named as OoTMM names it ("Minuet of Forest (MM)" ...), RI_SONG_MINUET ..
 * RI_SONG_PRELUDE appended before RI_MAX_TRAP (games/mm/2s2h/Rando/Types.h,
 * WarpSongsMM.cpp). Under the shared rule (RSBS_XITEM2_SHARED_SONG_MINUET ...,
 * which requires the extension) OoTMM replaces Ocarina of Time's copy with one
 * shared item named without the suffix ("Minuet of Forest") and adds no MM copy
 * (transform.ts), so MM pools none: the host's native copy is the shared row, and
 * the two songs are the family's two members (lane XW10, warp_song_family.h).
 * Combo_MMWarpSongCopies is that rule, one place, the twin of XW4's
 * Combo_SongSoaringOoTCopies.
 *
 * OWNERSHIP is one bit per song in gComboCtx.mmWarpSongsOwned (context.h, ADR 0009
 * claim 21), in OoTMM's order, each one bit below its rule bit. The reasons it is
 * there and not in MM's half, why it is not in the invalidation KEEP set and why
 * it is not folded into the fingerprint are the field's comment.
 *
 * WHAT OWNING ONE DOES (lane XW7, mm_warp_song_play.h): played on the ocarina in
 * Termina it is matched, "You played" shows, the warp prompt offers OK, and OK
 * crosses to Ocarina of Time as a child, landing where the creation's warp-song
 * record says the song leads (its pad unless the warp songs were shuffled). Both
 * games' logic know the crossing through lane XW9's travel facts; MM's logic plays
 * each song with OoTMM's can_play_tp_* button sets (Logic.h,
 * CAN_PLAY_OOT_WARP_SONG).
 *
 * OFFERED SINCE XW13. The six rules have Cross-Game Songs rows, off by default,
 * drawn since the integration row IntMMWarpSongMinuet played the Minuet in
 * Majora's Mask and arrived in Ocarina of Time. With every one off no world pools
 * one of these items, and the ownership byte stays 0.
 */

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Ownership bits in gComboCtx.mmWarpSongsOwned, in OoTMM's order. Each is its
 *  rule bit (RSBS_XITEM2_SONG_*_MM, 0x02 .. 0x40) shifted down by one. PINNED:
 *  .redsave and seed-package format. */
#define RSBS_MM_WARP_SONG_OWNED_MINUET 0x01u
#define RSBS_MM_WARP_SONG_OWNED_BOLERO 0x02u
#define RSBS_MM_WARP_SONG_OWNED_SERENADE 0x04u
#define RSBS_MM_WARP_SONG_OWNED_REQUIEM 0x08u
#define RSBS_MM_WARP_SONG_OWNED_NOCTURNE 0x10u
#define RSBS_MM_WARP_SONG_OWNED_PRELUDE 0x20u
/** Every allocated ownership bit; bits 6 and 7 are unallocated and stay zero. */
#define RSBS_MM_WARP_SONG_OWNED_ALL 0x3Fu
/** How many warp songs Majora's Mask can host. */
#define RSBS_MM_WARP_SONG_COUNT 6u

/**
 * The six MM rows' tooltips: OoTMM's descriptions (data.ts songMinuetMm ..
 * songPreludeMm: "Add the <song> in Majora's Mask. If Songs are on Song
 * Locations, you must share or start with at least one song") in the page's own
 * verb-first form. Since #1542 an MM warp song under Shuffle Songs: Song
 * Locations is one more row of the shared song pool of both games
 * (MM_ComboLogic_IsPairSongItem), 23 checks for 22 songs with the defaults, so
 * one added song fits and a second needs a shared or starting song (when
 * Majora's Mask's songs stay home, Crossing Direction Off or OoT Items to MM, a
 * Majora's Mask one: SongPoolOverCapacity legs (e2) and (f)), while an added
 * Song of Soaring (OoT) already takes the spare check: the warning
 * says "when", not OoTMM's "must". Under the other modes an MM warp song is a
 * general-pool item and never refused. The way out names only remedies this
 * build offers (Shuffle Songs: Anywhere, or a starting song): the Shared rows
 * stay hidden until XW10, which restores OoTMM's "share a song". The
 * Cross-Game Songs rows draw them (SohMenuCombo.cpp).
 */
#define RSBS_MM_WARP_SONG_TOOLTIP_MINUET                                                            \
    "Adds Ocarina of Time's Minuet of Forest to Majora's Mask. On Shuffle Songs: Song Locations, "  \
    "Generate refuses when the songs outnumber the song checks they can use: set Shuffle Songs to " \
    "Anywhere or start with a song."
#define RSBS_MM_WARP_SONG_TOOLTIP_BOLERO                                                            \
    "Adds Ocarina of Time's Bolero of Fire to Majora's Mask. On Shuffle Songs: Song Locations, "    \
    "Generate refuses when the songs outnumber the song checks they can use: set Shuffle Songs to " \
    "Anywhere or start with a song."
#define RSBS_MM_WARP_SONG_TOOLTIP_SERENADE                                                          \
    "Adds Ocarina of Time's Serenade of Water to Majora's Mask. On Shuffle Songs: Song Locations, " \
    "Generate refuses when the songs outnumber the song checks they can use: set Shuffle Songs to " \
    "Anywhere or start with a song."
#define RSBS_MM_WARP_SONG_TOOLTIP_REQUIEM                                                           \
    "Adds Ocarina of Time's Requiem of Spirit to Majora's Mask. On Shuffle Songs: Song Locations, " \
    "Generate refuses when the songs outnumber the song checks they can use: set Shuffle Songs to " \
    "Anywhere or start with a song."
#define RSBS_MM_WARP_SONG_TOOLTIP_NOCTURNE                                                           \
    "Adds Ocarina of Time's Nocturne of Shadow to Majora's Mask. On Shuffle Songs: Song Locations, " \
    "Generate refuses when the songs outnumber the song checks they can use: set Shuffle Songs to "  \
    "Anywhere or start with a song."
#define RSBS_MM_WARP_SONG_TOOLTIP_PRELUDE                                                           \
    "Adds Ocarina of Time's Prelude of Light to Majora's Mask. On Shuffle Songs: Song Locations, "  \
    "Generate refuses when the songs outnumber the song checks they can use: set Shuffle Songs to " \
    "Anywhere or start with a song."

/** The ownership bit of extension rule @p ruleBit (RSBS_XITEM2_SONG_MINUET_MM ..
 *  RSBS_XITEM2_SONG_PRELUDE_MM), or 0 for any other bit or mask. Pure. */
uint8_t Combo_MMWarpSongOwnedBit(uint32_t ruleBit);

/** The extension rule bit of ownership bit @p ownedBit, or 0 for any other bit
 *  or mask. Pure. */
uint32_t Combo_MMWarpSongRuleBit(uint8_t ownedBit);

/**
 * How many copies of the MM song whose extension rule is @p ruleBit a world whose
 * frozen second rules word is @p rules2 puts into Majora's Mask's pool: 1 with the
 * extension on and its shared song off; 0 with the extension off, and 0 with the
 * shared song on (OoTMM adds no copy under the shared rule; OoT's native copy is
 * the shared row).
 * 0 for any bit that is not one of the six extensions. Pure.
 */
uint32_t Combo_MMWarpSongCopies(uint32_t rules2, uint32_t ruleBit);

/** The MM item's name ("Minuet of Forest (MM)" ...; OoTMM's option name), or NULL
 *  for any other bit. */
const char* Combo_MMWarpSongName(uint32_t ruleBit);

/** The Ocarina of Time item's own name ("Minuet of Forest" ...), the name its
 *  get-item model is asked by, or NULL for any other bit. */
const char* Combo_MMWarpSongOoTName(uint32_t ruleBit);

/** The MM row's tooltip (RSBS_MM_WARP_SONG_TOOLTIP_*), or NULL for any other bit. */
const char* Combo_MMWarpSongTooltip(uint32_t ruleBit);

/** The ownership byte, allocated bits only. */
uint8_t Combo_MMWarpSongsOwned(void);

/** Is the song whose extension rule is @p ruleBit owned? The bit alone, whatever
 *  the rules say; false for any other bit. */
bool Combo_MMWarpSongOwned(uint32_t ruleBit);

/** Own the song whose extension rule is @p ruleBit (MM's give). No-op for any
 *  other bit. */
void Combo_MMWarpSongGive(uint32_t ruleBit);

/** Replace the ownership byte with @p owned (allocated bits only): a new MM file
 *  (0), and every snapshot bracket that restores MM's save after a simulated
 *  playthrough gave items into it. */
void Combo_MMWarpSongsSetOwned(uint8_t owned);

#ifdef __cplusplus
}
#endif
