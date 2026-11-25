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
 *
 * #1788: the shelf helpers (MM_SnbTest_Shelf*, MM_SnbTest_Hags*) drive the REAL
 * shelf functions a shopkeeper calls (EnGirlA.cpp: EnGirlA_RandoCanBuyFunc,
 * EnGirlA_RandoBuyFunc, EnGirlA_RandoRestock, ShopShelfLocked, ShopShelfItem,
 * ShopShelfSlotSold, the Hags' arming) against a shelf actor and a PlayState
 * calloc'd here, as ForeignShopSingleExe.cpp does: those functions read only the
 * actor's check id, play->msgCtx.unk1206C (the price) and gSaveContext. The
 * global MM_gPlayState is never set.
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
extern "C" int MM_SnbTest_ShelfCheck(int slot) {
    (void)slot;
    return -1;
}
extern "C" void MM_SnbTest_PlaceShelf(int slot, int ri, int obtained, int eligible) {
    (void)slot;
    (void)ri;
    (void)obtained;
    (void)eligible;
}
extern "C" int MM_SnbTest_ShelfForeign(int slot) {
    (void)slot;
    return -1;
}
extern "C" int MM_SnbTest_ShelfLocked(int slot) {
    (void)slot;
    return -1;
}
extern "C" int MM_SnbTest_ShelfItem(int slot) {
    (void)slot;
    return -1;
}
extern "C" int MM_SnbTest_ShelfNamesItem(int slot, int ri) {
    (void)slot;
    (void)ri;
    return -1;
}
extern "C" int MM_SnbTest_BombShopHandIsShelfItem(void) {
    return -1;
}
extern "C" int MM_SnbTest_ShelfSold(int slot) {
    (void)slot;
    return -1;
}
extern "C" int MM_SnbTest_ShelfRestockShows(int slot) {
    (void)slot;
    return -1;
}
extern "C" int MM_SnbTest_ShelfCanBuy(int slot, int rupees, int price) {
    (void)slot;
    (void)rupees;
    (void)price;
    return -1;
}
extern "C" int MM_SnbTest_ShelfBuy(int slot, int price) {
    (void)slot;
    (void)price;
    return -1;
}
extern "C" int MM_SnbTest_ShelfObtained(int slot) {
    (void)slot;
    return -1;
}
extern "C" int MM_SnbTest_HagsArmable(void) {
    return -1;
}
extern "C" int MM_SnbTest_HagsLineArms(int* outFreePotion) {
    *outFreePotion = -1;
    return -1;
}
#else

extern "C" {
#include "z64save.h"
extern SaveContext gSaveContext;
}

#include "2s2h/Rando/Rando.h"
#include "2s2h/Rando/ActorBehavior/ActorBehavior.h"
#include "2s2h/Rando/Foreign.h"

extern "C" {
#include "overlays/actors/ovl_En_GirlA/z_en_girla.h"
}

#include <cstdlib>
#include <string>

extern "C" void MM_Rando_TestColdJunkCache(void); // ConvertItem.cpp

// The shelf's own functions (ActorBehavior/EnGirlA.cpp), the ones a shopkeeper calls.
s32 EnGirlA_RandoCanBuyFunc(PlayState* play, EnGirlA* enGirlA);
void EnGirlA_RandoBuyFunc(PlayState* play, EnGirlA* enGirlA);
void EnGirlA_RandoRestock(PlayState* play, EnGirlA* enGirlA);

namespace {

s16 SnbMmUpgrade(int nut) {
    return nut ? (s16)UPG_DEKU_NUTS : (s16)UPG_DEKU_STICKS;
}

u8 SnbMmItem(int nut) {
    return nut ? (u8)ITEM_DEKU_NUT : (u8)ITEM_DEKU_STICK;
}

/** #1788: the shelves the row drives: 0 the Trading Post's first shelf, 1 its
 *  second, 2 the Hags' blue-potion (mushroom) shelf, 3 the Bomb Shop's first
 *  shelf (the one its owner holds); RC_UNKNOWN past them. */
RandoCheckId SnbMmShelfCheck(int slot) {
    switch (slot) {
        case 0:
            return RC_TRADING_POST_SHOP_ITEM_01;
        case 1:
            return RC_TRADING_POST_SHOP_ITEM_02;
        case 2:
            return RC_HAGS_POTION_SHOP_ITEM_01;
        case 3:
            return RC_BOMB_SHOP_ITEM_01;
        default:
            return RC_UNKNOWN;
    }
}

/** A shelf actor standing on @p rc and a PlayState whose message context holds
 *  @p price, as the shopkeeper loads it before it asks the shelf; freed by the
 *  destructor. */
struct SnbMmShelfActor {
    PlayState* play;
    EnGirlA* shelf;
    SnbMmShelfActor(RandoCheckId rc, int price)
        : play((PlayState*)calloc(1, sizeof(PlayState))), shelf((EnGirlA*)calloc(1, sizeof(EnGirlA))) {
        if (play != nullptr && shelf != nullptr) {
            shelf->actor.world.rot.z = (s16)rc;
            play->msgCtx.unk1206C = price;
        }
    }
    ~SnbMmShelfActor() {
        free(shelf);
        free(play);
    }
    bool Ok() const {
        return play != nullptr && shelf != nullptr;
    }
    SnbMmShelfActor(const SnbMmShelfActor&) = delete;
    SnbMmShelfActor& operator=(const SnbMmShelfActor&) = delete;
};

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
 *  8 RI_ARROWS_10 (MM's own ammo-without-capacity row), 9 RI_BOMBCHU_5 (#1702's
 *  shelf, not #1788's); -1 past it. */
extern "C" int MM_SnbTest_RandoId(int which) {
    static const int kIds[] = { RI_DEKU_STICK, RI_DEKU_STICKS_5, RI_DEKU_NUT, RI_DEKU_NUTS_5, RI_DEKU_NUTS_10,
                                RI_JUNK,       RI_NONE,          RI_UNKNOWN,  RI_ARROWS_10,   RI_BOMBCHU_5 };
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

/** #1788: shelf @p slot's check id (SnbMmShelfCheck), -1 past them. */
extern "C" int MM_SnbTest_ShelfCheck(int slot) {
    const RandoCheckId rc = SnbMmShelfCheck(slot);
    return rc == RC_UNKNOWN ? -1 : (int)rc;
}

/** Shelf @p slot holds RI_* @p ri, shuffled, at price 10, @p obtained (and
 *  cycleObtained) and @p eligible as given, as the fill and a save leave it. */
extern "C" void MM_SnbTest_PlaceShelf(int slot, int ri, int obtained, int eligible) {
    const RandoCheckId rc = SnbMmShelfCheck(slot);
    if (rc == RC_UNKNOWN) {
        return;
    }
    RandoSaveCheck& row = RANDO_SAVE_CHECKS[rc];
    row.randoItemId = (RandoItemId)ri;
    row.shuffled = true;
    row.eligible = eligible != 0;
    row.cycleObtained = obtained != 0;
    row.obtained = obtained != 0;
    row.skipped = false;
    row.price = 10;
}

/** Rando::Foreign::IsForeignCheck on shelf @p slot (a stale crossing would make
 *  a native leg measure a foreign host). */
extern "C" int MM_SnbTest_ShelfForeign(int slot) {
    return Rando::Foreign::IsForeignCheck(SnbMmShelfCheck(slot)) ? 1 : 0;
}

extern "C" int MM_SnbTest_ShelfLocked(int slot) {
    return Rando::ActorBehavior::ShopShelfLocked(SnbMmShelfCheck(slot)) ? 1 : 0;
}

/** What the shelf draws (EnGirlA_RandoDrawFunc's native branch). */
extern "C" int MM_SnbTest_ShelfItem(int slot) {
    return (int)Rando::ActorBehavior::ShopShelfItem(SnbMmShelfCheck(slot));
}

/** 1 when the shelf's lines (0x83F / 0x840 / 0x880) name RI_* @p ri. */
extern "C" int MM_SnbTest_ShelfNamesItem(int slot, int ri) {
    const std::string name = Rando::ActorBehavior::ShopShelfItemName(SnbMmShelfCheck(slot));
    return name == Rando::StaticData::GetItemDisplayName((RandoItemId)ri) ? 1 : 0;
}

/** 1 when the Bomb Shop owner holds what RC_BOMB_SHOP_ITEM_01's shelf draws. */
extern "C" int MM_SnbTest_BombShopHandIsShelfItem(void) {
    const RandoItemId hand = Rando::ActorBehavior::BombShopHandItem();
    return hand == Rando::ActorBehavior::ShopShelfItem(RC_BOMB_SHOP_ITEM_01) ? 1 : 0;
}

extern "C" int MM_SnbTest_ShelfSold(int slot) {
    return Rando::ActorBehavior::ShopShelfSlotSold(SnbMmShelfCheck(slot)) ? 1 : 0;
}

/** The shopkeeper's restock on an emptied shelf: 1 when it stocks it again. */
extern "C" int MM_SnbTest_ShelfRestockShows(int slot) {
    SnbMmShelfActor a(SnbMmShelfCheck(slot), 10);
    if (!a.Ok()) {
        return -1;
    }
    a.shelf->isOutOfStock = true;
    a.shelf->actor.draw = NULL;
    EnGirlA_RandoRestock(a.play, a.shelf);
    return (!a.shelf->isOutOfStock && a.shelf->actor.draw != NULL) ? 1 : 0;
}

/** The shelf's canBuyFunc with @p rupees held at @p price: 0 SUCCESS_2,
 *  1 CANNOT_GET_NOW, 2 NEED_RUPEES, 3 any other answer, -1 no memory. */
extern "C" int MM_SnbTest_ShelfCanBuy(int slot, int rupees, int price) {
    SnbMmShelfActor a(SnbMmShelfCheck(slot), price);
    if (!a.Ok()) {
        return -1;
    }
    gSaveContext.save.saveInfo.playerData.rupees = (s16)rupees;
    switch (EnGirlA_RandoCanBuyFunc(a.play, a.shelf)) {
        case CANBUY_RESULT_SUCCESS_2:
            return 0;
        case CANBUY_RESULT_CANNOT_GET_NOW:
            return 1;
        case CANBUY_RESULT_NEED_RUPEES:
            return 2;
        default:
            return 3;
    }
}

/** The shelf's buyFunc at @p price from a zero rupee accumulator: the change it
 *  leaves there (MM_Rupees_ChangeBy only adds to it, z_parameter.c). */
extern "C" int MM_SnbTest_ShelfBuy(int slot, int price) {
    SnbMmShelfActor a(SnbMmShelfCheck(slot), price);
    if (!a.Ok()) {
        return 9999;
    }
    gSaveContext.rupeeAccumulator = 0;
    EnGirlA_RandoBuyFunc(a.play, a.shelf);
    const int change = (int)gSaveContext.rupeeAccumulator;
    gSaveContext.rupeeAccumulator = 0;
    return change;
}

extern "C" int MM_SnbTest_ShelfObtained(int slot) {
    const RandoCheckId rc = SnbMmShelfCheck(slot);
    return rc == RC_UNKNOWN ? -1 : (RANDO_SAVE_CHECKS[rc].obtained ? 1 : 0);
}

/** The Hags' mushroom line's arming (text 0x884, EnGirlA.cpp). */
extern "C" int MM_SnbTest_HagsArmable(void) {
    return Rando::ActorBehavior::HagsMushroomSlotArmable() ? 1 : 0;
}

/** HagsMushroomLineArms with the cycle's free blue potion cleared first (the
 *  weekly byte written directly, as the three-day reset leaves it); writes
 *  whether the line settled it. */
extern "C" int MM_SnbTest_HagsLineArms(int* outFreePotion) {
    WEEKEVENTREG(WEEKEVENTREG_RECEIVED_FREE_BLUE_POTION >> 8) &= (u8) ~(WEEKEVENTREG_RECEIVED_FREE_BLUE_POTION & 0xFF);
    const int arms = Rando::ActorBehavior::HagsMushroomLineArms() ? 1 : 0;
    *outFreePotion = CHECK_WEEKEVENTREG(WEEKEVENTREG_RECEIVED_FREE_BLUE_POTION) ? 1 : 0;
    return arms;
}

#endif
