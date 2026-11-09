#pragma once
/**
 * @file ForeignMaskMM.h
 * @brief MM wears and draws an Ocarina of Time mask (#977, epic #975 P3). The
 *        definitions and the reasoning are in ForeignMaskMM.cpp; this header is
 *        what the mask page (z_kaleido_mask.c), the locks and the items P5 adds
 *        call. C-compatible.
 */
#ifdef RSBS_SINGLE_EXECUTABLE

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct Player;
struct PlayState;

/** The worn OoT mask: the stored ComboForeignMask when MM wears it and the
 *  frozen world added it (Combo_ForeignMask_Wearable), else NONE. */
uint8_t MM_ForeignMask_Worn(void);

/** Put `mask` on (or take it off when worn): human form only, and only a mask
 *  the frozen world added. Putting one on takes the native mask off. 1 when the
 *  worn field changed, else 0. P5's items call this. */
int MM_ForeignMask_Use(struct Player* player, uint8_t mask);

/** MM's mask rules on the worn field, once per player update: a transformation,
 *  a native mask put on, or a rule that is off takes it off. */
void MM_ForeignMask_ApplyRules(struct Player* player);

/** A shared pause slot's chosen variant: an owned OoT mask sharing it, or NONE
 *  (the slot's own mask). */
uint8_t MM_ForeignMask_SlotSelection(uint8_t slot);

/** Cycle a shared slot's variant (dir > 0 forward). 1 when it changed. */
int MM_ForeignMask_CycleSlot(uint8_t slot, int dir);

/** How many variants a shared slot cycles through (below 2: no cycling). */
int MM_ForeignMask_SlotVariantCount(uint8_t slot);

/** VB_USE_ITEM_EQUIP_MASK: native mask `maskId`'s C button was used. 1 when its
 *  slot's chosen foreign variant was used instead (the vanilla equip must not
 *  run), else 0. */
int MM_ForeignMask_OnEquipNative(struct Player* player, int maskId);

/** The mask page's icon for cell `maskSlot` (0-based), whose own item's icon is
 *  `nativeIcon`: the chosen variant's icon on a shared slot when
 *  redship-mm.o2r holds it, else `nativeIcon`. */
const char* MM_ForeignMask_PageIcon(int maskSlot, const char* nativeIcon);

/** #980: the item mask-page cell `maskSlot` (0-based) shows, whose inventory holds
 *  `stored`: `stored`, or for a shared slot whose own mask is not owned while MM
 *  owns an OoT mask sharing it, the slot's own ItemId (the inventory is never
 *  written). */
uint8_t MM_ForeignMask_PageCellItem(int maskSlot, uint8_t stored);

/** #1500: the OoT mask MM item `item` stands for wherever MM draws or names it (a
 *  shared slot's own item: the slot's live variant), or NONE. The button keeps the
 *  native ItemId; every label below reads this. */
uint8_t MM_ForeignMask_ItemShows(int item);

/** #1500: the icon for MM item `item` (C button, D-pad, equip flight, page):
 *  the OoT mask it stands for when mounted, else `nativeIcon`. */
const char* MM_ForeignMask_ItemIcon(int item, const char* nativeIcon);

/** #1500: Ocarina of Time's pause name texture for an OoT mask MM wears, or NULL. */
const char* MM_ForeignMask_NameTexture(uint8_t mask);

/** #1500: the pause name texture for MM item `item`: the OoT mask's when it stands
 *  for one and the texture is mounted, else `nativeName`. */
const char* MM_ForeignMask_ItemName(int item, const char* nativeName);

/** #1500: the A-press description of an OoT mask MM wears, or NULL. */
const char* MM_ForeignMask_DescriptionText(uint8_t mask);

/** #1500: the A-press description MM item `item` opens instead of its own, or NULL. */
const char* MM_ForeignMask_ItemDescription(int item);

/** #1500: the name texture MM item `item` asks for before the mount check (the OoT
 *  mask it stands for's), or NULL for the item's own name. */
const char* MM_ForeignMask_ItemNameTexture(int item);

/** #1500: what A on mask-page item `item` did (VB_KALEIDO_DISPLAY_ITEM_TEXT).
 *  VANILLA leaves MM's own description; CYCLED cycled the shared slot's variant
 *  (the caller plays the cursor sound); DESCRIBED opened the description of the
 *  OoT mask the cell shows. `vanillaShows` is the hook's `*should` as it arrives:
 *  0 (another handler took the press) describes nothing. */
enum { MM_FOREIGN_MASK_PRESS_A_VANILLA = 0, MM_FOREIGN_MASK_PRESS_A_CYCLED = 1, MM_FOREIGN_MASK_PRESS_A_DESCRIBED = 2 };
int MM_ForeignMask_OnPressA(struct PlayState* play, int item, int vanillaShows);

/** #1500 test seam: what DESCRIBED calls. The default opens pause text 0x1700 +
 *  item through func_801514B0, as z_kaleido_mask.c does, and writes the OoT
 *  mask's text over it; a test passes its own to see the call, NULL restores
 *  the default. */
typedef void (*MM_ForeignMaskDescriptionOpener)(struct PlayState* play, uint16_t textId, uint8_t boxPos,
                                                const char* text);
void MM_ForeignMask_SetDescriptionOpener(MM_ForeignMaskDescriptionOpener opener);

/** The list MM's head draw would emit for the live worn field, or NULL. */
const char* MM_ForeignMask_WornDl(struct Player* player);

/** The fields' raw bytes and the worn field's offset in SaveContext (0: no
 *  field). The locks read and write the fields through these, never by name. */
uint8_t MM_ForeignMask_StoredRaw(void);
void MM_ForeignMask_SetStoredRaw(uint8_t value);
uint8_t MM_ForeignMask_SlotSelRaw(uint8_t slot);
void MM_ForeignMask_SetSlotSelRaw(uint8_t slot, uint8_t value);
size_t MM_ForeignMask_FieldOffset(void);

#ifdef __cplusplus
}
#endif

#endif // RSBS_SINGLE_EXECUTABLE
