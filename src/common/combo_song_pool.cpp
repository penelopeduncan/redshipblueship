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

/** Combo_SongHostsForceForTest's override: -1 none, 0 off, 1 on. RAM only, never
 *  a CVar (#1542 S0). Atomic for the same reason as the capture: a test row on
 *  the game thread and a generation worker may both read it. */
std::atomic<int8_t> sSongHostsOverride{ -1 };

/** The S0 verdicts, one row per RsbsSongCheck (the traces are in the S0 pull
 *  request and in each engine's host-class comment). */
const RsbsSongCheckInfo kSongCheckInfo[RSBS_SONG_CHECK_COUNT] = {
    { RSBS_SONG_CHECK_GAME_OOT, RSBS_SONG_HOST_ALLOW, RSBS_SONG_GIVE_OOT_ACTOR_FLAG, "Song from Saria" },
    { RSBS_SONG_CHECK_GAME_OOT, RSBS_SONG_HOST_ALLOW, RSBS_SONG_GIVE_OOT_ACTOR_FLAG, "Sheik in Forest" },
    { RSBS_SONG_CHECK_GAME_OOT, RSBS_SONG_HOST_ALLOW, RSBS_SONG_GIVE_OOT_ACTOR_FLAG, "Song from Malon" },
    { RSBS_SONG_CHECK_GAME_OOT, RSBS_SONG_HOST_ALLOW, RSBS_SONG_GIVE_OOT_TRIGGER_FLAG, "Song from Ocarina of Time" },
    { RSBS_SONG_CHECK_GAME_OOT, RSBS_SONG_HOST_ALLOW_UNLESS_SKIP_CHILD_ZELDA, RSBS_SONG_GIVE_OOT_ACTOR_FLAG,
      "Song from Impa" },
    { RSBS_SONG_CHECK_GAME_OOT, RSBS_SONG_HOST_ALLOW, RSBS_SONG_GIVE_OOT_ACTOR_FLAG, "Song from Royal Family's Tomb" },
    { RSBS_SONG_CHECK_GAME_OOT, RSBS_SONG_HOST_ALLOW, RSBS_SONG_GIVE_OOT_ACTOR_FLAG, "Song from Windmill" },
    { RSBS_SONG_CHECK_GAME_OOT, RSBS_SONG_HOST_ALLOW, RSBS_SONG_GIVE_OOT_TRIGGER_FLAG, "Sheik in Kakariko" },
    { RSBS_SONG_CHECK_GAME_OOT, RSBS_SONG_HOST_ALLOW, RSBS_SONG_GIVE_OOT_ACTOR_FLAG, "Sheik in Crater" },
    { RSBS_SONG_CHECK_GAME_OOT, RSBS_SONG_HOST_ALLOW, RSBS_SONG_GIVE_OOT_ACTOR_FLAG, "Sheik in Ice Cavern" },
    { RSBS_SONG_CHECK_GAME_OOT, RSBS_SONG_HOST_ALLOW, RSBS_SONG_GIVE_OOT_TRIGGER_FLAG, "Sheik at Colossus" },
    { RSBS_SONG_CHECK_GAME_OOT, RSBS_SONG_HOST_ALLOW, RSBS_SONG_GIVE_OOT_ACTOR_FLAG, "Sheik at Temple" },
    { RSBS_SONG_CHECK_GAME_MM, RSBS_SONG_HOST_ALLOW, RSBS_SONG_GIVE_MM_CREATION, "Starting Song of Healing" },
    { RSBS_SONG_CHECK_GAME_MM, RSBS_SONG_HOST_ALLOW, RSBS_SONG_GIVE_MM_REARMS, "Clock Tower Roof Song of Time" },
    { RSBS_SONG_CHECK_GAME_MM, RSBS_SONG_HOST_ALLOW, RSBS_SONG_GIVE_MM_ONCE, "Romani Ranch Epona's Song" },
    { RSBS_SONG_CHECK_GAME_MM, RSBS_SONG_HOST_ALLOW, RSBS_SONG_GIVE_MM_REARMS, "Southern Swamp Song of Soaring" },
    { RSBS_SONG_CHECK_GAME_MM, RSBS_SONG_HOST_ALLOW, RSBS_SONG_GIVE_MM_REARMS, "Deku King's Chamber Sonata" },
    { RSBS_SONG_CHECK_GAME_MM, RSBS_SONG_HOST_ALLOW, RSBS_SONG_GIVE_MM_REARMS, "Goron Elder Lullaby Intro" },
    { RSBS_SONG_CHECK_GAME_MM, RSBS_SONG_HOST_ALLOW, RSBS_SONG_GIVE_MM_REARMS, "Goron Shrine Lullaby" },
    { RSBS_SONG_CHECK_GAME_MM, RSBS_SONG_HOST_ALLOW, RSBS_SONG_GIVE_MM_REARMS, "Marine Lab New Wave Bossa Nova" },
    { RSBS_SONG_CHECK_GAME_MM, RSBS_SONG_HOST_ALLOW, RSBS_SONG_GIVE_MM_REARMS, "Beneath the Graveyard Song of Storms" },
    { RSBS_SONG_CHECK_GAME_MM, RSBS_SONG_HOST_ALLOW, RSBS_SONG_GIVE_MM_REARMS, "Ikana Castle Elegy of Emptiness" },
    { RSBS_SONG_CHECK_GAME_MM, RSBS_SONG_HOST_ALLOW, RSBS_SONG_GIVE_MM_REARMS, "Giants' Chamber Oath to Order" },
};

static_assert(RSBS_SONG_CHECK_COUNT == RSBS_SONG_CHECK_OOT_COUNT + RSBS_SONG_CHECK_MM_COUNT,
              "the song checks are OoT's 12 then MM's 11 (OoTMM's list)");
static_assert(RSBS_SONG_CHECK_FIRST_MM == RSBS_SONG_CHECK_OOT_COUNT, "MM's song checks follow OoT's");

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

extern "C" const RsbsSongCheckInfo* Combo_SongCheckInfo(int songCheck) {
    if (songCheck < 0 || songCheck >= RSBS_SONG_CHECK_COUNT) {
        return nullptr;
    }
    return &kSongCheckInfo[songCheck];
}

extern "C" bool Combo_SongHostsArmed(uint32_t frozenOoTSongs) {
    // S0: dormant. S3 replaces the fall-through with the read of the frozen
    // value that arms the union (combo_song_pool.h, design decision 7).
    (void)frozenOoTSongs;
    const int8_t forced = sSongHostsOverride.load(std::memory_order_relaxed);
    if (forced >= 0) {
        return forced != 0;
    }
    return false;
}

extern "C" void Combo_SongHostsForceForTest(int value) {
    sSongHostsOverride.store((int8_t)(value < 0 ? -1 : (value != 0 ? 1 : 0)), std::memory_order_relaxed);
}

extern "C" int Combo_SongHostsTestOverride(void) {
    return (int)sSongHostsOverride.load(std::memory_order_relaxed);
}

extern "C" bool Combo_SongCheckAcceptsForeign(int songCheck, uint32_t frozenOoTSongs, bool frozenSkipChildZelda) {
    const RsbsSongCheckInfo* info = Combo_SongCheckInfo(songCheck);
    if (info == nullptr || !Combo_SongHostsArmed(frozenOoTSongs)) {
        return false;
    }
    switch (info->rule) {
        case RSBS_SONG_HOST_ALLOW:
            return true;
        case RSBS_SONG_HOST_ALLOW_UNLESS_SKIP_CHILD_ZELDA:
            return !frozenSkipChildZelda;
        default:
            return false;
    }
}
