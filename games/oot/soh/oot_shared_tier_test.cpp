/**
 * @file oot_shared_tier_test.cpp
 * OoT's half of the shared-tier pickup lock (#1120).
 *
 * THE SHIM HELPERS the redship row `shared-tier-pickup`
 * (src/common/tests/test_shared_tier.c) drives OoT's REAL
 * OoT_HarvestSharedResources / OoT_ApplySharedResources, the REAL pickup core
 * OoT_Rando_Foreign_RecordPickup and the REAL arrival consumer through. They
 * only put OoT's live save in a known state and read the bomb bag row back;
 * every merge decision is the production code's. The wallet legs reuse the
 * shared-wallet row's helpers (oot_shared_wallet_test.cpp) and the bottle legs
 * the shared-bottles row's (oot_shared_bottles_test.cpp).
 *
 * The live save is a VANILLA (QUEST_NORMAL) file, like the bombchu row's: the
 * ammo rows are shared for every world, and outside the randomizer no ammo
 * grant is blocked (OoT_SharedAmmoGrantBlocked), so the row needs no
 * Randomizer context.
 */

#include <cstring>

// At file scope, not inside an extern "C" block (see oot_shared_kokiri_sword_test.cpp).
#include <z64.h>
#include "variables.h"
#include "macros.h"
#include "soh/Enhancements/randomizer/randomizerTypes.h"

extern "C" void OoT_Inventory_ChangeUpgrade(s16 upgrade, s16 value); // GameExports_SingleExe.cpp's own use

/** OoT's live save as a real loaded vanilla file holding nothing: every item
 *  slot ITEM_NONE (as a real save's are, so a tier rising from zero has an empty
 *  slot to author the item into), GAMEMODE_NORMAL (what Combo_SaveIsLiveFile
 *  requires before the harvest or a pickup credit does anything), everything
 *  else zero, so the harvest offers no kind a nonzero value. It wipes the live
 *  save buffer both games share (src/common/unified_save.c), so a read of MM's
 *  live state must come before it. */
extern "C" void OoT_SharedTierTest_ArmLive(void) {
    memset(&gSaveContext, 0, sizeof(SaveContext));
    memset(gSaveContext.inventory.items, ITEM_NONE, sizeof(gSaveContext.inventory.items));
    gSaveContext.fileNum = 0;
    gSaveContext.gameMode = GAMEMODE_NORMAL;
    gSaveContext.ship.quest.id = QUEST_NORMAL;
}

/** Bomb bag at @p tier (the bomb item in its slot for a nonzero tier, as OoT's
 *  own give leaves it) holding @p bombs. */
extern "C" void OoT_SharedTierTest_SetBombBag(int tier, int bombs) {
    OoT_Inventory_ChangeUpgrade(UPG_BOMB_BAG, (s16)tier);
    if (tier > 0) {
        INV_CONTENT(ITEM_BOMB) = ITEM_BOMB;
    }
    AMMO(ITEM_BOMB) = (s8)bombs;
}

/** Set OoT's live bomb count (what the player holds mid-stint). */
extern "C" void OoT_SharedTierTest_SetBombs(int bombs) {
    AMMO(ITEM_BOMB) = (s8)bombs;
}

/** OoT's live bomb bag tier (UPG_BOMB_BAG). */
extern "C" int OoT_SharedTierTest_BombBagTier(void) {
    return (int)CUR_UPG_VALUE(UPG_BOMB_BAG);
}

/** OoT's live bomb count. */
extern "C" int OoT_SharedTierTest_Bombs(void) {
    return (int)AMMO(ITEM_BOMB);
}

/** 1 when the bomb item is in its slot. */
extern "C" int OoT_SharedTierTest_HasBombItem(void) {
    return INV_CONTENT(ITEM_BOMB) == ITEM_BOMB ? 1 : 0;
}

/** OoT's own id for a progressive bomb bag (RG_PROGRESSIVE_BOMB_BAG). */
extern "C" int OoT_SharedTierTest_BombBagId(void) {
    return (int)RG_PROGRESSIVE_BOMB_BAG;
}

/** OoT's own id for a progressive wallet (RG_PROGRESSIVE_WALLET). */
extern "C" int OoT_SharedTierTest_WalletId(void) {
    return (int)RG_PROGRESSIVE_WALLET;
}

// ---- #1702 C7: the shared Bombchu Bag tier ------------------------------------

// GameExports_SingleExe.cpp: the ROM-free stand-in for the seed's RSK_BOMBCHU_BAG
// (-1 reads the seed). Only this TU calls it.
extern "C" void OoT_SharedChuBagTest_SetMode(int mode);

/** OoT's Bombchu Bag option as the shared tier's shims read it: 0 None, 1
 *  Single, 2 Progressive (RO_BOMBCHU_BAG_*), or -1 to read the seed again. A row
 *  that sets it puts -1 back before it returns. */
extern "C" void OoT_SharedTierTest_SetChuBagMode(int mode) {
    OoT_SharedChuBagTest_SetMode(mode);
}

/** OoT's Bombchu Bag state: bombchuUpgradeLevel @p level and the Bombchu item in
 *  its slot when @p holds (cleared otherwise), holding @p chus. */
extern "C" void OoT_SharedTierTest_SetChuBag(int level, int holds, int chus) {
    gSaveContext.ship.quest.data.randomizer.bombchuUpgradeLevel = (u8)level;
    INV_CONTENT(ITEM_BOMBCHU) = holds ? ITEM_BOMBCHU : ITEM_NONE;
    AMMO(ITEM_BOMBCHU) = (s8)chus;
}

/** OoT's live bombchuUpgradeLevel. */
extern "C" int OoT_SharedTierTest_ChuBagLevel(void) {
    return (int)gSaveContext.ship.quest.data.randomizer.bombchuUpgradeLevel;
}

/** 1 when the Bombchu item is in its slot. */
extern "C" int OoT_SharedTierTest_HoldsBombchu(void) {
    return INV_CONTENT(ITEM_BOMBCHU) == ITEM_BOMBCHU ? 1 : 0;
}

/** OoT's live bombchu count. */
extern "C" int OoT_SharedTierTest_Bombchus(void) {
    return (int)AMMO(ITEM_BOMBCHU);
}

/** OoT's own id for the Bombchu Bag (RG_PROGRESSIVE_BOMBCHU_BAG): what OoT's pool
 *  places under either Bombchu Bag option (item_pool.cpp). */
extern "C" int OoT_SharedTierTest_ChuBagId(void) {
    return (int)RG_PROGRESSIVE_BOMBCHU_BAG;
}

// ---- #1652 A1 / A3 (mm-stick-nut-bag-gate, test_stick_nut_bag_mm.c) --------
// @p nut 0 names the sticks, 1 the nuts.

/** RG_PROGRESSIVE_STICK_UPGRADE / RG_PROGRESSIVE_NUT_UPGRADE: what OoT's pool
 *  places as the bag under its shuffle (resolved at OoT's tier 0, item.cpp). */
extern "C" int OoT_SharedTierTest_BagUpgradeId(int nut) {
    return nut ? (int)RG_PROGRESSIVE_NUT_UPGRADE : (int)RG_PROGRESSIVE_STICK_UPGRADE;
}

/** RG_DEKU_STICK_1 / RG_DEKU_NUTS_5: a pack OoT cannot hold before the bag. */
extern "C" int OoT_SharedTierTest_BagPackId(int nut) {
    return nut ? (int)RG_DEKU_NUTS_5 : (int)RG_DEKU_STICK_1;
}

/** RG_BOMBS_5: a pack no bag gate holds. */
extern "C" int OoT_SharedTierTest_BombPackId(void) {
    return (int)RG_BOMBS_5;
}

/** OoT's live stick (nut) capacity tier. */
extern "C" int OoT_SharedTierTest_BagTier(int nut) {
    return (int)(nut ? CUR_UPG_VALUE(UPG_NUTS) : CUR_UPG_VALUE(UPG_STICKS));
}

/** #1691: OoT's live stick (nut) capacity tier set to @p tier. */
extern "C" void OoT_SharedTierTest_SetBagTier(int nut, int tier) {
    OoT_Inventory_ChangeUpgrade(nut ? UPG_NUTS : UPG_STICKS, (s16)tier);
}

/** #1691: OoT's GI_* ids by index: 0 GI_STICKS_1 (a Deku Baba's stick), 1 _5,
 *  2 _10, 3 GI_NUTS_5, 4 GI_NUTS_5_2 (a scrub's), 5 GI_NUTS_10, 6 GI_RUPEE_BLUE
 *  (not gated); -1 past it. */
extern "C" int OoT_SharedTierTest_GetItemId(int which) {
    static const int kIds[] = { GI_STICKS_1, GI_STICKS_5, GI_STICKS_10, GI_NUTS_5,
                                GI_NUTS_5_2, GI_NUTS_10,  GI_RUPEE_BLUE };
    return (which >= 0 && which < (int)(sizeof(kIds) / sizeof(kIds[0]))) ? kIds[which] : -1;
}

/** OoT's live stick (nut) count. */
extern "C" int OoT_SharedTierTest_BagAmmo(int nut) {
    return (int)(nut ? AMMO(ITEM_NUT) : AMMO(ITEM_STICK));
}

/** 1 when the stick (nut) item is in its slot. */
extern "C" int OoT_SharedTierTest_BagHeld(int nut) {
    return nut ? (INV_CONTENT(ITEM_NUT) == ITEM_NUT ? 1 : 0) : (INV_CONTENT(ITEM_STICK) == ITEM_STICK ? 1 : 0);
}
