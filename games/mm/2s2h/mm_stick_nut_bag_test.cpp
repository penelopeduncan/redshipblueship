/**
 * @file mm_stick_nut_bag_test.cpp
 * MM's half of the mm-stick-nut-bag-gate lock (#1652 A1 and A3).
 *
 * THE HELPERS the redship row `mm-stick-nut-bag-gate`
 * (src/common/tests/test_stick_nut_bag_mm.c) drives MM's REAL give
 * (MM_Item_Give, Rando::GiveItem), obtainability and conversion
 * (Rando::IsItemObtainable, Rando::ConvertItem, Rando::CurrentJunkItem) through.
 * They only name MM's ids and put MM's live save in a known state; every decision
 * is the production code's (StickNutBagGate.h over src/common's Combo_SharedBag*).
 * They live here because only an MM TU has MM's enums and `gSaveContext` in scope.
 * Nothing here reads a CVar, OTRGlobals or MM_gPlayState: the give's stick and nut
 * branches touch gSaveContext only (z_parameter.c).
 */

#include "global.h"

#include <cstring>

#if !defined(RSBS_SINGLE_EXECUTABLE)
// The pair's bag gate is a single-exe surface; test_runner.cpp declares these
// unconditionally, so a standalone 2ship build links.
extern "C" void MM_SnbTest_ArmBorn(void) {
}
extern "C" void MM_SnbTest_SetBag(int nut, int tier, int held, int ammo) {
    (void)nut;
    (void)tier;
    (void)held;
    (void)ammo;
}
extern "C" int MM_SnbTest_Tier(int nut) {
    (void)nut;
    return -1;
}
extern "C" int MM_SnbTest_Ammo(int nut) {
    (void)nut;
    return -1;
}
extern "C" int MM_SnbTest_Held(int nut) {
    (void)nut;
    return -1;
}
extern "C" int MM_SnbTest_ItemId(int which) {
    (void)which;
    return -1;
}
extern "C" int MM_SnbTest_GetItemId(int which) {
    (void)which;
    return -1;
}
extern "C" int MM_SnbTest_Give(int item) {
    (void)item;
    return -1;
}
extern "C" int MM_SnbTest_RandoId(int which) {
    (void)which;
    return -1;
}
extern "C" int MM_SnbTest_Obtainable(int ri) {
    (void)ri;
    return -1;
}
extern "C" int MM_SnbTest_Convert(int ri) {
    (void)ri;
    return -1;
}
extern "C" int MM_SnbTest_ColdJunkDraw(void) {
    return -1;
}
extern "C" void MM_SnbTest_GiveRando(int ri) {
    (void)ri;
}
extern "C" int MM_SnbTest_Item00(int which) {
    (void)which;
    return -1;
}
#else

extern "C" {
#include "z64save.h"
extern SaveContext gSaveContext;
}

#include "2s2h/Rando/Rando.h"

extern "C" void MM_Rando_TestColdJunkCache(void); // ConvertItem.cpp

namespace {

s16 SnbMmUpgrade(int nut) {
    return nut ? (s16)UPG_DEKU_NUTS : (s16)UPG_DEKU_STICKS;
}

u8 SnbMmItem(int nut) {
    return nut ? (u8)ITEM_DEKU_NUT : (u8)ITEM_DEKU_STICK;
}

} // namespace

/** MM's live save as a fresh MM save is born (z_sram_NES.c's sSaveDefaultInventory):
 *  every item slot ITEM_NONE, no ammo, stick and nut capacity tier 1; the 0xFF
 *  "no flash slot" sentinel and GAMEMODE_NORMAL (a live file, as the shared
 *  shims require), human form, everything else zero. */
extern "C" void MM_SnbTest_ArmBorn(void) {
    memset(&gSaveContext, 0, sizeof(SaveContext));
    memset(gSaveContext.save.saveInfo.inventory.items, ITEM_NONE, sizeof(gSaveContext.save.saveInfo.inventory.items));
    gSaveContext.fileNum = 0xFF;
    gSaveContext.flashSaveAvailable = true;
    gSaveContext.gameMode = GAMEMODE_NORMAL;
    gSaveContext.save.playerForm = PLAYER_FORM_HUMAN;
    MM_Inventory_ChangeUpgrade(UPG_DEKU_STICKS, 1);
    MM_Inventory_ChangeUpgrade(UPG_DEKU_NUTS, 1);
}

/** Sticks (@p nut 0) or nuts (1) at capacity @p tier, the item in its slot when
 *  @p held, holding @p ammo. */
extern "C" void MM_SnbTest_SetBag(int nut, int tier, int held, int ammo) {
    MM_Inventory_ChangeUpgrade(SnbMmUpgrade(nut), (u32)tier);
    INV_CONTENT(SnbMmItem(nut)) = held ? SnbMmItem(nut) : (u8)ITEM_NONE;
    AMMO(SnbMmItem(nut)) = (s8)ammo;
}

extern "C" int MM_SnbTest_Tier(int nut) {
    return (int)CUR_UPG_VALUE(SnbMmUpgrade(nut));
}

extern "C" int MM_SnbTest_Ammo(int nut) {
    return (int)AMMO(SnbMmItem(nut));
}

extern "C" int MM_SnbTest_Held(int nut) {
    return INV_CONTENT(SnbMmItem(nut)) == SnbMmItem(nut) ? 1 : 0;
}

/** MM's ITEM_* ids by index: 0 ITEM_DEKU_STICK, 1 _STICKS_5, 2 _STICKS_10,
 *  3 ITEM_DEKU_NUT, 4 _NUTS_5, 5 _NUTS_10, 6 ITEM_BOMB (not gated); -1 past it. */
extern "C" int MM_SnbTest_ItemId(int which) {
    static const int kIds[] = { ITEM_DEKU_STICK, ITEM_DEKU_STICKS_5, ITEM_DEKU_STICKS_10,
                                ITEM_DEKU_NUT,   ITEM_DEKU_NUTS_5,   ITEM_DEKU_NUTS_10,
                                ITEM_BOMB };
    return (which >= 0 && which < (int)(sizeof(kIds) / sizeof(kIds[0]))) ? kIds[which] : -1;
}

/** #1691: MM's GI_* ids by index: 0 GI_DEKU_STICKS_1 (a Deku Baba's stick),
 *  1 GI_DEKU_NUTS_1, 2 _NUTS_5, 3 _NUTS_10, 4 GI_RUPEE_BLUE (not gated); -1 past it. */
extern "C" int MM_SnbTest_GetItemId(int which) {
    static const int kIds[] = { GI_DEKU_STICKS_1, GI_DEKU_NUTS_1, GI_DEKU_NUTS_5, GI_DEKU_NUTS_10, GI_RUPEE_BLUE };
    return (which >= 0 && which < (int)(sizeof(kIds) / sizeof(kIds[0]))) ? kIds[which] : -1;
}

/** MM's REAL give, MM_Item_Give(NULL, @p item); its return value. */
extern "C" int MM_SnbTest_Give(int item) {
    return (int)MM_Item_Give(nullptr, (u8)item);
}

/** MM's RI_* ids by index: 0 RI_DEKU_STICK, 1 RI_DEKU_STICKS_5, 2 RI_DEKU_NUT,
 *  3 RI_DEKU_NUTS_5, 4 RI_DEKU_NUTS_10, 5 RI_JUNK, 6 RI_NONE, 7 RI_UNKNOWN,
 *  8 RI_ARROWS_10 (MM's own ammo-without-capacity row); -1 past it. */
extern "C" int MM_SnbTest_RandoId(int which) {
    static const int kIds[] = { RI_DEKU_STICK, RI_DEKU_STICKS_5, RI_DEKU_NUT, RI_DEKU_NUTS_5, RI_DEKU_NUTS_10,
                                RI_JUNK,       RI_NONE,          RI_UNKNOWN,  RI_ARROWS_10 };
    return (which >= 0 && which < (int)(sizeof(kIds) / sizeof(kIds[0]))) ? kIds[which] : -1;
}

/** Rando::IsItemObtainable(@p ri), no check. */
extern "C" int MM_SnbTest_Obtainable(int ri) {
    return Rando::IsItemObtainable((RandoItemId)ri) ? 1 : 0;
}

/** Rando::ConvertItem(@p ri), no check. */
extern "C" int MM_SnbTest_Convert(int ri) {
    return (int)Rando::ConvertItem((RandoItemId)ri);
}

/** One junk draw from a cold cache (Rando::CurrentJunkItem caches its draw while
 *  no PlayState advances the frame counter, so each draw starts cold). */
extern "C" int MM_SnbTest_ColdJunkDraw(void) {
    MM_Rando_TestColdJunkCache();
    return (int)Rando::CurrentJunkItem();
}

/** MM's REAL rando give, Rando::GiveItem(@p ri) (its default branch is
 *  MM_Item_Give, as Consumables Full's give is, StartingItems.cpp). */
extern "C" void MM_SnbTest_GiveRando(int ri) {
    Rando::GiveItem((RandoItemId)ri);
}

/** EnItem00 params by index: 0 ITEM00_DEKU_STICK, 1 ITEM00_DEKU_NUTS_1,
 *  2 ITEM00_DEKU_NUTS_10, 3 ITEM00_NOTHING, 4 ITEM00_RUPEE_GREEN, 5 ITEM00_BOMBS_A,
 *  6 ITEM00_DEKU_STICK with collectible flag 3 in its high byte (as a scene's
 *  actor carries it); -1 past it. */
extern "C" int MM_SnbTest_Item00(int which) {
    static const int kParams[] = {
        ITEM00_DEKU_STICK,  ITEM00_DEKU_NUTS_1, ITEM00_DEKU_NUTS_10,          ITEM00_NOTHING,
        ITEM00_RUPEE_GREEN, ITEM00_BOMBS_A,     ITEM00_DEKU_STICK | (3 << 8),
    };
    return (which >= 0 && which < (int)(sizeof(kParams) / sizeof(kParams[0]))) ? kParams[which] : -1;
}

#endif
