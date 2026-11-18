#pragma once
/**
 * @file BlastMaskOoT.h
 * @brief Majora's Mask's Blast Mask as an Ocarina of Time item (#979, epic #975
 *        P4; OoTMM blastMaskOot, sharedMaskBlast, blastMaskCooldown). The
 *        definitions and the reasoning are in BlastMaskOoT.cpp; this header is
 *        what the pool, the logic, the worn-mask draw and the locks call.
 *        C-compatible.
 */
#ifdef RSBS_SINGLE_EXECUTABLE

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Is the frozen Blast Mask (OoT) rule on (RSBS_XITEM_BLAST_MASK_OOT)? False
 *  while nothing is frozen. The pool adds the item and the logic credits it only
 *  then (OoTMM's has_mask_blast: setting(blastMaskOot)). */
bool OoT_BlastMaskOoTOn(void);

/** The frames left on the worn Blast Mask's cooldown (0: ready). The worn-mask
 *  draw shows MM's glow pass while it is not 0 (ForeignMaskOoT.cpp). */
uint16_t OoT_BlastMask_CooldownLeft(void);

/** Register OoT's ownership source and the child trade cycle's extra for the
 *  Blast Mask (idempotent), and give its ItemID its age row. Run at start-up and
 *  at every file load; the locks call it after rows that clear them. */
void OoT_BlastMask_RegisterRuntime(void);

struct PlayState;
struct Player;

/** The worn Blast Mask's B press (#1718; OoTMM's Player_ProcessItemButtonsWrapper).
 *  z_player.c calls it from OoT_Player_UpdateItems just before
 *  OoT_Player_ProcessItemButtons, inside that function's own gate, so only on a
 *  frame on which the item buttons are processed. `bPressed` is whether B is
 *  pressed in Link's input that frame. Returns 1 when the press is the mask's (it
 *  is worn and PlayerFreeToBlast holds): the caller takes B out of Link's input,
 *  and a blast went off if the cooldown was spent. Returns 0 otherwise. */
int OoT_BlastMask_OnItemButtons(struct PlayState* play, struct Player* player, int bPressed);

/** TEST ONLY: the frames left on the cooldown, set directly. */
void OoT_BlastMask_SetCooldownForTest(uint16_t frames);

/** Register the Blast Mask's GameInteractor hooks (idempotent; ShipInit runs it at boot). */
void OoT_BlastMask_RegisterHooks(void);

#ifdef __cplusplus
}
#endif

#endif // RSBS_SINGLE_EXECUTABLE
