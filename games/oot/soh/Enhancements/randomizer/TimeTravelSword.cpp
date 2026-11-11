/**
 * @file TimeTravelSword.cpp
 * @brief OoTMM's "Time Travel requires Master Sword" (#1434) for Ocarina of
 *        Time's randomizer: RSK_TIME_TRAVEL_REQUIRES_MASTER_SWORD, on by default.
 *
 * OoTMM (5f352be01). The setting is `timeTravelSword`, "Choose whether or not
 * Link needs the Master Sword to travel through time", default true
 * (packages/core/src/settings/data.ts:1240-1246). The generator hands it to the
 * game as OOT_TIME_TRAVEL_REQUIRES_MS (generator/lib/combo/randomizer/
 * world-config.ts:246), and the pedestal offers the pull only to a Link who owns
 * the sword, at either age (generator/src/oot/actors/Bg/Bg_Toki_Swd.c:
 * `!Config_Flag(CFG_OOT_TIME_TRAVEL_REQUIRES_MS) || (swords & EQ_OOT_SWORD_MASTER)`
 * before Actor_OfferCarry). Its logic makes the age change an event of the Temple
 * of Time, `TIME_TRAVEL: event(DOOR_OF_TIME_OPEN) && (has_sword_master ||
 * !setting(timeTravelSword))` (data/world/oot/overworld/temple_of_time.yml), and
 * the other spawn needs that event (data/world/oot/overworld/_system.yml).
 *
 * HERE, in three places, each asking the one rule below:
 *  - THE PEDESTAL. z_bg_toki_swd.c's wait action offers the pull only while
 *    VB_BE_ABLE_TO_TIME_TRAVEL_AT_PEDESTAL holds; the randomizer answers it with
 *    OoT_TimeTravelSword_PedestalAllows (hook_handlers.cpp). Both directions, as
 *    in OoTMM: a child cannot leave for adult, nor an adult return to child,
 *    without the sword in the inventory.
 *  - THE LOGIC. 3drando/fill.cpp's PropagateTimeTravel is the one place where an
 *    age reaches RR_ROOT through the pedestal (RR_TOT_BEYOND_DOOR_OF_TIME); it now
 *    also asks OoT_TimeTravelSword_LogicAllows, on the search's own save. Every
 *    search goes through it: the fill, the beatability and All Locations proofs,
 *    the paired coordinator (ComboLogicEngineOoT.cpp's ReachabilitySearch), the
 *    hints and the Check Tracker's Available Checks.
 *  - THE IDENTITY. The settings fold (3drando/playthrough.cpp) folds the option
 *    only while OoT_TimeTravelSword_RuleCanBlock: off it, the option changes
 *    nothing in the world, so a world it cannot touch keeps its fingerprint.
 *
 * AN UNSHUFFLED SWORD. Without Shuffle Master Sword, Ship of Harkinian's pedestal
 * is the vanilla pull that hands the sword over (VB_GIVE_ITEM_MASTER_SWORD), so
 * Link always holds it as he travels: the rule is met by the pull itself, which
 * is the `swordShuffled` term. OoTMM reaches the same place another way (its
 * pedestal is a child check holding the vanilla sword, and TIME_TRAVEL then has
 * it).
 *
 * THE ADULT START (OoTMM's mustStartWithMasterSword, packages/logic/src/
 * helpers.ts:55-66, and its macro `starts_with_master_sword`). OoTMM gives an
 * adult start the sword only when the sword is NOT shuffled and the rule is on,
 * "to not be stuck as adult forever". Ship of Harkinian already does that: an
 * adult start collects the pedestal's check at RR_ROOT (location_access/root.cpp),
 * which holds the vanilla sword when it is not shuffled (3drando/item_pool.cpp),
 * and z_sram.c's load gives an adult the sword unless it is shuffled. With the
 * sword shuffled OoTMM forces nothing, and neither does this: the child side,
 * and with it the child-only crossing to Termina (ComboLogicEngineOoT.cpp's
 * crossingOpen, RR_MARKET_MASK_SHOP->Child()), waits for the sword in logic, so
 * the fill puts it where the adult reaches it first.
 *
 * FROZEN AT CREATION. The option is one of the world's settings: the save's
 * randoSettings array and the seed package's OoT world carry it (SaveManager.cpp).
 * Off is 0, so a world saved before this option, whose array ends before it,
 * loads it off (SaveManager::LoadArray default-constructs the tail) and keeps the
 * pedestal it was generated with.
 *
 * THE CHILD PEDESTAL CHECK (#1484). OoTMM's pedestal check is a CHILD check
 * ("Temple of Time Master Sword": `is_child && event(DOOR_OF_TIME_OPEN)`,
 * data/world/oot/overworld/temple_of_time.yml, with no setting term), handed over
 * as the child walks up (generator/src/oot/actors/Bg/Bg_Toki_Swd.c, before any
 * pull is offered). Here, while the frozen world has Shuffle Master Sword on:
 *  - THE LOGIC. RC_TOT_MASTER_SWORD beyond the Door of Time is `logic->IsChild`
 *    (location_access/overworld/temple_of_time.cpp); unshuffled it stays
 *    `logic->IsAdult`, so those worlds do not move. An adult start still collects
 *    it at RR_ROOT (location_access/root.cpp), as savefile.cpp's creation give does.
 *  - THE FLAG. Its collection check is RAND_INF_TOT_MASTER_SWORD, not the pull's
 *    EVENTCHKINF_PULLED_MASTER_SWORD_FROM_PEDESTAL (location_list.cpp), so the pull
 *    no longer gives it. The unshuffled pull sets that flag after marking the check
 *    (VB_GIVE_ITEM_MASTER_SWORD), so the load seam (CheckStateLoadSingleExe.cpp)
 *    still follows it; a vanilla file's tracker and seam keep reading the pull.
 *  - THE GIVE. OoT_PedestalChildCheck_OnPedestalUpdate below: a child in reach
 *    (OoTMM's comboGiveItemNpc range, 200 across and 50 up) outside a cutscene sets
 *    the flag, and the randomizer's flag-set hook queues the check. A save that
 *    already pulled before this change (the pull's flag set) is not given again.
 *  - THE PULL WAITS for that give (OoT_PedestalChildCheck_HoldsPull), as OoTMM's
 *    handler returns before offering it, so a child holding the sword cannot leave
 *    and receive the pedestal's item as adult.
 * The sword unshuffled, the pedestal holds the vanilla sword and the pull hands it
 * over, which already matches OoTMM's reachability. OoTMM's adult start gives
 * nothing at creation; Ship of Harkinian's creation give is kept (#1484 D1).
 */
#ifdef RSBS_SINGLE_EXECUTABLE

#include "TimeTravelSword.h"

#include <cmath>
#include <memory>

#include "soh/Enhancements/randomizer/randomizerTypes.h"
#include "soh/Enhancements/randomizer/SeedContext.h"
#include "soh/Enhancements/randomizer/item_location.h"
#include "soh/Enhancements/randomizer/logic.h"
#include "soh/Enhancements/randomizer/location_access.h" // logic: the object every search asks

// At file scope first: z64.h includes <memory> under C++, which must not get C
// linkage. Its include guard makes the nested include below a no-op.
#include <z64.h>
extern "C" {
#include "macros.h"
#include "functions.h"
#include "variables.h"
extern PlayState* OoT_gPlayState;
}

namespace {

/** The frozen option, read from the world's own context. */
bool RequiresSword(const std::shared_ptr<Rando::Context>& world) {
    return world->GetOption(RSK_TIME_TRAVEL_REQUIRES_MASTER_SWORD).Is(RO_GENERIC_ON);
}

/** The frozen Shuffle Master Sword: off, the pedestal holds the sword. */
bool SwordShuffled(const std::shared_ptr<Rando::Context>& world) {
    return world->GetOption(RSK_SHUFFLE_MASTER_SWORD).Is(RO_GENERIC_ON);
}

} // namespace

extern "C" bool OoT_TimeTravelSword_Allows(bool requiresSword, bool swordShuffled, bool ownsSword) {
    return !requiresSword || !swordShuffled || ownsSword;
}

extern "C" bool OoT_TimeTravelSword_RuleCanBlock(void) {
    const std::shared_ptr<Rando::Context> world = Rando::Context::GetInstance();
    return world != nullptr && RequiresSword(world) && SwordShuffled(world) &&
           world->GetOption(RSK_STARTING_MASTER_SWORD).Is(RO_GENERIC_OFF);
}

extern "C" bool OoT_TimeTravelSword_PedestalAllows(void) {
    const std::shared_ptr<Rando::Context> world = Rando::Context::GetInstance();
    if (world == nullptr || !IS_RANDO) {
        return true;
    }
    const bool owns = CHECK_OWNED_EQUIP(EQUIP_TYPE_SWORD, EQUIP_INV_SWORD_MASTER) != 0;
    return OoT_TimeTravelSword_Allows(RequiresSword(world), SwordShuffled(world), owns);
}

extern "C" bool OoT_TimeTravelSword_LogicAllows(void) {
    const std::shared_ptr<Rando::Context> world = Rando::Context::GetInstance();
    if (world == nullptr || logic == nullptr) {
        return true;
    }
    // Logic's own save: the search's simulated inventory, or the live save while
    // the Check Tracker counts Available Checks (Logic::SetSaveContext).
    return OoT_TimeTravelSword_Allows(RequiresSword(world), SwordShuffled(world), logic->HasItem(RG_MASTER_SWORD));
}

// OoTMM's comboGiveItemNpc reach at the pedestal (Bg_Toki_Swd.c: 200.f, 50.f).
constexpr float kPedestalGiveXZ = 200.0f;
constexpr float kPedestalGiveY = 50.0f;

extern "C" bool OoT_PedestalChildCheck_Applies(void) {
    const std::shared_ptr<Rando::Context> world = Rando::Context::GetInstance();
    return world != nullptr && IS_RANDO && SwordShuffled(world);
}

extern "C" void OoT_PedestalChildCheck_OnPedestalUpdate(void* actor) {
    const Actor* swd = static_cast<const Actor*>(actor);
    if (swd == nullptr || swd->id != ACTOR_BG_TOKI_SWD || OoT_gPlayState == nullptr || LINK_IS_ADULT ||
        !OoT_PedestalChildCheck_Applies()) {
        return;
    }
    // Given already, or pulled before this change (the pull then gave it).
    if (Flags_GetRandomizerInf(RAND_INF_TOT_MASTER_SWORD) ||
        OoT_Flags_GetEventChkInf(EVENTCHKINF_PULLED_MASTER_SWORD_FROM_PEDESTAL)) {
        return;
    }
    // In reach as OoT_Actor_OfferGetItem measures it (strictly inside both ranges).
    if (OoT_Play_InCsMode(OoT_gPlayState) || !(swd->xzDistToPlayer < kPedestalGiveXZ) ||
        !(std::fabs(swd->yDistToPlayer) < kPedestalGiveY)) {
        return;
    }
    // The randomizer's flag-set hook resolves the flag to RC_TOT_MASTER_SWORD and
    // queues it (hook_handlers.cpp, RandomizerOnFlagSetHandler).
    Flags_SetRandomizerInf(RAND_INF_TOT_MASTER_SWORD);
}

extern "C" bool OoT_PedestalChildCheck_HoldsPull(bool giveInFlight) {
    if (LINK_IS_ADULT || !OoT_PedestalChildCheck_Applies() ||
        OoT_Flags_GetEventChkInf(EVENTCHKINF_PULLED_MASTER_SWORD_FROM_PEDESTAL)) {
        return false;
    }
    const Rando::ItemLocation* pedestal = Rando::Context::GetInstance()->GetItemLocation(RC_TOT_MASTER_SWORD);
    if (pedestal == nullptr || pedestal->HasObtained()) {
        return false;
    }
    // Not given yet (the give comes on this frame's update), or given and still on
    // its way. A give that is neither (dropped) does not hold the pull: holding it
    // then would keep the child from ever travelling.
    return !Flags_GetRandomizerInf(RAND_INF_TOT_MASTER_SWORD) || giveInFlight;
}

#endif // RSBS_SINGLE_EXECUTABLE
