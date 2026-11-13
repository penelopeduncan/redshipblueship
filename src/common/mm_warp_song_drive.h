#pragma once
/**
 * @file mm_warp_song_drive.h
 * @brief The pure half of the integration row IntMMWarpSongMinuet (#1169 lane
 *        XW13): Majora's Mask's test input seam, the note schedule the row plays
 *        Ocarina of Time's Minuet of Forest with, and the row's two decisions
 *        about the crossing (is this the warp the song armed, and where must OoT
 *        land). Game-header-free; the MM pad manager, the MM drive
 *        (games/mm/2s2h/mm_int_warp_song_drive.cpp) and OoT's arrival verdict
 *        (games/oot/soh/GameExports_SingleExe.cpp) call these, and the ROM-free
 *        row MMWarpDriveSeam (src/common/tests/test_mm_warp_drive.c) locks them.
 *
 * THE INPUT SEAM. Majora's Mask has no emulated-buttons API (2Ship's
 * GameInteractor has none, and SoH's GameInteractor_SetEmulatedButtons is only
 * consumed by OoT's padmgr). MM's PadMgr_UpdateInputs (games/mm/src/code/padmgr.c)
 * ORs an MM-owned latch into controller 1's current buttons through
 * Combo_TestPad_Apply, which also zeroes the latch, so a stale value never lives
 * past one pad update. The latch's only writer is MM_PadMgr_SetTestButtons, which
 * refuses anything but 0 outside an integration run. Because the press and
 * release edges are computed by vanilla code from cur and prev, Player, the
 * message system and the ocarina reader all see real presses.
 */

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* N64 controller button bits (libultra's A_BUTTON, U_CBUTTONS, D_CBUTTONS,
 * L_CBUTTONS, R_CBUTTONS), spelled here so this module stays game-header-free;
 * MMWarpSongMatcher's leg M10 checks them against MM's BTN_* macros. */
#define RSBS_MWD_PAD_A 0x8000u
#define RSBS_MWD_PAD_CUP 0x0008u
#define RSBS_MWD_PAD_CDOWN 0x0004u
#define RSBS_MWD_PAD_CLEFT 0x0002u
#define RSBS_MWD_PAD_CRIGHT 0x0001u

/* The row's note schedule: each note held this many MM frames, then released this
 * many. MM's reader drops the frame after a pitch change (MM_sOcarinaDropInputTimer,
 * code_8019AF00.c), so the gap must be at least 2; M10 plays this exact schedule
 * through MM's real AudioOcarina_Update. */
#define RSBS_MWD_NOTE_HOLD 4
#define RSBS_MWD_NOTE_GAP 4

/** cur | *latch, then *latch = 0. A NULL latch, or a zero one, returns cur. */
uint16_t Combo_TestPad_Apply(uint16_t cur, uint16_t* latch);

/** The pad bit of MM's OCARINA_BTN index @p ocarinaBtn (A 0, C-down 1, C-right 2,
 *  C-left 3, C-up 4); 0 for anything else. */
uint16_t Combo_MMWarpDrive_PadButton(uint8_t ocarinaBtn);

/** The pad bit of note @p note (0-based) of warp song @p song (OoTMM's buttons,
 *  Combo_MMWarpPlay_Buttons); 0 out of range. */
uint16_t Combo_MMWarpDrive_NotePad(int song, int note);

/** How many notes warp song @p song has; 0 out of range. */
int Combo_MMWarpDrive_NoteCount(int song);

/**
 * Where OoT must land for warp song @p song under a frozen record whose flags are
 * @p recFlags and whose destination for that song is @p recDest: the record's
 * destination when it says SHUFFLED (RSBS_OOT_WARP_REC_SHUFFLED), otherwise the
 * song's vanilla pad (Combo_MMWarpPlay_PadEntrance). 0 for a song out of range.
 * Derived, never a literal: the row asks this for Minuet and checks the scene of
 * the answer too.
 */
uint16_t Combo_MMWarpDrive_ExpectedArrival(int song, uint8_t recFlags, uint16_t recDest);

/**
 * Is a one-shot warp target (ComboWarp_Peek's fields) the crossing warp song
 * @p song armed in Majora's Mask: armed, from MM at @p wantMarker (the entrance
 * Link stood at), to OoT's pad for @p song, of the warp-song kind?
 */
bool Combo_MMWarpDrive_TargetIsCrossing(bool armed, int sourceGame, uint16_t marker, int targetGame,
                                        uint16_t targetEntrance, uint8_t kind, uint16_t wantMarker, int song);

/**
 * Is a pending game switch (the target already spent by the transition's check)
 * that crossing: requested, to OoT, to @p song's pad, of the warp-song kind?
 */
bool Combo_MMWarpDrive_SwitchIsCrossing(bool requested, int targetGame, uint16_t targetEntrance, uint8_t kind,
                                        int song);

#ifdef __cplusplus
}
#endif
