/**
 * @file test_stick_nut_bag_mm.c
 * @brief Lock for #1652 stages A1 and A3 (operator ruling 2026-11-15, option A):
 *        under the pair's frozen Deku Stick Bag / Deku Nut Bag shuffle the bag
 *        gates BOTH games. Majora's Mask's half is born without the bag, refuses
 *        sticks (nuts) at its give, its drops, shelves and checks until the
 *        pair's one bag, and a bag found in Termina opens both games at its
 *        pickup. With both shuffles off nothing changes.
 *
 * One CTest row, mm-stick-nut-bag-gate (MMStickNutBagGate, redship tier,
 * ROM-free, generates nothing): gComboCtx, the RAM watermark table, MM's REAL
 * give (MM_Item_Give, Rando::GiveItem), obtainability, conversion and junk draw,
 * MM's gate helpers (2s2h/Rando/StickNutBagGate.h), the REAL MM pickup core and
 * both games' REAL shared shims, through helpers that only set each game's live
 * save up and read it back (games/mm/2s2h/mm_stick_nut_bag_test.cpp,
 * games/mm/2s2h/mm_shared_tier_test.cpp, games/oot/soh/oot_shared_tier_test.cpp).
 * No CVar, no OTRGlobals, no PlayState.
 *
 *   (H) the decisions (shared_resources.h, Combo_SharedBag*) in every world:
 *       unfrozen, frozen with the bits clear, stick only, nut only, both;
 *   (B) the birth: MM_Combo_ApplyPairedBagBirth drops exactly the shuffled
 *       bag's tier, item and ammo, and touches nothing with the bits clear;
 *   (G) the give: MM_Item_Give refuses all six stick and nut ids at tier 0 under
 *       its own bit (returning the id), gives at tier 1, gives the other bag's
 *       item, and with the bits clear raises tier 0 to 1 as MM always has; the
 *       rando give (Consumables Full's path) is refused the same way; a crossed
 *       pack is redeemed as junk before the bag and as itself after it;
 *   (O) obtainability: the five RI ids are unobtainable and convert to junk at
 *       tier 0 under the bit, a cold junk draw never yields a stick or nut pack,
 *       and every answer is today's with the bits clear or at tier 1;
 *   (D) drops: MM_Combo_Item00Gated holds out the stick and both nut drops under
 *       their own bit at tier 0, whatever the collectible flag, never
 *       ITEM00_NOTHING or another drop;
 *   (S) shelves: MM_Combo_StickGated / MM_Combo_NutGated, the answers the
 *       vanilla shelves ask;
 *   (K) #1691, the offers that are not a drop, in both games: a Deku Baba's or
 *       withered (Mini) Baba's stick, MM's unshuffled chests and OoT's vanilla
 *       business scrubs ask MM_Combo_GetItemGated / OoT_Combo_GetItemGated, each
 *       over its own game's tier: closed, no offer; open or shuffle off, it stays;
 *   (Y) harvest/apply symmetry at tier 0: the pool's count does not materialize
 *       and is not debited; the bag's tier then lands it;
 *   (P) A3, the step at pickup: OoT's bag found in Termina steps the pool, opens
 *       MM at once with the bag's ten sticks (nuts), records no crossing, and
 *       OoT's next arrival holds it; a second copy (OoT's next capacity or its
 *       Infinite Upgrades item) crosses; with the bits clear it crosses as
 *       before; OoT's own step of a redeemed bag copy fills the same way;
 *   (A) the source anchors: each production call site calls its helper, searched
 *       inside the function it names.
 *
 * Linkage note: #included into test_runner.cpp at FILE SCOPE (compiled as C++),
 * after test_combo_bag_shuffle.c; every symbol under test is C-linkage and every
 * name here carries the Snb prefix so it cannot meet a neighbour's.
 */

#include "../context.h"
#include "../foreign_items.h"
#include "../shared_items.h"
#include "../shared_resources.h"
#include "../test_runner.h"

#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>

#ifdef __cplusplus
extern "C" {
#endif
// games/mm/2s2h/Rando/StickNutBagGate.h (defined in GameExports_SingleExe.cpp and
// ForeignItemsSingleExe.cpp), restated because this TU includes no game header.
int MM_Combo_StickGated(void);
int MM_Combo_NutGated(void);
int MM_Combo_StickNutGiveRefused(uint8_t item);
int MM_Combo_Item00Gated(int16_t params);
int MM_Combo_GetItemGated(int16_t getItemId);
void MM_Combo_ApplyPairedBagBirth(void);
uint16_t MM_ForeignItem_RedeemAs(uint16_t riId);
// games/mm/2s2h/mm_stick_nut_bag_test.cpp
void MM_SnbTest_ArmBorn(void);
void MM_SnbTest_SetBag(int nut, int tier, int held, int ammo);
int MM_SnbTest_Tier(int nut);
int MM_SnbTest_Ammo(int nut);
int MM_SnbTest_Held(int nut);
int MM_SnbTest_ItemId(int which);
int MM_SnbTest_GetItemId(int which);
int MM_SnbTest_Give(int item);
int MM_SnbTest_RandoId(int which);
int MM_SnbTest_Obtainable(int ri);
int MM_SnbTest_Convert(int ri);
int MM_SnbTest_ColdJunkDraw(void);
void MM_SnbTest_GiveRando(int ri);
int MM_SnbTest_Item00(int which);
void MM_Rando_TestColdJunkCache(void); // games/mm/2s2h/Rando/ConvertItem.cpp
// games/mm/2s2h/mm_shared_tier_test.cpp, games/oot/soh/oot_shared_tier_test.cpp
int MM_SharedTierTest_HostCheck(int which);
void OoT_SharedTierTest_ArmLive(void);
int OoT_SharedTierTest_BagUpgradeId(int nut);
int OoT_SharedTierTest_BagPackId(int nut);
int OoT_SharedTierTest_BombPackId(void);
int OoT_SharedTierTest_BagTier(int nut);
void OoT_SharedTierTest_SetBagTier(int nut, int tier);
int OoT_SharedTierTest_GetItemId(int which);
// soh/Enhancements/randomizer/StickNutBagGate.h (defined in OoT's GameExports_SingleExe.cpp)
int OoT_Combo_StickGated(void);
int OoT_Combo_NutGated(void);
int OoT_Combo_GetItemGated(int16_t getItemId);
int OoT_SharedTierTest_BagAmmo(int nut);
int OoT_SharedTierTest_BagHeld(int nut);
// Both games' shims and MM's pickup core.
void MM_HarvestSharedResources(void);
void MM_ApplySharedResources(void);
void OoT_HarvestSharedResources(void);
void OoT_ApplySharedResources(void);
int MM_Rando_Foreign_RecordPickup(uint16_t randoCheckId);
void MM_ForeignItem_TestResetPending(void);
int OoT_ComboLogic_TestEnsureItemTableTransient(void);
#ifdef __cplusplus
}
#endif

static int sSnbFailures;

#define SNB_CHECK(cond, ...)                          \
    do {                                              \
        if (!(cond)) {                                \
            printf("[TEST] FAIL: ");                  \
            printf(__VA_ARGS__);                      \
            printf(" (%s:%d)\n", __FILE__, __LINE__); \
            sSnbFailures++;                           \
        }                                             \
    } while (0)

namespace {

const uint8_t kSnbStickTier = (uint8_t)RSBS_SHARED_RES_STICK_TIER;
const uint8_t kSnbNutTier = (uint8_t)RSBS_SHARED_RES_NUT_TIER;
const uint8_t kSnbStickCount = (uint8_t)RSBS_SHARED_RES_STICK_COUNT;
const uint8_t kSnbNutCount = (uint8_t)RSBS_SHARED_RES_NUT_COUNT;

/** The five worlds every leg runs. `frozen` false is an unpaired file. */
const struct {
    bool frozen;
    bool stick;
    bool nut;
    const char* what;
} kSnbWorlds[] = {
    { false, false, false, "unfrozen" },     { true, false, false, "frozen, both off" },
    { true, true, false, "stick bag only" }, { true, false, true, "nut bag only" },
    { true, true, true, "both on" },
};

/** Freeze a paired world the way the creation does (playthrough.cpp), with OoT's
 *  two bag options @p stick and @p nut, and drop every watermark, outbox entry,
 *  placement and queued MM give; @p frozen false leaves the record unfrozen. */
void SnbWorld(bool frozen, bool stick, bool nut) {
    ComboContext_Init();
    Combo_ResetSharedResourceWatermarks();
    Combo_ClearSharedItemOutbox();
    Combo_ClearForeignPlacements();
    MM_ForeignItem_TestResetPending();
    if (!frozen) {
        return;
    }
    gComboCtx.sourceIsRando = true;
    gComboCtx.sharedRandoSeed = 0x1652A1A3u;
    gComboCtx.sharedRandoSettingsHash = 0x1652CA5Eu;
    gComboCtx.mmProfileDigest = 0x1652D16Eu;
    ComboSettingsRecord rec;
    Combo_ComboSettingsDefaults(&rec);
    Combo_ComboSettingsApplyCreationBagShuffles(&rec, stick, nut);
    ComboItemRulesRecord rules;
    memset(&rules, 0, sizeof(rules));
    Combo_FreezeComboSettingsEx(&rec, &rules);
}

int SnbPool(uint8_t kind) {
    uint16_t value = 0;
    if (!Combo_GetSharedResource(kind, &value)) {
        return -1;
    }
    return (int)value;
}

SharedItem SnbItem(GameId origin, int id) {
    SharedItem item;
    memset(&item, 0, sizeof(item));
    item.originGame = (uint8_t)origin;
    item.id = (uint16_t)id;
    return item;
}

bool SnbIsStickNutRando(int ri) {
    for (int i = 0; i < 5; i++) {
        if (ri == MM_SnbTest_RandoId(i)) {
            return true;
        }
    }
    return false;
}

/** The text of @p rel under the checkout, or "" when it cannot be read. */
std::string SnbReadSource(const char* rel) {
#ifdef RSBS_SOURCE_DIR
    std::ifstream in(std::string(RSBS_SOURCE_DIR) + "/" + rel, std::ios::binary);
    if (!in.good()) {
        return std::string();
    }
    return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
#else
    (void)rel;
    return std::string();
#endif
}

/** The body of the function whose definition starts with @p signature: from the
 *  signature to the first closing brace in column 0 after it; "" when absent. */
std::string SnbBody(const std::string& text, const char* signature) {
    const size_t at = text.find(signature);
    if (at == std::string::npos) {
        return std::string();
    }
    size_t end = text.find("\n}", at);
    if (end == std::string::npos) {
        end = text.size();
    }
    return text.substr(at, end - at);
}

/** Does @p first occur in @p body, and before @p then (when @p then is given)? */
bool SnbBefore(const std::string& body, const char* first, const char* then) {
    const size_t a = body.find(first);
    if (a == std::string::npos) {
        return false;
    }
    if (then == nullptr) {
        return true;
    }
    const size_t b = body.find(then, a);
    return b != std::string::npos;
}

} // namespace

TestResult MMStickNutBagGate_Run(void) {
    printf("[TEST] mm-stick-nut-bag-gate: under the pair's frozen Deku Stick Bag / Deku Nut Bag shuffle Majora's Mask "
           "is born without the bag, refuses sticks and nuts until the pair's bag, and a bag found in Termina opens "
           "both games at its pickup; with the shuffles off nothing changes (#1652 A1, A3)\n");
    sSnbFailures = 0;

    if (OoT_ComboLogic_TestEnsureItemTableTransient() != 0) {
        printf("[TEST] FAIL: mm-stick-nut-bag-gate could not bring OoT's item table up\n");
        return TEST_FAIL;
    }
    if (MM_SnbTest_ItemId(0) < 0 || MM_SharedTierTest_HostCheck(0) <= 0) {
        printf("[TEST] FAIL: mm-stick-nut-bag-gate needs the single-exe MM seams\n");
        return TEST_FAIL;
    }

    // ---- (H) The decisions ----------------------------------------------------------
    // Catches: a decision that reads anything but the frozen bit (the unfrozen and
    // both-off worlds), the stick and nut kinds swapped (the one-bag worlds), the
    // tier test dropped (tier 1), and the legacy block surviving under the bit.
    for (const auto& w : kSnbWorlds) {
        SnbWorld(w.frozen, w.stick, w.nut);
        SNB_CHECK(Combo_SharedBagShuffled(kSnbStickTier) == w.stick && Combo_SharedBagShuffled(kSnbNutTier) == w.nut,
                  "%s: the bag predicates read stick %d nut %d", w.what, Combo_SharedBagShuffled(kSnbStickTier) ? 1 : 0,
                  Combo_SharedBagShuffled(kSnbNutTier) ? 1 : 0);
        SNB_CHECK(!Combo_SharedBagShuffled(kSnbStickCount) &&
                      !Combo_SharedBagShuffled((uint8_t)RSBS_SHARED_RES_QUIVER_TIER),
                  "%s: a kind other than the two tiers reads shuffled", w.what);
        SNB_CHECK(Combo_SharedBagGateClosed(kSnbStickTier, 0) == w.stick &&
                      Combo_SharedBagGateClosed(kSnbNutTier, 0) == w.nut &&
                      !Combo_SharedBagGateClosed(kSnbStickTier, 1) && !Combo_SharedBagGateClosed(kSnbNutTier, 2),
                  "%s: the gate answers wrong at tier 0 or held closed at a nonzero tier", w.what);
        SNB_CHECK(Combo_SharedBagFillsAfterStep(kSnbStickTier, 0, 1) == w.stick &&
                      Combo_SharedBagFillsAfterStep(kSnbNutTier, 0, 1) == w.nut &&
                      !Combo_SharedBagFillsAfterStep(kSnbStickTier, 1, 2) &&
                      !Combo_SharedBagFillsAfterStep(kSnbStickTier, 0, 0) &&
                      !Combo_SharedBagFillsAfterStep((uint8_t)RSBS_SHARED_RES_QUIVER_TIER, 0, 1),
                  "%s: the fill answers for a step other than the bag's", w.what);
        SNB_CHECK(Combo_SharedBagLegacyGrantBlocked(kSnbStickTier, true, 0) == !w.stick &&
                      Combo_SharedBagLegacyGrantBlocked(kSnbNutTier, true, 0) == !w.nut &&
                      !Combo_SharedBagLegacyGrantBlocked(kSnbStickTier, false, 0) &&
                      !Combo_SharedBagLegacyGrantBlocked(kSnbStickTier, true, 1) &&
                      !Combo_SharedBagLegacyGrantBlocked(kSnbStickCount, true, 0),
                  "%s: OoT's legacy grant block answers wrong (it must hold only without the bit)", w.what);
        // A3: the two tiers step at pickup exactly under their own bit.
        SNB_CHECK(Combo_SharedResourceStepsAtPickup(kSnbStickTier) == w.stick &&
                      Combo_SharedResourceStepsAtPickup(kSnbNutTier) == w.nut,
                  "%s: the stick / nut tiers step at pickup %d / %d", w.what,
                  Combo_SharedResourceStepsAtPickup(kSnbStickTier) ? 1 : 0,
                  Combo_SharedResourceStepsAtPickup(kSnbNutTier) ? 1 : 0);
    }

    // ---- (B) The birth ------------------------------------------------------------
    // Catches: the predicate dropped (the off worlds), stick and nut swapped (the
    // one-bag worlds), the ChangeUpgrade(..., 0) line removed (both on).
    for (const auto& w : kSnbWorlds) {
        SnbWorld(w.frozen, w.stick, w.nut);
        MM_SnbTest_ArmBorn();
        MM_SnbTest_SetBag(0, 1, 0, 0);
        MM_SnbTest_SetBag(1, 1, 0, 0);
        MM_Combo_ApplyPairedBagBirth();
        const int st = MM_SnbTest_Tier(0);
        const int nt = MM_SnbTest_Tier(1);
        printf("[TEST] (B) %s: born at stick tier %d, nut tier %d\n", w.what, st, nt);
        SNB_CHECK(st == (w.stick ? 0 : 1) && nt == (w.nut ? 0 : 1) && MM_SnbTest_Held(0) == 0 &&
                      MM_SnbTest_Held(1) == 0 && MM_SnbTest_Ammo(0) == 0 && MM_SnbTest_Ammo(1) == 0,
                  "%s: the birth left stick tier %d, nut tier %d (expected %d, %d)", w.what, st, nt, w.stick ? 0 : 1,
                  w.nut ? 0 : 1);
        // A half that already held sticks keeps them unless its own bag is shuffled.
        MM_SnbTest_SetBag(0, 1, 1, 7);
        MM_SnbTest_SetBag(1, 2, 1, 9);
        MM_Combo_ApplyPairedBagBirth();
        SNB_CHECK((w.stick ? (MM_SnbTest_Tier(0) == 0 && MM_SnbTest_Held(0) == 0 && MM_SnbTest_Ammo(0) == 0)
                           : (MM_SnbTest_Tier(0) == 1 && MM_SnbTest_Held(0) == 1 && MM_SnbTest_Ammo(0) == 7)) &&
                      (w.nut ? (MM_SnbTest_Tier(1) == 0 && MM_SnbTest_Held(1) == 0 && MM_SnbTest_Ammo(1) == 0)
                             : (MM_SnbTest_Tier(1) == 2 && MM_SnbTest_Held(1) == 1 && MM_SnbTest_Ammo(1) == 9)),
                  "%s: the birth touched a bag whose shuffle is off, or left a shuffled one", w.what);
    }

    // ---- (G) The give ---------------------------------------------------------------
    // Catches: the refusal after Item_GiveImpl or removed (the tier moves), the nut
    // ids missing, the wrong predicate per item (the one-bag worlds' other item),
    // the tier test dropped (tier 1), a refusal that returns ITEM_NONE.
    for (const auto& w : kSnbWorlds) {
        SnbWorld(w.frozen, w.stick, w.nut);
        for (int which = 0; which < 6; which++) {
            const int nut = which >= 3 ? 1 : 0;
            const bool gated = nut ? w.nut : w.stick;
            const int id = MM_SnbTest_ItemId(which);
            MM_SnbTest_ArmBorn();
            MM_SnbTest_SetBag(nut, 0, 0, 0);
            const int ret = MM_SnbTest_Give(id);
            SNB_CHECK(MM_Combo_StickNutGiveRefused((uint8_t)id) == (gated ? 1 : 0), "%s: item %d refused %d at tier 0",
                      w.what, id, MM_Combo_StickNutGiveRefused((uint8_t)id));
            if (gated) {
                SNB_CHECK(ret == id && MM_SnbTest_Tier(nut) == 0 && MM_SnbTest_Held(nut) == 0 &&
                              MM_SnbTest_Ammo(nut) == 0,
                          "%s: MM_Item_Give(%d) at tier 0 under its bag shuffle returned %d and left tier %d held %d "
                          "ammo %d (expected the id back and nothing given)",
                          w.what, id, ret, MM_SnbTest_Tier(nut), MM_SnbTest_Held(nut), MM_SnbTest_Ammo(nut));
            } else {
                // MM's own give raises tier 0 to 1 with the item (z_parameter.c): today's behaviour.
                SNB_CHECK(MM_SnbTest_Tier(nut) == 1 && MM_SnbTest_Held(nut) == 1 && MM_SnbTest_Ammo(nut) > 0,
                          "%s: MM_Item_Give(%d) at tier 0 without its bag shuffle left tier %d held %d ammo %d "
                          "(expected MM's own raise to tier 1)",
                          w.what, id, MM_SnbTest_Tier(nut), MM_SnbTest_Held(nut), MM_SnbTest_Ammo(nut));
            }
            // After the bag (tier 1, held), every world gives.
            MM_SnbTest_SetBag(nut, 1, 1, 1);
            (void)MM_SnbTest_Give(id);
            SNB_CHECK(MM_SnbTest_Ammo(nut) > 1, "%s: MM_Item_Give(%d) at tier 1 gave nothing (ammo %d)", w.what, id,
                      MM_SnbTest_Ammo(nut));
        }
        SNB_CHECK(MM_Combo_StickNutGiveRefused((uint8_t)MM_SnbTest_ItemId(6)) == 0, "%s: a bomb is refused", w.what);
        // The rando give, Consumables Full's path (StartingItems.cpp): refused the same way.
        MM_SnbTest_ArmBorn();
        MM_SnbTest_SetBag(0, w.stick ? 0 : 1, 0, 0);
        MM_SnbTest_GiveRando(MM_SnbTest_RandoId(0));
        SNB_CHECK(MM_SnbTest_Held(0) == (w.stick ? 0 : 1), "%s: Rando::GiveItem(RI_DEKU_STICK) left the stick held %d",
                  w.what, MM_SnbTest_Held(0));
        // A crossed pack (MM_ForeignItem_RedeemAs, GiveNow's id). Catches: the
        // conversion removed (a stick handed back before the bag), a conversion to
        // "nothing", a conversion after the bag.
        for (int which = 0; which < 5; which++) {
            const int ri = MM_SnbTest_RandoId(which);
            const int nut = which >= 2 ? 1 : 0;
            const bool gated = nut ? w.nut : w.stick;
            MM_SnbTest_ArmBorn();
            MM_SnbTest_SetBag(nut, 0, 0, 0);
            // Cold: with no PlayState the junk cache never expires, and a draw an
            // earlier leg made at tier 1 must not stand in for this one.
            MM_Rando_TestColdJunkCache();
            const int as = (int)MM_ForeignItem_RedeemAs((uint16_t)ri);
            if (gated) {
                // The junk is a draw from Consumables' list, which holds both families, so
                // a pack of the OTHER family is a legal draw while that family's bag is
                // held (nut bag only: five sticks). A stick or nut pack passes only if this
                // save can take it now, read before the bag below changes the answer; the
                // pack's own family cannot at tier 0.
                SNB_CHECK(as != ri && (!SnbIsStickNutRando(as) || MM_SnbTest_Obtainable(as) == 1) &&
                              as != MM_SnbTest_RandoId(5) && as != MM_SnbTest_RandoId(6) && as != MM_SnbTest_RandoId(7),
                          "%s: a crossed pack %d before the bag is redeemed as %d (expected a real junk item)", w.what,
                          ri, as);
            } else {
                SNB_CHECK(as == ri, "%s: a crossed pack %d without its bag shuffle is redeemed as %d", w.what, ri, as);
            }
            MM_SnbTest_SetBag(nut, 1, 1, 1);
            SNB_CHECK((int)MM_ForeignItem_RedeemAs((uint16_t)ri) == ri,
                      "%s: a crossed pack %d after the bag is not redeemed as itself", w.what, ri);
        }
    }

    // ---- (O) Obtainability ------------------------------------------------------------
    // Catches: a case line removed (that id), the stick and nut gates swapped,
    // the gate holding at tier 1, the junk list's stick or nut pack drawn at tier 0.
    for (const auto& w : kSnbWorlds) {
        SnbWorld(w.frozen, w.stick, w.nut);
        for (int which = 0; which < 5; which++) {
            const int ri = MM_SnbTest_RandoId(which);
            const int nut = which >= 2 ? 1 : 0;
            const bool gated = nut ? w.nut : w.stick;
            MM_SnbTest_ArmBorn();
            MM_SnbTest_SetBag(nut, 0, 0, 0);
            const int obtainable = MM_SnbTest_Obtainable(ri);
            const int converted = MM_SnbTest_Convert(ri);
            SNB_CHECK(obtainable == (gated ? 0 : 1) && (gated ? converted == MM_SnbTest_RandoId(5) : converted == ri),
                      "%s: RI %d at tier 0: obtainable %d, converts to %d", w.what, ri, obtainable, converted);
            MM_SnbTest_SetBag(nut, 1, 1, 1);
            SNB_CHECK(MM_SnbTest_Obtainable(ri) == 1 && MM_SnbTest_Convert(ri) == ri,
                      "%s: RI %d at tier 1 is not obtainable as itself", w.what, ri);
        }
        // MM's own ammo-without-capacity row is untouched (arrows with no quiver).
        MM_SnbTest_ArmBorn();
        SNB_CHECK(MM_SnbTest_Obtainable(MM_SnbTest_RandoId(8)) == 0, "%s: arrows with no quiver became obtainable",
                  w.what);
        MM_SnbTest_ArmBorn();
        MM_SnbTest_SetBag(0, 0, 0, 0);
        MM_SnbTest_SetBag(1, 0, 0, 0);
        bool drewGated = false;
        for (int i = 0; i < 64; i++) {
            const int junk = MM_SnbTest_ColdJunkDraw();
            const bool stickPack = junk == MM_SnbTest_RandoId(1);
            const bool nutPack = junk == MM_SnbTest_RandoId(3);
            drewGated = drewGated || (stickPack && w.stick) || (nutPack && w.nut);
        }
        SNB_CHECK(!drewGated, "%s: a cold junk draw at tier 0 handed a stick or nut pack the gate holds out", w.what);
    }

    // ---- (D) Drops, and (S) the shelves' answers --------------------------------------
    // Catches: the ITEM00_NOTHING exemption dropped is caught at the hook's source
    // anchor below; here a type mapped to the wrong bag, the high byte not masked,
    // and a gate that ignores the tier.
    for (const auto& w : kSnbWorlds) {
        SnbWorld(w.frozen, w.stick, w.nut);
        MM_SnbTest_ArmBorn();
        MM_SnbTest_SetBag(0, 0, 0, 0);
        MM_SnbTest_SetBag(1, 0, 0, 0);
        SNB_CHECK(MM_Combo_Item00Gated((int16_t)MM_SnbTest_Item00(0)) == (w.stick ? 1 : 0) &&
                      MM_Combo_Item00Gated((int16_t)MM_SnbTest_Item00(6)) == (w.stick ? 1 : 0) &&
                      MM_Combo_Item00Gated((int16_t)MM_SnbTest_Item00(1)) == (w.nut ? 1 : 0) &&
                      MM_Combo_Item00Gated((int16_t)MM_SnbTest_Item00(2)) == (w.nut ? 1 : 0) &&
                      MM_Combo_Item00Gated((int16_t)MM_SnbTest_Item00(3)) == 0 &&
                      MM_Combo_Item00Gated((int16_t)MM_SnbTest_Item00(4)) == 0 &&
                      MM_Combo_Item00Gated((int16_t)MM_SnbTest_Item00(5)) == 0,
                  "%s: the drop gate at tier 0 holds out the wrong drops", w.what);
        SNB_CHECK(MM_Combo_StickGated() == (w.stick ? 1 : 0) && MM_Combo_NutGated() == (w.nut ? 1 : 0),
                  "%s: the shelf gates at tier 0 read stick %d nut %d", w.what, MM_Combo_StickGated(),
                  MM_Combo_NutGated());
        MM_SnbTest_SetBag(0, 1, 1, 1);
        MM_SnbTest_SetBag(1, 1, 1, 1);
        SNB_CHECK(MM_Combo_StickGated() == 0 && MM_Combo_NutGated() == 0 &&
                      MM_Combo_Item00Gated((int16_t)MM_SnbTest_Item00(0)) == 0 &&
                      MM_Combo_Item00Gated((int16_t)MM_SnbTest_Item00(2)) == 0,
                  "%s: a gate still holds at tier 1", w.what);
    }

    // ---- (K) #1691: the offers that are not a drop, in both games ----------------------
    // A Deku Baba's (and a withered or Mini Baba's) stick is the baba offering a
    // stick get-item; MM's unshuffled chests and OoT's vanilla business scrubs offer
    // a stick or nut the same way. Gate closed: no offer; gate open or shuffle off:
    // the offer stays. Catches: the stick and nut gates swapped (the one-bag
    // worlds), an id missing (that id), a gate that ignores the tier (tier 1), a
    // gate on an id no bag holds (the blue rupee). The two games share ONE live
    // gSaveContext in the single executable, so each half arms its own game's live
    // save before it asks (as (P) does), never both at once.
    for (const auto& w : kSnbWorlds) {
        SnbWorld(w.frozen, w.stick, w.nut);

        // MM's half: MM's live save, both bags at tier 0, then at tier 1.
        MM_SnbTest_ArmBorn();
        MM_SnbTest_SetBag(0, 0, 0, 0);
        MM_SnbTest_SetBag(1, 0, 0, 0);
        printf("[TEST] (K) %s, MM at tier 0: the baba's stick offer gated %d (expected %d)\n", w.what,
               MM_Combo_GetItemGated((int16_t)MM_SnbTest_GetItemId(0)), w.stick ? 1 : 0);
        SNB_CHECK(MM_Combo_GetItemGated((int16_t)MM_SnbTest_GetItemId(0)) == (w.stick ? 1 : 0) &&
                      MM_Combo_GetItemGated((int16_t)MM_SnbTest_GetItemId(1)) == (w.nut ? 1 : 0) &&
                      MM_Combo_GetItemGated((int16_t)MM_SnbTest_GetItemId(2)) == (w.nut ? 1 : 0) &&
                      MM_Combo_GetItemGated((int16_t)MM_SnbTest_GetItemId(3)) == (w.nut ? 1 : 0) &&
                      MM_Combo_GetItemGated((int16_t)MM_SnbTest_GetItemId(4)) == 0,
                  "%s: MM's offer gate at tier 0 holds out the wrong offers", w.what);
        MM_SnbTest_SetBag(0, 1, 1, 1);
        MM_SnbTest_SetBag(1, 1, 1, 1);
        SNB_CHECK(MM_Combo_GetItemGated((int16_t)MM_SnbTest_GetItemId(0)) == 0 &&
                      MM_Combo_GetItemGated((int16_t)MM_SnbTest_GetItemId(3)) == 0,
                  "%s: MM's offer gate still holds at tier 1", w.what);

        // OoT's half: OoT's live save, both bags at tier 0, then at tier 1.
        OoT_SharedTierTest_ArmLive();
        OoT_SharedTierTest_SetBagTier(0, 0);
        OoT_SharedTierTest_SetBagTier(1, 0);
        printf("[TEST] (K) %s, OoT at tier 0: the baba's stick offer gated %d (expected %d)\n", w.what,
               OoT_Combo_GetItemGated((int16_t)OoT_SharedTierTest_GetItemId(0)), w.stick ? 1 : 0);
        SNB_CHECK(OoT_Combo_StickGated() == (w.stick ? 1 : 0) && OoT_Combo_NutGated() == (w.nut ? 1 : 0),
                  "%s: OoT's gates at tier 0 read stick %d nut %d", w.what, OoT_Combo_StickGated(),
                  OoT_Combo_NutGated());
        for (int which = 0; which < 6; which++) {
            const bool gated = which >= 3 ? w.nut : w.stick;
            const int id = OoT_SharedTierTest_GetItemId(which);
            SNB_CHECK(OoT_Combo_GetItemGated((int16_t)id) == (gated ? 1 : 0), "%s: OoT's GI %d at tier 0 gated %d",
                      w.what, id, OoT_Combo_GetItemGated((int16_t)id));
        }
        SNB_CHECK(OoT_Combo_GetItemGated((int16_t)OoT_SharedTierTest_GetItemId(6)) == 0,
                  "%s: a blue rupee is gated in OoT", w.what);
        OoT_SharedTierTest_SetBagTier(0, 1);
        OoT_SharedTierTest_SetBagTier(1, 1);
        SNB_CHECK(OoT_Combo_StickGated() == 0 && OoT_Combo_NutGated() == 0 &&
                      OoT_Combo_GetItemGated((int16_t)OoT_SharedTierTest_GetItemId(0)) == 0 &&
                      OoT_Combo_GetItemGated((int16_t)OoT_SharedTierTest_GetItemId(4)) == 0,
                  "%s: OoT's offer gate still holds at tier 1", w.what);
    }

    // ---- (Y) Harvest/apply symmetry at tier 0 ------------------------------------------
    // Catches: MM_AmmoCapacity answering a nonzero cap at tier 0 (the count
    // materializes), a harvest-side clamp or debit (the pool loses its 7).
    {
        SnbWorld(true, true, true);
        MM_SnbTest_ArmBorn();
        MM_Combo_ApplyPairedBagBirth();
        Combo_HarvestSharedResource(GAME_OOT, kSnbStickCount, 7); // seven sticks in the pool, no bag yet
        MM_ApplySharedResources();
        const int held = MM_SnbTest_Held(0);
        const int ammo = MM_SnbTest_Ammo(0);
        MM_HarvestSharedResources();
        printf("[TEST] (Y) tier 0 arrival with 7 pooled sticks: held %d, ammo %d, pool %d (expected 0, 0, 7)\n", held,
               ammo, SnbPool(kSnbStickCount));
        SNB_CHECK(held == 0 && ammo == 0 && SnbPool(kSnbStickCount) == 7,
                  "the tier-0 arrival materialized pooled sticks or the harvest debited them");
        Combo_HarvestSharedResource(GAME_OOT, kSnbStickTier, 1); // the pair's bag, found in Hyrule
        MM_ApplySharedResources();
        printf("[TEST] (Y) arrival with the bag: tier %d, held %d, ammo %d (expected 1, 1, 7)\n", MM_SnbTest_Tier(0),
               MM_SnbTest_Held(0), MM_SnbTest_Ammo(0));
        SNB_CHECK(MM_SnbTest_Tier(0) == 1 && MM_SnbTest_Held(0) == 1 && MM_SnbTest_Ammo(0) == 7,
                  "the arrival after the bag did not hand MM the bag and the pooled sticks");
    }

    // ---- (P) A3: the bag steps at its pickup -------------------------------------------
    for (int nut = 0; nut < 2; nut++) {
        const uint8_t tierKind = nut ? kSnbNutTier : kSnbStickTier;
        const uint8_t countKind = nut ? kSnbNutCount : kSnbStickCount;
        const SharedItem ootBag = SnbItem(GAME_OOT, OoT_SharedTierTest_BagUpgradeId(nut));
        const char* what = nut ? "nut" : "stick";
        // The bag's fill is its tier-1 capacity, the same row in both games
        // (z_inventory.c / OoT's gUpgradeCapacities): ten sticks, twenty nuts.
        const int full = nut ? 20 : 10;
        SNB_CHECK(Combo_ItemClassSharedKind(ootBag) == tierKind && Combo_ItemClassSharedUnits(ootBag) == 1u,
                  "%s: OoT's progressive upgrade feeds kind %u with %u unit(s)", what,
                  (unsigned)Combo_ItemClassSharedKind(ootBag), (unsigned)Combo_ItemClassSharedUnits(ootBag));

        // P1: under the bit, OoT's bag found in Termina. Catches: the case missing
        // from StepsAtPickup (recorded 1 but a crossing, the tier still 0), the
        // fill missing (ammo 0), the admission rule missing (the second copy steps).
        SnbWorld(true, true, true);
        const uint16_t host0 = (uint16_t)MM_SharedTierTest_HostCheck(10 + 2 * nut);
        const uint16_t host1 = (uint16_t)MM_SharedTierTest_HostCheck(11 + 2 * nut);
        SNB_CHECK(Combo_SetForeignPlacement(host0, ootBag) >= 0 && Combo_SetForeignPlacement(host1, ootBag) >= 0,
                  "%s: could not place OoT's two upgrades on MM checks", what);
        MM_SnbTest_ArmBorn();
        MM_Combo_ApplyPairedBagBirth();
        MM_HarvestSharedResources();
        MM_ApplySharedResources();
        {
            const int recorded = MM_Rando_Foreign_RecordPickup(host0);
            const int crossings = Combo_CountSharedItems(GAME_OOT, /*includeRedeemed=*/true);
            printf("[TEST] (P1) %s bag found in Termina: pickup %d, MM tier %d held %d ammo %d, pool tier %d, OoT "
                   "crossings %d (expected 1, 1, 1, %d, 1, 0)\n",
                   what, recorded, MM_SnbTest_Tier(nut), MM_SnbTest_Held(nut), MM_SnbTest_Ammo(nut), SnbPool(tierKind),
                   crossings, full);
            SNB_CHECK(recorded == 1 && crossings == 0 && SnbPool(tierKind) == 1 && MM_SnbTest_Tier(nut) == 1 &&
                          MM_SnbTest_Held(nut) == 1 && MM_SnbTest_Ammo(nut) == full,
                      "%s: OoT's bag found in Termina did not open MM at its pickup, filled", what);
            MM_HarvestSharedResources();
            SNB_CHECK(SnbPool(countKind) == full, "%s: the bag's fill did not reach the pool (%d)", what,
                      SnbPool(countKind));
        }
        {
            const int recorded = MM_Rando_Foreign_RecordPickup(host1);
            const int crossings = Combo_CountSharedItems(GAME_OOT, /*includeRedeemed=*/true);
            printf("[TEST] (P1) a second %s upgrade in Termina: pickup %d, pool tier %d, MM tier %d, OoT crossings %d "
                   "(expected the crossing: pool 1, MM 1, 1 crossing)\n",
                   what, recorded, SnbPool(tierKind), MM_SnbTest_Tier(nut), crossings);
            SNB_CHECK(SnbPool(tierKind) == 1 && MM_SnbTest_Tier(nut) == 1 && crossings == 1,
                      "%s: a second upgrade (OoT's next capacity or its infinite item) stepped instead of crossing",
                      what);
        }
        // OoT's next arrival holds the bag and its ten.
        MM_HarvestSharedResources();
        OoT_SharedTierTest_ArmLive();
        OoT_ApplySharedResources();
        printf("[TEST] (P1) OoT's next arrival: tier %d held %d ammo %d (expected 1, 1, %d)\n",
               OoT_SharedTierTest_BagTier(nut), OoT_SharedTierTest_BagHeld(nut), OoT_SharedTierTest_BagAmmo(nut), full);
        SNB_CHECK(OoT_SharedTierTest_BagTier(nut) == 1 && OoT_SharedTierTest_BagHeld(nut) == 1 &&
                      OoT_SharedTierTest_BagAmmo(nut) == full,
                  "%s: OoT's arrival did not hold the pair's bag found in Termina", what);

        // I1: the bits clear, the same pickup crosses as before. Catches: the
        // StepsAtPickup case ungated.
        SnbWorld(true, false, false);
        SNB_CHECK(Combo_SetForeignPlacement(host0, ootBag) >= 0, "%s: could not place OoT's upgrade", what);
        MM_SnbTest_ArmBorn();
        MM_Combo_ApplyPairedBagBirth();
        {
            const int recorded = MM_Rando_Foreign_RecordPickup(host0);
            const int crossings = Combo_CountSharedItems(GAME_OOT, /*includeRedeemed=*/true);
            SNB_CHECK(crossings == 1 && SnbPool(tierKind) == -1 && MM_SnbTest_Tier(nut) == 1 &&
                          MM_SnbTest_Ammo(nut) == 0,
                      "%s: with the bag shuffles off OoT's upgrade in Termina did not cross as before (pickup %d, "
                      "crossings %d, pool %d)",
                      what, recorded, crossings, SnbPool(tierKind));
        }

        // P2: OoT's own step of a redeemed bag copy (OoT_CreditSharedPickup, the
        // redemption branch) fills the same way. Catches: OoT_FillBagAfterStep
        // missing (ammo 0), a fill on a later copy.
        SnbWorld(true, true, true);
        OoT_SharedTierTest_ArmLive();
        SNB_CHECK(OoT_CreditSharedPickup(&ootBag) == 1 && OoT_SharedTierTest_BagTier(nut) == 1 &&
                      OoT_SharedTierTest_BagHeld(nut) == 1 && OoT_SharedTierTest_BagAmmo(nut) == full &&
                      SnbPool(tierKind) == 1,
                  "%s: OoT's step of a redeemed bag copy did not give tier 1 and the bag's fill (tier %d ammo %d)",
                  what, OoT_SharedTierTest_BagTier(nut), OoT_SharedTierTest_BagAmmo(nut));
        SNB_CHECK(OoT_CreditSharedPickup(&ootBag) == 0 && SnbPool(tierKind) == 1,
                  "%s: OoT stepped a second redeemed copy instead of giving it through its own give", what);
    }

    // ---- (R) OoT's redemption of a crossed pack ----------------------------------------
    // The decision OoT_ForeignItem_Give takes (it needs OoT's rando context to give,
    // so the give itself is anchored below). Catches: a pack mapped to the wrong
    // bag, a bomb pack held by a bag.
    for (const auto& w : kSnbWorlds) {
        SnbWorld(w.frozen, w.stick, w.nut);
        SNB_CHECK(OoT_ForeignItem_BagGateKind((uint16_t)OoT_SharedTierTest_BagPackId(0)) == kSnbStickTier &&
                      OoT_ForeignItem_BagGateKind((uint16_t)OoT_SharedTierTest_BagPackId(1)) == kSnbNutTier &&
                      OoT_ForeignItem_BagGateKind((uint16_t)OoT_SharedTierTest_BombPackId()) == 0u &&
                      OoT_ForeignItem_BagGateKind((uint16_t)OoT_SharedTierTest_BagUpgradeId(0)) == 0u,
                  "%s: OoT's crossed packs map to the wrong bag gate", w.what);
    }

    // ---- (A) The source anchors ----------------------------------------------------------
    {
        const std::string give = SnbReadSource("games/mm/src/code/z_parameter.c");
        const std::string girla = SnbReadSource("games/mm/src/overlays/actors/ovl_En_GirlA/z_en_girla.c");
        const std::string item00 = SnbReadSource("games/mm/2s2h/Rando/ActorBehavior/EnItem00.cpp");
        const std::string convert = SnbReadSource("games/mm/2s2h/Rando/ConvertItem.cpp");
        const std::string create = SnbReadSource("games/mm/2s2h/Rando/MiscBehavior/OnFileCreate.cpp");
        const std::string engine = SnbReadSource("games/mm/2s2h/Rando/ComboLogicEngineSingleExe.cpp");
        const std::string mmForeign = SnbReadSource("games/mm/2s2h/Rando/ForeignItemsSingleExe.cpp");
        const std::string mmExports = SnbReadSource("games/mm/2s2h/GameExports_SingleExe.cpp");
        const std::string ootForeign = SnbReadSource("games/oot/soh/Enhancements/randomizer/ForeignItemsSingleExe.cpp");
        const std::string ootExports = SnbReadSource("games/oot/soh/GameExports_SingleExe.cpp");
        SNB_CHECK(!give.empty() && !girla.empty() && !item00.empty() && !convert.empty() && !create.empty() &&
                      !engine.empty() && !mmForeign.empty() && !mmExports.empty() && !ootForeign.empty() &&
                      !ootExports.empty(),
                  "the checkout's sources could not be read (RSBS_SOURCE_DIR)");
        // The refusal sits BEFORE Item_GiveImpl and OnItemGive.
        SNB_CHECK(SnbBefore(SnbBody(give, "u8 MM_Item_Give(PlayState* play, u8 item) {"),
                            "MM_Combo_StickNutGiveRefused(item)", "Item_GiveImpl(play, item)"),
                  "MM_Item_Give does not refuse sticks and nuts before Item_GiveImpl");
        SNB_CHECK(SnbBefore(SnbBody(girla, "s32 EnGirlA_CanBuyStick(PlayState* play, EnGirlA* this) {"),
                            "MM_Combo_StickGated()", "CANBUY_RESULT_CANNOT_GET_NOW") &&
                      SnbBefore(SnbBody(girla, "s32 EnGirlA_CanBuyNuts(PlayState* play, EnGirlA* this) {"),
                                "MM_Combo_NutGated()", "CANBUY_RESULT_CANNOT_GET_NOW"),
                  "a vanilla shelf does not refuse sticks or nuts before the bag");
        const std::string item00Body = SnbBody(item00, "void Rando::ActorBehavior::InitEnItem00Behavior() {");
        SNB_CHECK(
            SnbBefore(item00Body, "actor->params != ITEM00_NOTHING && MM_Combo_Item00Gated(actor->params)",
                      "*should = false;"),
            "InitEnItem00Behavior does not kill a gated stick or nut drop (or lost the ITEM00_NOTHING exemption)");
        // #1691: the babas' stick offers, both games; MM's unshuffled chests; OoT's vanilla scrubs.
        // (No needle spans a line: a Windows checkout may end its lines with CRLF.)
        const char* kSnbMmBabaGate =
            "enDekubaba->actionFunc == MM_EnDekubaba_DeadStickDrop && MM_Combo_GetItemGated(GI_DEKU_STICKS_1)";
        const char* kSnbMmKarebabaGate =
            "enKarebaba->actionFunc == MM_EnKarebaba_DeadItemDrop && MM_Combo_GetItemGated(GI_DEKU_STICKS_1)";
        SNB_CHECK(
            SnbBefore(item00Body, "COND_ID_HOOK(OnActorUpdate, ACTOR_EN_DEKUBABA, IS_RANDO", kSnbMmBabaGate) &&
                SnbBefore(item00Body, kSnbMmBabaGate, "MM_Actor_Kill(actor);") &&
                SnbBefore(item00Body, "COND_ID_HOOK(OnActorUpdate, ACTOR_EN_KAREBABA, IS_RANDO", kSnbMmKarebabaGate) &&
                SnbBefore(item00Body, kSnbMmKarebabaGate, "MM_EnKarebaba_SetupDead(enKarebaba);"),
            "MM's Deku Baba or withered / Mini Baba still offers its stick before the pair's bag");
        const std::string mmBox = SnbReadSource("games/mm/2s2h/Rando/ActorBehavior/EnBox.cpp");
        SNB_CHECK(SnbBefore(SnbBody(mmBox, "static void EnBox_RsbsWithholdGatedStickNut(Actor* actor, RandoCheckId "
                                           "randoCheckId) {"),
                            "MM_Combo_GetItemGated((int16_t)ENBOX_GET_ITEM(actor))",
                            "ENBOX_SET_ITEM(actor, GI_RUPEE_BLUE);") &&
                      SnbBefore(SnbBody(mmBox, "void Rando::ActorBehavior::InitEnBoxBehavior() {"),
                                "COND_ID_HOOK(ShouldActorInit, ACTOR_EN_BOX, IS_RANDO",
                                "EnBox_RsbsWithholdGatedStickNut(actor, randoCheckId);"),
                  "MM's unshuffled chest still holds a stick or nut before the pair's bag");
        const std::string ootHooks = SnbReadSource("games/oot/soh/Enhancements/randomizer/hook_handlers.cpp");
        const std::string withhold = SnbBody(ootHooks, "static void OoT_DekuBaba_WithholdStick(Actor* actor) {");
        SNB_CHECK(!ootHooks.empty() &&
                      SnbBefore(withhold, "if (!OoT_Combo_GetItemGated(GI_STICKS_1)) {",
                                "enDekubaba->actionFunc == OoT_EnDekubaba_DeadStickDrop") &&
                      SnbBefore(withhold, "enDekubaba->actionFunc == OoT_EnDekubaba_DeadStickDrop",
                                "OoT_Actor_Kill(actor);") &&
                      SnbBefore(withhold, "enKarebaba->actionFunc == OoT_EnKarebaba_DeadItemDrop",
                                "OoT_EnKarebaba_SetupDead(enKarebaba);") &&
                      SnbBefore(SnbBody(ootHooks, "void RandomizerOnActorUpdateHandler(void* refActor) {"),
                                "actor->id == ACTOR_EN_DEKUBABA || actor->id == ACTOR_EN_KAREBABA",
                                "OoT_DekuBaba_WithholdStick(actor);"),
                  "OoT's Deku Baba or withered Deku Baba still offers its stick before the pair's bag");
        const std::string ootDns = SnbReadSource("games/oot/src/overlays/actors/ovl_En_Dns/z_en_dns.c");
        SNB_CHECK(SnbBefore(SnbBody(ootDns, "u32 EnDns_CanBuyDekuNuts(EnDns* this) {"),
                            "OoT_Combo_GetItemGated((int16_t)this->dnsItemEntry->getItemId)",
                            "DNS_CANBUY_RESULT_CANT_GET_NOW") &&
                      SnbBefore(SnbBody(ootDns, "u32 EnDns_CanBuyDekuSticks(EnDns* this) {"),
                                "OoT_Combo_GetItemGated((int16_t)this->dnsItemEntry->getItemId)",
                                "DNS_CANBUY_RESULT_CANT_GET_NOW"),
                  "an OoT business scrub still sells sticks or nuts before the pair's bag");
        const std::string obtainBody = SnbBody(convert, "bool Rando::IsItemObtainable(");
        SNB_CHECK(SnbBefore(obtainBody, "case RI_DEKU_STICKS_5:", "MM_Combo_StickGated()") &&
                      SnbBefore(obtainBody, "case RI_DEKU_NUTS_10:", "MM_Combo_NutGated()"),
                  "Rando::IsItemObtainable does not gate the stick and nut packs");
        const std::string createBody = SnbBody(create, "void Rando::MiscBehavior::OnFileCreate(s16 fileNum) {");
        const size_t clearAt = createBody.find("Combo_ClearForeignPlacements();");
        const size_t birthAt = createBody.find("MM_Combo_ApplyPairedBagBirth();");
        const size_t snapAt = createBody.find("memcpy(&sPreAttemptSave, &gSaveContext");
        const size_t grantAt = createBody.find("Rando::GrantStartingItems();");
        SNB_CHECK(clearAt != std::string::npos && birthAt != std::string::npos && snapAt != std::string::npos &&
                      grantAt != std::string::npos && clearAt < birthAt && birthAt < snapAt && birthAt < grantAt &&
                      createBody.rfind("if (rsbsPaired) {", birthAt) != std::string::npos &&
                      createBody.rfind("if (rsbsPaired) {", birthAt) > clearAt,
                  "OnFileCreate does not birth a paired half without the bag before the ladder's snapshot and the "
                  "starting grant, inside a pairing test");
        SNB_CHECK(SnbBefore(SnbBody(engine, "extern \"C\" int MM_ComboLogic_ApplyShippedProfile(void) {"),
                            "MM_Sram_InitNewSave();", "MM_Combo_ApplyPairedBagBirth();"),
                  "the measured profile does not birth MM from the production helper");
        SNB_CHECK(SnbBefore(SnbBody(mmForeign, "void GiveNow(uint16_t riId, bool announce) {"),
                            "MM_ForeignItem_RedeemAs(riId)", "Rando::GiveItem(randoItemId);"),
                  "MM's redemption give does not convert a crossed pack before the bag");
        SNB_CHECK(SnbBefore(SnbBody(mmExports, "static int MM_StepSharedTierAtPickup("), "MM_ApplySharedKind(kind);",
                            "MM_FillBagAfterStep(kind, ammoTierBefore);"),
                  "MM's step at pickup does not fill the bag after the apply");
        SNB_CHECK(SnbBefore(SnbBody(ootExports, "static int OoT_StepSharedTierAtPickup("), "OoT_ApplySharedKind(kind);",
                            "OoT_FillBagAfterStep(ammoRow, ammoTierBefore);"),
                  "OoT's step at pickup does not fill the bag after the apply");
        const std::string blocked = SnbBody(ootExports, "static bool OoT_SharedAmmoGrantBlocked(");
        SNB_CHECK(SnbBefore(blocked, "Combo_SharedBagLegacyGrantBlocked(RSBS_SHARED_RES_STICK_TIER", nullptr) &&
                      SnbBefore(blocked, "Combo_SharedBagLegacyGrantBlocked(RSBS_SHARED_RES_NUT_TIER", nullptr),
                  "OoT's grant block does not lift under the pair's bag bit");
        SNB_CHECK(
            SnbBefore(SnbBody(ootForeign, "extern \"C\" int OoT_ForeignItem_Give(uint16_t rgId) {"),
                      "Combo_SharedBagGateClosed(bagKind", "GiveLinkRupees(5);") &&
                SnbBefore(SnbBody(ootExports, "static void OoT_AwardSharedItem("), "given == 2", "Notification::Emit("),
            "OoT's redemption does not give and announce a blue rupee for a crossed pack before the bag");
    }

    ComboContext_Init();
    Combo_ResetSharedResourceWatermarks();
    Combo_ClearSharedItemOutbox();
    Combo_ClearForeignPlacements();
    MM_ForeignItem_TestResetPending();
    SNB_CHECK(!Combo_ComboStickBagShuffled() && !Combo_ComboNutBagShuffled(),
              "the row left a bag shuffle frozen on for the rest of the process");
    printf("[TEST] mm-stick-nut-bag-gate: %s\n", sSnbFailures == 0 ? "PASS" : "FAIL");
    return sSnbFailures == 0 ? TEST_PASS : TEST_FAIL;
}
