/**
 * @file mm_warp_song_drive.c
 * @brief The pure half of IntMMWarpSongMinuet (#1169 lane XW13). The reasoning is
 *        in mm_warp_song_drive.h.
 */

#include "mm_warp_song_drive.h"

#include "context.h"  // RSBS_OOT_WARP_REC_SHUFFLED
#include "entrance.h" // COMBO_SWITCH_KIND_WARP_SONG
#include "game.h"     // GAME_OOT, GAME_MM
#include "mm_warp_song_play.h"

#include <stddef.h>

uint16_t Combo_TestPad_Apply(uint16_t cur, uint16_t* latch) {
    if (latch == NULL) {
        return cur;
    }
    const uint16_t out = (uint16_t)(cur | *latch);
    *latch = 0u;
    return out;
}

uint16_t Combo_MMWarpDrive_PadButton(uint8_t ocarinaBtn) {
    switch (ocarinaBtn) {
        case 0:
            return (uint16_t)RSBS_MWD_PAD_A;
        case 1:
            return (uint16_t)RSBS_MWD_PAD_CDOWN;
        case 2:
            return (uint16_t)RSBS_MWD_PAD_CRIGHT;
        case 3:
            return (uint16_t)RSBS_MWD_PAD_CLEFT;
        case 4:
            return (uint16_t)RSBS_MWD_PAD_CUP;
        default:
            return 0u;
    }
}

int Combo_MMWarpDrive_NoteCount(int song) {
    uint8_t n = 0;
    return Combo_MMWarpPlay_Buttons(song, &n) != NULL ? (int)n : 0;
}

uint16_t Combo_MMWarpDrive_NotePad(int song, int note) {
    uint8_t n = 0;
    const uint8_t* buttons = Combo_MMWarpPlay_Buttons(song, &n);
    if (buttons == NULL || note < 0 || note >= (int)n) {
        return 0u;
    }
    return Combo_MMWarpDrive_PadButton(buttons[note]);
}

uint16_t Combo_MMWarpDrive_ExpectedArrival(int song, uint8_t recFlags, uint16_t recDest) {
    const uint16_t pad = Combo_MMWarpPlay_PadEntrance(song);
    if (pad == 0u) {
        return 0u;
    }
    return (recFlags & RSBS_OOT_WARP_REC_SHUFFLED) != 0u ? recDest : pad;
}

bool Combo_MMWarpDrive_TargetIsCrossing(bool armed, int sourceGame, uint16_t marker, int targetGame,
                                        uint16_t targetEntrance, uint8_t kind, uint16_t wantMarker, int song) {
    const uint16_t pad = Combo_MMWarpPlay_PadEntrance(song);
    return armed && pad != 0u && sourceGame == (int)GAME_MM && marker == wantMarker && targetGame == (int)GAME_OOT &&
           targetEntrance == pad && kind == (uint8_t)COMBO_SWITCH_KIND_WARP_SONG;
}

bool Combo_MMWarpDrive_SwitchIsCrossing(bool requested, int targetGame, uint16_t targetEntrance, uint8_t kind,
                                        int song) {
    const uint16_t pad = Combo_MMWarpPlay_PadEntrance(song);
    return requested && pad != 0u && targetGame == (int)GAME_OOT && targetEntrance == pad &&
           kind == (uint8_t)COMBO_SWITCH_KIND_WARP_SONG;
}
