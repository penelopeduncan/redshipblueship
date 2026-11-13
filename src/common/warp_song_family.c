/**
 * @file warp_song_family.c
 * @brief The seven shared warp songs' pure rules (#1169 lane XW10). The reasoning
 *        is in warp_song_family.h.
 */

#include "warp_song_family.h"

#include "context.h"       // GAME_OOT, GAME_MM, GAME_NONE, RSBS_CTX_STATIC_ASSERT
#include "foreign_items.h" // RSBS_XITEM2_SONG_*, RSBS_XITEM2_SHARED_SONG_*, RSBS_XITEM2_WARP_SONG_SHARED_SHIFT

#include <stddef.h>

RSBS_CTX_STATIC_ASSERT(RSBS_WARP_SONG_COUNT == 7 && (1u << RSBS_WARP_SONG_SOARING) == RSBS_XITEM2_SONG_SOARING_OOT &&
                           (1u << RSBS_WARP_SONG_MINUET) == RSBS_XITEM2_SONG_MINUET_MM &&
                           (1u << RSBS_WARP_SONG_PRELUDE) == RSBS_XITEM2_SONG_PRELUDE_MM &&
                           ((1u << RSBS_WARP_SONG_COUNT) - 1u) == RSBS_XITEM2_WARP_SONG_EXTENSIONS,
                       "a warp song's index is its extension rule's bit index (#1169 XW10)");

static bool WarpSongInRange(int song) {
    return song >= 0 && song < (int)RSBS_WARP_SONG_COUNT;
}

uint32_t Combo_WarpSongExtensionBit(int song) {
    return WarpSongInRange(song) ? (1u << (unsigned)song) : 0u;
}

uint32_t Combo_WarpSongSharedBit(int song) {
    return WarpSongInRange(song) ? (1u << ((unsigned)song + RSBS_XITEM2_WARP_SONG_SHARED_SHIFT)) : 0u;
}

int Combo_WarpSongHost(int song) {
    if (!WarpSongInRange(song)) {
        return (int)GAME_NONE;
    }
    // The Song of Soaring is Majora's Mask's own song; the six are Ocarina of Time's.
    return song == (int)RSBS_WARP_SONG_SOARING ? (int)GAME_MM : (int)GAME_OOT;
}

uint8_t Combo_WarpSongHeldMMBits(uint8_t mmWarpSongsOwned, bool mmSoaring) {
    // The ownership byte holds Minuet .. Prelude at 0x01 .. 0x20 (warp_songs_mm.h);
    // here they are songs 1 .. 6.
    return (uint8_t)((((unsigned)mmWarpSongsOwned & 0x3Fu) << 1) | (mmSoaring ? 1u : 0u));
}

bool Combo_WarpSongHeld(const ComboWarpSongHoldings* h, int game, int song) {
    if (h == NULL || !WarpSongInRange(song) || (game != (int)GAME_OOT && game != (int)GAME_MM)) {
        return false;
    }
    const uint32_t ext = Combo_WarpSongExtensionBit(song);
    const uint32_t shared = Combo_WarpSongSharedBit(song);
    const int host = Combo_WarpSongHost(song);
    if (game != host && (h->rules2 & ext) == 0u) {
        return false; // the game the extension adds the song to has no such song
    }
    const uint8_t songBit = (uint8_t)(1u << (unsigned)song);
    const uint8_t own = game == (int)GAME_OOT ? h->heldOoT : h->heldMM;
    const uint8_t other = game == (int)GAME_OOT ? h->heldMM : h->heldOoT;
    if ((own & songBit) != 0u) {
        return true;
    }
    // OoTMM's has(SHARED_SONG_X): under the shared rule (which requires the
    // extension, Combo_ComboItemRules2Valid) one item is the song in both games.
    if ((h->rules2 & shared) != 0u && (h->rules2 & ext) != 0u) {
        return (h->owned2 & shared) != 0u || (other & songBit) != 0u;
    }
    return false;
}
