#ifndef OOT_RANDO_STICK_NUT_BAG_GATE_H
#define OOT_RANDO_STICK_NUT_BAG_GATE_H

/*
 * #1691: Ocarina of Time's twin of games/mm/2s2h/Rando/StickNutBagGate.h, for
 * the stick and nut sources that are not an ITEM00 drop (OoT's actor-init hook
 * holds those out, hook_handlers.cpp). Under the pair's frozen Deku Stick Bag /
 * Deku Nut Bag shuffle (#1652) OoT's own give refuses sticks (nuts) before the
 * bag (z_parameter.c), so a source that OFFERS one would play the get-item scene
 * and hand nothing. Every decision is src/common's (Combo_SharedBagGateClosed);
 * these only read OoT's own capacity tier, never a CVar or a PlayState.
 *
 * Defined in games/oot/soh/GameExports_SingleExe.cpp; included there and by every
 * caller (hook_handlers.cpp, z_en_dns.c), so each signature is checked against
 * the definition. C-compatible: z_en_dns.c is C.
 */

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** 1 while the frozen stick bag shuffle holds OoT's sticks out (OoT's
 *  UPG_STICKS tier is 0); 0 otherwise, always with the shuffle off. */
int OoT_Combo_StickGated(void);

/** The same for nuts (UPG_NUTS, the frozen nut bag shuffle). */
int OoT_Combo_NutGated(void);

/** 1 when an offer of OoT's GI_* @p getItemId must not be made: a stick
 *  (GI_STICKS_1, _5, _10) under OoT_Combo_StickGated or a nut (GI_NUTS_5,
 *  GI_NUTS_5_2, GI_NUTS_10) under OoT_Combo_NutGated. A Deku Baba's or withered
 *  Deku Baba's stick (GI_STICKS_1) is asked here. */
int OoT_Combo_GetItemGated(int16_t getItemId);

#ifdef __cplusplus
}
#endif

#endif // OOT_RANDO_STICK_NUT_BAG_GATE_H
