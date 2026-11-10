/**
 * @file mm_logic_gates_test.cpp
 * @brief #1168's Majora's Mask siblings: two logic gates 2Ship2Harkinian added
 *        after our import base, driven through MM's REAL reachability crawl.
 *        CTest row `MMLogicGates` (label `rando`), test name `mm-logic-gates`,
 *        registered in src/common/test_runner.cpp.
 *
 * WHY THE CRAWL AND NOT THE LAMBDA. Both defects come from how the crawl
 * propagates time: an exit is evaluated against the source region's WHOLE time
 * mask and, when it passes, the whole mask flows into the target. Milk Road's
 * exit to Romani Ranch is `AFTER(TIME_DAY3_AM_06_00) || boulder`, so a Milk Road
 * reachable at every time hands Romani Ranch every time slice — Night 1 included
 * — without the boulder ever breaking; the Milk Bar's door is open by day, so the
 * bar is handed the night windows too. A probe of the check lambda alone, under a
 * hand-picked time mask, would not show that; CrawlReachableRegions +
 * EvaluateReachableChecks (the tracker's and the combo engine's crawl) does.
 * (Since #1453 an edge carries only the half-days its own condition holds on,
 * so the Day 3 road hands the ranch Day 3 and Night 3 only; mm-time-edges locks
 * that. These legs gate the check rows themselves and hold either way.)
 *
 *  R. Romani Ranch (2S2H 9b699dee1). From Milk Road with the Ocarina of Time,
 *     Epona's Song and the Bow but no way to break the boulder (no Goron Mask, no
 *     Powder Keg): the ranch and the barn ARE reached (by the Day 3 road, which is
 *     the point), and none of the six cows, Epona's Song or the alien drop is.
 *     With the Goron Mask and a Powder Keg all eight are.
 *  C. The Circus Leader's Mask (2S2H cc5d87143). From East Clock Town with the
 *     Ocarina of Time, the Deku, Goron and Zora Masks and every ocarina button,
 *     but no Romani's Mask: the Milk Bar IS reached and the Circus Leader's check
 *     is not. With Romani's Mask it is.
 *  O. The Bombers' hideout door to the Astral Observatory (#1306, OoTMM
 *     clock_town.yml:171). On the door lambda itself, a projectile alone with
 *     RE_BOMBER_CODE == 0 must stay shut and RE_BOMBER_CODE == 1 with no
 *     projectile must open; in the full crawl from East Clock Town with an empty
 *     inventory, MMRT_BOMBER_GUESS (which fires RE_BOMBER_CODE in North Clock Town
 *     without a projectile) and MMRT_BOMBER_BACKFLIP (which bypasses the Bomber
 *     on the door without firing RE_BOMBER_CODE) each reach the observatory and
 *     the Moon's Tear, while the Deku Mask alone reaches the observatory from the
 *     Termina Field balcony without magic or the code.
 *  G. Great Bay Coast's minigame platforms (#1398; 2S2H 4661df2e6 split them into
 *     their own region, 20b09fe5b made the edge Hookshot-only). On the row
 *     lambdas: the cliffside edge opens with the Hookshot after the temple, stays
 *     shut without the temple and with Ice Arrows alone; the platforms connect
 *     back to the cliffside only; the Fisherman minigame is open from 07:00 to
 *     04:00 on each of the three days and closed at 06:00 and at 04:00.
 *  P. The Peahat (#1398; 2S2H f237dd293 moved IS_DAY() into CanKillEnemy). Both
 *     Peahat grotto rows, the enemy drop and the chest, pass by day with the
 *     Zora Mask and fail under every night half-day.
 *
 * Every "not reached" leg is paired with a "reached" leg on the same save plus
 * one thing, and each names a region or check that must be reached, so a probe
 * that reaches nothing fails instead of passing.
 */

#ifdef RSBS_SINGLE_EXECUTABLE

#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <memory>
#include <set>

#include "Rando/Rando.h"
#include "Rando/Types.h"
#include "Rando/Logic/Logic.h"

extern "C" {
#include "variables.h"
}

// games/mm/2s2h/GameExports_SingleExe.cpp (see mm_trick_bindings_test.cpp).
extern "C" void MM_Rando_InitCore(void);

namespace {

int sFailures = 0;

void GateFail(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    fprintf(stderr, "[MM-LOGIC-GATES] FAIL: ");
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");
    va_end(args);
    sFailures++;
}

/** Empty inventory (every slot ITEM_NONE, not zero: a zeroed slot reads as
 *  holding item 0x00, the Ocarina of Time), options zeroed (no clock shuffle),
 *  human form. */
void ResetSave() {
    memset(&gSaveContext, 0, sizeof(gSaveContext));
    memset(gSaveContext.save.saveInfo.inventory.items, ITEM_NONE, sizeof(gSaveContext.save.saveInfo.inventory.items));
    gSaveContext.save.playerForm = PLAYER_FORM_HUMAN;
}

void Give(ItemId itemId) {
    INV_CONTENT(itemId) = itemId;
}

void GiveEveryOcarinaButton() {
    for (int i = RANDO_INF_OBTAINED_OCARINA_BUTTON_A; i <= RANDO_INF_OBTAINED_OCARINA_BUTTON_C_UP; i++) {
        Flags_SetRandoInf((RandoInf)i);
    }
}

struct Reach {
    Rando::Logic::ReachabilityCrawl crawl;
    std::set<RandoCheckId> checks;
};

Reach CrawlFrom(s32 entrance) {
    Reach reach;
    reach.crawl = Rando::Logic::CrawlReachableRegions(entrance);
    reach.checks = Rando::Logic::EvaluateReachableChecks(reach.crawl);
    return reach;
}

void SetTrick(MMRandoTrickId id, bool on) {
    gSaveContext.save.shipSaveInfo.rando.randoSaveTricks[id] = on ? 1 : 0;
}

void RequireRegion(const char* leg, const Reach& reach, RandoRegionId region, const char* name) {
    if (!reach.crawl.reachableRegions.contains(region)) {
        GateFail("%s: %s is not reached, so this leg probes nothing", leg, name);
    }
}

void ExpectRegion(const char* leg, const Reach& reach, RandoRegionId region, const char* name, bool expected,
                  const char* why) {
    const bool got = reach.crawl.reachableRegions.contains(region);
    if (got != expected) {
        GateFail("%s: %s is %s: %s", leg, name, got ? "reachable" : "unreachable", why);
        return;
    }
    printf("[TEST]   ok: %s: %s %s\n", leg, name, expected ? "reachable" : "unreachable");
}

void ExpectCheck(const char* leg, const Reach& reach, RandoCheckId check, const char* name, bool expected,
                 const char* why) {
    const bool got = reach.checks.contains(check);
    if (got != expected) {
        GateFail("%s: %s is %s: %s", leg, name, got ? "reachable" : "unreachable", why);
        return;
    }
    printf("[TEST]   ok: %s: %s %s\n", leg, name, expected ? "reachable" : "unreachable");
}

struct RanchCheck {
    RandoCheckId check;
    const char* name;
};

const RanchCheck kRanchChecks[] = {
    { RC_ROMANI_RANCH_BARN_COW_LEFT, "barn cow (left)" },
    { RC_ROMANI_RANCH_BARN_COW_MIDDLE, "barn cow (middle)" },
    { RC_ROMANI_RANCH_BARN_COW_RIGHT, "barn cow (right)" },
    { RC_ROMANI_RANCH_FIELD_COW_ENTRANCE, "field cow (entrance)" },
    { RC_ROMANI_RANCH_FIELD_COW_NEAR_HOUSE_BACK, "field cow (near house, back)" },
    { RC_ROMANI_RANCH_FIELD_COW_NEAR_HOUSE_FRONT, "field cow (near house, front)" },
    { RC_ROMANI_RANCH_EPONAS_SONG, "Epona's Song" },
    { RC_ENEMY_DROP_ALIEN, "alien drop" },
};

void ArmRanchSave() {
    ResetSave();
    Give(ITEM_OCARINA_OF_TIME);
    Give(ITEM_BOW);
    SET_QUEST_ITEM(QUEST_SONG_EPONA);
    GiveEveryOcarinaButton();
}

void ArmMilkBarSave() {
    ResetSave();
    Give(ITEM_OCARINA_OF_TIME);
    Give(ITEM_MASK_DEKU);
    Give(ITEM_MASK_GORON);
    Give(ITEM_MASK_ZORA);
    GiveEveryOcarinaButton();
}

} // namespace

extern "C" int MM_LogicGates_RunHeadless(void) {
    printf("[TEST] mm-logic-gates: Romani Ranch's cows, Epona's Song and the alien drop need the Milk Road boulder "
           "broken, the Circus Leader needs Romani's Mask (#1168), and East Clock Town's Bombers' hideout door needs "
           "the Bombers' code or backflip trick (#1306), Great Bay Coast's minigame platforms need the Hookshot after "
           "the temple and the Fisherman plays 07:00-04:00 each day, and the Peahat dies by day only (#1398)\n");
    sFailures = 0;

    // Populate the region graph (file-scope ShipInit registrars; see
    // mm_trick_bindings_test.cpp for why CORE only). Idempotent.
    MM_Rando_InitCore();
    if (gRegEditor == NULL) {
        static RegEditor sProbeRegEditor = {};
        gRegEditor = &sProbeRegEditor;
    }

    // Heap, not stack: the port's SaveContext is tens of kilobytes.
    auto saved = std::make_unique<SaveContext>();
    memcpy(saved.get(), &gSaveContext, sizeof(SaveContext));
    const uint64_t savedTime = Rando::Logic::gCurrentRegionTime;

    // ---- R: Romani Ranch behind the Milk Road boulder (2S2H 9b699dee1) ------
    const s32 kMilkRoad = ENTRANCE(MILK_ROAD, 0);
    {
        ArmRanchSave();
        const Reach reach = CrawlFrom(kMilkRoad);
        const char* leg = "R1 no Goron Mask, no Powder Keg";
        RequireRegion(leg, reach, RR_MILK_ROAD, "Milk Road");
        RequireRegion(leg, reach, RR_ROMANI_RANCH, "Romani Ranch");
        RequireRegion(leg, reach, RR_RANCH_BARN, "the ranch barn");
        ExpectCheck(leg, reach, RC_ROMANI_RANCH_FIELD_LARGE_CRATE, "the field crate", true,
                    "the ranch's unconditional crate is not reached, so the crawl reached nothing");
        for (const RanchCheck& rc : kRanchChecks) {
            ExpectCheck(leg, reach, rc.check, rc.name, false,
                        "reached without breaking the boulder: the ranch is open only from Day 3, after the cows' "
                        "Night 1 window and Romani's Day 1 lesson");
        }
    }
    {
        ArmRanchSave();
        Give(ITEM_MASK_GORON);
        Give(ITEM_POWDER_KEG);
        const Reach reach = CrawlFrom(kMilkRoad);
        const char* leg = "R2 Goron Mask and Powder Keg";
        RequireRegion(leg, reach, RR_ROMANI_RANCH, "Romani Ranch");
        for (const RanchCheck& rc : kRanchChecks) {
            ExpectCheck(leg, reach, rc.check, rc.name, true,
                        "not reached with the boulder breakable on Day 1, so the gate refuses its own route");
        }
    }

    // ---- C: the Circus Leader's Mask needs Romani's Mask (2S2H cc5d87143) --
    const s32 kEastClockTown = ENTRANCE(EAST_CLOCK_TOWN, 11);
    {
        ArmMilkBarSave();
        const Reach reach = CrawlFrom(kEastClockTown);
        const char* leg = "C1 no Romani's Mask";
        RequireRegion(leg, reach, RR_CLOCK_TOWN_EAST, "East Clock Town");
        RequireRegion(leg, reach, RR_MILK_BAR, "the Milk Bar");
        ExpectCheck(leg, reach, RC_MILK_BAR_CIRCUS_LEADER_MASK, "the Circus Leader's Mask", false,
                    "reached without Romani's Mask: the bar's night show is behind the mask, its day door is not");
    }
    {
        ArmMilkBarSave();
        Give(ITEM_MASK_ROMANI);
        const Reach reach = CrawlFrom(kEastClockTown);
        ExpectCheck("C2 with Romani's Mask", reach, RC_MILK_BAR_CIRCUS_LEADER_MASK, "the Circus Leader's Mask", true,
                    "not reached with Romani's Mask, the ocarina, the three masks and every button");
    }

    // ---- O: East Clock Town's Bombers' hideout door (#1306) ----------------
    //
    // Direct check on the exit lambda first: in a full crawl from Clock Town with
    // tricks off, North Clock Town is always reachable with all time slices, so a
    // projectile fires RE_HIDE_SEEK_DAY1/2/3 -> RE_BOMBERS_* -> RE_BOMBER_CODE in
    // the same pass. Evaluating the door lambda directly separates `CAN_USE_PROJECTILE`
    // from `RANDO_EVENTS[RE_BOMBER_CODE]`, and the crawl legs below then drive the
    // full region/event graph through MMRT_BOMBER_GUESS, MMRT_BOMBER_BACKFLIP, Bow
    // hide-and-seek, and the Termina Field balcony.
    {
        const s32 kObservatoryEntrance = ENTRANCE(ASTRAL_OBSERVATORY, 0);
        auto regionIt = Rando::Logic::Regions.find(RR_CLOCK_TOWN_EAST);
        if (regionIt == Rando::Logic::Regions.end() || !regionIt->second.exits.contains(kObservatoryEntrance)) {
            GateFail("O1 East Clock Town -> Astral Observatory exit is missing from Rando::Logic::Regions");
        } else {
            const auto& cond = regionIt->second.exits.at(kObservatoryEntrance).condition;

            ResetSave();
            Give(ITEM_BOW);
            Rando::Logic::gCurrentRegionTime = Rando::Logic::TIME_ALL_SLICES;
            if (cond()) {
                GateFail("O1a Bow with RE_BOMBER_CODE == 0: East Clock Town -> Astral Observatory exit opened on a "
                         "projectile alone without the Bombers' code event");
            } else {
                printf("[TEST]   ok: O1a Bow with RE_BOMBER_CODE == 0: hideout door shut\n");
            }

            ResetSave();
            RANDO_EVENTS[RE_BOMBER_CODE] = 1;
            Rando::Logic::gCurrentRegionTime = Rando::Logic::TIME_ALL_SLICES;
            if (!cond()) {
                GateFail("O1b RE_BOMBER_CODE == 1 with no projectile: East Clock Town -> Astral Observatory exit "
                         "stayed shut when the Bombers' code event was set");
            } else {
                printf("[TEST]   ok: O1b RE_BOMBER_CODE == 1 with no projectile: hideout door open\n");
            }
        }
    }
    {
        ResetSave();
        const Reach reach = CrawlFrom(kEastClockTown);
        const char* leg = "O2 empty inventory, tricks off";
        RequireRegion(leg, reach, RR_CLOCK_TOWN_EAST, "East Clock Town");
        RequireRegion(leg, reach, RR_CLOCK_TOWN_NORTH, "North Clock Town");
        ExpectRegion(leg, reach, RR_ASTRAL_OBSERVATORY_PASSAGE, "the Astral Observatory passage", false,
                     "reached with no Bombers' code, no backflip trick and no way onto the balcony");
        ExpectRegion(leg, reach, RR_ASTRAL_OBSERVATORY, "the Astral Observatory", false,
                     "reached with no Bombers' code, no backflip trick and no way onto the balcony");
        ExpectCheck(leg, reach, RC_ASTRAL_OBSERVATORY_MOON_TEAR, "the Moon's Tear", false,
                    "reached with no Bombers' code, no backflip trick and no way onto the balcony");
        ExpectCheck(leg, reach, RC_CLOCK_TOWN_BOMBERS_NOTEBOOK, "the Bomber's Notebook", false,
                    "reached without the Bombers' code");
    }
    {
        ResetSave();
        SetTrick(MMRT_BOMBER_GUESS, true);
        const Reach reach = CrawlFrom(kEastClockTown);
        const char* leg = "O3 MMRT_BOMBER_GUESS on, no projectile";
        RequireRegion(leg, reach, RR_CLOCK_TOWN_NORTH, "North Clock Town");
        ExpectCheck(leg, reach, RC_CLOCK_TOWN_BOMBERS_NOTEBOOK, "the Bomber's Notebook", true,
                    "not reached when MMRT_BOMBER_GUESS fired RE_BOMBER_CODE in North Clock Town");
        ExpectRegion(leg, reach, RR_ASTRAL_OBSERVATORY_PASSAGE, "the Astral Observatory passage", true,
                     "not reached when RE_BOMBER_CODE fired via MMRT_BOMBER_GUESS without a projectile");
        ExpectRegion(leg, reach, RR_ASTRAL_OBSERVATORY, "the Astral Observatory", true,
                     "not reached when RE_BOMBER_CODE fired via MMRT_BOMBER_GUESS without a projectile");
        ExpectCheck(leg, reach, RC_ASTRAL_OBSERVATORY_MOON_TEAR, "the Moon's Tear", true,
                    "not reached when RE_BOMBER_CODE fired via MMRT_BOMBER_GUESS without a projectile");
    }
    {
        ResetSave();
        SetTrick(MMRT_BOMBER_BACKFLIP, true);
        const Reach reach = CrawlFrom(kEastClockTown);
        const char* leg = "O4 MMRT_BOMBER_BACKFLIP on, no code";
        RequireRegion(leg, reach, RR_CLOCK_TOWN_EAST, "East Clock Town");
        ExpectRegion(leg, reach, RR_ASTRAL_OBSERVATORY_PASSAGE, "the Astral Observatory passage", true,
                     "not reached with MMRT_BOMBER_BACKFLIP enabled on East Clock Town's hideout door");
        ExpectRegion(leg, reach, RR_ASTRAL_OBSERVATORY, "the Astral Observatory", true,
                     "not reached with MMRT_BOMBER_BACKFLIP enabled on East Clock Town's hideout door");
        ExpectCheck(leg, reach, RC_ASTRAL_OBSERVATORY_MOON_TEAR, "the Moon's Tear", true,
                    "not reached with MMRT_BOMBER_BACKFLIP enabled on East Clock Town's hideout door");
        ExpectCheck(leg, reach, RC_CLOCK_TOWN_BOMBERS_NOTEBOOK, "the Bomber's Notebook", false,
                    "reached by backflipping past the guard without learning the Bombers' code");
    }
    {
        ResetSave();
        Give(ITEM_BOW);
        const Reach reach = CrawlFrom(kEastClockTown);
        const char* leg = "O5 Bow hide-and-seek, tricks off";
        RequireRegion(leg, reach, RR_ASTRAL_OBSERVATORY_PASSAGE, "the Astral Observatory passage");
        RequireRegion(leg, reach, RR_ASTRAL_OBSERVATORY, "the Astral Observatory");
        ExpectCheck(leg, reach, RC_CLOCK_TOWN_BOMBERS_NOTEBOOK, "the Bomber's Notebook", true,
                    "not reached when a Bow shoots the balloon in North Clock Town and completes hide-and-seek");
        ExpectCheck(leg, reach, RC_ASTRAL_OBSERVATORY_MOON_TEAR, "the Moon's Tear", true,
                    "not reached when a Bow completes hide-and-seek and opens the Bombers' hideout door");
    }
    {
        ResetSave();
        Give(ITEM_MASK_DEKU);
        const Reach reach = CrawlFrom(kEastClockTown);
        const char* leg = "O6 Deku Mask without magic (Termina Field balcony route)";
        RequireRegion(leg, reach, RR_TERMINA_FIELD, "Termina Field");
        RequireRegion(leg, reach, RR_ASTRAL_OBSERVATORY_OUTSIDE, "Outside Astral Observatory");
        RequireRegion(leg, reach, RR_ASTRAL_OBSERVATORY, "the Astral Observatory");
        ExpectCheck(leg, reach, RC_ASTRAL_OBSERVATORY_MOON_TEAR, "the Moon's Tear", true,
                    "not reached from Termina Field's Deku Flower onto the observatory balcony");
        ExpectCheck(leg, reach, RC_CLOCK_TOWN_BOMBERS_NOTEBOOK, "the Bomber's Notebook", false,
                    "reached with the Deku Mask and no magic (cannot pop the balloon or learn the code)");
    }

    // ---- G: Great Bay Coast's minigame platforms (#1398, 2S2H 4661df2e6 and
    // 20b09fe5b). Probed on the row lambdas, as O1 does, because each leg is
    // about one edge or one check row under a chosen time mask.
    {
        auto cliffIt = Rando::Logic::Regions.find(RR_GREAT_BAY_COAST_CLIFFSIDE);
        auto platIt = Rando::Logic::Regions.find(RR_GREAT_BAY_COAST_MINIGAME_PLATFORMS);
        if (cliffIt == Rando::Logic::Regions.end() ||
            !cliffIt->second.connections.contains(RR_GREAT_BAY_COAST_MINIGAME_PLATFORMS)) {
            GateFail("G1 the cliffside -> minigame platforms edge is missing from Rando::Logic::Regions");
        } else if (platIt == Rando::Logic::Regions.end() ||
                   !platIt->second.checks.contains(RC_GREAT_BAY_COAST_FISHERMAN_MINIGAME)) {
            GateFail("G1 the Fisherman minigame row is missing from RR_GREAT_BAY_COAST_MINIGAME_PLATFORMS");
        } else {
            const auto& edge = cliffIt->second.connections.at(RR_GREAT_BAY_COAST_MINIGAME_PLATFORMS).first;
            const auto& fisherman = platIt->second.checks.at(RC_GREAT_BAY_COAST_FISHERMAN_MINIGAME).first;
            auto expectEdge = [&](const char* leg, bool expected, const char* why) {
                Rando::Logic::gCurrentRegionTime = Rando::Logic::TIME_ALL_SLICES;
                if (edge() != expected) {
                    GateFail("%s: the minigame platforms edge is %s: %s", leg, expected ? "shut" : "open", why);
                    return;
                }
                printf("[TEST]   ok: %s: platforms edge %s\n", leg, expected ? "open" : "shut");
            };

            ResetSave();
            Give(ITEM_HOOKSHOT);
            RANDO_EVENTS[RE_CLEARED_GREAT_BAY_TEMPLE] = 1;
            expectEdge("G1 Hookshot, temple cleared", true, "the Hookshot route after the temple no longer opens it");

            ResetSave();
            Give(ITEM_HOOKSHOT);
            expectEdge("G2 Hookshot, temple not cleared", false,
                       "the platforms are only up after Great Bay Temple is cleared (upstream 20b09fe5b)");

            ResetSave();
            Give(ITEM_BOW);
            Give(ITEM_ARROW_ICE);
            gSaveContext.save.saveInfo.playerData.isMagicAcquired = true;
            RANDO_EVENTS[RE_CLEARED_GREAT_BAY_TEMPLE] = 1;
            if (!CAN_USE_MAGIC_ARROW(ICE)) {
                GateFail("G3 the save does not satisfy CAN_USE_MAGIC_ARROW(ICE), so this leg probes nothing");
            }
            expectEdge("G3 Ice Arrows, temple cleared, no Hookshot", false,
                       "upstream 20b09fe5b REMOVED the Ice Arrow route to the platforms; only the Hookshot works");

            if (platIt->second.connections.contains(RR_GREAT_BAY_COAST)) {
                GateFail("G4 the platforms have a direct edge to the beach that upstream's region does not have");
            } else if (!platIt->second.connections.contains(RR_GREAT_BAY_COAST_CLIFFSIDE)) {
                GateFail("G4 the platforms have no edge back to the cliffside");
            } else {
                printf("[TEST]   ok: G4 platforms connect back to the cliffside only\n");
            }

            // The Fisherman's window: 07:00 to 04:00 on each of the three days.
            auto expectFisherman = [&](const char* leg, Rando::Logic::TimeSlice slice, bool expected, const char* why) {
                Rando::Logic::gCurrentRegionTime = Rando::Logic::TIME_BIT_ONE << slice;
                if (fisherman() != expected) {
                    GateFail("%s: the Fisherman minigame is %s: %s", leg, expected ? "closed" : "open", why);
                    return;
                }
                printf("[TEST]   ok: %s: Fisherman minigame %s\n", leg, expected ? "open" : "closed");
            };
            ResetSave();
            Give(ITEM_HOOKSHOT);
            RANDO_EVENTS[RE_CLEARED_GREAT_BAY_TEMPLE] = 1;
            expectFisherman("F1 Day 1 07:00", Rando::Logic::TIME_DAY1_AM_07_00, true, "the Day 1 window is closed");
            expectFisherman("F2 Day 2 07:00", Rando::Logic::TIME_DAY2_AM_07_00, true,
                            "the window is Day 1 only, but the game runs on all three days");
            expectFisherman("F3 Day 3 07:00", Rando::Logic::TIME_DAY3_AM_07_00, true,
                            "the window is missing Day 3, which upstream and main both have");
            expectFisherman("F4 Day 1 06:00", Rando::Logic::TIME_DAY1_AM_06_00, false,
                            "the window opens at 06:00, an hour before the game does");
            expectFisherman("F5 Night 2 04:00", Rando::Logic::TIME_NIGHT2_AM_04_00, false,
                            "the window runs past 04:00");

            ResetSave();
            RANDO_EVENTS[RE_CLEARED_GREAT_BAY_TEMPLE] = 1;
            expectFisherman("F6 no Hookshot, Day 2 07:00", Rando::Logic::TIME_DAY2_AM_07_00, false,
                            "the minigame needs the Hookshot");
        }
    }

    // ---- P: the Peahat dies by day only (#1398, 2S2H f237dd293) ------------
    // The term lives in CanKillEnemy now, so the ENEMY DROP row, which never had
    // it, is gated as well as the grotto chest that used to carry it alone.
    {
        auto grottoIt = Rando::Logic::Regions.find(RR_TERMINA_FIELD_PEAHAT_GROTTO);
        if (grottoIt == Rando::Logic::Regions.end() || !grottoIt->second.checks.contains(RC_ENEMY_DROP_PEAHAT) ||
            !grottoIt->second.checks.contains(RC_TERMINA_FIELD_PEAHAT_GROTTO_CHEST)) {
            GateFail("P1 the Peahat grotto's drop or chest row is missing from Rando::Logic::Regions");
        } else {
            struct PeahatRow {
                RandoCheckId check;
                const char* name;
            };
            const PeahatRow kPeahatRows[] = {
                { RC_ENEMY_DROP_PEAHAT, "the Peahat drop" },
                { RC_TERMINA_FIELD_PEAHAT_GROTTO_CHEST, "the Peahat grotto chest" },
            };
            const uint64_t kDays = Rando::Logic::GetHalfDayTimeMask(0) | Rando::Logic::GetHalfDayTimeMask(2) |
                                   Rando::Logic::GetHalfDayTimeMask(4);
            const uint64_t kNights = Rando::Logic::GetHalfDayTimeMask(1) | Rando::Logic::GetHalfDayTimeMask(3) |
                                     Rando::Logic::GetHalfDayTimeMask(5);
            ResetSave();
            Give(ITEM_MASK_ZORA);
            for (const PeahatRow& row : kPeahatRows) {
                const auto& cond = grottoIt->second.checks.at(row.check).first;
                Rando::Logic::gCurrentRegionTime = Rando::Logic::GetHalfDayTimeMask(2);
                if (!cond()) {
                    GateFail("P2 Zora Mask, Day 2: %s is out of logic by day, so the night leg probes nothing",
                             row.name);
                } else {
                    printf("[TEST]   ok: P2 Zora Mask, Day 2: %s in logic\n", row.name);
                }
                Rando::Logic::gCurrentRegionTime = kNights;
                if (cond()) {
                    GateFail("P3 Zora Mask, every night: %s is in logic, but the Peahat cannot be killed at night",
                             row.name);
                } else {
                    printf("[TEST]   ok: P3 Zora Mask, every night: %s out of logic\n", row.name);
                }
                Rando::Logic::gCurrentRegionTime = kDays;
                if (!cond()) {
                    GateFail("P4 Zora Mask, every day: %s is out of logic", row.name);
                }
            }
        }
    }

    memcpy(&gSaveContext, saved.get(), sizeof(SaveContext));
    Rando::Logic::gCurrentRegionTime = savedTime;

    if (sFailures == 0) {
        printf("[TEST] PASS: mm-logic-gates\n");
    } else {
        printf("[TEST] FAIL: mm-logic-gates: %d leg(s) failed\n", sFailures);
    }
    return sFailures;
}

#endif // RSBS_SINGLE_EXECUTABLE
