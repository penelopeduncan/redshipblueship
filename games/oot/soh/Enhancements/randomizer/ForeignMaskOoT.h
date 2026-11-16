#pragma once
/**
 * @file ForeignMaskOoT.h
 * @brief OoT wears and draws a Majora's Mask mask (#977, epic #975 P3). The
 *        definitions and the reasoning are in ForeignMaskOoT.cpp; this header is
 *        what the head limb draw (z_player_lib.c), the child trade cycle
 *        (ShuffleTradeItems.c) and the items P4 adds call. C-compatible.
 */
#ifdef RSBS_SINGLE_EXECUTABLE

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct PlayState;
struct Player;

/**
 * The head limb's post-limb draw (OoT_Player_PostLimbDrawGameplay): draws the
 * worn MM mask in the head limb's space from redship-oot.o2r. `headDrawn` is 0
 * when the head list was not drawn (first person), and then nothing is.
 */
void OoT_ForeignMask_DrawOnHead(struct PlayState* play, struct Player* player, int headDrawn);

/**
 * The list OoT_ForeignMask_DrawOnHead draws for `player`, or NULL: the worn MM
 * mask's table list when Link may wear it at his age (a child, or an adult
 * wearing the Blast or Stone Mask while OoT_ForeignMask_AdultMayWear()) with no
 * native mask on (currentMask) and the head list was drawn (`headDrawn`).
 * Whether a mounted archive holds the path, and whether `player` is the real
 * player, are the draw's checks. The For variant takes the adult's permission
 * as `adultMayWear` and reads no CVar (the ROM-free rows call it).
 */
const char* OoT_ForeignMask_WornDl(struct Player* player, int headDrawn);
const char* OoT_ForeignMask_WornDlFor(struct Player* player, int headDrawn, int adultMayWear);

/**
 * May an adult Link wear the Blast and Stone Masks (operator 2026-11-15, "Make
 * them adult-wearable")? OoT_MasksEquippableAsAdult(), the reader the eight
 * native masks follow: the Masks Equippable as Adult enhancement, the loaded
 * randomizer file's Masks as Adult seed setting or the Timeless Equipment
 * cheat. Reads CVars: never call it without a Ship::Context.
 */
int OoT_ForeignMask_AdultMayWear(void);

/** The worn MM mask: the stored ComboForeignMask when OoT wears it and the
 *  frozen world added it (Combo_ForeignMask_Wearable), else NONE. */
uint8_t OoT_ForeignMask_Worn(void);

/**
 * Use (put on, or take off when worn) MM mask `mask`: the equip path the items
 * P4 adds call from VB_CHANGE_HELD_ITEM_AND_USE_ITEM. Putting one on takes the
 * native mask off (currentMask and SoH's maskMemory to NONE). Refused (0, no
 * change) for an adult (except the Blast and Stone Masks while
 * OoT_ForeignMask_AdultMayWear()), for NONE, for anything OoT does not wear and
 * for a mask whose extension rule the world did not freeze on; 1 when the worn
 * field changed. The live call reads the adult's permission only for an adult;
 * the For variant takes it as `adultMayWear` and reads no CVar.
 */
int OoT_ForeignMask_Use(struct Player* player, uint8_t mask);
int OoT_ForeignMask_UseFor(struct Player* player, uint8_t mask, int adultMayWear);

/**
 * OoT's mask rules, applied to the worn field once per player update: a native
 * mask on (currentMask) or an adult Link who may not wear it (see
 * OoT_ForeignMask_Use) takes it off, and a value OoT does not wear (or whose
 * rule is off) is cleared. The For variant takes the adult's permission.
 */
void OoT_ForeignMask_ApplyRules(struct Player* player);
void OoT_ForeignMask_ApplyRulesFor(struct Player* player, int adultMayWear);

/**
 * Player_Init's rule (z_player.c, "keep masks thru loading zones"): a mask
 * comes off at every scene load unless SoH's Persistent Masks is on, and then
 * also when the child trade slot is sold out or the Master Sword pedestal
 * asked for it (OoT_ForeignMask_DropAtNextInit); then the rules, with the
 * adult's permission `adultMayWear`. `persistentMasks` is that CVar.
 */
void OoT_ForeignMask_OnPlayerInit(struct Player* player, int persistentMasks, int adultMayWear);

/**
 * The Master Sword pedestal (z_player.c, func_8083A0F4), where a native mask's
 * maskMemory is cleared: the worn MM mask comes off at the next Player_Init,
 * as the native one does, not at once (it stays on through the cutscene).
 * Forgotten at every file load.
 */
void OoT_ForeignMask_DropAtNextInit(void);

/**
 * The pause screen's age rows for the Blast and Stone Masks, as z_play.c and
 * z_kaleido_item.c set the eight native masks' (#1647): both custom ItemIDs'
 * gItemAgeReqs rows to any age while `adultMayWear`, else the child's, and the
 * child trade slot's gSlotAgeReqs to any age while `adultMayWear` and it holds
 * one of them (never restricted here). Reads no CVar.
 */
void OoT_ForeignMask_ApplyAgeReqs(int adultMayWear);

/**
 * The child trade slot's cycle (Randomizer_Get{Next,Prev}ChildTradeItem):
 * list-based, so custom ItemIDs can join it (#977). The list is OoT's eleven
 * contiguous trade items followed by every registered extra; with no extra it
 * returns exactly what the contiguous cycle did.
 */
uint8_t OoT_ChildTradeCycle_Next(void);
uint8_t OoT_ChildTradeCycle_Prev(void);

/** Whether the player owns a registered extra (P4: its RAND_INF flag). */
typedef int (*OoTChildTradeOwnedFn)(void);

/**
 * Add custom ItemID `itemId` to the child trade cycle (P4's masks). 1 when
 * added, 0 when the id is already a member or the list is full. Registration
 * order is cycle order.
 */
int OoT_ChildTradeCycle_RegisterExtra(uint8_t itemId, OoTChildTradeOwnedFn owned);

/** The worn field's raw byte and its offset in SaveContext (0: no field).
 *  The locks read and write the field through these, never by name. */
uint8_t OoT_ForeignMask_StoredRaw(void);
void OoT_ForeignMask_SetStoredRaw(uint8_t value);
size_t OoT_ForeignMask_FieldOffset(void);

/** TEST ONLY (the OoTChildTradeCycle row): forget every registered extra. */
void OoT_ChildTradeCycle_ClearExtrasForTest(void);

#ifdef __cplusplus
}
#endif

#endif // RSBS_SINGLE_EXECUTABLE
