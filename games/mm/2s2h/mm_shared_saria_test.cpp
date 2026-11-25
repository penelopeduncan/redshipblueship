/**
 * @file mm_shared_saria_test.cpp
 * ROM-free, display-free locks for Majora's Mask's half of #1587 stage A: under the
 * frozen Shared Sun's Song / Shared Saria's Song rule MM pools no copy of its own,
 * and a shared Saria's Song is permanent in MM (no prompt, no hint, no removal).
 * Two rows, two entry points, both CTest label "redship", neither generating a
 * world:
 *
 * ============================================================================
 * mm-shared-song-pool-skip (MMSharedSongPoolSkip) -- the GeneratePools skip
 * ============================================================================
 *
 * Runs MM's REAL Rando::Logic::GeneratePools over a fresh RandoSaveInfo (every
 * option at its default, Glitchless, a fixed Ship_Random seed) with MM's own Sun's
 * Song and Saria's Song rows at YES (the state Foreign.cpp's step 2d resolves under
 * the rule), over a combo record frozen with the given second word and no pairing
 * (so no other paired branch of GeneratePools runs). The skip reads the frozen
 * record and is not gated on Offered, so no reveal is needed.
 *  (a) both rules frozen on: neither RI_SONG_SUN nor RI_SONG_SARIA is in the item
 *      pool. RED when either [RSBS #1587] skip in GeneratePools.cpp is removed.
 *  (b) only the Sun's rule: RI_SONG_SARIA once, RI_SONG_SUN absent; then the
 *      reverse. RED when a skip reads the other song's bit, or both.
 *  (c) both rules off, rows at YES: each once (2Ship's unshared pool). RED for an
 *      over-broad skip.
 *  (d) rows at NO under both rules: neither (today's default).
 * The end-to-end rows (SharedSunSariaOneCopy*) cannot be this gate: the bag's pair
 * pass trims a second MM copy to MM filler (combo_logic.c, combo_single_bag.c), so a
 * reverted skip still places one copy there.
 *
 * ============================================================================
 * mm-shared-saria-hooks (MMSharedSariaHooks) -- SariasSongHint under the rule
 * ============================================================================
 *
 * Over a calloc'd PlayState as MM_gPlayState, a rando save with MM's own Saria's
 * Song row on and Saria's Song owned, SariasSongHint() armed, the hooks driven
 * through MM_GameHooks_ExecuteVBShould and MM_GameHooks_ExecuteOnOpenText (the
 * calls z_message.c makes).
 *  Shared (the record frozen with RSBS_XITEM2_SHARED_SONG_SARIAS):
 *   1. the fanfare fix holds: sOcarinaSongFanfares[OCARINA_SONG_SARIAS] is
 *      NA_BGM_SARIAS_SONG (2Ship's fix, kept; #1587 question 2 is open);
 *   2. VB_SONG_AVAILABLE_TO_PLAY for Saria's Song answers true;
 *   3. VB_MSG_CAPTURE_MSGMODE_TEXT_DONE in OCARINA_MODE_PROCESS_RESTRICTED_SONG
 *      answers false and leaves ocarinaMode alone. RED when its early return is
 *      removed (the hook then answers true). No button is pressed, so a removed
 *      return shows as that answer, never as a textbox call on the stand-in state;
 *   4. VB_MSG_CAPTURE_MSGMODE_TEXT_CLOSING_OCARINA_ACTION after Saria's Song
 *      answers false and leaves ocarinaMode alone. RED when its early return is
 *      removed: the hook then starts 0x1B95 on the stand-in state, which can fault
 *      instead of failing cleanly (the only way to reach that line);
 *   5. 0x1B95 opened after leg 2 (2Ship's state 1) keeps the ROM text: load stays
 *      true and no message is written. RED when the OnOpenText early return is
 *      removed (2Ship's prompt loads);
 *   6. QUEST_SONG_SARIA is still set.
 *  Control (rule off, the row still on): TEXT_DONE in the restricted-song mode with
 *  no button pressed answers true, and 0x1B95 loads 2Ship's prompt, so the hooks are
 *  armed and the shared legs are not vacuous. The unshared redirect and choice
 *  paths are never driven: they call the message code.
 *
 * CLEANUP (pass and fail): the SaveContext image, gComboCtx, MM_gPlayState,
 * MM_sLastPlayedSong and sOcarinaSongFanfares[OCARINA_SONG_SARIAS] are put back and
 * SariasSongHint() is re-run under the restored save, so the row is order-free but
 * for registering hooks while it runs (it sits after mm-registrar-coverage).
 */

#include "global.h"

#include <cstdio>

#if !defined(RSBS_SINGLE_EXECUTABLE)
/** The combo record and the S2H registries these locks drive exist only in the single-exe build. */
extern "C" int MM_SharedSongPoolSkip_RunHeadless(void) {
    printf("[TEST] mm-shared-song-pool-skip: PASS (not applicable outside RSBS_SINGLE_EXECUTABLE)\n");
    return 0;
}
extern "C" int MM_SharedSariaHooks_RunHeadless(void) {
    printf("[TEST] mm-shared-saria-hooks: PASS (not applicable outside RSBS_SINGLE_EXECUTABLE)\n");
    return 0;
}
#else

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <vector>

#include <libultraship/bridge/consolevariablebridge.h>
#include <ship/Context.h>

#include "2s2h/GameInteractor/GameInteractor.h"
#include "2s2h/Rando/Logic/Logic.h"
#include "2s2h/Rando/MiscBehavior/MiscBehavior.h"
#include "2s2h/Rando/Rando.h"
#include "2s2h/Rando/StaticData/StaticData.h"
#include "2s2h/ShipUtils.h"

// src/common: outside any extern "C" block (context.h's <type_traits>).
#include "combo_song_pool.h"
#include "foreign_items.h"

extern "C" {
#include "variables.h"
#include <z64ocarina.h>
extern s16 sOcarinaSongFanfares[17];
extern s16 MM_sLastPlayedSong;
}

namespace {

int MssFail(const char* row, int code, const char* msg) {
    printf("[TEST] FAIL(%d): %s: %s\n", code, row, msg);
    return code;
}

/** A combo record frozen with @p rules2 as the second word and no pairing published. */
void MssFreezeRules2(uint32_t rules2) {
    ComboContext_Init();
    ComboSettingsRecord rec;
    Combo_ComboSettingsDefaults(&rec);
    Combo_FreezeComboSettingsEx2(&rec, NULL, rules2);
}

/** How many times @p id is in @p pool. */
int MssCount(const std::vector<RandoItemId>& pool, RandoItemId id) {
    return (int)std::count(pool.begin(), pool.end(), id);
}

/** GeneratePools over a fresh save info, both MM song rows at @p rowValue, the record
 *  frozen with @p rules2; the Sun's and Saria's counts in @p sun / @p saria. */
void MssGenerate(uint32_t rules2, uint32_t rowValue, int* sun, int* saria) {
    MssFreezeRules2(rules2);
    auto info = std::make_unique<RandoSaveInfo>();
    memset(info.get(), 0, sizeof(RandoSaveInfo));
    for (auto& [randoOptionId, row] : Rando::StaticData::Options) {
        info->randoSaveOptions[randoOptionId] = row.defaultValue;
    }
    info->randoSaveOptions[RO_LOGIC] = RO_LOGIC_GLITCHLESS;
    info->randoSaveOptions[RO_SHUFFLE_SONG_SUN] = rowValue;
    info->randoSaveOptions[RO_SHUFFLE_SONG_SARIA] = rowValue;
    std::vector<RandoCheckId> checkPool;
    std::vector<RandoItemId> itemPool;
    Ship_Random_Seed(0x1587u);
    Rando::Logic::GeneratePools(*info, checkPool, itemPool);
    *sun = MssCount(itemPool, RI_SONG_SUN);
    *saria = MssCount(itemPool, RI_SONG_SARIA);
}

int MssRunPoolLegs() {
    const char* const kRow = "mm-shared-song-pool-skip";
    const uint32_t sunBit = RSBS_XITEM2_SHARED_SONG_SUN;
    const uint32_t sariaBit = RSBS_XITEM2_SHARED_SONG_SARIAS;
    int sun = -1;
    int saria = -1;

    MssGenerate(0u, RO_GENERIC_YES, &sun, &saria);
    printf("[TEST] %s (c) rules off, rows YES: Sun's Song %d, Saria's Song %d\n", kRow, sun, saria);
    if (sun != 1 || saria != 1) {
        return MssFail(kRow, 10, "(c) with both rules off and both rows on MM's pool does not hold one copy of each");
    }
    MssGenerate(sunBit | sariaBit, RO_GENERIC_YES, &sun, &saria);
    printf("[TEST] %s (a) both rules frozen on, rows YES: Sun's Song %d, Saria's Song %d\n", kRow, sun, saria);
    if (sun != 0 || saria != 0) {
        return MssFail(kRow, 11,
                       "(a) under both frozen shared rules MM pooled its own copy: a [RSBS #1587] skip in "
                       "GeneratePools.cpp is missing");
    }
    MssGenerate(sunBit, RO_GENERIC_YES, &sun, &saria);
    printf("[TEST] %s (b) Sun's rule only: Sun's Song %d, Saria's Song %d\n", kRow, sun, saria);
    if (sun != 0 || saria != 1) {
        return MssFail(kRow, 12, "(b) under the Shared Sun's Song alone MM's pool is not {no Sun's Song, one Saria's}");
    }
    MssGenerate(sariaBit, RO_GENERIC_YES, &sun, &saria);
    printf("[TEST] %s (b) Saria's rule only: Sun's Song %d, Saria's Song %d\n", kRow, sun, saria);
    if (sun != 1 || saria != 0) {
        return MssFail(kRow, 13, "(b) under the Shared Saria's Song alone MM's pool is not {one Sun's, no Saria's}");
    }
    MssGenerate(sunBit | sariaBit, RO_GENERIC_NO, &sun, &saria);
    if (sun != 0 || saria != 0) {
        return MssFail(kRow, 14, "(d) with both rows off MM's pool holds a Sun's Song or a Saria's Song");
    }
    return 0;
}

int MssRunSariaLegs(PlayState* play) {
    const char* const kRow = "mm-shared-saria-hooks";
    gSaveContext.save.shipSaveInfo.saveType = SAVETYPE_RANDO;
    RANDO_SAVE_OPTIONS[RO_SHUFFLE_SONG_SARIA] = 1;
    SET_QUEST_ITEM(QUEST_SONG_SARIA);

    // ---- shared ----------------------------------------------------------------
    MssFreezeRules2(RSBS_XITEM2_SHARED_SONG_SARIAS);
    if (!Combo_SharedSongFrozenShared(RSBS_XITEM2_SHARED_SONG_SARIAS)) {
        return MssFail(kRow, 20, "the frozen Shared Saria's Song does not read as shared: every leg would be vacuous");
    }
    sOcarinaSongFanfares[OCARINA_SONG_SARIAS] = NA_BGM_MAJORAS_LAIR;
    Rando::MiscBehavior::SariasSongHint(); // armed on the frozen row; state 0
    if (sOcarinaSongFanfares[OCARINA_SONG_SARIAS] != NA_BGM_SARIAS_SONG) {
        return MssFail(kRow, 21, "(1) a shared Saria's Song lost 2Ship's fanfare fix");
    }
    uint8_t song = OCARINA_SONG_SARIAS;
    if (!MM_GameHooks_ExecuteVBShould(VB_SONG_AVAILABLE_TO_PLAY, false, &song)) {
        return MssFail(kRow, 22, "(2) a shared, owned Saria's Song is not available to play in MM");
    }
    play->msgCtx.ocarinaMode = OCARINA_MODE_PROCESS_RESTRICTED_SONG;
    CONTROLLER1(&play->state)->press.button = 0;
    if (MM_GameHooks_ExecuteVBShould(VB_MSG_CAPTURE_MSGMODE_TEXT_DONE, false) ||
        play->msgCtx.ocarinaMode != OCARINA_MODE_PROCESS_RESTRICTED_SONG) {
        return MssFail(kRow, 23,
                       "(3) TEXT_DONE after a shared Saria's Song was captured by 2Ship's prompt (its [RSBS #1587] "
                       "early return is missing)");
    }
    MM_sLastPlayedSong = OCARINA_SONG_SARIAS;
    play->msgCtx.ocarinaMode = OCARINA_MODE_EVENT;
    if (MM_GameHooks_ExecuteVBShould(VB_MSG_CAPTURE_MSGMODE_TEXT_CLOSING_OCARINA_ACTION, false) ||
        play->msgCtx.ocarinaMode != OCARINA_MODE_EVENT) {
        return MssFail(kRow, 24,
                       "(4) closing a shared Saria's Song was redirected to 2Ship's prompt (its [RSBS #1587] early "
                       "return is missing)");
    }
    play->msgCtx.msgLength = 0;
    u16 textId = 0x1B95;
    bool load = true;
    MM_GameHooks_ExecuteOnOpenText(&textId, &load);
    if (!load || play->msgCtx.msgLength != 0) {
        return MssFail(kRow, 25,
                       "(5) 0x1B95 after a shared Saria's Song loaded 2Ship's prompt (the OnOpenText [RSBS #1587] "
                       "early return is missing)");
    }
    if (!CHECK_QUEST_ITEM(QUEST_SONG_SARIA)) {
        return MssFail(kRow, 26, "(6) a shared Saria's Song was removed from MM's save");
    }

    // ---- control: the rule off, the row still on -------------------------------
    MssFreezeRules2(0u);
    Rando::MiscBehavior::SariasSongHint(); // state 0
    song = OCARINA_SONG_SARIAS;
    (void)MM_GameHooks_ExecuteVBShould(VB_SONG_AVAILABLE_TO_PLAY, false, &song); // state 1
    play->msgCtx.ocarinaMode = OCARINA_MODE_PROCESS_RESTRICTED_SONG;
    CONTROLLER1(&play->state)->press.button = 0; // no answer: no textbox call
    if (!MM_GameHooks_ExecuteVBShould(VB_MSG_CAPTURE_MSGMODE_TEXT_DONE, false)) {
        return MssFail(kRow, 27,
                       "(control) unshared, TEXT_DONE was not captured: the hook is not armed, so leg 3 is "
                       "vacuous");
    }
    play->msgCtx.msgLength = 0;
    textId = 0x1B95;
    load = true;
    MM_GameHooks_ExecuteOnOpenText(&textId, &load);
    if (load || play->msgCtx.msgLength == 0) {
        return MssFail(kRow, 28, "(control) unshared, 0x1B95 did not load 2Ship's prompt: leg 5 is vacuous");
    }
    return 0;
}

} // namespace

extern "C" int MM_SharedSongPoolSkip_RunHeadless(void) {
    printf("[TEST] mm-shared-song-pool-skip: under the frozen Shared Sun's Song / Saria's Song MM pools no copy of "
           "its own (#1587 stage A)\n");
    auto ctx = Ship::Context::GetInstance();
    if (ctx == nullptr || ctx->GetConsoleVariables() == nullptr) {
        return MssFail("mm-shared-song-pool-skip", 1, "Ship::Context or its ConsoleVariables are missing");
    }
    auto savedCombo = std::make_unique<ComboContext>();
    memcpy(savedCombo.get(), &gComboCtx, sizeof(ComboContext));
    const int rc = MssRunPoolLegs();
    memcpy(&gComboCtx, savedCombo.get(), sizeof(ComboContext));
    if (rc != 0) {
        return rc;
    }
    printf("[TEST] mm-shared-song-pool-skip: PASS\n");
    return 0;
}

extern "C" int MM_SharedSariaHooks_RunHeadless(void) {
    printf("[TEST] mm-shared-saria-hooks: a shared Saria's Song is permanent in MM: no prompt, no hint, no removal "
           "(#1587 stage A)\n");
    auto ctx = Ship::Context::GetInstance();
    if (ctx == nullptr || ctx->GetConsoleVariables() == nullptr) {
        return MssFail("mm-shared-saria-hooks", 1, "Ship::Context or its ConsoleVariables are missing");
    }
    auto saved = std::make_unique<uint8_t[]>(sizeof(gSaveContext));
    memcpy(saved.get(), &gSaveContext, sizeof(gSaveContext));
    auto savedCombo = std::make_unique<ComboContext>();
    memcpy(savedCombo.get(), &gComboCtx, sizeof(ComboContext));
    PlayState* const savedPlay = MM_gPlayState;
    const s16 savedFanfare = sOcarinaSongFanfares[OCARINA_SONG_SARIAS];
    const s16 savedLastSong = MM_sLastPlayedSong;

    PlayState* play = static_cast<PlayState*>(calloc(1, sizeof(PlayState)));
    int rc = 1;
    if (play != nullptr) {
        MM_gPlayState = play;
        rc = MssRunSariaLegs(play);
    } else {
        printf("[TEST] FAIL(1): mm-shared-saria-hooks: could not allocate the stand-in play state\n");
    }

    // ---- cleanup (pass and fail) ------------------------------------------------
    memcpy(&gSaveContext, saved.get(), sizeof(gSaveContext));
    memcpy(&gComboCtx, savedCombo.get(), sizeof(ComboContext));
    Rando::MiscBehavior::SariasSongHint(); // re-armed (or disarmed) under the restored save
    sOcarinaSongFanfares[OCARINA_SONG_SARIAS] = savedFanfare;
    MM_sLastPlayedSong = savedLastSong;
    MM_gPlayState = savedPlay;
    free(play);
    if (rc != 0) {
        return rc;
    }
    printf("[TEST] mm-shared-saria-hooks: PASS (shared: fanfare fix, playable, no prompt, no redirect, ROM text, "
           "kept; unshared: 2Ship's prompt)\n");
    return 0;
}

#endif /* RSBS_SINGLE_EXECUTABLE */
