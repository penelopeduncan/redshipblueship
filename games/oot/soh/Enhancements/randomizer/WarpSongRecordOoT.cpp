/**
 * @file WarpSongRecordOoT.cpp
 * @brief The creation's record of Ocarina of Time's six warp songs as Majora's
 *        Mask plays them (#1169 lane XW7; ComboOoTWarpRecord, src/common/context.h,
 *        ADR 0009 claim 22).
 *
 * WHAT IT WRITES. For each song, in OoTMM's order (Minuet .. Prelude), the
 * entrance its vanilla pad leads to in the world being created, exactly what
 * Entrance_GetOverride(pad) answers once Entrance_Init has installed that world
 * (the last generated override row whose index is the pad, the fake blue warp's
 * remap included; the pad itself with no row), and, when Warp Song Hints are on,
 * the area OoT's own warp prompt names (the RH_*_WARP_LOC hint CreateWarpSongTexts
 * built, through the shared area vocabulary). Plus the world's Shuffle Warp Songs
 * and Warp Song Hints flags.
 *
 * WHEN. Once, at Generate, from OoT_Creation_FinishPairedHalf, after OoT's
 * remainder has created the overrides and the warp-song texts (fill.cpp's
 * RsbsFinishPairedGeneralPass). Reading the Rando::Context heap here is the
 * creation reading its own world; nothing reads it for these answers afterwards
 * (MM's prompt and OoT's arrival read the record). Paired worlds only: an
 * unpaired world leaves the record UNSET, and the songs do nothing unpaired.
 */
#ifdef RSBS_SINGLE_EXECUTABLE

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <vector>

// At file scope first: z64.h includes <memory> under C++, which must not get C
// linkage (see SongOfSoaringOoT.cpp).
#include <z64.h>
#include "soh/Enhancements/randomizer/randomizerTypes.h"
#include "soh/Enhancements/randomizer/randomizer_entrance.h"
#include "soh/Enhancements/randomizer/SeedContext.h"
#include "soh/Enhancements/randomizer/hint.h"

#include "combo_area.h"        // src/common: COMBO_AREA_POCKET
#include "context.h"           // src/common: gComboCtx.comboOoTWarps
#include "mm_warp_song_play.h" // src/common: the record's builder, the pad table

// entrance.h declares this inside its C++-only region; redeclared here so this TU
// stays header-light (oot_warp_arrival_test.cpp does the same).
extern "C" EntranceOverride* Randomizer_GetEntranceOverrides();
// HintAreaOoT.cpp: a RandomizerArea in the OoT half of the shared vocabulary.
extern "C" uint8_t OoT_HintArea_ComboAreaOfRandomizerArea(int ra);

namespace {

// The hint OoT's own warp prompt reads for each song (StaticHints.cpp,
// Build*WarpMessage), in OoTMM's order.
const RandomizerHint kWarpSongRecordHints[RSBS_MMWP_SONG_COUNT] = {
    RH_MINUET_WARP_LOC,  RH_BOLERO_WARP_LOC,   RH_SERENADE_WARP_LOC,
    RH_REQUIEM_WARP_LOC, RH_NOCTURNE_WARP_LOC, RH_PRELUDE_WARP_LOC,
};

/** Entrance_GetOverride(@p pad) as Entrance_Init would install this world's rows:
 *  the last row with that index (Entrance_Init's last write wins), stopping at
 *  the first null row; the pad itself with no row. */
uint16_t WarpSongRecordOverrideOf(const EntranceOverride* rows, uint16_t pad) {
    int16_t resolved = (int16_t)pad;
    for (size_t i = 0; rows != nullptr && i < ENTRANCE_OVERRIDES_MAX_COUNT; i++) {
        const EntranceOverride& row = rows[i];
        if (row.index == 0 && row.destination == 0 && row.override == 0 && row.overrideDestination == 0) {
            break; // Entrance_EntranceIsNull
        }
        if (row.index == (int16_t)pad) {
            resolved = row.override;
        }
    }
    // Entrance_GetOverride's special index for the fake Ganon's Castle blue warp.
    if (resolved == ENTR_OUTSIDE_GANONS_CASTLE_1_2) {
        resolved = ENTR_CASTLE_GROUNDS_RAINBOW_BRIDGE_EXIT;
    }
    return (uint16_t)resolved;
}

// TEST ONLY (OoT_Creation_TestForceWarpRecordInvalid, #1169 XW12): the freeze
// treats the record it built as malformed. Never set in production.
int sWarpSongRecordForceInvalid = 0;

} // namespace

/** TEST BRIDGE (oot-warp-song-record, #1169 XW12 D3): with @p on nonzero the next
 *  freezes refuse the record they build, as a malformed one would be refused. */
extern "C" void OoT_Creation_TestForceWarpRecordInvalid(int on) {
    sWarpSongRecordForceInvalid = on != 0 ? 1 : 0;
}

/**
 * Write gComboCtx.comboOoTWarps from the world being created. Returns 0 when the
 * record is PRESENT, -1 (the record left UNSET) with no randomizer context or a
 * record the validator refuses.
 */
extern "C" int OoT_Creation_FreezeWarpSongRecord(void) {
    Combo_OoTWarpRecord_Clear(&gComboCtx.comboOoTWarps);
    const auto ctx = Rando::Context::GetInstance();
    if (!ctx) {
        fprintf(stderr, "[OoT] creation: no randomizer context, the warp-song record stays unset (#1169 XW7)\n");
        return -1;
    }
    const bool shuffled = static_cast<bool>(ctx->GetOption(RSK_SHUFFLE_WARP_SONGS));
    const bool hints = shuffled && static_cast<bool>(ctx->GetOption(RSK_WARP_SONG_HINTS));
    const EntranceOverride* rows = Randomizer_GetEntranceOverrides();
    uint16_t dest[RSBS_MMWP_SONG_COUNT];
    uint8_t area[RSBS_MMWP_SONG_COUNT];
    for (int song = 0; song < RSBS_MMWP_SONG_COUNT; song++) {
        dest[song] = WarpSongRecordOverrideOf(rows, Combo_MMWarpPlay_PadEntrance(song));
        area[song] = 0;
        if (hints) {
            const std::vector<RandomizerArea> areas = ctx->GetHint(kWarpSongRecordHints[song])->GetHintedAreas();
            const uint8_t a = areas.empty() ? (uint8_t)0 : OoT_HintArea_ComboAreaOfRandomizerArea((int)areas[0]);
            area[song] = a == COMBO_AREA_POCKET ? (uint8_t)0 : a; // a pad never leads to Link's Pocket
        }
    }
    int status = Combo_OoTWarpRecord_Build(&gComboCtx.comboOoTWarps, shuffled, hints, dest, area);
    if (sWarpSongRecordForceInvalid != 0) {
        status = COMBO_OOT_WARP_REC_INVALID; // the test seam above
    }
    if (status != COMBO_OOT_WARP_REC_PRESENT) {
        fprintf(stderr, "[OoT] creation: the warp-song record is malformed (status %d); left unset (#1169 XW7)\n",
                status);
        Combo_OoTWarpRecord_Clear(&gComboCtx.comboOoTWarps);
        return -1;
    }
    fprintf(stderr,
            "[OoT] creation: warp-song record frozen: shuffled %d, hints %d, pads -> %04X %04X %04X %04X %04X %04X "
            "(#1169 XW7)\n",
            shuffled ? 1 : 0, hints ? 1 : 0, (unsigned)dest[0], (unsigned)dest[1], (unsigned)dest[2], (unsigned)dest[3],
            (unsigned)dest[4], (unsigned)dest[5]);
    return 0;
}

// ============================================================================
// TEST BRIDGES (src/common/tests/test_mm_warp_song_play.c; rando-warp-arrival)
// ============================================================================

/** The writer's override resolution over @p n rows {index, override} (a null row
 *  appended), for pad @p pad. */
extern "C" uint16_t OoT_WarpSongRecord_TestResolve(const int16_t* index, const int16_t* override, int n, uint16_t pad) {
    std::vector<EntranceOverride> rows((size_t)(n > 0 ? n : 0) + 1);
    for (int i = 0; i < n; i++) {
        rows[(size_t)i] = EntranceOverride{ 0, index[i], 1, override[i], 0 };
    }
    rows[(size_t)(n > 0 ? n : 0)] = EntranceOverride{ 0, 0, 0, 0, 0 };
    // WarpSongRecordOverrideOf stops at the first null row, never past the vector.
    return WarpSongRecordOverrideOf(rows.data(), pad);
}

/**
 * oot-warp-song-record (rando tier): over the world just created, the songs whose
 * recorded destination differs from what the REAL Entrance_Init installs and
 * Entrance_GetOverride answers for the song's pad; -1 with no PRESENT record. The
 * table is reset afterwards (Entrance_ResetEntranceTable) when it was not ready
 * before, as the arrival row leaves it.
 */
extern "C" int OoT_WarpSongRecord_TestTableDisagreements(void) {
    if (Combo_OoTWarpRecord_Status() != COMBO_OOT_WARP_REC_PRESENT) {
        return -1;
    }
    const bool wasReady = Entrance_WarpArrivalOverridesReady() != 0;
    Entrance_Init();
    int disagreements = 0;
    for (int song = 0; song < RSBS_MMWP_SONG_COUNT; song++) {
        const uint16_t pad = Combo_MMWarpPlay_PadEntrance(song);
        const uint16_t table = (uint16_t)Entrance_GetOverride((int16_t)pad);
        if (table != Combo_OoTWarpRecord_Dest(song)) {
            fprintf(stderr,
                    "[TEST] oot-warp-song-record: song %d pad 0x%04X: the table says 0x%04X, the record 0x%04X\n", song,
                    (unsigned)pad, (unsigned)table, (unsigned)Combo_OoTWarpRecord_Dest(song));
            disagreements++;
        }
    }
    if (!wasReady) {
        Entrance_ResetEntranceTable();
    }
    return disagreements;
}

/** oot-warp-song-record: the area OoT's own warp prompt names for @p song in the
 *  world just created (its RH_*_WARP_LOC hint's first area, in the shared
 *  vocabulary; Link's Pocket as none), 0 for none, -1 out of range or with no
 *  randomizer context. */
extern "C" int OoT_WarpSongRecord_TestHintArea(int song) {
    if (song < 0 || song >= RSBS_MMWP_SONG_COUNT) {
        return -1;
    }
    const auto ctx = Rando::Context::GetInstance();
    if (!ctx) {
        return -1;
    }
    const std::vector<RandomizerArea> areas = ctx->GetHint(kWarpSongRecordHints[song])->GetHintedAreas();
    if (areas.empty()) {
        return 0;
    }
    const uint8_t a = OoT_HintArea_ComboAreaOfRandomizerArea((int)areas[0]);
    return a == COMBO_AREA_POCKET ? 0 : (int)a;
}

/**
 * Freeze an Ocarina of Time half whose s32 linkAge is @p linkAge, as a started
 * file (heart capacity, a slot, GAMEMODE_NORMAL) when @p started, else as the
 * title screen's save (fileNum 0xFF, GAMEMODE_TITLE_SCREEN), with OoT's real
 * SaveContext layout. @p live 0 notes the departure as not a live file first.
 */
extern "C" void OoT_WarpSongRecord_TestFreezeOoTHalf(int linkAge, int started, int live) {
    std::unique_ptr<SaveContext> save = std::make_unique<SaveContext>();
    std::memset(save.get(), 0, sizeof(SaveContext));
    if (started) {
        save->healthCapacity = 0x30;
        save->fileNum = 0;
        save->gameMode = GAMEMODE_NORMAL;
    } else {
        save->healthCapacity = 0x30;
        save->fileNum = 0xFF;
        save->gameMode = GAMEMODE_TITLE_SCREEN;
    }
    save->linkAge = linkAge;
    if (!live) {
        Context_NoteDepartureLiveFile(GAME_OOT, 0);
    }
    Context_FreezeState(GAME_OOT, 0, save.get(), sizeof(SaveContext));
}

#endif // RSBS_SINGLE_EXECUTABLE
