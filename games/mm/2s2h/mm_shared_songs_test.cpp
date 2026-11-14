/**
 * @file mm_shared_songs_test.cpp
 * MM's half of the Shared Epona's Song / Shared Song of Storms lock (#1066; the
 * rules are RSBS_XITEM_SHARED_SONG_EPONA and RSBS_XITEM_SHARED_SONG_STORMS in
 * src/common/foreign_items.h).
 *
 * THE SHIM HELPERS the redship row `shared-songs-merge`
 * (src/common/tests/test_shared_songs.c) drives MM's REAL
 * MM_HarvestSharedResources / MM_ApplySharedResources through. They only put a
 * song's quest bit in a known state and read it back; every merge decision is
 * the production shims' and the registered family member's. A song is playable
 * on MM's ocarina exactly when its quest bit is set: free play builds the
 * available-song mask from the quest bits alone (z_message.c,
 * Message_DisplayOcarinaStaffImpl's CHECK_QUEST_ITEM(QUEST_SONG_SONATA + i)
 * loop, the #1070 finding). `song` is 0 for Epona's Song and 1 for the Song of
 * Storms.
 */

#include "global.h"

#if !defined(RSBS_SINGLE_EXECUTABLE)
// The shared songs are a single-exe surface; test_runner.cpp declares these
// unconditionally, so a standalone 2ship build links.
extern "C" void MM_SharedSongsTest_SetHeld(int song, int held) {
    (void)song;
    (void)held;
}
extern "C" int MM_SharedSongsTest_Held(int song) {
    (void)song;
    return -1;
}
extern "C" int MM_SharedSongsTest_PoolId(int song) {
    (void)song;
    return -1;
}
extern "C" void MM_SharedWarpSongsTest_SetHeld(int song, int held) {
    (void)song;
    (void)held;
}
extern "C" int MM_SharedWarpSongsTest_Held(int song) {
    (void)song;
    return -1;
}
extern "C" int MM_SharedWarpSongsTest_PoolId(int song) {
    (void)song;
    return -1;
}
extern "C" void MM_SharedWarpSongsTest_SetTitleScreen(void) {
}
extern "C" int MM_SharedSongsTest_OffVanillaCheckId(int song) {
    (void)song;
    return -1;
}
extern "C" int MM_SharedSongsTest_OffConvertSong(int song, int mmShuffleSongs) {
    (void)song;
    (void)mmShuffleSongs;
    return -1;
}
extern "C" uint16_t MM_SharedSongsTest_UnsharedSongId(void) {
    return 0;
}
#else

extern "C" {
#include "z64save.h"
extern SaveContext gSaveContext;
}

#include "2s2h/Rando/Rando.h"

#include "warp_songs_mm.h" // src/common: the ownership byte (#1169 XW10)

namespace {
int MMSongQuestBit(int song) {
    return (song == 0) ? (int)QUEST_SONG_EPONA : (int)QUEST_SONG_STORMS;
}
} // namespace

/** Teach (1) or forget (0) the song, as MM's own Item_Give(ITEM_SONG_*) writes
 *  it: its quest bit and nothing else (the heart-piece bits above are kept). */
extern "C" void MM_SharedSongsTest_SetHeld(int song, int held) {
    const uint32_t bit = 1u << MMSongQuestBit(song);
    if (held != 0) {
        gSaveContext.save.saveInfo.inventory.questItems |= bit;
    } else {
        gSaveContext.save.saveInfo.inventory.questItems &= ~bit;
    }
}

/** 1 when MM knows the song (its ocarina can play it), else 0. */
extern "C" int MM_SharedSongsTest_Held(int song) {
    return CHECK_QUEST_ITEM(MMSongQuestBit(song)) ? 1 : 0;
}

/** MM's pool id for the song: the RI_* its pool places it as. */
extern "C" int MM_SharedSongsTest_PoolId(int song) {
    return (song == 0) ? (int)RI_SONG_EPONA : (int)RI_SONG_STORMS;
}

// ---- #1169 XW10: the shared warp songs' MM members -----------------------------
//
// `song` is the warp song's index (src/common/warp_song_family.h): 0 MM's own Song
// of Soaring, owned through its quest bit; 1 .. 6 the Minuet of Forest (MM) .. the
// Prelude of Light (MM), owned through gComboCtx.mmWarpSongsOwned (bit song - 1).

extern "C" void MM_SharedWarpSongsTest_SetHeld(int song, int held) {
    if (song == 0) {
        const uint32_t bit = 1u << QUEST_SONG_SOARING;
        if (held != 0) {
            gSaveContext.save.saveInfo.inventory.questItems |= bit;
        } else {
            gSaveContext.save.saveInfo.inventory.questItems &= ~bit;
        }
    } else if (song >= 1 && song <= 6) {
        const uint8_t bit = (uint8_t)(1u << (song - 1));
        const uint8_t owned = Combo_MMWarpSongsOwned();
        Combo_MMWarpSongsSetOwned(held != 0 ? (uint8_t)(owned | bit) : (uint8_t)(owned & (uint8_t)~bit));
    }
}

extern "C" int MM_SharedWarpSongsTest_Held(int song) {
    if (song == 0) {
        return CHECK_QUEST_ITEM(QUEST_SONG_SOARING) ? 1 : 0;
    }
    if (song >= 1 && song <= 6) {
        return (Combo_MMWarpSongsOwned() & (1u << (song - 1))) != 0 ? 1 : 0;
    }
    return -1;
}

/** MM's id for the song: the RI_* its pool places (or would place) it as. */
extern "C" int MM_SharedWarpSongsTest_PoolId(int song) {
    if (song == 0) {
        return (int)RI_SONG_SOARING;
    }
    return (song >= 1 && song <= 6) ? (int)RI_SONG_MINUET + song - 1 : -1;
}

/** MM's live save under its title screen, where MM plays real frames with the
 *  bootstrap save resident (MM_SaveIsLiveFile refuses it). */
extern "C" void MM_SharedWarpSongsTest_SetTitleScreen(void) {
    gSaveContext.gameMode = GAMEMODE_TITLE_SCREEN;
}

// ---- #1561: Shuffle Songs Off vanilla checks and junk give ----------------------
//
// `song` is 0 for Epona's Song, 1 for the Song of Storms, and 2 for the Song of Time.

namespace {
static bool MMSharedSongsOffValid(int song) {
    return song >= 0 && song <= 2;
}

static RandoCheckId MMSharedSongsOffCheck(int song) {
    if (song == 0) {
        return RC_ROMANI_RANCH_EPONAS_SONG;
    }
    if (song == 1) {
        return RC_BENEATH_THE_GRAVEYARD_SONG_OF_STORMS;
    }
    return RC_CLOCK_TOWER_ROOF_SONG_OF_TIME;
}

static RandoItemId MMSharedSongsOffItem(int song) {
    if (song == 0) {
        return RI_SONG_EPONA;
    }
    if (song == 1) {
        return RI_SONG_STORMS;
    }
    return RI_SONG_TIME;
}
} // namespace

/** MM's default check id for @p song (0 Epona, 1 Storms, 2 Time), or -1. */
extern "C" int MM_SharedSongsTest_OffVanillaCheckId(int song) {
    if (!MMSharedSongsOffValid(song)) {
        return -1;
    }
    return (int)MMSharedSongsOffCheck(song);
}

/** 1 when Rando::ConvertItem and Rando::UncollectedNamedItem both convert @p song
 *  at its MM vanilla check to RI_JUNK, 0 when both keep the song, -1 on mismatch.
 *  When @p mmShuffleSongs >= 0, RANDO_SAVE_OPTIONS[RO_SHUFFLE_SONGS] is set to it
 *  for the call and restored before returning. */
extern "C" int MM_SharedSongsTest_OffConvertSong(int song, int mmShuffleSongs) {
    if (!MMSharedSongsOffValid(song)) {
        return -1;
    }
    const RandoCheckId rc = MMSharedSongsOffCheck(song);
    const RandoItemId ri = MMSharedSongsOffItem(song);
    const uint32_t prior = RANDO_SAVE_OPTIONS[RO_SHUFFLE_SONGS];
    if (mmShuffleSongs >= 0) {
        RANDO_SAVE_OPTIONS[RO_SHUFFLE_SONGS] = (uint32_t)mmShuffleSongs;
    }
    const RandoItemId converted = Rando::ConvertItem(ri, rc);
    const RandoItemId named = Rando::UncollectedNamedItem(ri, rc);
    RANDO_SAVE_OPTIONS[RO_SHUFFLE_SONGS] = prior;
    if (converted == RI_JUNK && named == RI_JUNK) {
        return 1;
    }
    if (converted == ri && named == ri) {
        return 0;
    }
    return -1;
}

/** An MM song no shared rule names, for negative junk-give checks. */
extern "C" uint16_t MM_SharedSongsTest_UnsharedSongId(void) {
    return (uint16_t)RI_SONG_SONATA;
}

#endif
