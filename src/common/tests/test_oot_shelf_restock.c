/**
 * @file test_oot_shelf_restock.c
 * @brief #1482: a bought shuffled OoT shop shelf sells its item again when OoTMM
 *        restocks it (games/oot/soh/Enhancements/randomizer/ShopRestockSingleExe.cpp).
 *
 * ROM-free and display-free. The shelf actors (z_en_girla.c, z_en_ossan.c) ask
 * four functions of that TU; this row locks those functions over real ids, a
 * real shelf check and the live save, through its test bridges:
 *
 *   A. THE LIST. OoTMM's OoT restock list (progressive.c comboRenewable), as the
 *      RG ids that restock as themselves, answers itself and gives its own plain
 *      OoT item; bottles, rupees, a trap, a Piece of Heart, a major item, the
 *      Mirror Shield, a bean and a vanilla shelf row answer RG_NONE and give
 *      nothing. Catches an item dropped from or added to the list, and a restock
 *      that would hand OoT_Item_Give the wrong item.
 *   B. THE SHELF. Kokiri Forest's first shelf holding a restocking item answers
 *      it and, bought, is not sold out; holding a Piece of Heart, an ice trap, a
 *      bottle or a Hookshot, bought, it is sold out; unbought it is on sale.
 *      Catches the sold-out rule reverting to "every bought shelf" (main before
 *      #1482) or restocking everything.
 *   C. A FOREIGN HOST. The same shelf hosting a Majora's Mask item restocks
 *      nothing and is sold out once bought, whatever the OoT table holds there
 *      (the crossing store delivers once). Catches the restock predicate losing
 *      its foreign-host guard.
 *   B/C, THE SALE (steps 6 to 14, both modes). The shelf's CanBuy decision
 *      (OoT_Rando_ShopRestockBuyState, which EnGirlA_CanBuy_Randomizer asks) and
 *      the sale itself (OoT_Rando_ShopRestockSell, which
 *      EnGirlA_ItemGive_Randomizer asks), on the live save, put back byte for
 *      byte. Bought, a Deku Nuts (5) shelf with room is on sale; at capacity it
 *      is refused for nothing to add; a Piece of Heart shelf is sold out. The
 *      bought sale really gives: OoT_Item_Give runs (NULL play, its hooks set
 *      aside), the nut count rises by 5, the price is charged, and no
 *      RandomizerInf changes; the unbought sale is left to the first-sale path.
 *      With a foreign host the bought shelf is sold out and its sale gives and
 *      charges nothing. Catches CanBuy refusing every bought shelf (main before
 *      #1482), a dropped or wrong give, a give through the flag (which gives
 *      nothing on an obtained check), a dropped charge, and a charge for nothing.
 *   D. NOTHING TO ADD. Deku Nuts at capacity, a Recovery Heart at full health and
 *      Bombchus at 50 refuse a restocked sale, one short they do not, and a
 *      shield never does. Catches a restocked sale charging for nothing.
 *
 * What this row does NOT drive: the actors themselves (they need a PlayState and
 * a loaded shop scene). Each actor call site is one call into the functions
 * above, read in review: reverting a call site does NOT turn this row red. The
 * Bombchu no-explosives refusal needs a randomizer context's options and is not
 * driven either. The playtest shows a bought shelf selling again.
 *
 * FILE SCOPE (compiled as C++ inside test_runner.cpp); every symbol it drives is
 * C-linkage.
 */

#include "../context.h"
#include "../foreign_items.h"
#include "../test_runner.h"

#include <cstdio>

#ifdef __cplusplus
extern "C" {
#endif
uint16_t OoT_Rando_ShopRestockTestShelfCheck(void);
int OoT_Rando_ShopRestockTestTable(void);
int OoT_Rando_ShopRestockTestShelves(int foreignHosted);
int OoT_Rando_ShopRestockTestFull(void);
int OoT_Rando_ShopRestockTestMessages(int foreignHosted);
int OoT_Rando_ShopRestockTestLogic(void);
#ifdef __cplusplus
}
#endif

TestResult Test_OoTShelfRestock(void) {
    printf("[TEST] oot-shelf-restock: a bought shuffled OoT shelf sells OoTMM's renewables again (#1482)\n");

    // A. The list, by real id.
    const int table = OoT_Rando_ShopRestockTestTable();
    if (table != 0) {
        printf("[TEST] FAIL: the restock list is not OoTMM's OoT list (step %d: 1 no item table, 2 a renewable does "
               "not restock, 3 it gives the wrong OoT item, 4 a non-renewable restocks, 5 a non-check restocks)\n",
               table);
        return TEST_FAIL;
    }

    // B. The shelf, with no foreign placement anywhere.
    ComboContext_Init();
    const int shelves = OoT_Rando_ShopRestockTestShelves(0);
    if (shelves != 0) {
        printf("[TEST] FAIL: the shelf's restock answer is wrong (step %d: 1 no item table, 2 no item location, 3 "
               "the restock item, 4 sold out once bought, 5 sold out before it is bought, 6-8 the CanBuy decision, "
               "9-13 the restocked sale's give, charge and flags, 14 a Piece of Heart shelf's sale, 15-17 a red "
               "potion bottle shelf's obtainability, entry and refill sale, 18 a milk bottle shelf, 19 a fairy "
               "bottle shelf sells out, 20 the obtainability decision asks for the refill)\n",
               shelves);
        ComboContext_Init();
        return TEST_FAIL;
    }

    // C. The same shelf hosting a Majora's Mask item.
    SharedItem mmItem;
    mmItem.originGame = (uint8_t)GAME_MM;
    mmItem.flags = 0;
    mmItem.id = 1;
    if (Combo_SetForeignPlacementOoT(OoT_Rando_ShopRestockTestShelfCheck(), mmItem) < 0) {
        printf("[TEST] FAIL: the shelf could not be made to host a Majora's Mask item\n");
        ComboContext_Init();
        return TEST_FAIL;
    }
    const int foreign = OoT_Rando_ShopRestockTestShelves(1);
    const int foreignMsgs = foreign == 0 ? OoT_Rando_ShopRestockTestMessages(1) : 0;
    ComboContext_Init();
    if (foreign != 0) {
        printf("[TEST] FAIL: a shelf hosting a Majora's Mask item restocks, or is not sold out once bought (step "
               "%d: 3-5 the shelf, 6-8 the CanBuy decision, 9-19 the sale gives or charges, 20 the obtainability "
               "decision asks by check)\n",
               foreign);
        return TEST_FAIL;
    }
    if (foreignMsgs != 0) {
        printf("[TEST] FAIL: a foreign-hosted shelf's offer text dropped ONE LEFT! (step %d)\n", foreignMsgs);
        return TEST_FAIL;
    }

    // D. Nothing to add.
    const int full = OoT_Rando_ShopRestockTestFull();
    if (full != 0) {
        printf("[TEST] FAIL: a restocked sale's nothing-to-add refusal is wrong (step %d: 1 no nut capacity, 2/3 "
               "nuts, 4/5 heart, 6/7 bombchus, 8 a shield)\n",
               full);
        return TEST_FAIL;
    }

    // E. Offer text, ONE LEFT!, and post-buy bottle->refill naming (#1547).
    const int msgs = OoT_Rando_ShopRestockTestMessages(0);
    if (msgs != 0) {
        printf("[TEST] FAIL: shop offer text / ONE LEFT! rule is wrong (step %d)\n", msgs);
        return TEST_FAIL;
    }

    // F. Fill credit (OoTCritHostRenews) and logic reachability (ApplyItemEffect) (#1482 §2 / #1547).
    const int logic = OoT_Rando_ShopRestockTestLogic();
    if (logic != 0) {
        printf("[TEST] FAIL: fill/logic credit for restocking shelves is wrong (step %d)\n", logic);
        return TEST_FAIL;
    }

    printf("[TEST] PASS: oot-shelf-restock\n");
    return TEST_PASS;
}
