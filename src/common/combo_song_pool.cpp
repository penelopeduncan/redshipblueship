/**
 * @file combo_song_pool.cpp
 * @brief The pair's one songs setting (#1542 D2). See combo_song_pool.h.
 */

#include "combo_song_pool.h"

#include <atomic>
#include <cstddef>

namespace {

/** OoT's finalized Shuffle Songs between the creation freeze and the completion,
 *  or -1 when none. RAM only, by design (combo_song_pool.h). ATOMIC because two
 *  threads touch it: Playthrough_Init writes it on Generate's worker, and MM's
 *  options page reads it on the game thread (MM_Rando_PairedSongsShown) while
 *  that worker runs. Relaxed order is enough: no other data is published through
 *  it, and the completion that consumes it runs on the game thread after the
 *  worker is joined (OoT_SeedPackage_FileSelectTick). */
std::atomic<int16_t> sPairSongsCapture{ -1 };

} // namespace

extern "C" uint32_t Combo_PairSongsMmValue(uint32_t ootValue) {
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

extern "C" void Combo_PairSongsCapture(uint32_t ootValue) {
    sPairSongsCapture.store((int16_t)(ootValue > 255u ? 255u : ootValue), std::memory_order_relaxed);
}

extern "C" bool Combo_PairSongsCaptured(uint32_t* out) {
    const int16_t held = sPairSongsCapture.load(std::memory_order_relaxed);
    if (held < 0) {
        return false;
    }
    if (out != nullptr) {
        *out = (uint32_t)held;
    }
    return true;
}

extern "C" void Combo_PairSongsClear(void) {
    sPairSongsCapture.store((int16_t)-1, std::memory_order_relaxed);
}

extern "C" int16_t Combo_PairSongsSnapshot(void) {
    return sPairSongsCapture.load(std::memory_order_relaxed);
}

extern "C" void Combo_PairSongsRestore(int16_t snapshot) {
    sPairSongsCapture.store(snapshot < 0 ? (int16_t)-1 : (int16_t)(snapshot > 255 ? 255 : snapshot),
                            std::memory_order_relaxed);
}

extern "C" uint32_t Combo_PairSongsOoTValue(uint32_t ootKeyValue) {
    uint32_t captured = 0u;
    return Combo_PairSongsCaptured(&captured) ? captured : ootKeyValue;
}
