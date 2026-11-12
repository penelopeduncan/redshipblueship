/**
 * @file combo_song_pool.c
 * @brief The pair's one songs setting (#1542 D2). See combo_song_pool.h.
 */

#include "combo_song_pool.h"

#include <stddef.h>

/** OoT's finalized Shuffle Songs between the creation freeze and the completion,
 *  or -1 when none. RAM only, by design (combo_song_pool.h). */
static int16_t sPairSongsCapture = -1;

uint32_t Combo_PairSongsMmValue(uint32_t ootValue) {
    switch (ootValue) {
        case RSBS_OOT_SONGS_OFF:
            return RSBS_MM_SONGS_VANILLA;
        case RSBS_OOT_SONGS_ANYWHERE:
            return RSBS_MM_SONGS_ANYWHERE;
        case RSBS_OOT_SONGS_SONG_LOCATIONS:
        case RSBS_OOT_SONGS_DUNGEON_REWARDS:
        default:
            return RSBS_MM_SONGS_SONG_LOCATIONS;
    }
}

void Combo_PairSongsCapture(uint32_t ootValue) {
    sPairSongsCapture = (int16_t)(ootValue > 255u ? 255u : ootValue);
}

bool Combo_PairSongsCaptured(uint32_t* out) {
    if (sPairSongsCapture < 0) {
        return false;
    }
    if (out != NULL) {
        *out = (uint32_t)sPairSongsCapture;
    }
    return true;
}

void Combo_PairSongsClear(void) {
    sPairSongsCapture = -1;
}

int16_t Combo_PairSongsSnapshot(void) {
    return sPairSongsCapture;
}

void Combo_PairSongsRestore(int16_t snapshot) {
    sPairSongsCapture = snapshot < 0 ? (int16_t)-1 : (int16_t)(snapshot > 255 ? 255 : snapshot);
}

uint32_t Combo_PairSongsOoTValue(uint32_t ootKeyValue) {
    uint32_t captured = 0u;
    return Combo_PairSongsCaptured(&captured) ? captured : ootKeyValue;
}
