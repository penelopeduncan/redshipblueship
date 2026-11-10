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
 * VB_OFFER_BLUE_POTION) and OoT_Rando_EarlyGrannyFolds (playthrough.cpp: 1 when
 * the settings fold folds the option):
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
 *
 * RED HALVES, derived, not run (this lane had no build): without the
 * VB_OFFER_BLUE_POTION fix E1 reads 1 and E5 reads 0; without the fold skip the
 * row does not link (no OoT_Rando_EarlyGrannyFolds).
 *
 * Everything a leg changes (three options, the SaveContext) is put back.
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
// ForeignItemsSingleExe.cpp: 1 when Granny's check hosts an MM item already
// collected, which makes her sale gate answer no whatever the option says.
extern "C" int OoT_Rando_Foreign_HostCollected(uint16_t rc);

namespace {

int sFailures = 0;

const RandomizerSettingKey kTouched[] = {
    RSK_SHUFFLE_MERCHANTS,
    RSK_SHUFFLE_ADULT_TRADE,
    RSK_EARLY_GRANNYS_SHOP,
};
constexpr int kTouchedCount = (int)(sizeof(kTouched) / sizeof(kTouched[0]));

/** One leg: set the three options and the save, then compare both readings. */
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
    printf("[TEST] oot-early-granny-shop: %s: merchants %u, adult trade %d, early %d, claim check %d: sells %d, "
           "offers %d (want %d, %d); folded %d (want %d)\n",
           name, (unsigned)merchants, adultTrade ? 1 : 0, early ? 1 : 0, claimCheck ? 1 : 0, gates & 1,
           (gates >> 1) & 1, wantGates & 1, (wantGates >> 1) & 1, folds, wantFolds);
    if (gates != wantGates || folds != wantFolds) {
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
