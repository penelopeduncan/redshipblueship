#ifndef RANDO_STICK_NUT_BAG_GATE_H
#define RANDO_STICK_NUT_BAG_GATE_H

/*
 * #1652 A1: Majora's Mask's half of the pair's one Deku Stick Bag and Deku Nut
 * Bag (src/common/shared_resources.h, "THE PAIR'S ONE DEKU STICK BAG AND DEKU NUT
 * BAG"). Under the frozen bag shuffle a paired MM half is born at capacity tier 0
 * with none of the item, and MM's give, drops, shop shelves and obtainability
 * refuse sticks (or nuts) until the pair's one bag arrives. Every decision is
 * src/common's (Combo_SharedBag*); these only read MM's own capacity tier.
 *
 * Defined in games/mm/2s2h/GameExports_SingleExe.cpp; included there and by
 * every caller (z_parameter.c, z_en_girla.c, ConvertItem.cpp, EnItem00.cpp,
 * EnBox.cpp, OnFileCreate.cpp, ComboLogicEngineSingleExe.cpp), so each signature is checked
 * against the definition. C-compatible: two of the callers are C.
 */

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** 1 while the frozen stick bag shuffle holds MM's sticks out (MM's
 *  UPG_DEKU_STICKS tier is 0); 0 otherwise, always with the shuffle off. */
int MM_Combo_StickGated(void);

/** The same for nuts (UPG_DEKU_NUTS, the frozen nut bag shuffle). */
int MM_Combo_NutGated(void);

/** 1 when MM's give must refuse ITEM_* @p item: a stick (ITEM_DEKU_STICK,
 *  _STICKS_5, _STICKS_10) under MM_Combo_StickGated, or a nut (ITEM_DEKU_NUT,
 *  _NUTS_5, _NUTS_10) under MM_Combo_NutGated. MM_Item_Give returns @p item
 *  untouched then, as OoT_Item_Give does. */
int MM_Combo_StickNutGiveRefused(uint8_t item);

/** 1 when an EnItem00 with @p params (the low byte is the ITEM00_* type) is a
 *  stick or nut drop the gate holds out: the actor is not spawned. */
int MM_Combo_Item00Gated(int16_t params);

/** #1691: 1 when an offer of MM's GI_* @p getItemId must not be made: a stick
 *  (GI_DEKU_STICKS_1) under MM_Combo_StickGated or a nut (GI_DEKU_NUTS_1, _5,
 *  _10) under MM_Combo_NutGated. A Deku Baba's or withered / Mini Baba's stick
 *  (EnItem00.cpp) and an unshuffled chest's item (EnBox.cpp) are asked here. */
int MM_Combo_GetItemGated(int16_t getItemId);

/** The birth flip: a paired MM half authored under the frozen stick (nut) bag
 *  shuffle drops to tier 0 with no item and no ammo. Each bag on its own; a bag
 *  whose shuffle is off is not touched. Called only for a paired creation. */
void MM_Combo_ApplyPairedBagBirth(void);

/** The RI_* id a crossed MM item @p riId is redeemed as (defined in
 *  Rando/ForeignItemsSingleExe.cpp, called by its give): @p riId itself, except a
 *  stick or nut pack (RI_DEKU_STICK, _STICKS_5, RI_DEKU_NUT, _NUTS_5, _NUTS_10)
 *  that MM cannot obtain now (the gates above, through Rando::IsItemObtainable),
 *  which becomes a junk item MM can hold, never "nothing" (CurrentPaidJunkItem).
 *  Draws from MM's random stream only in that case. */
uint16_t MM_ForeignItem_RedeemAs(uint16_t riId);

#ifdef __cplusplus
}
#endif

#endif // RANDO_STICK_NUT_BAG_GATE_H
