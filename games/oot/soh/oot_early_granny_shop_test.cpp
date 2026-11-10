/**
 * @file oot_early_granny_shop_test.cpp
 * #1414: Early Granny's Potion Shop (Shipwright#6693) opens Granny's shop at
 * runtime, not only in logic, and moves no world while it cannot matter.
 *
 * THE DEFECT. The upstream port changed Granny's sale gate
 * (EnDs_RandoCanGetGrannyItem) and her logic row (kakariko.cpp) but left her offer
 * (VB_OFFER_BLUE_POTION, the only way z_en_ds.c's EnDs_Wait reaches her sale) on
 * the Claim Check, so with the option on, Shuffle Adult Trade off and no Claim
 * Check, logic opened RC_KAK_GRANNYS_SHOP and Granny never offered it. The fold in
 * Playthrough_Init also folded the option's "Off" text, moving every world.
 *
 * WHAT THIS ROW ASSERTS. On a save with no Claim Check, no Odd Mushroom traded and
 * Granny's item unbought (the whole SaveContext zeroed, inventory ITEM_NONE), with
 * merchants shuffled (All But Beans), through OoT_Rando_TestGrannyGates
 * (hook_handlers.cpp: bit 0 her real sale gate, bit 1 the real handler's answer to
 * VB_OFFER_BLUE_POTION), OoT_Rando_EarlyGrannyFolds (playthrough.cpp: 1 when
 * the settings fold folds the option) and OoT_Rando_TestFoldLines (playthrough.cpp:
 * the line count of the REAL settings fold, the function Playthrough_Init hashes,
 * run over the leg's options; each leg wants exactly the lines this row counts
 * from the option groups, with the option counted only where the leg says it
 * folds):
 *   E1 option on, adult trade off: gates 3 (sells and offers), folded.
 *   E2 option off, adult trade off: gates 0 (as before the option), not folded.
 *   E3 option off, adult trade off, Claim Check owned: gates 3 (the old way in
 *      still works).
 *   E4 option on, adult trade on (the option is disabled there), Odd Mushroom not
 *      traded: gates 0, not folded.
 *   E5 option on, adult trade off, merchants not shuffled: gates 2 (her vanilla
 *      Blue Potion offer opens, as the logic row does; no randomized sale).
 *   F  the option sits where the fold reads it (a non-subgroup group, Setting
 *      category, a nonzero option count), so only the skip keeps it out.
 *   G  the logic row (kakariko.cpp, RC_KAK_GRANNYS_SHOP), asked the way the
 *      search asks it (LocationAccess::ConditionsMet for the adult in
 *      RR_KAK_ODD_POTION_BUILDING, Glitchless, as oot_logic_gates_test.cpp does),
 *      Speak Hylian and a child wallet held:
 *        G1 adult trade off, option on, no Claim Check, price 50: reachable (E1).
 *        G2 option off, no Claim Check: unreachable (E2).
 *        G3 option off, Claim Check: reachable (E3).
 *        G4 adult trade on, option on, no Odd Mushroom: unreachable (E4).
 *        G5 adult trade on, Odd Mushroom, price 50: reachable (G6's control).
 *        G6 adult trade on, Odd Mushroom, price 1000 (over every wallet):
 *           unreachable (#1457's price test on the Odd Mushroom branch).
 *        G7 adult trade off, option on, price 1000: unreachable.
 *
 * RED HALVES, derived, not run (this lane had no build): without the
 * VB_OFFER_BLUE_POTION fix E1 reads 1 and E5 reads 0; without the fold's skip
 * every leg's folded line count is one over except E1's and E5's, and a skip
 * that ignored adult trade leaves E4 one over; with kakariko.cpp's upstream
 * parenthesis G6 reads reachable; without its `|| EARLY` term G1 reads
 * unreachable.
 *
 * Everything a leg changes (four options, the SaveContext, Granny's price) is put
 * back. Leg G re-binds the search's globals (RegionTable_Init) to the context the
 * row found.
 */
#ifdef RSBS_SINGLE_EXECUTABLE

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <vector>

#include "soh/OTRGlobals.h"
#include "soh/Enhancements/randomizer/randomizerTypes.h"
#include "soh/Enhancements/randomizer/SeedContext.h"
#include "soh/Enhancements/randomizer/settings.h"
#include "soh/Enhancements/randomizer/location_access.h"
#include "soh/Enhancements/randomizer/logic.h"
#include "soh/Enhancements/randomizer/TimeTravelSword.h" // the fold's #1434 term

// At file scope, not inside an extern "C" block (see oot_shared_kokiri_sword_test.cpp).
#include <z64.h>
extern "C" {
#include "macros.h"
#include "variables.h"
}

// hook_handlers.cpp: Granny's sale gate (bit 0) and offer (bit 1).
extern "C" int OoT_Rando_TestGrannyGates(void);
// playthrough.cpp: 1 when the settings fold folds Early Granny's Potion Shop.
extern "C" int OoT_Rando_EarlyGrannyFolds(void);
// playthrough.cpp: the line count of the real settings fold over the current options.
extern "C" int OoT_Rando_TestFoldLines(void);
// ForeignItemsSingleExe.cpp: 1 when Granny's check hosts an MM item already
// collected, which makes her sale gate answer no whatever the option says.
extern "C" int OoT_Rando_Foreign_HostCollected(uint16_t rc);

namespace {

int sFailures = 0;

const RandomizerSettingKey kTouched[] = {
    RSK_SHUFFLE_MERCHANTS,
    RSK_SHUFFLE_ADULT_TRADE,
    RSK_EARLY_GRANNYS_SHOP,
    RSK_LOGIC_RULES,
};
constexpr int kTouchedCount = (int)(sizeof(kTouched) / sizeof(kTouched[0]));

/**
 * The lines the settings fold should fold under the current options, counted
 * from the option groups: every Setting-category option with option texts in a
 * non-subgroup group, less the Hookshot Ladders trick while it is off and the
 * #1434 term while it cannot block (the fold's own skips, which this row does not
 * test), and less Early Granny's Potion Shop unless `grannyFolds`.
 */
int CountFoldLines(Rando::Context& world, bool grannyFolds) {
    auto settings = Rando::Settings::GetInstance();
    int lines = 0;
    for (size_t g = 0; g < RSG_MAX; g++) {
        const auto& group = settings->GetOptionGroup((RandomizerSettingGroupKey)g);
        if (group.GetContainsType() == Rando::OptionGroupType::SUBGROUP) {
            continue;
        }
        const bool excludes = g >= RSG_EXCLUDES_KOKIRI_FOREST && g <= RSG_EXCLUDES_GANONS_CASTLE;
        for (const Rando::Option* option : group.GetOptions()) {
            if (!option->IsCategory(Rando::OptionCategory::Setting) || option->GetOptionCount() == 0) {
                continue;
            }
            if (g == RSG_TRICKS) {
                const auto* trick = static_cast<const Rando::TrickOption*>(option);
                if (trick->GetKey() == RT_HOOKSHOT_LADDERS && world.GetTrickOption(RT_HOOKSHOT_LADDERS).Get() == 0) {
                    continue;
                }
            } else if (!excludes) {
                if (option->GetKey() == RSK_TIME_TRAVEL_REQUIRES_MASTER_SWORD && !OoT_TimeTravelSword_RuleCanBlock()) {
                    continue;
                }
                if (option->GetKey() == RSK_EARLY_GRANNYS_SHOP && !grannyFolds) {
                    continue;
                }
            }
            lines++;
        }
    }
    return lines;
}

/** One leg: set the three options and the save, then compare the three readings. */
void Leg(Rando::Context& world, const char* name, uint8_t merchants, bool adultTrade, bool early, bool claimCheck,
         int wantGates, int wantFolds) {
    world.GetOption(RSK_SHUFFLE_MERCHANTS).Set(merchants);
    world.GetOption(RSK_SHUFFLE_ADULT_TRADE).Set(adultTrade ? RO_GENERIC_ON : RO_GENERIC_OFF);
    world.GetOption(RSK_EARLY_GRANNYS_SHOP).Set(early ? RO_GENERIC_ON : RO_GENERIC_OFF);
    std::memset(&gSaveContext, 0, sizeof(SaveContext));
    std::memset(gSaveContext.inventory.items, ITEM_NONE, sizeof(gSaveContext.inventory.items));
    if (claimCheck) {
        INV_CONTENT(ITEM_CLAIM_CHECK) = ITEM_CLAIM_CHECK;
    }
    const int gates = OoT_Rando_TestGrannyGates();
    const int folds = OoT_Rando_EarlyGrannyFolds();
    const int foldLines = OoT_Rando_TestFoldLines();
    const int wantLines = CountFoldLines(world, wantFolds != 0);
    printf("[TEST] oot-early-granny-shop: %s: merchants %u, adult trade %d, early %d, claim check %d: sells %d, "
           "offers %d (want %d, %d); folded %d (want %d); the fold read %d line(s) (want %d)\n",
           name, (unsigned)merchants, adultTrade ? 1 : 0, early ? 1 : 0, claimCheck ? 1 : 0, gates & 1,
           (gates >> 1) & 1, wantGates & 1, (wantGates >> 1) & 1, folds, wantFolds, foldLines, wantLines);
    if (gates != wantGates || folds != wantFolds || foldLines != wantLines || wantLines == 0) {
        printf("[TEST] oot-early-granny-shop: FAIL: %s\n", name);
        sFailures++;
    }
}

/** F: the fold would read the option without its skip (playthrough.cpp's loop). */
void CheckFoldReachesOption() {
    auto settings = Rando::Settings::GetInstance();
    Rando::Option* option = &settings->GetOption(RSK_EARLY_GRANNYS_SHOP);
    int groupsHolding = 0;
    for (size_t g = 0; g < RSG_MAX; g++) {
        const auto& group = settings->GetOptionGroup((RandomizerSettingGroupKey)g);
        if (group.GetContainsType() == Rando::OptionGroupType::SUBGROUP) {
            continue;
        }
        for (const Rando::Option* held : group.GetOptions()) {
            groupsHolding += held == option ? 1 : 0;
        }
    }
    const bool setting = option->IsCategory(Rando::OptionCategory::Setting);
    const size_t count = option->GetOptionCount();
    printf("[TEST] oot-early-granny-shop: F: '%s' in %d folded group(s), Setting %d, %zu option text(s)\n",
           option->GetName().c_str(), groupsHolding, setting ? 1 : 0, count);
    if (groupsHolding != 1 || !setting || count == 0) {
        printf("[TEST] oot-early-granny-shop: FAIL: F: the fold does not reach the option, so the skip legs prove "
               "nothing\n");
        sFailures++;
    }
}

// ---- G: the logic row ------------------------------------------------------------------

const LocationAccess* FindGrannyAccess(Region* region) {
    for (const LocationAccess& loc : region->locations) {
        if (loc.GetLocation() == RC_KAK_GRANNYS_SHOP) {
            return &loc;
        }
    }
    return nullptr;
}

/** RC_KAK_GRANNYS_SHOP for the adult, day and night, as ProcessRegion asks it; -1
 *  when the probe cannot be made. */
int GrannyReachable() {
    Region* region = RegionTable(RR_KAK_ODD_POTION_BUILDING);
    const LocationAccess* loc = region != nullptr ? FindGrannyAccess(region) : nullptr;
    if (loc == nullptr) {
        return -1;
    }
    const bool saved[4] = { region->childDay, region->childNight, region->adultDay, region->adultNight };
    const RandomizerRegion savedRegionKey = logic->CurrentRegionKey;
    region->childDay = false;
    region->childNight = false;
    region->adultDay = true;
    region->adultNight = true;
    logic->CurrentRegionKey = RR_KAK_ODD_POTION_BUILDING;
    logic->CurrentCheckKey = RC_KAK_GRANNYS_SHOP;
    const bool met = loc->ConditionsMet(region, false);
    logic->CurrentCheckKey = RC_UNKNOWN_CHECK;
    logic->CurrentRegionKey = savedRegionKey;
    region->childDay = saved[0];
    region->childNight = saved[1];
    region->adultDay = saved[2];
    region->adultNight = saved[3];
    return met ? 1 : 0;
}

void LogicLeg(Rando::Context& world, const char* name, bool adultTrade, bool early, bool claimCheck, bool mushroom,
              uint16_t price, bool want) {
    world.GetOption(RSK_SHUFFLE_ADULT_TRADE).Set(adultTrade ? RO_GENERIC_ON : RO_GENERIC_OFF);
    world.GetOption(RSK_EARLY_GRANNYS_SHOP).Set(early ? RO_GENERIC_ON : RO_GENERIC_OFF);
    world.GetItemLocation(RC_KAK_GRANNYS_SHOP)->SetPrice(price);
    logic->Reset(true);
    logic->CurrentCheckKey = RC_UNKNOWN_CHECK;
    logic->CalculatingAvailableChecks = false; // GetCheckPrice reads the placed price only then
    // Each leg holds exactly these, whatever Reset grants under the world's shuffles.
    logic->SetRandoInf(RAND_INF_CAN_SPEAK_HYLIAN, true);
    logic->SetRandoInf(RAND_INF_HAS_WALLET, true); // 99 rupees
    logic->SetRandoInf(RAND_INF_ADULT_TRADES_HAS_CLAIM_CHECK, claimCheck);
    logic->SetRandoInf(RAND_INF_ADULT_TRADES_HAS_ODD_MUSHROOM, mushroom);
    const int got = GrannyReachable();
    printf("[TEST] oot-early-granny-shop: %s: adult trade %d, early %d, claim check %d, odd mushroom %d, price %u: "
           "the logic row reads %d (want %d)\n",
           name, adultTrade ? 1 : 0, early ? 1 : 0, claimCheck ? 1 : 0, mushroom ? 1 : 0, (unsigned)price, got,
           want ? 1 : 0);
    if (got != (want ? 1 : 0)) {
        printf("[TEST] oot-early-granny-shop: FAIL: %s%s\n", name,
               got < 0 ? ": RC_KAK_GRANNYS_SHOP is not in RR_KAK_ODD_POTION_BUILDING" : "");
        sFailures++;
    }
}

void CheckLogicRow(Rando::Context& world) {
    world.GetOption(RSK_LOGIC_RULES).Set(RO_LOGIC_GLITCHLESS); // otherwise every location answers true
    // The world graph, bound to this context's logic (location_access.cpp).
    RegionTable_Init();
    if (logic == nullptr) {
        printf("[TEST] oot-early-granny-shop: FAIL: G: RegionTable_Init bound no logic object\n");
        sFailures++;
        return;
    }
    Rando::ItemLocation* granny = world.GetItemLocation(RC_KAK_GRANNYS_SHOP);
    const uint16_t savedPrice = granny->GetPrice();
    granny->SetPrice(1000);
    if (granny->GetPrice() != 1000) {
        printf("[TEST] oot-early-granny-shop: FAIL: G: Granny's price did not take, so the price legs are "
               "vacuous\n");
        sFailures++;
    } else {
        const bool savedChild = logic->IsChild;
        const bool savedAdult = logic->IsAdult;
        const bool savedDay = logic->AtDay;
        const bool savedNight = logic->AtNight;
        LogicLeg(world, "G1", false, true, false, false, 50, true);
        LogicLeg(world, "G2", false, false, false, false, 50, false);
        LogicLeg(world, "G3", false, false, true, false, 50, true);
        LogicLeg(world, "G4", true, true, false, false, 50, false);
        LogicLeg(world, "G5", true, false, false, true, 50, true);
        LogicLeg(world, "G6", true, false, false, true, 1000, false);
        LogicLeg(world, "G7", false, true, false, false, 1000, false);
        logic->Reset(true);
        logic->IsChild = savedChild;
        logic->IsAdult = savedAdult;
        logic->AtDay = savedDay;
        logic->AtNight = savedNight;
    }
    granny->SetPrice(savedPrice);
}

} // namespace

extern "C" int OoT_EarlyGrannyShopTest_Run(void) {
    printf("[TEST] oot-early-granny-shop: Early Granny's Potion Shop opens Granny's offer and sale without the Claim "
           "Check, and only folds into the settings fingerprint while it can matter (#1414)\n");
    if (OTRGlobals::Instance == nullptr || OTRGlobals::Instance->gRandoContext == nullptr) {
        printf("[TEST] FAIL: oot-early-granny-shop needs OTRGlobals' randomizer context\n");
        return 1;
    }
    if (OoT_Rando_Foreign_HostCollected(RC_KAK_GRANNYS_SHOP) != 0) {
        printf("[TEST] FAIL: oot-early-granny-shop: Granny's check hosts a collected MM item, so her sale gate "
               "answers no whatever the option says\n");
        return 1;
    }
    sFailures = 0;
    Rando::Settings::GetInstance()->CreateOptions(); // idempotent; the fold walks its groups
    const std::shared_ptr<Rando::Context> world = Rando::Context::GetInstance();
    std::unique_ptr<SaveContext> priorSave = std::make_unique<SaveContext>();
    std::memcpy(priorSave.get(), &gSaveContext, sizeof(SaveContext));
    uint8_t savedOptions[kTouchedCount];
    for (int i = 0; i < kTouchedCount; i++) {
        savedOptions[i] = world->GetOption(kTouched[i]).Get();
    }

    const uint8_t shuffled = RO_SHUFFLE_MERCHANTS_ALL_BUT_BEANS;
    Leg(*world, "E1", shuffled, false, true, false, 3, 1);
    Leg(*world, "E2", shuffled, false, false, false, 0, 0);
    Leg(*world, "E3", shuffled, false, false, true, 3, 0);
    Leg(*world, "E4", shuffled, true, true, false, 0, 0);
    Leg(*world, "E5", RO_SHUFFLE_MERCHANTS_OFF, false, true, false, 2, 1);
    CheckFoldReachesOption();
    CheckLogicRow(*world);

    for (int i = 0; i < kTouchedCount; i++) {
        world->GetOption(kTouched[i]).Set(savedOptions[i]);
    }
    std::memcpy(&gSaveContext, priorSave.get(), sizeof(SaveContext));
    if (sFailures != 0) {
        printf("[TEST] FAIL (oot-early-granny-shop): %d failure(s)\n", sFailures);
        return 1;
    }
    printf("[TEST] PASS: oot-early-granny-shop\n");
    return 0;
}

#endif // RSBS_SINGLE_EXECUTABLE
