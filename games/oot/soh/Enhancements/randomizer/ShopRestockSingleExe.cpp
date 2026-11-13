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
 * THE LIST is OoTMM's OoT list (progressive.c), including the bottled potion and
 * milk conversions (#1547): red/green/blue potion bottles restock as
 * RG_*_POTION_REFILL and bottled milk restocks as RG_MILK (giving the refill,
 * naming it, drawing its entry, and checking an empty bottle). Bottled fairy,
 * poe, blue fire, fish, bugs and big poe, rupees, traps and everything else
 * not on the list sell out, and so does a shelf hosting a Majora's Mask item:
 * the crossing store delivers once.
 *
 * THE SECOND SALE. The first sale sets the RandomizerInf and the check queue
 * gives the placed item once (Flags_SetRandomizerInf fires OnFlagSet only on a
 * flag that was off; the check handler drops a check already obtained). So a
 * restocked sale cannot go through the check: it gives the item directly, the
 * way a vanilla shelf's ItemGive does (OoT_Item_Give with the entry's OoT item
 * id), and leaves the check, the tracker and the hints alone.
 *
 * THE FILL matches OoTMM (#1547): OoTCritHostRenews (ComboLogicEngineOoT.cpp)
 * credits any shelf holding a restocking item (including bottle->refill), and
 * logic.cpp's ApplyItemEffect credits restocking shop shelves in reachability.
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
#include "soh/OTRGlobals.h"
#include "soh/Enhancements/randomizer/randomizer.h"

#include "context.h"             // src/common — GAME_OOT
#include "critical_renewables.h" // src/common — Combo_GetCriticalRenewableSource
#include "foreign_items.h"       // src/common — Combo_SetForeignPlacementOoT

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
// Messages/MerchantMessages.cpp: shop offer English text for tests (#1547).
int OoT_Rando_ShopOfferText(uint16_t rc, int bought, int confirm, int mysterious, char* out, size_t cap);
}

// The sale lock's give runs OoT_Item_Give ROM-free (oot_load_reconcile_test.cpp's
// recipe): its hooks set aside, an empty MOD_NONE item table standing in.
#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/Enhancements/item-tables/ItemTableManager.h"

#include <cstring>

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

/** What a shelf holding @p rg sells again once bought: OoTMM's OoT restock
 *  (progressive.c comboRenewable), converting bottled potions and bottled milk
 *  to their contents refill and keeping self-restocking items as @p rg.
 *  RG_NONE otherwise. Reads no state. */
extern "C" uint16_t OoT_Rando_ShopRestockItemFor(uint16_t rg) {
    switch ((RandomizerGet)rg) {
        case RG_BOTTLE_WITH_RED_POTION:
            return (uint16_t)RG_RED_POTION_REFILL;
        case RG_BOTTLE_WITH_GREEN_POTION:
            return (uint16_t)RG_GREEN_POTION_REFILL;
        case RG_BOTTLE_WITH_BLUE_POTION:
            return (uint16_t)RG_BLUE_POTION_REFILL;
        case RG_BOTTLE_WITH_MILK:
            return (uint16_t)RG_MILK;
        default:
            return ShopRestocksAsItself((RandomizerGet)rg) ? rg : (uint16_t)RG_NONE;
    }
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
 *  own refusals (z_en_girla.c EnGirlA_CanBuy_*, Bombchus.cpp): ammo at capacity,
 *  health full, Bombchus with no explosives to use them (no Bombchu Bag, no bomb
 *  bag).
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
            // Bombchus.cpp's VB_CAN_BUY_BOMBCHUS: with no Bombchu Bag a player
            // without a bomb bag may not buy them (the item's obtainability says
            // CAN_OBTAIN there; with a bag it asks for the bag itself); the cap is
            // 50, or the progressive bag's.
            int capacity = 50;
            const std::shared_ptr<Rando::Context> ctx = Rando::Context::GetInstance();
            if (IS_RANDO && ctx != nullptr) {
                if (ctx->GetOption(RSK_BOMBCHU_BAG).Is(RO_BOMBCHU_BAG_NONE) && CUR_CAPACITY(UPG_BOMB_BAG) == 0) {
                    return 1;
                }
                if (ctx->GetOption(RSK_BOMBCHU_BAG).Is(RO_BOMBCHU_BAG_PROGRESSIVE)) {
                    capacity = ctx->GetBombchuCapacity();
                }
            }
            return AMMO(ITEM_BOMBCHU) >= capacity ? 1 : 0;
        }
        default:
            return 0;
    }
}

/** The OoT item id a restocked sale of @p rg hands OoT_Item_Give: the restock
 *  item's own get-item entry, as the player's give takes it for a MOD_NONE
 *  entry (z_player.c). ITEM_NONE for an item that does not restock or has no
 *  plain OoT entry. */
extern "C" uint8_t OoT_Rando_ShopRestockGiveItemId(uint16_t rg) {
    const uint16_t restockRg = OoT_Rando_ShopRestockItemFor(rg);
    if (restockRg == (uint16_t)RG_NONE) {
        return ITEM_NONE;
    }
    const GetItemEntry* entry = Rando::StaticData::RetrieveItem((RandomizerGet)restockRg).GetStaticGIEntry();
    if (entry == nullptr || entry->modIndex != MOD_NONE) {
        return ITEM_NONE;
    }
    return (uint8_t)entry->itemId;
}

/** The RandomizerGet a shuffled shelf @p rc displays and names: when @p bought
 *  is non-zero and the shelf restocks, the restock item (refill for bottled
 *  potions/milk, otherwise the placed item itself). Unbought or non-restocking
 *  returns the placed RandomizerGet. */
extern "C" uint16_t OoT_Rando_ShopShelfDisplayedItem(uint16_t rc, int bought) {
    if (rc == (uint16_t)RC_UNKNOWN_CHECK || rc >= (uint16_t)RC_MAX) {
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
    const uint16_t placed = (uint16_t)il->GetPlacedRandomizerGet();
    if (bought == 0) {
        return placed;
    }
    const uint16_t restock = OoT_Rando_ShopRestockItem(rc);
    return restock != (uint16_t)RG_NONE ? restock : placed;
}

/** Should shelf @p rc's description box say "Special deal! ONE LEFT!"?
 *  Shelves that restock omit it (matching OoTMM's item+price display); shelves
 *  that sell out (including foreign-hosted shelves) keep it. Mysterious Shuffle
 *  preserves it until bought so the first buy does not spoil restocking. */
extern "C" int OoT_Rando_ShopMessageIncludesOneLeft(uint16_t rc, int bought, int mysterious) {
    if (mysterious != 0 && bought == 0) {
        return 1;
    }
    return OoT_Rando_ShopRestockItem(rc) == (uint16_t)RG_NONE ? 1 : 0;
}

/** GetItemEntry for shelf @p rc (init, update, draw): after first purchase, if
 *  the shelf restocks as a refill distinct from its original bottle placement,
 *  return that refill's GetItemEntry; otherwise delegate to SoH's lookup. */
extern "C" GetItemEntry OoT_Rando_ShopShelfGetItemEntry(uint16_t rc, uint16_t ogId, int bought) {
    if (bought != 0 && rc != (uint16_t)RC_UNKNOWN_CHECK && rc < (uint16_t)RC_MAX) {
        const std::shared_ptr<Rando::Context> ctx = Rando::Context::GetInstance();
        const Rando::ItemLocation* il = ctx != nullptr ? ctx->GetItemLocation((RandomizerCheck)rc) : nullptr;
        const uint16_t restock = OoT_Rando_ShopRestockItem(rc);
        if (il != nullptr && restock != (uint16_t)RG_NONE && (RandomizerGet)restock != il->GetPlacedRandomizerGet()) {
            const std::shared_ptr<GetItemEntry> gi =
                Rando::StaticData::RetrieveItem((RandomizerGet)restock).GetGIEntry();
            if (gi != nullptr) {
                return *gi;
            }
        }
    }
    return Randomizer_GetItemFromKnownCheckWithoutObtainabilityCheck((RandomizerCheck)rc, (GetItemID)ogId);
}

/** Obtainability for shelf @p rc: before first purchase, checks the placed item
 *  (e.g. empty bottle slot for a new bottle); once @p bought and restocking,
 *  checks the restock item (e.g. OoT_Inventory_HasEmptyBottle for potion/milk
 *  refills) so a full bottle refuses as NEED_BOTTLE and 4 bottles with a free
 *  empty bottle succeeds. */
extern "C" ItemObtainability OoT_Rando_ShopShelfObtainability(uint16_t rc, int bought) {
    if (bought != 0) {
        const uint16_t restock = OoT_Rando_ShopRestockItem(rc);
        if (restock != (uint16_t)RG_NONE) {
            if (OTRGlobals::Instance != nullptr && OTRGlobals::Instance->gRandomizer != nullptr) {
                return OTRGlobals::Instance->gRandomizer->GetItemObtainabilityFromRandomizerGet(
                    (RandomizerGet)restock);
            }
            switch ((RandomizerGet)restock) {
                case RG_MILK:
                case RG_FISH:
                case RG_RED_POTION_REFILL:
                case RG_GREEN_POTION_REFILL:
                case RG_BLUE_POTION_REFILL:
                    return OoT_Inventory_HasEmptyBottle() ? CAN_OBTAIN : CANT_OBTAIN_NEED_EMPTY_BOTTLE;
                default:
                    return CAN_OBTAIN;
            }
        }
    }
    if (OTRGlobals::Instance == nullptr || OTRGlobals::Instance->gRandomizer == nullptr) {
        return CAN_OBTAIN;
    }
    return Randomizer_GetItemObtainabilityFromRandomizerCheck((RandomizerCheck)rc);
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

/** The restock rule at the shelf's CanBuy (z_en_girla.c EnGirlA_CanBuy_Randomizer)
 *  for shuffled shelf @p rc, whose RandomizerInf is @p bought. @return 0 on sale
 *  (unbought, or bought and restocking with room for it), 1 bought and sold out,
 *  2 bought and restocking but the sale would add nothing (the caller refuses it
 *  as CANBUY_RESULT_CANT_GET_NOW). */
extern "C" int OoT_Rando_ShopRestockBuyState(uint16_t rc, int bought) {
    if (bought == 0) {
        return 0;
    }
    const uint16_t rg = OoT_Rando_ShopRestockItem(rc);
    if (rg == (uint16_t)RG_NONE) {
        return 1;
    }
    return OoT_Rando_ShopRestockFull(rg) != 0 ? 2 : 0;
}

/** The sale at the shelf's ItemGive (z_en_girla.c EnGirlA_ItemGive_Randomizer)
 *  for shuffled shelf @p rc, whose RandomizerInf is @p bought. @return 0 when it
 *  is unbought: the first sale, which the caller makes (it sets the flag, the
 *  check queue gives the placed item, and it charges). Otherwise 1: a restocked
 *  sale, made here (the item given directly and @p price charged, or, if the
 *  shelf has nothing to sell again, neither), and the flag left alone. */
extern "C" int OoT_Rando_ShopRestockSell(PlayState* play, uint16_t rc, int bought, int16_t price) {
    if (bought == 0) {
        return 0;
    }
    if (OoT_Rando_ShopRestockGive(play, rc) != 0) {
        OoT_Rupees_ChangeBy((s16)-price);
    }
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
    const struct {
        RandomizerGet bottle;
        RandomizerGet refill;
        uint8_t item;
    } kBottleRestocks[] = {
        { RG_BOTTLE_WITH_RED_POTION, RG_RED_POTION_REFILL, ITEM_POTION_RED },
        { RG_BOTTLE_WITH_GREEN_POTION, RG_GREEN_POTION_REFILL, ITEM_POTION_GREEN },
        { RG_BOTTLE_WITH_BLUE_POTION, RG_BLUE_POTION_REFILL, ITEM_POTION_BLUE },
        { RG_BOTTLE_WITH_MILK, RG_MILK, ITEM_MILK },
    };
    for (const auto& b : kBottleRestocks) {
        if (OoT_Rando_ShopRestockItemFor((uint16_t)b.bottle) != (uint16_t)b.refill) {
            fprintf(stderr, "[OoT ShopRestock] bottle RG %d restocks as %u, expected %d\n", (int)b.bottle,
                    (unsigned)OoT_Rando_ShopRestockItemFor((uint16_t)b.bottle), (int)b.refill);
            return 2;
        }
        const uint8_t given = OoT_Rando_ShopRestockGiveItemId((uint16_t)b.bottle);
        if (given != b.item) {
            fprintf(stderr, "[OoT ShopRestock] bottle RG %d gives OoT item %u, expected %u\n", (int)b.bottle,
                    (unsigned)given, (unsigned)b.item);
            return 3;
        }
    }
    const RandomizerGet kSellOut[] = {
        RG_BOTTLE_WITH_FAIRY,
        RG_BOTTLE_WITH_FISH,
        RG_BOTTLE_WITH_BLUE_FIRE,
        RG_BOTTLE_WITH_BUGS,
        RG_BOTTLE_WITH_POE,
        RG_BOTTLE_WITH_BIG_POE,
        RG_RUTOS_LETTER,
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

namespace {

template <typename H> struct ShopRestockHooksAside {
    decltype(GameInteractor::RegisteredGameHooks<H>::functions) functions;
    decltype(GameInteractor::RegisteredGameHooks<H>::functionsForFilter) filters;
    ShopRestockHooksAside() {
        functions.swap(GameInteractor::RegisteredGameHooks<H>::functions);
        filters.swap(GameInteractor::RegisteredGameHooks<H>::functionsForFilter);
    }
    ~ShopRestockHooksAside() {
        functions.swap(GameInteractor::RegisteredGameHooks<H>::functions);
        filters.swap(GameInteractor::RegisteredGameHooks<H>::functionsForFilter);
    }
};

int ShopRestockPurse(void) {
    return (int)gSaveContext.rupees + (int)gSaveContext.rupeeAccumulator;
}

/**
 * The CanBuy decision and the sale itself, on the test shelf @p il / @p rc, on
 * the live save, which is put back byte for byte. The vanilla quest is set for
 * the leg so OoT_Item_Give asks no randomizer setting. @return 0, or a step code
 * from 6.
 *
 *   6  unbought, the shelf is on sale (both modes).
 *   7  bought, holding Deku Nuts (5) with room: on sale (state 0), or with a
 *      foreign host sold out (state 1). Catches CanBuy refusing every bought
 *      shelf (main before #1482: CANBUY_RESULT_CANT_GET_NOW).
 *   8  bought, nuts at capacity: refused for nothing to add (state 2), or with a
 *      foreign host sold out (1).
 *   9  the unbought sale is left to the caller (0) and changes nothing.
 *  10  the bought sale is made here (1).
 *  11  it adds the pack: nuts +5 (nothing with a foreign host). Catches a dropped
 *      or wrong OoT_Item_Give, or a give through the flag (the check queue gives
 *      nothing on a check already obtained).
 *  12  it charges the price (nothing with a foreign host). Catches a dropped
 *      OoT_Rupees_ChangeBy, or charging for a sale that gave nothing.
 *  13  it leaves every RandomizerInf as it was (the check stays as it is).
 *  14  bought, holding a Piece of Heart: sold out (1), and its sale gives and
 *      charges nothing.
 */
int ShopRestockSaleLeg(Rando::ItemLocation* il, RandomizerCheck rc, int foreignHosted) {
    std::unique_ptr<SaveContext> savedSave = std::make_unique<SaveContext>();
    memcpy(savedSave.get(), &gSaveContext, sizeof(SaveContext));
    GameInteractor* const savedInteractor = GameInteractor::Instance;
    std::unique_ptr<GameInteractor> harnessInteractor;
    if (GameInteractor::Instance == nullptr) {
        harnessInteractor = std::make_unique<GameInteractor>();
        GameInteractor::Instance = harnessInteractor.get();
    }
    ItemTableManager* const savedItemTables = ItemTableManager::Instance;
    std::unique_ptr<ItemTableManager> harnessItemTables;
    if (ItemTableManager::Instance == nullptr) {
        harnessItemTables = std::make_unique<ItemTableManager>();
        harnessItemTables->AddItemTable(MOD_NONE);
        ItemTableManager::Instance = harnessItemTables.get();
    }

    int step = 0;
    {
        ShopRestockHooksAside<GameInteractor::OnItemReceive> receive;
        ShopRestockHooksAside<GameInteractor::OnTimestamp> timestamp;
        const bool foreign = foreignHosted != 0;
        const int16_t price = 20;

        gSaveContext.ship.quest.id = QUEST_NORMAL;
        OoT_Inventory_ChangeUpgrade(UPG_NUTS, 1);
        INV_CONTENT(ITEM_NUT) = ITEM_NUT;
        const int nutCap = CUR_CAPACITY(UPG_NUTS);
        AMMO(ITEM_NUT) = (s8)(nutCap - 10);
        gSaveContext.rupees = 100;
        gSaveContext.rupeeAccumulator = 0;
        il->SetPlacedItem(RG_DEKU_NUTS_5);

        if (nutCap <= 10) {
            step = 6;
        } else if (OoT_Rando_ShopRestockBuyState((uint16_t)rc, 0) != 0) {
            step = 6;
        } else if (OoT_Rando_ShopRestockBuyState((uint16_t)rc, 1) != (foreign ? 1 : 0)) {
            step = 7;
        }
        if (step == 0) {
            AMMO(ITEM_NUT) = (s8)nutCap;
            if (OoT_Rando_ShopRestockBuyState((uint16_t)rc, 1) != (foreign ? 1 : 2)) {
                step = 8;
            }
            AMMO(ITEM_NUT) = (s8)(nutCap - 10);
        }
        if (step == 0) {
            if (OoT_Rando_ShopRestockSell(nullptr, (uint16_t)rc, 0, price) != 0 || AMMO(ITEM_NUT) != nutCap - 10 ||
                ShopRestockPurse() != 100) {
                step = 9;
            }
        }
        if (step == 0) {
            decltype(gSaveContext.ship.randomizerInf) infBefore;
            memcpy(infBefore, gSaveContext.ship.randomizerInf, sizeof(infBefore));
            if (OoT_Rando_ShopRestockSell(nullptr, (uint16_t)rc, 1, price) != 1) {
                step = 10;
            } else if (AMMO(ITEM_NUT) != (foreign ? nutCap - 10 : nutCap - 5)) {
                fprintf(stderr, "[OoT ShopRestock] restocked nut sale (foreign %d): ammo %d, expected %d\n",
                        foreignHosted, (int)AMMO(ITEM_NUT), foreign ? nutCap - 10 : nutCap - 5);
                step = 11;
            } else if (ShopRestockPurse() != (foreign ? 100 : 100 - price)) {
                fprintf(stderr, "[OoT ShopRestock] restocked nut sale (foreign %d): purse %d, expected %d\n",
                        foreignHosted, ShopRestockPurse(), foreign ? 100 : 100 - price);
                step = 12;
            } else if (memcmp(infBefore, gSaveContext.ship.randomizerInf, sizeof(infBefore)) != 0) {
                step = 13;
            }
        }
        if (step == 0) {
            il->SetPlacedItem(RG_PIECE_OF_HEART);
            const int nuts = AMMO(ITEM_NUT);
            const int purse = ShopRestockPurse();
            if (OoT_Rando_ShopRestockBuyState((uint16_t)rc, 1) != 1 ||
                OoT_Rando_ShopRestockSell(nullptr, (uint16_t)rc, 1, price) != 1 || AMMO(ITEM_NUT) != nuts ||
                ShopRestockPurse() != purse) {
                step = 14;
            }
        }
        if (step == 0) {
            il->SetPlacedItem(RG_BOTTLE_WITH_RED_POTION);
            for (int slot = SLOT_BOTTLE_1; slot <= SLOT_BOTTLE_4; slot++) {
                gSaveContext.inventory.items[slot] = ITEM_NONE;
            }
            const ItemObtainability wantNoBottle = foreign ? CAN_OBTAIN : CANT_OBTAIN_NEED_EMPTY_BOTTLE;
            if (OoT_Rando_ShopShelfObtainability((uint16_t)rc, 1) != wantNoBottle) {
                step = 15;
            } else if (!foreign) {
                const GetItemEntry entry = OoT_Rando_ShopShelfGetItemEntry((uint16_t)rc, GI_NONE, 1);
                if (entry.getItemId != GI_POTION_RED || entry.gid != GID_POTION_RED || entry.modIndex != MOD_NONE ||
                    entry.itemId != ITEM_POTION_RED) {
                    step = 15;
                }
            }
        }
        if (step == 0) {
            gSaveContext.inventory.items[SLOT_BOTTLE_1] = ITEM_BOTTLE;
            gSaveContext.inventory.items[SLOT_BOTTLE_2] = ITEM_POTION_GREEN;
            gSaveContext.inventory.items[SLOT_BOTTLE_3] = ITEM_POTION_RED;
            gSaveContext.inventory.items[SLOT_BOTTLE_4] = ITEM_MILK_BOTTLE;
            if (OoT_Rando_ShopShelfObtainability((uint16_t)rc, 1) != CAN_OBTAIN ||
                OoT_Rando_ShopRestockBuyState((uint16_t)rc, 1) != (foreign ? 1 : 0)) {
                step = 16;
            }
        }
        if (step == 0) {
            gSaveContext.rupees = 100;
            gSaveContext.rupeeAccumulator = 0;
            decltype(gSaveContext.ship.randomizerInf) infBefore;
            memcpy(infBefore, gSaveContext.ship.randomizerInf, sizeof(infBefore));
            const uint8_t wantSlot = foreign ? ITEM_BOTTLE : ITEM_POTION_RED;
            if (OoT_Rando_ShopRestockSell(nullptr, (uint16_t)rc, 1, price) != 1 ||
                gSaveContext.inventory.items[SLOT_BOTTLE_1] != wantSlot ||
                ShopRestockPurse() != (foreign ? 100 : 100 - price) ||
                memcmp(infBefore, gSaveContext.ship.randomizerInf, sizeof(infBefore)) != 0) {
                step = 17;
            }
        }
        if (step == 0) {
            il->SetPlacedItem(RG_BOTTLE_WITH_MILK);
            gSaveContext.inventory.items[SLOT_BOTTLE_1] = ITEM_BOTTLE;
            gSaveContext.rupees = 100;
            gSaveContext.rupeeAccumulator = 0;
            if (!foreign) {
                const GetItemEntry entry = OoT_Rando_ShopShelfGetItemEntry((uint16_t)rc, GI_NONE, 1);
                if (entry.getItemId != GI_MILK || entry.gid != GID_MILK || entry.modIndex != MOD_NONE ||
                    entry.itemId != ITEM_MILK) {
                    step = 18;
                }
            }
            const uint8_t wantSlot = foreign ? ITEM_BOTTLE : ITEM_MILK_BOTTLE;
            if (step == 0 && (OoT_Rando_ShopRestockSell(nullptr, (uint16_t)rc, 1, price) != 1 ||
                              gSaveContext.inventory.items[SLOT_BOTTLE_1] != wantSlot ||
                              ShopRestockPurse() != (foreign ? 100 : 100 - price))) {
                step = 18;
            }
        }
        if (step == 0) {
            il->SetPlacedItem(RG_BOTTLE_WITH_FAIRY);
            gSaveContext.inventory.items[SLOT_BOTTLE_1] = ITEM_BOTTLE;
            if (OoT_Rando_ShopRestockBuyState((uint16_t)rc, 1) != 1) {
                step = 19;
            }
        }
    }

    ItemTableManager::Instance = savedItemTables;
    GameInteractor::Instance = savedInteractor;
    memcpy(&gSaveContext, savedSave.get(), sizeof(SaveContext));
    return step;
}

} // namespace

/**
 * The shelf, by placement: the test shelf holding a restocking item answers it
 * and is not sold out once bought; holding a Piece of Heart, a bottle or an ice
 * trap it is sold out once bought; unbought it is on sale either way. With
 * @p foreignHosted the caller has made the shelf host a Majora's Mask item
 * (Combo_SetForeignPlacementOoT), and then nothing restocks and a bought shelf
 * is sold out whatever the OoT table holds there. Then the CanBuy decision and
 * the sale itself (ShopRestockSaleLeg, steps 6 to 14).
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
                RandomizerGet restock;
            } kCases[] = {
                { RG_DEKU_NUTS_5, RG_DEKU_NUTS_5 },
                { RG_DEKU_SHIELD, RG_DEKU_SHIELD },
                { RG_RED_POTION_REFILL, RG_RED_POTION_REFILL },
                { RG_BOTTLE_WITH_RED_POTION, RG_RED_POTION_REFILL },
                { RG_BOTTLE_WITH_GREEN_POTION, RG_GREEN_POTION_REFILL },
                { RG_BOTTLE_WITH_BLUE_POTION, RG_BLUE_POTION_REFILL },
                { RG_BOTTLE_WITH_MILK, RG_MILK },
                { RG_PIECE_OF_HEART, RG_NONE },
                { RG_ICE_TRAP, RG_NONE },
                { RG_BOTTLE_WITH_FAIRY, RG_NONE },
                { RG_BOTTLE_WITH_POE, RG_NONE },
                { RG_BOTTLE_WITH_BIG_POE, RG_NONE },
                { RG_BOTTLE_WITH_BLUE_FIRE, RG_NONE },
                { RG_BOTTLE_WITH_FISH, RG_NONE },
                { RG_BOTTLE_WITH_BUGS, RG_NONE },
                { RG_BOTTLE_EMPTY, RG_NONE },
                { RG_RUTOS_LETTER, RG_NONE },
                { RG_PROGRESSIVE_HOOKSHOT, RG_NONE },
            };
            for (const auto& c : kCases) {
                il->SetPlacedItem(c.placed);
                const bool restocks = c.restock != RG_NONE && foreignHosted == 0;
                const uint16_t want = restocks ? (uint16_t)c.restock : (uint16_t)RG_NONE;
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
                if (OoT_Rando_ShopShelfDisplayedItem((uint16_t)rc, 0) != (uint16_t)c.placed ||
                    OoT_Rando_ShopShelfDisplayedItem((uint16_t)rc, 1) !=
                        (uint16_t)(restocks ? c.restock : c.placed)) {
                    step = 5;
                    break;
                }
            }
            if (step == 0) {
                step = ShopRestockSaleLeg(il, rc, foreignHosted);
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

/**
 * Shop offer text + "ONE LEFT" behavior (#1547): restocking shelves omit
 * "Special deal! ONE LEFT!" (Mysterious Shuffle preserves it before the first
 * buy), bottled potion/milk shelves switch to the refill's name after purchase,
 * and non-restocking or foreign-hosted shelves keep "ONE LEFT!".
 */
extern "C" int OoT_Rando_ShopRestockTestMessages(int foreignHosted) {
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
            il->SetPrice(30);
            char buf[512] = {};

            if (foreignHosted != 0) {
                il->SetPlacedItem(RG_DEKU_NUTS_5);
                if (OoT_Rando_ShopMessageIncludesOneLeft((uint16_t)rc, 0, 0) != 1 ||
                    OoT_Rando_ShopMessageIncludesOneLeft((uint16_t)rc, 1, 0) != 1) {
                    step = 3;
                } else if (OoT_Rando_ShopOfferText((uint16_t)rc, 0, 0, 0, buf, sizeof(buf)) <= 0 ||
                           std::strstr(buf, "ONE LEFT") == nullptr) {
                    step = 4;
                }
            } else {
                il->SetPlacedItem(RG_DEKU_NUTS_5);
                if (OoT_Rando_ShopMessageIncludesOneLeft((uint16_t)rc, 0, 0) != 0 ||
                    OoT_Rando_ShopMessageIncludesOneLeft((uint16_t)rc, 1, 0) != 0) {
                    step = 5;
                } else if (OoT_Rando_ShopMessageIncludesOneLeft((uint16_t)rc, 0, 1) != 1 ||
                           OoT_Rando_ShopMessageIncludesOneLeft((uint16_t)rc, 1, 1) != 0) {
                    step = 6;
                } else if (OoT_Rando_ShopOfferText((uint16_t)rc, 0, 0, 0, buf, sizeof(buf)) <= 0 ||
                           std::strstr(buf, "Deku Nuts") == nullptr || std::strstr(buf, "ONE LEFT") != nullptr ||
                           OoT_Rando_ShopOfferText((uint16_t)rc, 1, 0, 0, buf, sizeof(buf)) <= 0 ||
                           std::strstr(buf, "Deku Nuts") == nullptr || std::strstr(buf, "ONE LEFT") != nullptr ||
                           OoT_Rando_ShopOfferText((uint16_t)rc, 0, 1, 0, buf, sizeof(buf)) <= 0 ||
                           std::strstr(buf, "Buy") == nullptr || std::strstr(buf, "ONE LEFT") != nullptr) {
                    step = 7;
                }

                if (step == 0) {
                    il->SetPlacedItem(RG_BOTTLE_WITH_RED_POTION);
                    if (OoT_Rando_ShopOfferText((uint16_t)rc, 0, 0, 0, buf, sizeof(buf)) <= 0 ||
                        std::strstr(buf, "Bottle with Red Potion") == nullptr ||
                        std::strstr(buf, "ONE LEFT") != nullptr) {
                        step = 8;
                    } else if (OoT_Rando_ShopOfferText((uint16_t)rc, 1, 0, 0, buf, sizeof(buf)) <= 0 ||
                               std::strstr(buf, "Red Potion Refill") == nullptr ||
                               std::strstr(buf, "Bottle") != nullptr || std::strstr(buf, "ONE LEFT") != nullptr) {
                        step = 9;
                    }
                }

                if (step == 0) {
                    il->SetPlacedItem(RG_BOTTLE_WITH_MILK);
                    if (OoT_Rando_ShopOfferText((uint16_t)rc, 0, 0, 0, buf, sizeof(buf)) <= 0 ||
                        std::strstr(buf, "Bottle with Milk") == nullptr || std::strstr(buf, "ONE LEFT") != nullptr) {
                        step = 10;
                    } else if (OoT_Rando_ShopOfferText((uint16_t)rc, 1, 0, 0, buf, sizeof(buf)) <= 0 ||
                               std::strstr(buf, "Milk") == nullptr || std::strstr(buf, "Bottle") != nullptr ||
                               std::strstr(buf, "ONE LEFT") != nullptr) {
                        step = 11;
                    }
                }

                if (step == 0) {
                    il->SetPlacedItem(RG_PIECE_OF_HEART);
                    if (OoT_Rando_ShopMessageIncludesOneLeft((uint16_t)rc, 0, 0) != 1 ||
                        OoT_Rando_ShopOfferText((uint16_t)rc, 0, 0, 0, buf, sizeof(buf)) <= 0 ||
                        std::strstr(buf, "Piece of Heart") == nullptr || std::strstr(buf, "ONE LEFT") == nullptr ||
                        OoT_Rando_ShopOfferText((uint16_t)rc, 0, 1, 0, buf, sizeof(buf)) <= 0 ||
                        std::strstr(buf, "Buy") == nullptr || std::strstr(buf, "ONE LEFT") != nullptr) {
                        step = 12;
                    }
                }

                if (step == 0) {
                    il->SetPlacedItem(RG_BOTTLE_WITH_FAIRY);
                    if (OoT_Rando_ShopMessageIncludesOneLeft((uint16_t)rc, 0, 0) != 1 ||
                        OoT_Rando_ShopOfferText((uint16_t)rc, 0, 0, 0, buf, sizeof(buf)) <= 0 ||
                        std::strstr(buf, "ONE LEFT") == nullptr) {
                        step = 13;
                    }
                }
            }

            *il = saved;
        }
    }
    if (created != nullptr) {
        created->GetLogic()->SetContext(nullptr);
    }
    return step;
}

/**
 * Fill & logic credit (#1482 §2 / #1547): OoTCritHostRenews credits any
 * restocking shop shelf (including bottle->refill and empty shelf), and
 * Logic::ApplyItemEffect grants repeatable access for shop-hosted refills,
 * Bombchus, Deku Nuts/Sticks, shields, and fish only when placed on a shop
 * shelf.
 */
extern "C" int OoT_Rando_ShopRestockTestLogic(void) {
    if (OoT_ComboLogic_TestEnsureItemTableTransient() != 0) {
        return 1;
    }
    const ComboCriticalRenewableSource* src = Combo_GetCriticalRenewableSource(GAME_OOT);
    if (src == nullptr || src->hostRenews == nullptr) {
        return 2;
    }
    std::shared_ptr<Rando::Context> created;
    if (Rando::Context::GetInstance() == nullptr) {
        created = Rando::Context::CreateInstance();
    }
    int step = 0;
    {
        const std::shared_ptr<Rando::Context> ctx = Rando::Context::GetInstance();
        const std::shared_ptr<Rando::Logic> logic = ctx != nullptr ? ctx->GetLogic() : nullptr;
        const RandomizerCheck shopRc = RC_KF_SHOP_ITEM_1;
        const RandomizerCheck chestRc = RC_KF_KOKIRI_SWORD_CHEST;
        Rando::ItemLocation* ilShop = ctx != nullptr ? ctx->GetItemLocation(shopRc) : nullptr;
        Rando::ItemLocation* ilChest = ctx != nullptr ? ctx->GetItemLocation(chestRc) : nullptr;
        if (logic == nullptr || ilShop == nullptr || ilChest == nullptr) {
            step = 2;
        } else {
            const Rando::ItemLocation savedShop = *ilShop;
            const Rando::ItemLocation savedChest = *ilChest;
            const RandomizerCheck savedCheckKey = logic->CurrentCheckKey;
            const bool savedIsChild = logic->IsChild;
            const uint8_t savedChuBag = ctx->GetOption(RSK_BOMBCHU_BAG).Get();
            const uint8_t savedChuDrops = ctx->GetOption(RSK_ENABLE_BOMBCHU_DROPS).Get();

            ilChest->SetPlacedItem(RG_DEKU_SHIELD);
            if (src->hostRenews((uint16_t)chestRc) != 0) {
                step = 3;
            }

            if (step == 0) {
                const struct {
                    RandomizerGet item;
                    int renews;
                } kRenewCases[] = {
                    { RG_NONE, 1 },
                    { RG_DEKU_SHIELD, 1 },
                    { RG_BOMBCHU_5, 1 },
                    { RG_BOTTLE_WITH_RED_POTION, 1 },
                    { RG_BOTTLE_WITH_GREEN_POTION, 1 },
                    { RG_BOTTLE_WITH_BLUE_POTION, 1 },
                    { RG_BOTTLE_WITH_MILK, 1 },
                    { RG_RED_POTION_REFILL, 1 },
                    { RG_MILK, 1 },
                    { RG_SOLD_OUT, 0 },
                    { RG_PIECE_OF_HEART, 0 },
                    { RG_BOTTLE_WITH_FAIRY, 0 },
                };
                for (const auto& c : kRenewCases) {
                    ilShop->SetPlacedItem(c.item);
                    if (src->hostRenews((uint16_t)shopRc) != c.renews) {
                        step = 4;
                        break;
                    }
                }
            }

            const RandomizerGet kLogicItems[] = {
                RG_DEKU_SHIELD, RG_HYLIAN_SHIELD, RG_BOMBCHU_5, RG_DEKU_NUTS_5, RG_DEKU_STICK_1,
                RG_ARROWS_5, RG_BOMBS_5, RG_DEKU_SEEDS_30, RG_RED_POTION_REFILL, RG_FISH,
            };

            if (step == 0) {
                ctx->GetOption(RSK_BOMBCHU_BAG).Set(RO_BOMBCHU_BAG_NONE);
                ctx->GetOption(RSK_ENABLE_BOMBCHU_DROPS).Set(RO_AMMO_DROPS_OFF);
                logic->IsChild = true;

                logic->Reset(true);
                logic->CurrentCheckKey = chestRc;
                for (RandomizerGet rg : kLogicItems) {
                    Rando::StaticData::RetrieveItem(rg).ApplyEffect();
                }
                if (logic->HasItem(RG_DEKU_SHIELD) || logic->HasItem(RG_HYLIAN_SHIELD) ||
                    logic->Get(LOGIC_BUY_BOMBCHUS) || logic->CanUse(RG_BOMBCHU_5) || logic->Get(LOGIC_NUT_ACCESS) ||
                    logic->Get(LOGIC_STICK_ACCESS) || logic->Get(LOGIC_BUY_ARROW) || logic->Get(LOGIC_BUY_BOMB) ||
                    logic->Get(LOGIC_BUY_SEED) || logic->Get(LOGIC_BUY_MAGIC_POTION) || logic->Get(LOGIC_FISH_ACCESS)) {
                    step = 5;
                }
            }

            if (step == 0) {
                logic->Reset(true);
                logic->CurrentCheckKey = shopRc;
                for (RandomizerGet rg : kLogicItems) {
                    Rando::StaticData::RetrieveItem(rg).ApplyEffect();
                }
                if (!logic->HasItem(RG_DEKU_SHIELD) || !logic->HasItem(RG_HYLIAN_SHIELD) ||
                    !logic->Get(LOGIC_BUY_BOMBCHUS) || !logic->CanUse(RG_BOMBCHU_5) || !logic->Get(LOGIC_NUT_ACCESS) ||
                    !logic->Get(LOGIC_STICK_ACCESS) || !logic->Get(LOGIC_BUY_ARROW) || !logic->Get(LOGIC_BUY_BOMB) ||
                    !logic->Get(LOGIC_BUY_SEED) || !logic->Get(LOGIC_BUY_MAGIC_POTION) ||
                    !logic->Get(LOGIC_FISH_ACCESS)) {
                    step = 6;
                }
            }

            if (step == 0) {
                logic->Reset(true);
                logic->CurrentCheckKey = chestRc;
                Rando::StaticData::RetrieveItem(RG_BOTTLE_WITH_RED_POTION).ApplyEffect();
                if (logic->Get(LOGIC_BUY_MAGIC_POTION)) {
                    step = 7;
                } else {
                    logic->Reset(true);
                    logic->CurrentCheckKey = shopRc;
                    Rando::StaticData::RetrieveItem(RG_BOTTLE_WITH_RED_POTION).ApplyEffect();
                    if (!logic->Get(LOGIC_BUY_MAGIC_POTION)) {
                        step = 8;
                    }
                }
            }

            logic->Reset(true);
            logic->IsChild = savedIsChild;
            logic->CurrentCheckKey = savedCheckKey;
            ctx->GetOption(RSK_BOMBCHU_BAG).Set(savedChuBag);
            ctx->GetOption(RSK_ENABLE_BOMBCHU_DROPS).Set(savedChuDrops);
            *ilShop = savedShop;
            *ilChest = savedChest;
        }
    }
    if (created != nullptr) {
        created->GetLogic()->SetContext(nullptr);
    }
    return step;
}

#endif // RSBS_SINGLE_EXECUTABLE
