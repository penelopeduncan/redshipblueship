/**
 * @file test_mm_suns_song_played_text.c
 * @brief Locks for #1587 stage T: playing the Sun's Song in any Majora's Mask file
 *        shows OoTMM's "You played the Sun's Song!" (the name in yellow) in
 *        0x1B78's box, not MM's empty 0x1B7D (src/common/mm_warp_song_play.{h,c},
 *        the Combo_MMSunsSong_* helpers; MM's MM_SunsSongMM_* in
 *        games/mm/2s2h/Rando/WarpSongsMM.cpp; the call site in z_message.c).
 *
 * ONE ROM-FREE ROW (redship tier, generating nothing; the display-free shared
 * context is brought up first, as mm-warp-song-play does):
 *
 *   mm-sun-song-played-text (CTest MMSunSongPlayedText):
 *     T1 the pure helpers: state 11 alone is the Sun's Song (0..0x1FF); its text
 *        is exactly "You played the %ySun's Song%w!"; a buffer one byte short, a
 *        zero length and NULL are refused with "" left behind; 0x1B72 + 11 is
 *        0x1B7D. Catches a gate on the patch (ruling 2.6 is "always"), a wrong
 *        colour or wording, and a truncated text.
 *     T2 MM's helpers over a stand-in play state and message table
 *        (games/mm/2s2h/mm_suns_song_played_text_test.cpp, legs S1-S4): the
 *        custom id, the armed text with 0x1B78's header, the bytes the OnOpenText
 *        hook loads, and every other state left on 0x1B72 + state.
 *     T3 the seam, by source anchor inside MM_Message_DrawMain's
 *        MSGMODE_DISPLAY_SONG_PLAYED_TEXT_BEGIN case: the warp-song branch, then
 *        the Sun's Song branch continuing to MM_SunsSongMM_PlayedTextId, then the
 *        #endif, the scarecrow arm and the plain 0x1B72 + songPlayed arm, in that
 *        order. Red if the Sun's branch is removed, moved after the plain arm, or
 *        moved out of the #ifdef RSBS_SINGLE_EXECUTABLE block. A live drive of
 *        the case needs MM's message table and font from mm.o2r (the integration
 *        tier); see the pull request.
 *
 * Linkage note: #included into test_runner.cpp at FILE SCOPE (compiled as C++)
 * after test_mm_warp_song_play.c, whose anonymous-namespace source readers
 * (MwpReadSource, MwpFunctionBody, MwpRegion, MwpInOrder) it reuses. Helpers
 * carry the Mssp prefix.
 */

#include "../mm_warp_song_play.h"
#include "../test_runner.h"

#include <cstdio>
#include <cstring>
#include <string>

#ifdef __cplusplus
extern "C" {
#endif
// games/mm/2s2h/mm_suns_song_played_text_test.cpp
int MM_SunsSongPlayedText_RunHeadless(void);
#ifdef __cplusplus
}
#endif

static int sMsspFailures;

#define MSSP_CHECK(cond, ...)                                      \
    do {                                                           \
        if (!(cond)) {                                             \
            printf("[TEST] FAIL (mm-sun-song-played-text): ");     \
            printf(__VA_ARGS__);                                   \
            printf("  [%s] (%s:%d)\n", #cond, __FILE__, __LINE__); \
            sMsspFailures++;                                       \
        }                                                          \
    } while (0)

namespace {

void MsspCheckPure() {
    for (unsigned state = 0; state < 0x200u; state++) {
        MSSP_CHECK(Combo_MMSunsSong_IsPlayedState(state) == (state == 11u),
                   "T1: Combo_MMSunsSong_IsPlayedState(%u) is not true for 11 alone\n", state);
    }
    MSSP_CHECK(RSBS_MMSUN_SONG_STATE == 11u && RSBS_MMSUN_TEXT_VANILLA_BASE + RSBS_MMSUN_SONG_STATE == 0x1B7Du,
               "T1: the Sun's Song's vanilla text is not 0x1B72 + 11 = 0x1B7D\n");

    const char* const want = "You played the %ySun's Song%w!";
    const size_t wantLen = strlen(want);
    char out[64];
    MSSP_CHECK(Combo_MMSunsSong_PlayedText(out, sizeof(out)) == (int)wantLen && strcmp(out, want) == 0,
               "T1: the Sun's Song's text is \"%s\", want \"%s\"\n", out, want);
    char exact[64];
    MSSP_CHECK(Combo_MMSunsSong_PlayedText(exact, wantLen + 1) == (int)wantLen && strcmp(exact, want) == 0,
               "T1: a buffer of exactly the text's size is refused\n");
    char shortBuf[64];
    shortBuf[0] = 'x';
    MSSP_CHECK(Combo_MMSunsSong_PlayedText(shortBuf, wantLen) == -1 && shortBuf[0] == '\0',
               "T1: a buffer one byte short is not refused with \"\" left behind\n");
    char untouched = 'x';
    MSSP_CHECK(Combo_MMSunsSong_PlayedText(&untouched, 0) == -1 && untouched == 'x',
               "T1: a zero-length buffer is written or accepted\n");
    MSSP_CHECK(Combo_MMSunsSong_PlayedText(NULL, sizeof(out)) == -1, "T1: a NULL buffer is accepted\n");
}

void MsspCheckAnchors() {
    const std::string message = MwpReadSource("games/mm/src/code/z_message.c");
    if (message.empty()) {
        MSSP_CHECK(false, "T3: games/mm/src/code/z_message.c could not be read (RSBS_SOURCE_DIR)\n");
        return;
    }
    const std::string drawMain = MwpFunctionBody(message, "void MM_Message_DrawMain(PlayState* play, Gfx** gfxP)");
    const std::string textBegin =
        MwpRegion(drawMain, "case MSGMODE_DISPLAY_SONG_PLAYED_TEXT_BEGIN:", "case MSGMODE_16:");
    MSSP_CHECK(MwpInOrder(textBegin,
                          { "#ifdef RSBS_SINGLE_EXECUTABLE", "if (MM_WarpSongMM_IsStaffState(msgCtx->songPlayed)) {",
                            "MM_Message_ContinueTextbox(play, MM_WarpSongMM_PlayedTextId(msgCtx->songPlayed));",
                            "} else if (MM_SunsSongMM_IsPlayedState(msgCtx->songPlayed)) {",
                            "MM_Message_ContinueTextbox(play, MM_SunsSongMM_PlayedTextId(msgCtx->songPlayed));",
                            "} else", "#endif", "if (msgCtx->songPlayed == OCARINA_SONG_SCARECROW_SPAWN) {",
                            "MM_Message_ContinueTextbox(play, 0x1B72 + msgCtx->songPlayed);" }),
               "T3: MSGMODE_DISPLAY_SONG_PLAYED_TEXT_BEGIN in MM_Message_DrawMain does not take the Sun's Song "
               "after the warp songs and before the scarecrow and plain 0x1B72 + song arms, inside the "
               "RSBS_SINGLE_EXECUTABLE block\n");
}

} // namespace

TestResult MMSunsSongPlayedText_Run(void) {
    sMsspFailures = 0;
    MsspCheckPure();
    MSSP_CHECK(MM_SunsSongPlayedText_RunHeadless() == 0, "T2: MM's half failed (its FAIL line is above)\n");
    MsspCheckAnchors();
    if (sMsspFailures != 0) {
        printf("[TEST] FAIL (mm-sun-song-played-text): %d failure(s)\n", sMsspFailures);
        return TEST_FAIL;
    }
    printf("[TEST] PASS: mm-sun-song-played-text\n");
    return TEST_PASS;
}
