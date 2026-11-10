/**
 * ShopRestockSingleExe.cpp — a bought shuffled OoT shop shelf sells its item
 * again when OoTMM restocks it (#1482).
 *
 * WHAT OoTMM DOES. After a sale the shelf's slot bit is set (common/shop.c
 * Shop_AfterBuy) and the shelf's next query carries OVF_RENEW
 * (common/actors/En_GirlA.c), which comboRenewable (common/item/progressive.c)
 * answers: an item on its list stays on sale, anything else is sold out. The
 * code is in common/, so OoT's shelves follow the same rule as MM's.
 *
 * WHAT THIS PORT DID. SoH turns a shuffled shelf SI_SOLD_OUT the moment its
 * RandomizerInf is set (z_en_girla.c EnGirlA_TryChangeShopItemShip, z_en_ossan.c
 * at spawn), so every shuffled OoT shelf sold once. That is upstream SoH's own
 * behaviour, inherited.
 *
 * THE LIST is OoTMM's OoT list (progressive.c) mapped to RG ids, restricted to
 * the items that restock AS THEMSELVES: the shelf already draws and names the
 * placed item, so nothing else about it has to change. OoTMM also restocks a
 * bottled potion or milk as its contents; that needs the shelf to draw and name
 * the refill instead of the bottle, which this file does not do, so a bottle
 * sells out (as do a bottled fairy, poe, blue fire, fish, bugs and big poe).
 * Rupees, traps and everything not on the list sell out too, and so does a
 * shelf hosting a Majora's Mask item: the crossing store delivers once.
 *
 * THE SECOND SALE. The first sale sets the RandomizerInf and the check queue
 * gives the placed item once (Flags_SetRandomizerInf fires OnFlagSet only on a
 * flag that was off; the check handler drops a check already obtained). So a
 * restocked sale cannot go through the check: it gives the item directly, the
 * way a vanilla shelf's ItemGive does (OoT_Item_Give with the entry's OoT item
 * id), and leaves the check, the tracker and the hints alone.
 *
 * THE FILL is unchanged: OoTCritHostRenews (ComboLogicEngineOoT.cpp) still
 * credits only a shelf's VANILLA stock, so the fill assumes less than the
 * runtime now gives (an under-credit, which is sound), and no world moves.
 */
#ifdef RSBS_SINGLE_EXECUTABLE

#include <cstdint>
#include <cstdio>
#include <memory>

#include "soh/Enhancements/randomizer/randomizerTypes.h"
#include "soh/Enhancements/randomizer/static_data.h"
#include "soh/Enhancements/randomizer/item.h"
#include "soh/Enhancements/randomizer/item_location.h"
#include "soh/Enhancements/randomizer/SeedContext.h"
#include "soh/Enhancements/randomizer/logic.h"

#include <macros.h>

extern "C" {
#include <z64.h>
#include "variables.h"
#include "functions.h" // OoT_Item_Give, OoT_Inventory_ChangeUpgrade
// ForeignItemsSingleExe.cpp (#800 S1): the shelf hosts a Majora's Mask item /
// that item's check is collected.
int OoT_Rando_Foreign_HostsForeign(uint16_t rc);
int OoT_Rando_Foreign_HostCollected(uint16_t rc);
// ComboLogicEngineOoT.cpp: the item table, for a process that never ran the OTR
// bring-up (the redship tier); idempotent, leaves no Rando::Context behind.
int OoT_ComboLogic_TestEnsureItemTableTransient(void);
}

namespace {

/** OoTMM's OoT restock list (progressive.c comboRenewable), as the RG ids that
 *  restock as themselves. */
bool ShopRestocksAsItself(RandomizerGet rg) {
    switch (rg) {
        case RG_RECOVERY_HEART:
        case RG_DEKU_STICK_1:
        case RG_DEKU_NUTS_5:
        case RG_DEKU_NUTS_10:
        case RG_BOMBS_5:
        case RG_BOMBS_10:
        case RG_BOMBS_20:
        case RG_ARROWS_5:
        case RG_ARROWS_10:
        case RG_ARROWS_30:
        case RG_DEKU_SEEDS_30:
        case RG_BOMBCHU_5:
        case RG_BOMBCHU_10:
        case RG_BOMBCHU_20:
        case RG_DEKU_SHIELD:
        case RG_HYLIAN_SHIELD:
        case RG_RED_POTION_REFILL:
        case RG_GREEN_POTION_REFILL:
        case RG_BLUE_POTION_REFILL:
        case RG_MILK:
        case RG_FISH:
            return true;
        default:
            return false;
    }
}

} // namespace

/** What a shelf holding @p rg sells again once bought: @p rg itself when OoTMM
 *  restocks it, RG_NONE otherwise. Reads no state. */
extern "C" uint16_t OoT_Rando_ShopRestockItemFor(uint16_t rg) {
    return ShopRestocksAsItself((RandomizerGet)rg) ? rg : (uint16_t)RG_NONE;
}

/** What shuffled shop shelf @p rc sells again once bought, or RG_NONE: the
 *  shelf's placed item when OoTMM restocks it, never for a shelf hosting a
 *  Majora's Mask item. Only the shelf call sites ask (SI_RANDOMIZED_ITEM). */
extern "C" uint16_t OoT_Rando_ShopRestockItem(uint16_t rc) {
    if (rc == (uint16_t)RC_UNKNOWN_CHECK || rc >= (uint16_t)RC_MAX) {
        return (uint16_t)RG_NONE;
    }
    if (OoT_Rando_Foreign_HostsForeign(rc) != 0) {
        return (uint16_t)RG_NONE;
    }
    const std::shared_ptr<Rando::Context> ctx = Rando::Context::GetInstance();
    if (ctx == nullptr) {
        return (uint16_t)RG_NONE;
    }
    const Rando::ItemLocation* il = ctx->GetItemLocation((RandomizerCheck)rc);
    if (il == nullptr) {
        return (uint16_t)RG_NONE;
    }
    return OoT_Rando_ShopRestockItemFor((uint16_t)il->GetPlacedRandomizerGet());
}

/** Is shuffled shelf @p rc sold out? @p bought is its RandomizerInf. A bought
 *  shelf is sold out unless it restocks; a shelf hosting a Majora's Mask item
 *  whose check is collected is sold out either way (#800 S1). */
extern "C" int OoT_Rando_ShopShelfSoldOut(uint16_t rc, int bought) {
    if (OoT_Rando_Foreign_HostCollected(rc) != 0) {
        return 1;
    }
    return (bought != 0 && OoT_Rando_ShopRestockItem(rc) == (uint16_t)RG_NONE) ? 1 : 0;
}

/** Would a restocked sale of @p rg add nothing right now? The vanilla shelves'
 *  own refusals (z_en_girla.c EnGirlA_CanBuy_*): ammo at capacity, health full.
 *  Whether the player may hold it at all (a bag, an empty bottle, a shield
 *  already owned) is the item's obtainability, which the caller asks first. */
extern "C" int OoT_Rando_ShopRestockFull(uint16_t rg) {
    switch ((RandomizerGet)rg) {
        case RG_RECOVERY_HEART:
            return gSaveContext.health >= gSaveContext.healthCapacity ? 1 : 0;
        case RG_DEKU_STICK_1:
            return (CUR_CAPACITY(UPG_STICKS) != 0 && AMMO(ITEM_STICK) >= CUR_CAPACITY(UPG_STICKS)) ? 1 : 0;
        case RG_DEKU_NUTS_5:
        case RG_DEKU_NUTS_10:
            return (CUR_CAPACITY(UPG_NUTS) != 0 && AMMO(ITEM_NUT) >= CUR_CAPACITY(UPG_NUTS)) ? 1 : 0;
        case RG_BOMBS_5:
        case RG_BOMBS_10:
        case RG_BOMBS_20:
            return AMMO(ITEM_BOMB) >= CUR_CAPACITY(UPG_BOMB_BAG) ? 1 : 0;
        case RG_ARROWS_5:
        case RG_ARROWS_10:
        case RG_ARROWS_30:
            return AMMO(ITEM_BOW) >= CUR_CAPACITY(UPG_QUIVER) ? 1 : 0;
        case RG_DEKU_SEEDS_30:
            return AMMO(ITEM_SLINGSHOT) >= CUR_CAPACITY(UPG_BULLET_BAG) ? 1 : 0;
        case RG_BOMBCHU_5:
        case RG_BOMBCHU_10:
        case RG_BOMBCHU_20: {
            // Bombchus.cpp's VB_CAN_BUY_BOMBCHUS: 50, or the progressive bag's.
            int capacity = 50;
            const std::shared_ptr<Rando::Context> ctx = Rando::Context::GetInstance();
            if (IS_RANDO && ctx != nullptr && ctx->GetOption(RSK_BOMBCHU_BAG).Is(RO_BOMBCHU_BAG_PROGRESSIVE)) {
                capacity = ctx->GetBombchuCapacity();
            }
            return AMMO(ITEM_BOMBCHU) >= capacity ? 1 : 0;
        }
        default:
            return 0;
    }
}

/** The OoT item id a restocked sale of @p rg hands OoT_Item_Give: the item's
 *  own get-item entry, as the player's give takes it for a MOD_NONE entry
 *  (z_player.c). ITEM_NONE for an item that does not restock or has no plain
 *  OoT entry. */
extern "C" uint8_t OoT_Rando_ShopRestockGiveItemId(uint16_t rg) {
    if (OoT_Rando_ShopRestockItemFor(rg) == (uint16_t)RG_NONE) {
        return ITEM_NONE;
    }
    const GetItemEntry* entry = Rando::StaticData::RetrieveItem((RandomizerGet)rg).GetStaticGIEntry();
    if (entry == nullptr || entry->modIndex != MOD_NONE) {
        return ITEM_NONE;
    }
    return (uint8_t)entry->itemId;
}

/** A restocked sale on shuffled shelf @p rc: give the item it sells again,
 *  directly (the check is already obtained, so the queue would give nothing).
 *  @return 1 when something was given (the caller then charges the price), 0
 *  otherwise. */
extern "C" int OoT_Rando_ShopRestockGive(PlayState* play, uint16_t rc) {
    const uint8_t itemId = OoT_Rando_ShopRestockGiveItemId(OoT_Rando_ShopRestockItem(rc));
    if (itemId == ITEM_NONE) {
        fprintf(stderr, "[OoT] shop restock: shelf check %u has nothing to sell again\n", (unsigned)rc);
        return 0;
    }
    OoT_Item_Give(play, itemId);
    return 1;
}

// ============================================================================
// TEST BRIDGES (redship tier, ROM-free; src/common/tests/test_oot_shelf_restock.c)
// ============================================================================

/** The shelf the locks place items on (Kokiri Forest shop, slot 1). */
extern "C" uint16_t OoT_Rando_ShopRestockTestShelfCheck(void) {
    return (uint16_t)RC_KF_SHOP_ITEM_1;
}

/**
 * The list, by real id. Every row of OoTMM's OoT list that restocks as itself
 * answers itself and gives its own plain OoT item; the bottles, rupees, a trap, a
 * Piece of Heart, a major item, the Mirror Shield, a bean and a vanilla shelf row
 * answer RG_NONE and give nothing. @return 0, or a step code.
 */
extern "C" int OoT_Rando_ShopRestockTestTable(void) {
    if (OoT_ComboLogic_TestEnsureItemTableTransient() != 0) {
        return 1;
    }
    const struct {
        RandomizerGet rg;
        uint8_t item;
    } kRestocks[] = {
        { RG_RECOVERY_HEART, ITEM_HEART },
        { RG_DEKU_STICK_1, ITEM_STICK },
        { RG_DEKU_NUTS_5, ITEM_NUTS_5 },
        { RG_DEKU_NUTS_10, ITEM_NUTS_10 },
        { RG_BOMBS_5, ITEM_BOMBS_5 },
        { RG_BOMBS_10, ITEM_BOMBS_10 },
        { RG_BOMBS_20, ITEM_BOMBS_20 },
        { RG_ARROWS_5, ITEM_ARROWS_SMALL },
        { RG_ARROWS_10, ITEM_ARROWS_MEDIUM },
        { RG_ARROWS_30, ITEM_ARROWS_LARGE },
        { RG_DEKU_SEEDS_30, ITEM_SEEDS_30 },
        { RG_BOMBCHU_5, ITEM_BOMBCHUS_5 },
        { RG_BOMBCHU_10, ITEM_BOMBCHU },
        { RG_BOMBCHU_20, ITEM_BOMBCHUS_20 },
        { RG_DEKU_SHIELD, ITEM_SHIELD_DEKU },
        { RG_HYLIAN_SHIELD, ITEM_SHIELD_HYLIAN },
        { RG_RED_POTION_REFILL, ITEM_POTION_RED },
        { RG_GREEN_POTION_REFILL, ITEM_POTION_GREEN },
        { RG_BLUE_POTION_REFILL, ITEM_POTION_BLUE },
        { RG_MILK, ITEM_MILK },
        { RG_FISH, ITEM_FISH },
    };
    for (const auto& r : kRestocks) {
        if (OoT_Rando_ShopRestockItemFor((uint16_t)r.rg) != (uint16_t)r.rg) {
            fprintf(stderr, "[OoT ShopRestock] RG %d does not restock as itself\n", (int)r.rg);
            return 2;
        }
        const uint8_t given = OoT_Rando_ShopRestockGiveItemId((uint16_t)r.rg);
        if (given != r.item) {
            fprintf(stderr, "[OoT ShopRestock] RG %d gives OoT item %u, expected %u\n", (int)r.rg, (unsigned)given,
                    (unsigned)r.item);
            return 3;
        }
    }
    const RandomizerGet kSellOut[] = {
        RG_BOTTLE_WITH_RED_POTION,
        RG_BOTTLE_WITH_BLUE_POTION,
        RG_BOTTLE_WITH_MILK,
        RG_BOTTLE_WITH_FAIRY,
        RG_BOTTLE_WITH_FISH,
        RG_BOTTLE_WITH_BIG_POE,
        RG_EMPTY_BOTTLE,
        RG_GREEN_RUPEE,
        RG_HUGE_RUPEE,
        RG_ICE_TRAP,
        RG_PIECE_OF_HEART,
        RG_HEART_CONTAINER,
        RG_PROGRESSIVE_HOOKSHOT,
        RG_MIRROR_SHIELD,
        RG_MAGIC_BEAN,
        RG_BUY_DEKU_NUTS_5,
        RG_BUY_DEKU_SHIELD,
        RG_NONE,
    };
    for (const RandomizerGet rg : kSellOut) {
        if (OoT_Rando_ShopRestockItemFor((uint16_t)rg) != (uint16_t)RG_NONE ||
            OoT_Rando_ShopRestockGiveItemId((uint16_t)rg) != ITEM_NONE) {
            fprintf(stderr, "[OoT ShopRestock] RG %d restocks, but OoTMM sells it out on a shuffled shelf\n", (int)rg);
            return 4;
        }
    }
    if (OoT_Rando_ShopRestockItem((uint16_t)RC_UNKNOWN_CHECK) != (uint16_t)RG_NONE ||
        OoT_Rando_ShopRestockItem((uint16_t)RC_MAX) != (uint16_t)RG_NONE) {
        return 5;
    }
    return 0;
}

/**
 * The shelf, by placement: the test shelf holding a restocking item answers it
 * and is not sold out once bought; holding a Piece of Heart, a bottle or an ice
 * trap it is sold out once bought; unbought it is on sale either way. With
 * @p foreignHosted the caller has made the shelf host a Majora's Mask item
 * (Combo_SetForeignPlacementOoT), and then nothing restocks and a bought shelf
 * is sold out whatever the OoT table holds there.
 *
 * Uses the live Rando::Context when there is one and puts the shelf's
 * ItemLocation back exactly; otherwise creates one and releases it again
 * (OoT_HintArea_TestForeignWothText's shape), so no row of `--test all` sees it
 * left behind. @return 0, or a step code.
 */
extern "C" int OoT_Rando_ShopRestockTestShelves(int foreignHosted) {
    if (OoT_ComboLogic_TestEnsureItemTableTransient() != 0) {
        return 1;
    }
    std::shared_ptr<Rando::Context> created;
    if (Rando::Context::GetInstance() == nullptr) {
        created = Rando::Context::CreateInstance();
    }
    int step = 0;
    {
        const std::shared_ptr<Rando::Context> ctx = Rando::Context::GetInstance();
        const RandomizerCheck rc = RC_KF_SHOP_ITEM_1;
        Rando::ItemLocation* il = ctx != nullptr ? ctx->GetItemLocation(rc) : nullptr;
        if (il == nullptr) {
            step = 2;
        } else {
            const Rando::ItemLocation saved = *il;
            const struct {
                RandomizerGet placed;
                bool restocks;
            } kCases[] = {
                { RG_DEKU_NUTS_5, true },           { RG_DEKU_SHIELD, true }, { RG_RED_POTION_REFILL, true },
                { RG_PIECE_OF_HEART, false },       { RG_ICE_TRAP, false },   { RG_BOTTLE_WITH_RED_POTION, false },
                { RG_PROGRESSIVE_HOOKSHOT, false },
            };
            for (const auto& c : kCases) {
                il->SetPlacedItem(c.placed);
                const bool restocks = c.restocks && foreignHosted == 0;
                const uint16_t want = restocks ? (uint16_t)c.placed : (uint16_t)RG_NONE;
                if (OoT_Rando_ShopRestockItem((uint16_t)rc) != want) {
                    fprintf(stderr, "[OoT ShopRestock] shelf holding RG %d (foreign %d) restocks %u, expected %u\n",
                            (int)c.placed, foreignHosted, (unsigned)OoT_Rando_ShopRestockItem((uint16_t)rc),
                            (unsigned)want);
                    step = 3;
                    break;
                }
                if (OoT_Rando_ShopShelfSoldOut((uint16_t)rc, 1) != (restocks ? 0 : 1)) {
                    fprintf(stderr, "[OoT ShopRestock] bought shelf holding RG %d (foreign %d): sold out %d\n",
                            (int)c.placed, foreignHosted, OoT_Rando_ShopShelfSoldOut((uint16_t)rc, 1));
                    step = 4;
                    break;
                }
                if (OoT_Rando_ShopShelfSoldOut((uint16_t)rc, 0) != 0) {
                    fprintf(stderr, "[OoT ShopRestock] unbought shelf holding RG %d (foreign %d) is sold out\n",
                            (int)c.placed, foreignHosted);
                    step = 5;
                    break;
                }
            }
            *il = saved;
        }
    }
    if (created != nullptr) {
        created->GetLogic()->SetContext(nullptr); // the back-edge that would keep it alive
    }
    return step;
}

/**
 * The vanilla shelves' "nothing to add" refusals, on the live save, which is put
 * back exactly: Deku Nuts at capacity refuse and one short do not; a Recovery
 * Heart at full health refuses and with a heart missing does not; Bombchus at 50
 * refuse and at 49 do not (no randomizer file is loaded, so the cap is 50); a
 * shield never does (owning it is its obtainability). @return 0, or a step code.
 */
extern "C" int OoT_Rando_ShopRestockTestFull(void) {
    const Inventory savedInventory = gSaveContext.inventory;
    const s16 savedHealth = gSaveContext.health;
    const s16 savedCapacity = gSaveContext.healthCapacity;
    int step = 0;

    OoT_Inventory_ChangeUpgrade(UPG_NUTS, 1);
    const int nutCap = CUR_CAPACITY(UPG_NUTS);
    if (nutCap <= 1) {
        step = 1;
    } else {
        AMMO(ITEM_NUT) = (s8)nutCap;
        if (OoT_Rando_ShopRestockFull((uint16_t)RG_DEKU_NUTS_5) != 1) {
            step = 2;
        }
        AMMO(ITEM_NUT) = (s8)(nutCap - 1);
        if (step == 0 && OoT_Rando_ShopRestockFull((uint16_t)RG_DEKU_NUTS_10) != 0) {
            step = 3;
        }
    }
    if (step == 0) {
        gSaveContext.healthCapacity = 0x30;
        gSaveContext.health = 0x30;
        if (OoT_Rando_ShopRestockFull((uint16_t)RG_RECOVERY_HEART) != 1) {
            step = 4;
        }
        gSaveContext.health = 0x20;
        if (step == 0 && OoT_Rando_ShopRestockFull((uint16_t)RG_RECOVERY_HEART) != 0) {
            step = 5;
        }
    }
    if (step == 0 && !IS_RANDO) {
        AMMO(ITEM_BOMBCHU) = 50;
        if (OoT_Rando_ShopRestockFull((uint16_t)RG_BOMBCHU_10) != 1) {
            step = 6;
        }
        AMMO(ITEM_BOMBCHU) = 49;
        if (step == 0 && OoT_Rando_ShopRestockFull((uint16_t)RG_BOMBCHU_20) != 0) {
            step = 7;
        }
    }
    if (step == 0 && OoT_Rando_ShopRestockFull((uint16_t)RG_DEKU_SHIELD) != 0) {
        step = 8;
    }

    gSaveContext.inventory = savedInventory;
    gSaveContext.health = savedHealth;
    gSaveContext.healthCapacity = savedCapacity;
    return step;
}

#endif // RSBS_SINGLE_EXECUTABLE
