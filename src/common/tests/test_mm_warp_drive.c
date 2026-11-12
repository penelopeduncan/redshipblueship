/**
 * @file test_mm_warp_drive.c
 * @brief Locks for #1169 lane XW13's pure half (src/common/mm_warp_song_drive.{h,c})
 *        and Majora's Mask's test input seam (games/mm/src/code/padmgr.c), the two
 *        pieces the integration row IntMMWarpSongMinuet stands on.
 *
 * ONE ROM-FREE ROW (redship tier, generating nothing, no bring-up: nothing here
 * reads a CVar or a play state):
 *
 *   mm-warp-drive-seam (CTest MMWarpDriveSeam):
 *     W1 Combo_TestPad_Apply ORs the latch into the buttons and zeroes it; a zero
 *        latch changes nothing; a NULL latch is the buttons unchanged. Catches a
 *        seam that forgets to clear (a held button would live past one update, so
 *        a hold could never be released) or that assigns instead of ORing (a
 *        player's own press would be dropped).
 *     W2 the note schedule: MM's five ocarina buttons map to five distinct pad
 *        bits, and Minuet's six notes are A, C-up, C-left, C-right, C-left,
 *        C-right (OoTMM's sCustomSongs); out of range is 0. The values against
 *        MM's own BTN_* macros are leg M10 of MMWarpSongMatcher, which also plays
 *        this schedule through MM's real reader.
 *     W3 where OoT must land: the record's destination when it says shuffled,
 *        otherwise the song's pad whatever the record holds; 0 for no song.
 *     W4 the arm proofs: the right target and the right pending switch are
 *        accepted, and each one with any single field wrong is refused.
 *     W5 MM_PadMgr_SetTestButtons outside an integration run: a nonzero value is
 *        refused (the getter stays 0); 0 is accepted.
 *     W6 source anchors, each searched inside the function it claims: the seam
 *        sits in PadMgr_UpdateInputs after the error switch and before the D-pad
 *        masks and the edge computation; the setter refuses before it writes;
 *        the integration row's verdict cleanup clears MM's test buttons before
 *        its no-keys return (pass or fail).
 *
 * Linkage note: #included into test_runner.cpp at FILE SCOPE (compiled as C++)
 * directly after test_mm_warp_song_play.c, whose anonymous-namespace source
 * readers (MwpReadSource, MwpFunctionBody, MwpInOrder) it reuses. Helpers carry
 * the Mwd prefix.
 */

#include "../game.h"
#include "../entrance.h"
#include "../integration_test_hooks.h"
#include "../mm_warp_song_drive.h"
#include "../mm_warp_song_play.h"
#include "../test_runner.h"

#include <cstdio>
#include <string>

#ifdef __cplusplus
extern "C" {
#endif
// games/mm/src/code/padmgr.c (games/mm/include/padmgr.h declares them with MM's
// u16; these are the same unsigned 16-bit type).
void MM_PadMgr_SetTestButtons(uint16_t buttons);
uint16_t MM_PadMgr_GetTestButtons(void);
#ifdef __cplusplus
}
#endif

static int sMwdFailures;

#define MWD_CHECK(cond, ...)                                       \
    do {                                                           \
        if (!(cond)) {                                             \
            printf("[TEST] FAIL (mm-warp-drive-seam): ");          \
            printf(__VA_ARGS__);                                   \
            printf("  [%s] (%s:%d)\n", #cond, __FILE__, __LINE__); \
            sMwdFailures++;                                        \
        }                                                          \
    } while (0)

namespace {

void MwdCheckApply() {
    uint16_t latch = 0x8002u;
    MWD_CHECK(Combo_TestPad_Apply(0x0004u, &latch) == 0x8006u, "W1: the latch is not ORed into the buttons\n");
    MWD_CHECK(latch == 0u, "W1: the latch survived the update (0x%04X)\n", (unsigned)latch);
    MWD_CHECK(Combo_TestPad_Apply(0x0004u, &latch) == 0x0004u, "W1: a cleared latch still changes the buttons\n");
    latch = 0u;
    MWD_CHECK(Combo_TestPad_Apply(0x1234u, &latch) == 0x1234u && latch == 0u, "W1: a zero latch changed the buttons\n");
    MWD_CHECK(Combo_TestPad_Apply(0x0001u, NULL) == 0x0001u, "W1: a NULL latch changed the buttons\n");
}

void MwdCheckSchedule() {
    uint16_t seen = 0u;
    for (uint8_t b = 0; b < 5; b++) {
        const uint16_t pad = Combo_MMWarpDrive_PadButton(b);
        MWD_CHECK(pad != 0u && (pad & (uint16_t)(pad - 1u)) == 0u && (seen & pad) == 0u,
                  "W2: ocarina button %u has no pad bit of its own (0x%04X)\n", (unsigned)b, (unsigned)pad);
        seen = (uint16_t)(seen | pad);
    }
    MWD_CHECK(Combo_MMWarpDrive_PadButton(5) == 0u, "W2: a sixth ocarina button maps to a pad bit\n");
    const uint16_t minuet[6] = { RSBS_MWD_PAD_A,      RSBS_MWD_PAD_CUP,   RSBS_MWD_PAD_CLEFT,
                                 RSBS_MWD_PAD_CRIGHT, RSBS_MWD_PAD_CLEFT, RSBS_MWD_PAD_CRIGHT };
    MWD_CHECK(Combo_MMWarpDrive_NoteCount(0) == 6, "W2: Minuet does not have six notes\n");
    for (int i = 0; i < 6; i++) {
        MWD_CHECK(Combo_MMWarpDrive_NotePad(0, i) == minuet[i], "W2: Minuet's note %d is pad 0x%04X, want 0x%04X\n",
                  i + 1, (unsigned)Combo_MMWarpDrive_NotePad(0, i), (unsigned)minuet[i]);
    }
    MWD_CHECK(Combo_MMWarpDrive_NotePad(0, 6) == 0u && Combo_MMWarpDrive_NotePad(0, -1) == 0u &&
                  Combo_MMWarpDrive_NotePad(6, 0) == 0u && Combo_MMWarpDrive_NoteCount(6) == 0,
              "W2: an out-of-range note or song has a pad bit\n");
}

// MM's reader drops the frame after a pitch change, so a one-frame gap would merge
// two notes (M10 proves the exact schedule against the real reader).
static_assert(RSBS_MWD_NOTE_GAP >= 2 && RSBS_MWD_NOTE_HOLD >= 1, "the drive's note gap must outlast the dropped frame");

void MwdCheckExpectedArrival() {
    MWD_CHECK(Combo_MMWarpDrive_ExpectedArrival(0, 0u, 0x0123u) == 0x0600u,
              "W3: an unshuffled record does not land on Minuet's pad\n");
    MWD_CHECK(Combo_MMWarpDrive_ExpectedArrival(0, (uint8_t)RSBS_OOT_WARP_REC_SHUFFLED, 0x0123u) == 0x0123u,
              "W3: a shuffled record does not land on its destination\n");
    MWD_CHECK(Combo_MMWarpDrive_ExpectedArrival(5, 0u, 0u) == Combo_MMWarpPlay_PadEntrance(5),
              "W3: Prelude's unshuffled arrival is not its pad\n");
    MWD_CHECK(Combo_MMWarpDrive_ExpectedArrival(6, (uint8_t)RSBS_OOT_WARP_REC_SHUFFLED, 0x0123u) == 0u,
              "W3: a song out of range has an arrival\n");
}

void MwdCheckArmProofs() {
    const int mm = (int)GAME_MM;
    const int oot = (int)GAME_OOT;
    const uint8_t warp = (uint8_t)COMBO_SWITCH_KIND_WARP_SONG;
    const uint8_t soar = (uint8_t)COMBO_SWITCH_KIND_SOARING;
    const uint16_t marker = 0xD800u;
    MWD_CHECK(Combo_MMWarpDrive_TargetIsCrossing(true, mm, marker, oot, 0x0600u, warp, marker, 0),
              "W4: Minuet's own target is not accepted\n");
    MWD_CHECK(!Combo_MMWarpDrive_TargetIsCrossing(false, mm, marker, oot, 0x0600u, warp, marker, 0),
              "W4: a disarmed target is accepted\n");
    MWD_CHECK(!Combo_MMWarpDrive_TargetIsCrossing(true, oot, marker, oot, 0x0600u, warp, marker, 0),
              "W4: a target from OoT is accepted\n");
    MWD_CHECK(!Combo_MMWarpDrive_TargetIsCrossing(true, mm, 0xD810u, oot, 0x0600u, warp, marker, 0),
              "W4: a target at another marker is accepted\n");
    MWD_CHECK(!Combo_MMWarpDrive_TargetIsCrossing(true, mm, marker, mm, 0x0600u, warp, marker, 0),
              "W4: a target into MM is accepted\n");
    MWD_CHECK(!Combo_MMWarpDrive_TargetIsCrossing(true, mm, marker, oot, 0x0604u, warp, marker, 0),
              "W4: a target at another pad is accepted\n");
    MWD_CHECK(!Combo_MMWarpDrive_TargetIsCrossing(true, mm, marker, oot, 0x0600u, soar, marker, 0),
              "W4: a target of another kind is accepted\n");
    MWD_CHECK(!Combo_MMWarpDrive_TargetIsCrossing(true, mm, marker, oot, 0x0600u, warp, marker, 6),
              "W4: a target for no song is accepted\n");
    MWD_CHECK(Combo_MMWarpDrive_SwitchIsCrossing(true, oot, 0x0600u, warp, 0),
              "W4: Minuet's own pending switch is not accepted\n");
    MWD_CHECK(!Combo_MMWarpDrive_SwitchIsCrossing(false, oot, 0x0600u, warp, 0), "W4: no pending switch is accepted\n");
    MWD_CHECK(!Combo_MMWarpDrive_SwitchIsCrossing(true, mm, 0x0600u, warp, 0), "W4: a switch into MM is accepted\n");
    MWD_CHECK(!Combo_MMWarpDrive_SwitchIsCrossing(true, oot, 0x0604u, warp, 0),
              "W4: a switch to another pad is accepted\n");
    MWD_CHECK(!Combo_MMWarpDrive_SwitchIsCrossing(true, oot, 0x0600u, (uint8_t)COMBO_SWITCH_KIND_DOOR, 0),
              "W4: a door switch is accepted\n");
}

void MwdCheckSetterRefuses() {
    if (IntegrationTest_IsActive()) {
        MWD_CHECK(false, "W5 setup: an integration run is active in a --test process\n");
        return;
    }
    MM_PadMgr_SetTestButtons(0u);
    MM_PadMgr_SetTestButtons((uint16_t)RSBS_MWD_PAD_A);
    MWD_CHECK(MM_PadMgr_GetTestButtons() == 0u, "W5: MM_PadMgr_SetTestButtons took 0x%04X outside an integration run\n",
              (unsigned)MM_PadMgr_GetTestButtons());
    MM_PadMgr_SetTestButtons(0u);
    MWD_CHECK(MM_PadMgr_GetTestButtons() == 0u, "W5: clearing the test buttons did not leave 0\n");
}

void MwdCheckAnchors() {
    const std::string padmgr = MwpReadSource("games/mm/src/code/padmgr.c");
    if (padmgr.empty()) {
        MWD_CHECK(false, "W6: games/mm/src/code/padmgr.c could not be read (RSBS_SOURCE_DIR)\n");
        return;
    }
    const std::string update = MwpFunctionBody(padmgr, "void PadMgr_UpdateInputs(void)");
    MWD_CHECK(MwpInOrder(update, { "switch (pad->err_no) {",
                                   "case CHNL_ERR_NORESP >> 4:", "#ifdef RSBS_SINGLE_EXECUTABLE", "if (i == 0) {",
                                   "input->cur.button = Combo_TestPad_Apply(input->cur.button, &sMMTestPadButtons);",
                                   "#endif", "(BTN_DDOWN | BTN_DUP)", "diff = input->prev.button ^ input->cur.button;",
                                   "input->press.button |=" }),
              "W6: the seam is not in PadMgr_UpdateInputs after the error switch and before the D-pad masks and the "
              "press/release edges\n");
    const std::string setter = MwpFunctionBody(padmgr, "void MM_PadMgr_SetTestButtons(u16 buttons)");
    MWD_CHECK(MwpInOrder(setter, { "(buttons != 0) && !IntegrationTest_IsActive()", "return;",
                                   "sMMTestPadButtons = buttons;" }),
              "W6: MM_PadMgr_SetTestButtons does not refuse outside an integration run before it writes\n");
    const std::string hooks = MwpReadSource("src/common/integration_test_hooks.cpp");
    const std::string cleanup = MwpFunctionBody(hooks, "void IntegrationTest_SoaringCleanup(void)");
    MWD_CHECK(MwpInOrder(cleanup, { "IntegrationTest_MMWarpSongMinuet()", "Combo_ComboItemRules2RevealForTest(0);",
                                    "MM_PadMgr_SetTestButtons(0);", "if (sSoaringKeys.empty())" }),
              "W6: int-mm-warp-song-minuet's verdict cleanup does not clear MM's test buttons before its no-keys "
              "return\n");
}

} // namespace

TestResult MMWarpDriveSeam_Run(void) {
    sMwdFailures = 0;
    MwdCheckApply();
    MwdCheckSchedule();
    MwdCheckExpectedArrival();
    MwdCheckArmProofs();
    MwdCheckSetterRefuses();
    MwdCheckAnchors();
    if (sMwdFailures != 0) {
        printf("[TEST] FAIL (mm-warp-drive-seam): %d failure(s)\n", sMwdFailures);
        return TEST_FAIL;
    }
    printf("[TEST] PASS: mm-warp-drive-seam\n");
    return TEST_PASS;
}
