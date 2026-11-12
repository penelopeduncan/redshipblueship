/**
 * @file warp_song_items.c
 * @brief The cross-game warp songs' game-neutral item rules (#1169 lane XW4).
 *        The reasoning is in warp_song_items.h.
 */

#include "warp_song_items.h"

#include "foreign_items.h" // RSBS_XITEM2_SONG_SOARING_OOT, RSBS_XITEM2_SHARED_SONG_SOARING

#include <stdio.h>
#include <string.h>

uint32_t Combo_SongSoaringOoTCopies(uint32_t rules2) {
    if ((rules2 & (uint32_t)RSBS_XITEM2_SONG_SOARING_OOT) == 0u) {
        return 0u; // OoTMM songSoaringOot off: OoT never gains the item
    }
    if ((rules2 & (uint32_t)RSBS_XITEM2_SHARED_SONG_SOARING) != 0u) {
        // OoTMM sharedSongSoaring: Majora's Mask's copy becomes the one shared
        // item (transform.ts adds no OoT copy). The family is lane XW10's; OoT's
        // separate copy is held back, never placed.
        return 0u;
    }
    return 1u;
}

bool Combo_SongSoaringOoTRoamsUnderOff(uint32_t shuffleSongs, uint32_t rules2) {
    return shuffleSongs == (uint32_t)RSBS_SONG_SPOTS_OFF && Combo_SongSoaringOoTCopies(rules2) > 0u;
}

uint32_t Combo_SongSpotsOverflow(uint32_t songs, uint32_t spots, uint32_t mode, bool soaringOoT, char* msg,
                                 size_t len) {
    const uint32_t over = songs > spots ? songs - spots : 0u;
    if (msg == NULL || len == 0) {
        return over;
    }
    msg[0] = '\0';
    if (over == 0u) {
        return 0u;
    }
    const char* modeName = mode == RSBS_SONG_SPOTS_DUNGEON_REWARDS ? "Dungeon Rewards" : "Song Locations";
    if (soaringOoT) {
        snprintf(msg, len,
                 "%s: %u songs for the %u spots of Shuffle Songs: %s. The Song of Soaring (OoT) is a song too: share "
                 "a song or start with an Ocarina of Time song, or shuffle songs Anywhere",
                 RSBS_SONG_SPOTS_REFUSAL, (unsigned)songs, (unsigned)spots, modeName);
    } else {
        snprintf(msg, len, "%s: %u songs for the %u spots of Shuffle Songs: %s", RSBS_SONG_SPOTS_REFUSAL,
                 (unsigned)songs, (unsigned)spots, modeName);
    }
    return over;
}

static char sSongSpotsLastRefusal[512];

void Combo_SongSpotsRecordRefusal(const char* msg) {
    if (msg == NULL) {
        sSongSpotsLastRefusal[0] = '\0';
        return;
    }
    snprintf(sSongSpotsLastRefusal, sizeof(sSongSpotsLastRefusal), "%s", msg);
}

const char* Combo_SongSpotsLastRefusal(void) {
    return sSongSpotsLastRefusal;
}

// #1169 XW12 (D4): why the OoT generation refused, from the worker to the game
// thread. The ordering argument is in warp_song_items.h.
static int sGenRefusalNote = RSBS_GEN_REFUSAL_NONE;

void Combo_GenRefusal_Note(int kind) {
    sGenRefusalNote = kind;
}

int Combo_GenRefusal_Take(void) {
    const int kind = sGenRefusalNote;
    sGenRefusalNote = RSBS_GEN_REFUSAL_NONE;
    return kind;
}

const char* Combo_GenRefusal_Line(int kind) {
    switch (kind) {
        case RSBS_GEN_REFUSAL_SONG_SPOTS:
            return "too many songs for the song spots.";
        case RSBS_GEN_REFUSAL_HIDDEN_RULE:
            return "a rule needs a later build.";
        default:
            return NULL;
    }
}
