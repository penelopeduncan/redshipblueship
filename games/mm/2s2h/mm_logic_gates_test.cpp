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
 *  M. The MM logic batch M3 (2S2H 5fbb86ab #1954, e4061b6f #1984, b43af818
 *     #2006, 224ec437 #2017, a9a6009a #2018), on the row lambdas: the Twin
 *     Islands ledge needs the scarecrow with the Hookshot (M1); the Termina
 *     Field wall's night-only wonder items are 10-12, not 9-11 (M2), and three
 *     field wonder items loosen (M3); the Clock Town stray fairy takes the Great
 *     Fairy's Mask, the Curiosity Shop is open 22:00-05:00 each night in
 *     half-day form and turns you out at 05:00, and the Grandma's stories swap
 *     rules (M4); Sakon's hideout past the Deku Babas (M5); the Moon's Zora
 *     trial needs Swim (M6); Pinnacle Rock's first chest needs the Deep Python
 *     (M7); Woodfall Temple's princess needs a sword, its entrance pot nothing
 *     and its maze bubble Swim or the Great Fairy's Mask with a popper (M8);
 *     Stone Tower Temple's underwater switch and Armos (M9); the potion shop
 *     after Kotake is met in the woods (M10); the Beneath the Well grass needs
 *     the room lit (M11).
 *  T. Beneath the Well's Two Spikes Room grass (2S2H 51e52727d #1893), on the
 *     row lambdas: both grass rows are out of logic with no way to light a torch
 *     from another (an empty save, or Fire Arrows without magic) and in logic
 *     with a Deku Stick or with Fire Arrows.
 *
 * Every "not reached" leg is paired with a "reached" leg on the same save plus
 * one thing, and each names a region or check that must be reached, so a probe
 * that reaches nothing fails instead of passing.
 */

#ifdef RSBS_SINGLE_EXECUTABLE

#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <functional>
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

// ---- The M3 legs' row probes (2Ship #1954, #1984, #2006, #2017, #2018) -----
// Each returns the row's lambda, or nullptr when the row is not where the port
// put it (M3Expect then fails the leg instead of probing nothing).

using M3Cond = std::function<bool()>;

const M3Cond* M3CheckRow(RandoRegionId region, RandoCheckId check) {
    auto it = Rando::Logic::Regions.find(region);
    if (it == Rando::Logic::Regions.end() || !it->second.checks.contains(check)) {
        return nullptr;
    }
    return &it->second.checks.at(check).first;
}

const M3Cond* M3EventRow(RandoRegionId region, RandoEvent event) {
    auto it = Rando::Logic::Regions.find(region);
    if (it == Rando::Logic::Regions.end()) {
        return nullptr;
    }
    for (const auto& row : it->second.events) {
        if (row.first == event) {
            return &row.second;
        }
    }
    return nullptr;
}

const M3Cond* M3ExitRow(RandoRegionId region, s32 entrance) {
    auto it = Rando::Logic::Regions.find(region);
    if (it == Rando::Logic::Regions.end() || !it->second.exits.contains(entrance)) {
        return nullptr;
    }
    return &it->second.exits.at(entrance).condition;
}

void M3Expect(const char* leg, const char* row, const M3Cond* cond, uint64_t time, bool expected, const char* why) {
    if (cond == nullptr) {
        GateFail("%s: %s is missing from Rando::Logic::Regions, so this leg probes nothing", leg, row);
        return;
    }
    Rando::Logic::gCurrentRegionTime = time;
    const bool got = (*cond)();
    if (got != expected) {
        GateFail("%s: %s is %s: %s", leg, row, got ? "in logic" : "out of logic", why);
        return;
    }
    printf("[TEST]   ok: %s: %s %s\n", leg, row, expected ? "in logic" : "out of logic");
}

uint64_t M3Slice(Rando::Logic::TimeSlice slice) {
    return Rando::Logic::TIME_BIT_ONE << slice;
}

uint64_t M3Days() {
    return Rando::Logic::GetHalfDayTimeMask(0) | Rando::Logic::GetHalfDayTimeMask(2) |
           Rando::Logic::GetHalfDayTimeMask(4);
}

/** randoInf written directly, as mm-combo-logic-engine's SetResetKit does:
 *  nothing here needs the flag-set hooks. */
void M3SetRandoInf(int flag) {
    gSaveContext.save.shipSaveInfo.rando.randoInf[flag >> 4] |= (u16)(1u << (flag & 0xF));
}

void M3GiveSoul(RandoItemId soul) {
    M3SetRandoInf(SOUL_RI_TO_RANDO_INF(soul));
}

void M3GiveSwim() {
    M3SetRandoInf(RANDO_INF_OBTAINED_SWIM);
}

} // namespace

extern "C" int MM_LogicGates_RunHeadless(void) {
    printf("[TEST] mm-logic-gates: Romani Ranch's cows, Epona's Song and the alien drop need the Milk Road boulder "
           "broken, the Circus Leader needs Romani's Mask (#1168), and East Clock Town's Bombers' hideout door needs "
           "the Bombers' code or backflip trick (#1306), Great Bay Coast's minigame platforms need the Hookshot after "
           "the temple and the Fisherman plays 07:00-04:00 each day, and the Peahat dies by day only (#1398), and the "
           "2Ship logic batch M3 holds (#1954, #1984, #2006, #2017, #2018), and the Two Spikes Room grass needs a "
           "torch lit from another (#1893)\n");
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

    // ---- M: the MM logic batch M3 (2Ship #1954, #1984, #2006, #2017, #2018) --
    // On the row lambdas, as G and P are. Each "out of logic" leg holds what the
    // row needed before the port, so the old rule would put it in logic; each is
    // paired with an "in logic" leg on the same save plus the new requirement.
    const uint64_t kAll = Rando::Logic::TIME_ALL_SLICES;
    {
        // M1 (#1954): the Twin Islands snowballs and trees on the far ledge take
        // the Hookshot only by the scarecrow (Hookshot + two ocarina buttons).
        struct M3TwinRow {
            RandoCheckId check;
            const char* name;
        };
        const M3TwinRow kTwinRows[] = {
            { RC_TWIN_ISLANDS_SMALL_SNOWBALL_04, "Twin Islands small snowball 4" },
            { RC_TWIN_ISLANDS_SMALL_SNOWBALL_05, "Twin Islands small snowball 5" },
            { RC_TWIN_ISLANDS_SMALL_SNOWBALL_06, "Twin Islands small snowball 6" },
            { RC_TWIN_ISLANDS_SPRING_TREE_03, "Twin Islands spring tree 3" },
            { RC_TWIN_ISLANDS_TREE_03, "Twin Islands tree 3" },
        };
        for (const M3TwinRow& row : kTwinRows) {
            const M3Cond* cond = M3CheckRow(RR_PATH_TO_GORON_VILLAGE, row.check);
            ResetSave();
            RANDO_EVENTS[RE_CLEARED_SNOWHEAD_TEMPLE] = 1;
            Give(ITEM_HOOKSHOT);
            M3Expect("M1a Hookshot, no ocarina", row.name, cond, kAll, false,
                     "the Hookshot reaches the ledge only by the scarecrow (2Ship #1954, CAN_HOOK_SCARECROW)");
            Give(ITEM_OCARINA_OF_TIME);
            Flags_SetRandoInf(RANDO_INF_OBTAINED_OCARINA_BUTTON_A);
            Flags_SetRandoInf(RANDO_INF_OBTAINED_OCARINA_BUTTON_C_DOWN);
            M3Expect("M1b Hookshot and the scarecrow's song", row.name, cond, kAll, true,
                     "the Hookshot with the scarecrow (two buttons) no longer reaches the ledge");
            ResetSave();
            RANDO_EVENTS[RE_CLEARED_SNOWHEAD_TEMPLE] = 1;
            Give(ITEM_MASK_GORON);
            M3Expect("M1c Goron Mask", row.name, cond, kAll, true, "the Goron route was lost");
        }
    }
    {
        // M2 (#1984): the Termina Field wall's night-only group is items 10-12
        // (sRewardDropsIndex 9, 04:00 and 20:00); item 9 is in the 02:00 / 10:00
        // group and falls by day too.
        ResetSave();
        Give(ITEM_OCARINA_OF_TIME);
        GiveEveryOcarinaButton();
        const M3Cond* wall09 = M3CheckRow(RR_TERMINA_FIELD, RC_TERMINA_FIELD_WALL_WONDER_ITEM_09);
        const M3Cond* wall12 = M3CheckRow(RR_TERMINA_FIELD, RC_TERMINA_FIELD_WALL_WONDER_ITEM_12);
        M3Expect("M2a every day half-day", "wall wonder item 9", wall09, M3Days(), true,
                 "item 9 drops at 10:00 too, so it is not night-only (2Ship #1984)");
        M3Expect("M2b every day half-day", "wall wonder item 12", wall12, M3Days(), false,
                 "item 12 drops only at 20:00 and 04:00 (2Ship #1984)");
        M3Expect("M2c Night 1", "wall wonder item 12", wall12, Rando::Logic::GetHalfDayTimeMask(1), true,
                 "item 12 is out of logic at night, when it drops");
        // M3 (#2006): Termina Field's other wonder items.
        ResetSave();
        M3Expect("M3a empty save", "wonder item 10", M3CheckRow(RR_TERMINA_FIELD, RC_TERMINA_FIELD_WONDER_ITEM_10),
                 kAll, true, "upstream #2006 needs nothing for it");
        M3Expect("M3b empty save", "wonder item 11", M3CheckRow(RR_TERMINA_FIELD, RC_TERMINA_FIELD_WONDER_ITEM_11),
                 kAll, true, "upstream #2006 needs nothing for it");
        const M3Cond* wonder06 = M3CheckRow(RR_TERMINA_FIELD, RC_TERMINA_FIELD_WONDER_ITEM_06);
        M3Expect("M3c empty save", "wonder item 6", wonder06, kAll, false, "item 6 needs the Deku or Goron route");
        Give(ITEM_MASK_DEKU);
        M3Expect("M3d Deku Mask", "wonder item 6", wonder06, kAll, true, "the Deku Mask reaches it (2Ship #2006)");
    }
    {
        // M4 (#2006): Clock Town. The stray fairy takes the Great Fairy's Mask
        // at night; the Curiosity Shop opens 22:00-05:00 each night, written so
        // each disjunct holds on one half-day (#1453), and turns you out at
        // 05:00; the Grandma's stories swap their time rules.
        const M3Cond* fairy = M3CheckRow(RR_CLOCK_TOWN_EAST, RC_CLOCK_TOWN_STRAY_FAIRY);
        ResetSave();
        Give(ITEM_MASK_GREAT_FAIRY);
        M3Expect("M4a Great Fairy's Mask, Night 2", "the Clock Town stray fairy", fairy,
                 Rando::Logic::GetHalfDayTimeMask(3), true, "the Great Fairy's Mask draws it down (2Ship #2006)");
        M3Expect("M4b Great Fairy's Mask, every day", "the Clock Town stray fairy", fairy, M3Days(), false,
                 "the fairy is out only at night");
        ResetSave();
        M3Expect("M4c empty save, Night 2", "the Clock Town stray fairy", fairy, Rando::Logic::GetHalfDayTimeMask(3),
                 false, "it needs the Deku Mask or the Great Fairy's Mask");

        const M3Cond* shop = M3ExitRow(RR_CLOCK_TOWN_WEST, ENTRANCE(CURIOSITY_SHOP, 0));
        struct M3ShopSlice {
            Rando::Logic::TimeSlice slice;
            const char* name;
            bool open;
        };
        const M3ShopSlice kShopSlices[] = {
            { Rando::Logic::TIME_NIGHT1_PM_10_00, "the Curiosity Shop door at Night 1 22:00", true },
            { Rando::Logic::TIME_NIGHT1_AM_04_00, "the Curiosity Shop door at Night 1 04:00", true },
            { Rando::Logic::TIME_NIGHT1_AM_05_00, "the Curiosity Shop door at Night 1 05:00", false },
            { Rando::Logic::TIME_NIGHT2_AM_05_00, "the Curiosity Shop door at Night 2 05:00", false },
            { Rando::Logic::TIME_NIGHT2_AM_05_30, "the Curiosity Shop door at Night 2 05:30", false },
            { Rando::Logic::TIME_NIGHT3_PM_10_00, "the Curiosity Shop door at Night 3 22:00", true },
            { Rando::Logic::TIME_NIGHT3_AM_05_00, "the Curiosity Shop door at Night 3 05:00", false },
            { Rando::Logic::TIME_NIGHT3_PM_09_00, "the Curiosity Shop door at Night 3 21:00", false },
        };
        for (const M3ShopSlice& s : kShopSlices) {
            M3Expect("M4d", s.name, shop, M3Slice(s.slice), s.open,
                     "the shop is open from 22:00 to 05:00 each night (2Ship #2006)");
        }
        for (int h = 0; h < 6; ++h) {
            M3Expect("M4e one half-day alone", "the Curiosity Shop door", shop, Rando::Logic::GetHalfDayTimeMask(h),
                     (h % 2) == 1, "each night opens the door on its own half-day and no day does (#1453)");
        }
        auto frontIt = Rando::Logic::Regions.find(RR_CURIOSITY_SHOP_FRONT);
        for (Rando::Logic::TimeSlice slice : { Rando::Logic::TIME_NIGHT1_AM_05_00, Rando::Logic::TIME_NIGHT2_AM_05_00,
                                               Rando::Logic::TIME_NIGHT3_AM_05_00 }) {
            if (frontIt == Rando::Logic::Regions.end() || !frontIt->second.timeStayRestrictions.contains(slice)) {
                GateFail("M4f the Curiosity Shop's front has no stay rule at slice %d (2Ship #2006 closes it at 05:00)",
                         (int)slice);
            } else if (frontIt->second.timeStayRestrictions.at(slice)()) {
                GateFail("M4f the Curiosity Shop's front lets you stay past 05:00 at slice %d", (int)slice);
            } else {
                printf("[TEST]   ok: M4f the Curiosity Shop's front turns you out at slice %d\n", (int)slice);
            }
        }

        // The stories under Clock Shuffle (random mode): with only the Day 1
        // clock the short story is told, the long one needs a later clock.
        const M3Cond* shortStory = M3CheckRow(RR_INN, RC_STOCK_POT_INN_GRANDMA_SHORT_STORY);
        const M3Cond* longStory = M3CheckRow(RR_INN, RC_STOCK_POT_INN_GRANDMA_LONG_STORY);
        ResetSave();
        RANDO_SAVE_OPTIONS[RO_CLOCK_SHUFFLE] = RO_GENERIC_YES;
        RANDO_SAVE_OPTIONS[RO_CLOCK_SHUFFLE_PROGRESSIVE] = RO_CLOCK_SHUFFLE_RANDOM;
        M3SetRandoInf(RANDO_INF_OBTAINED_CLOCK_DAY_1);
        Give(ITEM_MASK_ALL_NIGHT);
        const uint64_t kDay1 = Rando::Logic::GetHalfDayTimeMask(0);
        M3Expect("M4g All-Night Mask, Day 1 clock only", "the Grandma's short story", shortStory, kDay1, true,
                 "the short story is told on Day 1 without a later clock (2Ship #2006 swapped the two rules)");
        M3Expect("M4h All-Night Mask, Day 1 clock only", "the Grandma's long story", longStory, kDay1, false,
                 "the long story runs into a later half-day, so it needs a later clock");
        M3SetRandoInf(RANDO_INF_OBTAINED_CLOCK_DAY_1 + 2); // the Day 2 clock
        M3Expect("M4i All-Night Mask, Day 1 and Day 2 clocks", "the Grandma's long story", longStory, kDay1, true,
                 "the long story is out of logic with the Day 2 clock held");
    }
    {
        // M5 (#2006): Sakon's hideout. The second and third rooms' pots and the
        // Sun's Mask (and the Wolfos drop) are past the Deku Babas.
        struct M3SakonPot {
            RandoCheckId check;
            const char* name;
        };
        const M3SakonPot kSakonPots[] = {
            { RC_SAKON_HIDEOUT_SECOND_ROOM_POT_01, "Sakon's hideout second room pot 1" },
            { RC_SAKON_HIDEOUT_SECOND_ROOM_POT_02, "Sakon's hideout second room pot 2" },
            { RC_SAKON_HIDEOUT_THIRD_ROOM_POT, "Sakon's hideout third room pot" },
        };
        for (const M3SakonPot& row : kSakonPots) {
            const M3Cond* pot = M3CheckRow(RR_SAKON_HIDEOUT, row.check);
            ResetSave();
            M3Expect("M5a empty save", row.name, pot, kAll, false, "the pot is past the Deku Babas (2Ship #2006)");
            Give(ITEM_MASK_DEKU);
            M3Expect("M5c Deku Mask", row.name, pot, kAll, true, "the Deku Mask kills a Deku Baba");
        }
        ResetSave();
        M3Expect("M5b empty save", "Sakon's hideout first room pot",
                 M3CheckRow(RR_SAKON_HIDEOUT, RC_SAKON_HIDEOUT_FIRST_ROOM_POT_01), kAll, true,
                 "the first room is before the Deku Babas");
        // Every weapon that kills a Wolfos kills a Deku Baba, so the Sun's Mask
        // and Wolfos drop terms show only with the enemy souls shuffled.
        const M3Cond* sun = M3EventRow(RR_SAKON_HIDEOUT, RE_RETRIEVE_SUN_MASK);
        const M3Cond* wolfos = M3CheckRow(RR_SAKON_HIDEOUT, RC_ENEMY_DROP_WOLFOS);
        ResetSave();
        RANDO_SAVE_OPTIONS[RO_SHUFFLE_ENEMY_SOULS] = RO_GENERIC_YES;
        SET_EQUIP_VALUE(EQUIP_TYPE_SWORD, EQUIP_VALUE_SWORD_KOKIRI);
        M3GiveSoul(RI_SOUL_ENEMY_WOLFOS);
        M3Expect("M5d sword, Wolfos soul only", "the Sun's Mask", sun, kAll, false,
                 "the Sun's Mask is past the Deku Babas, whose soul is not held (2Ship #2006)");
        M3Expect("M5f sword, Wolfos soul only", "Sakon's hideout Wolfos drop", wolfos, kAll, false,
                 "the Wolfos is past the Deku Babas, whose soul is not held (2Ship #2006)");
        M3GiveSoul(RI_SOUL_ENEMY_DEKU_BABA);
        M3Expect("M5e sword, both souls", "the Sun's Mask", sun, kAll, true, "the Sun's Mask is out of logic");
        M3Expect("M5g sword, both souls", "Sakon's hideout Wolfos drop", wolfos, kAll, true,
                 "the Wolfos drop is out of logic");
    }
    {
        // M6 (#2006): the Moon's Zora trial Heart Piece needs Swim.
        const M3Cond* poh = M3CheckRow(RR_MOON_ZORA_TRIAL, RC_MOON_TRIAL_ZORA_PIECE_OF_HEART);
        ResetSave();
        M3Expect("M6a no Swim", "the Zora trial Heart Piece", poh, kAll, false, "the trial is a swim (2Ship #2006)");
        M3GiveSwim();
        M3Expect("M6b Swim", "the Zora trial Heart Piece", poh, kAll, true, "Swim does not reach it");
    }
    {
        // M7 (#2006): Pinnacle Rock's first chest is guarded by a Deep Python.
        const M3Cond* chest = M3CheckRow(RR_PINNACLE_ROCK_INNER, RC_PINNACLE_ROCK_CHEST_01);
        ResetSave();
        Give(ITEM_MASK_ZORA);
        M3Expect("M7a Zora Mask, no magic", "Pinnacle Rock chest 1", chest, kAll, false,
                 "the chest needs the Deep Python killed, as chest 2 does (2Ship #2006)");
        gSaveContext.save.saveInfo.playerData.isMagicAcquired = true;
        M3Expect("M7b Zora Mask and magic", "Pinnacle Rock chest 1", chest, kAll, true,
                 "the Zora's barrier does not open the chest");
    }
    {
        // M8 (#2006): Woodfall Temple. Freeing the Deku Princess takes a sword;
        // the entrance pot needs nothing; the maze's bubble fairy takes Swim, or
        // the Great Fairy's Mask with a way to pop it.
        const M3Cond* princess = M3EventRow(RR_WOODFALL_TEMPLE_DEKU_PRINCESS_ROOM, RE_ACCESS_DEKU_PRINCESS);
        ResetSave();
        M3Expect("M8a no sword", "the Deku Princess", princess, kAll, false, "freeing her takes a sword (2Ship #2006)");
        SET_EQUIP_VALUE(EQUIP_TYPE_SWORD, EQUIP_VALUE_SWORD_KOKIRI);
        M3Expect("M8b Kokiri Sword", "the Deku Princess", princess, kAll, true, "the sword does not free her");
        ResetSave();
        M3Expect("M8c empty save", "the Woodfall Temple entrance pot",
                 M3CheckRow(RR_WOODFALL_TEMPLE_ENTRANCE, RC_WOODFALL_TEMPLE_ENTRANCE_POT), kAll, true,
                 "the pot is on the floor and needs nothing (2Ship #2006)");
        const M3Cond* bubble = M3CheckRow(RR_WOODFALL_TEMPLE_MAZE_ROOM, RC_WOODFALL_TEMPLE_SF_MAZE_BUBBLE);
        M3Expect("M8d empty save", "the maze bubble fairy", bubble, kAll, false,
                 "the bubble is over the water (2Ship #2006)");
        Give(ITEM_MASK_GREAT_FAIRY);
        M3Expect("M8e Great Fairy's Mask alone", "the maze bubble fairy", bubble, kAll, false,
                 "the mask cannot pop the bubble");
        Give(ITEM_BOW);
        M3Expect("M8f Great Fairy's Mask and Bow", "the maze bubble fairy", bubble, kAll, true,
                 "the Bow pops it and the mask draws the fairy");
        ResetSave();
        M3GiveSwim();
        M3Expect("M8g Swim", "the maze bubble fairy", bubble, kAll, true, "Swim does not reach the bubble");
    }
    {
        // M9 (#2006, #2017): Stone Tower Temple. The inverted east upper chest
        // waits on the deep pool's underwater switch (Zora and Swim), and the
        // lava chest in the Armos room needs the Armos killed.
        const M3Cond* sw = M3EventRow(RR_STONE_TOWER_TEMPLE_DEEP_POOL_ROOM_UNDERWATER,
                                      RE_STONE_TOWER_TEMPLE_DEEP_POOL_UNDERWATER_SWITCH);
        ResetSave();
        Give(ITEM_MASK_ZORA);
        M3Expect("M9a Zora Mask, no Swim", "the deep pool's underwater switch", sw, kAll, false,
                 "the switch is under water");
        M3GiveSwim();
        M3Expect("M9b Zora Mask and Swim", "the deep pool's underwater switch", sw, kAll, true,
                 "the Zora does not reach the switch");
        const M3Cond* upper =
            M3CheckRow(RR_STONE_TOWER_TEMPLE_INVERTED_WIND_ROOM, RC_STONE_TOWER_TEMPLE_INVERTED_EAST_UPPER_CHEST);
        ResetSave();
        Give(ITEM_MASK_DEKU);
        Give(ITEM_OCARINA_OF_TIME);
        GiveEveryOcarinaButton();
        SET_QUEST_ITEM(QUEST_SONG_ELEGY);
        M3Expect("M9c Deku and Elegy, switch not pressed", "the inverted east upper chest", upper, kAll, false,
                 "the chest appears only after the underwater switch (2Ship #2006)");
        RANDO_EVENTS[RE_STONE_TOWER_TEMPLE_DEEP_POOL_UNDERWATER_SWITCH] = 1;
        M3Expect("M9d Deku and Elegy, switch pressed", "the inverted east upper chest", upper, kAll, true,
                 "the chest is out of logic with the switch pressed");
        // The Goron kills an Armos, so the term shows with the enemy souls shuffled.
        const M3Cond* lava =
            M3CheckRow(RR_STONE_TOWER_TEMPLE_ARMOS_ROOM, RC_STONE_TOWER_TEMPLE_UNDER_WEST_GARDEN_LAVA_CHEST);
        ResetSave();
        RANDO_SAVE_OPTIONS[RO_SHUFFLE_ENEMY_SOULS] = RO_GENERIC_YES;
        Give(ITEM_MASK_GORON);
        Give(ITEM_BOW);
        Give(ITEM_ARROW_LIGHT);
        gSaveContext.save.saveInfo.playerData.isMagicAcquired = true;
        M3Expect("M9e Goron and Light Arrows, no Armos soul", "the lava chest", lava, kAll, false,
                 "the chest needs the Armos killed (2Ship #2017)");
        M3GiveSoul(RI_SOUL_ENEMY_ARMOS);
        M3Expect("M9f Goron and Light Arrows, Armos soul", "the lava chest", lava, kAll, true,
                 "the chest is out of logic with the Armos killable");
    }
    {
        // M10 (#2017): after Day 1 the potion shop sells once Kotake is met in
        // the Woods of Mystery (Day 2 or the final day), not once Koume is saved.
        // Item 1 also needs the Mask of Scents and a bottle, held on both legs.
        struct M3PotionRow {
            RandoCheckId check;
            const char* name;
        };
        const M3PotionRow kPotionRows[] = {
            { RC_HAGS_POTION_SHOP_ITEM_01, "the potion shop's first item" },
            { RC_HAGS_POTION_SHOP_ITEM_02, "the potion shop's second item" },
            { RC_HAGS_POTION_SHOP_ITEM_03, "the potion shop's third item" },
        };
        const uint64_t kDay2 = Rando::Logic::GetHalfDayTimeMask(2);
        for (const M3PotionRow& row : kPotionRows) {
            const M3Cond* item = M3CheckRow(RR_MAGIC_HAGS_POTION_SHOP, row.check);
            ResetSave();
            Give(ITEM_MASK_SCENTS);
            INV_CONTENT(ITEM_BOTTLE) = ITEM_BOTTLE;
            RANDO_EVENTS[RE_SAVED_KOUME] = 1;
            M3Expect("M10a Koume saved, Day 2", row.name, item, kDay2, false,
                     "after Day 1 the shop needs Kotake met in the woods (2Ship #2017)");
            RANDO_EVENTS[RE_SAVED_KOUME] = 0;
            RANDO_EVENTS[RE_MEET_KOTAKE_WOODS] = 1;
            M3Expect("M10b Kotake met, Day 2", row.name, item, kDay2, true,
                     "the shop is out of logic after meeting Kotake");
        }
        const M3Cond* kotake = M3EventRow(RR_WOODS_OF_MYSTERY, RE_MEET_KOTAKE_WOODS);
        ResetSave();
        M3Expect("M10c Day 1 and Night 1", "meeting Kotake in the woods", kotake,
                 Rando::Logic::GetHalfDayTimeMask(0) | Rando::Logic::GetHalfDayTimeMask(1), false,
                 "Kotake is in the woods from Day 2");
        M3Expect("M10d Day 2", "meeting Kotake in the woods", kotake, kDay2, true, "Kotake is in the woods on Day 2");
        M3Expect("M10e Night 3", "meeting Kotake in the woods", kotake, Rando::Logic::GetHalfDayTimeMask(5), true,
                 "Kotake is in the woods on the final day");
    }
    {
        // M11 (#2018): the Beneath the Well four-spikes room's grass needs the
        // room lit: Fire Arrows, or a Deku Stick carried from the Big Poe room
        // (a bottle, the Big Poe and the Gibdo Mask).
        struct M3GrassRow {
            RandoCheckId check;
            const char* name;
        };
        const M3GrassRow kGrassRows[] = {
            { RC_BENEATH_THE_WELL_FOUR_SPIKED_BARS_GRASS_01, "the four-spikes room grass 1" },
            { RC_BENEATH_THE_WELL_FOUR_SPIKED_BARS_GRASS_02, "the four-spikes room grass 2" },
            { RC_BENEATH_THE_WELL_FOUR_SPIKED_BARS_GRASS_03, "the four-spikes room grass 3" },
            { RC_BENEATH_THE_WELL_FOUR_SPIKED_BARS_GRASS_04, "the four-spikes room grass 4" },
            { RC_BENEATH_THE_WELL_FOUR_SPIKED_BARS_GRASS_05, "the four-spikes room grass 5" },
        };
        for (const M3GrassRow& row : kGrassRows) {
            const M3Cond* grass = M3CheckRow(RR_BENEATH_THE_WELL_FOUR_SPIKED_BARS, row.check);
            ResetSave();
            M3Expect("M11a empty save", row.name, grass, kAll, false, "the grass needs the room lit (2Ship #2018)");
            Give(ITEM_DEKU_STICK);
            M3Expect("M11b Deku Stick alone", row.name, grass, kAll, false,
                     "the stick needs a lit torch the Gibdo Mask and the Big Poe lead to");
            INV_CONTENT(ITEM_BOTTLE) = ITEM_BOTTLE;
            RANDO_EVENTS[RE_ACCESS_BIG_POE] = 1;
            Give(ITEM_MASK_GIBDO);
            M3Expect("M11c Deku Stick, bottle, Big Poe and Gibdo Mask", row.name, grass, kAll, true,
                     "the stick route is out of logic");
            ResetSave();
            Give(ITEM_BOW);
            Give(ITEM_ARROW_FIRE);
            gSaveContext.save.saveInfo.playerData.isMagicAcquired = true;
            M3Expect("M11d Fire Arrows", row.name, grass, kAll, true, "Fire Arrows do not light the room");
        }
    }

    // ---- T: the Two Spikes Room grass (2Ship 51e52727d #1893) --------------
    // CAN_LIGHT_TORCH_NEAR_ANOTHER, as the room's pots and its bug event: a
    // Deku Stick or Fire Arrows. An empty save would reach the grass under the
    // old `true` rows.
    {
        struct TsGrassRow {
            RandoCheckId check;
            const char* name;
        };
        const TsGrassRow kTsGrassRows[] = {
            { RC_BENEATH_THE_WELL_TWO_SPIKED_BARS_GRASS_01, "the two-spikes room grass 1" },
            { RC_BENEATH_THE_WELL_TWO_SPIKED_BARS_GRASS_02, "the two-spikes room grass 2" },
        };
        for (const TsGrassRow& row : kTsGrassRows) {
            const M3Cond* grass = M3CheckRow(RR_BENEATH_THE_WELL_TWO_SPIKED_BARS, row.check);
            ResetSave();
            M3Expect("T1 empty save", row.name, grass, kAll, false,
                     "the grass needs a torch lit from another (2Ship #1893)");
            Give(ITEM_BOW);
            Give(ITEM_ARROW_FIRE);
            M3Expect("T2 Bow and Fire Arrows, no magic", row.name, grass, kAll, false,
                     "Fire Arrows without magic cannot light a torch");
            ResetSave();
            Give(ITEM_DEKU_STICK);
            M3Expect("T3 Deku Stick", row.name, grass, kAll, true, "a Deku Stick does not light the torch");
            ResetSave();
            Give(ITEM_BOW);
            Give(ITEM_ARROW_FIRE);
            gSaveContext.save.saveInfo.playerData.isMagicAcquired = true;
            M3Expect("T4 Fire Arrows", row.name, grass, kAll, true, "Fire Arrows do not light the torch");
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
