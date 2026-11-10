/**
 * @file mm_time_edges_test.cpp
 * @brief #1453: an MM logic edge carries only the half-days its own condition
 *        holds on, in both traversals. CTest row `MMTimeEdges` (label `rando`),
 *        test name `mm-time-edges`, registered in src/common/test_runner.cpp.
 *
 * WHAT WAS BROKEN. CrawlReachableRegions (paired fills, both trackers) and
 * FindReachableRegions (the solo glitchless fill) evaluated an edge once against
 * the source region's whole time mask and, when it passed, copied that whole
 * mask into the target. Conditions read time as "any slice in range", so an
 * `IS_NIGHT2()` edge out of a region reachable at every owned slice handed its
 * target the Day 1 morning too, and a day-gated check row there was satisfied
 * by a route that only exists at night.
 *
 * THE GRAPH. A synthetic region map swapped in for Rando::Logic::Regions (and
 * swapped back), so the legs name exactly the edges they probe. Shuffle Time on
 * in Random mode with all six half-days owned, so every slice is owned, and no
 * region can stay over time, so ExpandTimeForward adds nothing:
 *
 *   RR_MAX (root) --true--> SRC
 *   SRC --IS_NIGHT2()-------------> NIGHT  (checks: IS_DAY1 day row, IS_NIGHT2 control)
 *   SRC --HAS_ITEM(ITEM_BOW)------> FREE   (check: IS_DAY1 row; the Bow is given)
 *   SRC --IS_DAY1() || IS_NIGHT2()-> TWO
 *   STAY (can stay, not linked from the root) --IS_NIGHT2()--> STAYNIGHT
 *
 * LEGS (each on CrawlReachableRegions + EvaluateReachableChecks, then on
 * FindReachableRegions from SRC with every slice owned):
 *   a1  NIGHT's mask is exactly Night 2: no day slice crosses the edge.
 *   a2  NIGHT's IS_DAY1 check row is NOT satisfied; its IS_NIGHT2 row IS
 *       (non-vacuity: the region is reached and its own time works).
 *   b   FREE's mask is every slice and its IS_DAY1 row IS satisfied: a
 *       time-free edge still carries the whole mask.
 *   c   TWO's mask is Day 1 | Night 2: a two-half-day edge keeps both.
 *   d   (FindReachableRegions only) STAY seeded at Day 1 06:00 alone stays
 *       to every slice, and STAYNIGHT's mask is exactly Night 2: the
 *       narrowing applies after ExpandTimeForward, the default-settings
 *       shape (a Day 1 seed plus a stay).
 *   e   (#1509, the solo fill's clock-gain path) the fill's own sweep from
 *       { RR_MAX } with only Night 2 owned, then Day 1 gained and
 *       ResetRegionTimeStatesForTimeGain, then the sweep again:
 *       e1  the reset leaves the set at { RR_MAX } and the root at the new
 *           owned time (Day 1 | Night 2) with no other state;
 *       e2  SRC and FREE take Day 1 | Night 2 and FREE's IS_DAY1 row IS
 *           satisfied (non-vacuity: the gain reached the rebuilt regions);
 *       e3  NIGHT's mask is still exactly Night 2 and its IS_DAY1 row is NOT
 *           satisfied: the clock did not widen the region in place.
 * Unit legs on NarrowEdgeTime itself:
 *   u1  a source in one half-day is returned as is with no extra evaluation;
 *   u2  a time-free condition over every slice costs exactly six evaluations
 *       and returns every slice;
 *   u3  a condition no single half-day satisfies falls back to the whole mask;
 *   u4  gCurrentRegionTime is restored.
 *
 * Touches the process-wide region map and the entrance->region cache, so it is
 * skipped by `--test all`; it restores both, the SaveContext image and
 * gCurrentRegionTime.
 */

#ifdef RSBS_SINGLE_EXECUTABLE

#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <map>
#include <memory>
#include <set>
#include <unordered_map>

#include "Rando/Rando.h"
#include "Rando/Types.h"
#include "Rando/Logic/Logic.h"

extern "C" {
#include "variables.h"
}

namespace {

using namespace Rando::Logic;

int sFailures = 0;

void TeFail(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    fprintf(stderr, "[MM-TIME-EDGES] FAIL: ");
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");
    va_end(args);
    sFailures++;
}

void TeOk(const char* leg) {
    printf("[TEST]   ok: %s\n", leg);
}

// Synthetic region ids: any distinct ids work, the map is ours for the row.
constexpr RandoRegionId kSrc = RR_MILK_ROAD;
constexpr RandoRegionId kNight = RR_ROMANI_RANCH;
constexpr RandoRegionId kFree = RR_RANCH_BARN;
constexpr RandoRegionId kTwo = RR_MILK_BAR;
constexpr RandoRegionId kStay = RR_GORMAN_TRACK;
constexpr RandoRegionId kStayNight = RR_DOGGY_RACETRACK;

constexpr RandoCheckId kNightDayRow = RC_ROMANI_RANCH_FIELD_LARGE_CRATE;
constexpr RandoCheckId kNightNightRow = RC_ROMANI_RANCH_EPONAS_SONG;
constexpr RandoCheckId kFreeDayRow = RC_MILK_BAR_CIRCUS_LEADER_MASK;

/** Arms Shuffle Time in Random mode owning exactly the half-days whose bit is
 *  set in `ownedHalfDays` (bit i = half-day i, Day 1 = bit 0). */
void ArmSaveOwning(unsigned ownedHalfDays) {
    memset(&gSaveContext, 0, sizeof(gSaveContext));
    memset(gSaveContext.save.saveInfo.inventory.items, ITEM_NONE, sizeof(gSaveContext.save.saveInfo.inventory.items));
    gSaveContext.save.playerForm = PLAYER_FORM_HUMAN;
    INV_CONTENT(ITEM_BOW) = ITEM_BOW;
    RANDO_SAVE_OPTIONS[RO_CLOCK_SHUFFLE] = RO_GENERIC_ON;
    RANDO_SAVE_OPTIONS[RO_CLOCK_SHUFFLE_PROGRESSIVE] = RO_CLOCK_SHUFFLE_RANDOM;
    for (int i = 0; i < 6; i++) {
        if ((ownedHalfDays & (1u << i)) != 0) {
            Flags_SetRandoInf((RandoInf)(RANDO_INF_OBTAINED_CLOCK_DAY_1 + i));
        }
    }
}

void ArmSave() {
    ArmSaveOwning(0x3F);
}

RandoRegion NoStayRegion(const char* name) {
    RandoRegion region{};
    region.name = name;
    region.canStayOverTime = false;
    return region;
}

std::map<RandoRegionId, RandoRegion> BuildGraph() {
    std::map<RandoRegionId, RandoRegion> graph;

    RandoRegion root = NoStayRegion("time-edges root");
    root.connections[kSrc] = { [] { return true; }, "true" };
    graph[RR_MAX] = root;

    RandoRegion src = NoStayRegion("time-edges source");
    src.connections[kNight] = { [] { return IS_NIGHT2(); }, "IS_NIGHT2()" };
    src.connections[kFree] = { [] { return HAS_ITEM(ITEM_BOW); }, "HAS_ITEM(ITEM_BOW)" };
    src.connections[kTwo] = { [] { return IS_DAY1() || IS_NIGHT2(); }, "IS_DAY1() || IS_NIGHT2()" };
    graph[kSrc] = src;

    RandoRegion night = NoStayRegion("time-edges night-only target");
    night.checks[kNightDayRow] = { [] { return IS_DAY1(); }, "IS_DAY1()" };
    night.checks[kNightNightRow] = { [] { return IS_NIGHT2(); }, "IS_NIGHT2()" };
    graph[kNight] = night;

    RandoRegion freeTarget = NoStayRegion("time-edges time-free target");
    freeTarget.checks[kFreeDayRow] = { [] { return IS_DAY1(); }, "IS_DAY1()" };
    graph[kFree] = freeTarget;

    graph[kTwo] = NoStayRegion("time-edges two-half-day target");

    RandoRegion stay = NoStayRegion("time-edges staying source");
    stay.canStayOverTime = true;
    stay.connections[kStayNight] = { [] { return IS_NIGHT2(); }, "IS_NIGHT2()" };
    graph[kStay] = stay;
    graph[kStayNight] = NoStayRegion("time-edges night-only target of a staying source");
    return graph;
}

uint64_t MaskOf(const std::unordered_map<RandoRegionId, RegionTimeState>& states, RandoRegionId region) {
    auto it = states.find(region);
    return it == states.end() ? 0 : it->second.timeSlices;
}

bool RowHolds(const std::unordered_map<RandoRegionId, RegionTimeState>& states, RandoRegionId region,
              RandoCheckId check) {
    if (!states.contains(region)) {
        return false;
    }
    SetCurrentRegionTime(states, region);
    return Regions.at(region).checks.at(check).first();
}

void ExpectMask(const char* leg, uint64_t got, uint64_t expected, const char* why) {
    if (got != expected) {
        TeFail("%s: mask 0x%llX, expected 0x%llX: %s", leg, (unsigned long long)got, (unsigned long long)expected, why);
        return;
    }
    TeOk(leg);
}

void ExpectBool(const char* leg, bool got, bool expected, const char* why) {
    if (got != expected) {
        TeFail("%s: %s: %s", leg, got ? "true" : "false", why);
        return;
    }
    TeOk(leg);
}

/** Legs a1, a2, b and c over one traversal's result. */
void CheckTraversal(const char* name, const std::set<RandoRegionId>& reached,
                    const std::unordered_map<RandoRegionId, RegionTimeState>& states,
                    const std::set<RandoCheckId>& satisfiable) {
    char leg[160];
    const uint64_t day1 = GetHalfDayTimeMask(0);
    const uint64_t night2 = GetHalfDayTimeMask(3);

    for (RandoRegionId region : { kSrc, kNight, kFree, kTwo }) {
        if (!reached.contains(region)) {
            TeFail("%s: synthetic region %d is not reached, so the legs below probe nothing", name, (int)region);
        }
    }

    snprintf(leg, sizeof(leg), "%s a1 IS_NIGHT2 edge carries Night 2 only", name);
    ExpectMask(leg, MaskOf(states, kNight), night2,
               "the night-only edge carried slices outside Night 2 into its target (#1453)");

    snprintf(leg, sizeof(leg), "%s a2 day row behind the night-only edge is not satisfied", name);
    ExpectBool(leg, satisfiable.contains(kNightDayRow), false,
               "an IS_DAY1 check row passed in a region reached only through an IS_NIGHT2 edge (#1453)");
    snprintf(leg, sizeof(leg), "%s a2 night row behind the night-only edge is satisfied", name);
    ExpectBool(leg, satisfiable.contains(kNightNightRow), true,
               "the region's own Night 2 row failed, so the night-only edge carried nothing");

    snprintf(leg, sizeof(leg), "%s b time-free edge carries every slice", name);
    ExpectMask(leg, MaskOf(states, kFree), TIME_ALL_SLICES, "a time-free edge narrowed its source's mask");
    snprintf(leg, sizeof(leg), "%s b day row behind the time-free edge is satisfied", name);
    ExpectBool(leg, satisfiable.contains(kFreeDayRow), true, "a time-free edge lost the Day 1 slices");

    snprintf(leg, sizeof(leg), "%s c IS_DAY1 || IS_NIGHT2 edge carries both half-days", name);
    ExpectMask(leg, MaskOf(states, kTwo), day1 | night2, "a two-half-day edge did not carry exactly Day 1 and Night 2");
}

void RunCrawl() {
    ArmSave();
    // An entrance nothing in the synthetic graph owns resolves to RR_MAX, which
    // collapses the seed to the root.
    const ReachabilityCrawl crawl = CrawlReachableRegions(ENTRANCE(MILK_ROAD, 0));
    const std::set<RandoCheckId> satisfiable = EvaluateReachableChecks(crawl);
    CheckTraversal("crawl", crawl.reachableRegions, crawl.regionTimeStates, satisfiable);
}

void RunFind() {
    ArmSave();
    std::set<RandoRegionId> reached = { kSrc };
    std::unordered_map<RandoRegionId, RegionTimeState> states;
    states[kSrc] = { .timeSlices = TimeLogic::GetOwnedTimeSlices(), .canStayOverTime = false };
    if (states[kSrc].timeSlices != TIME_ALL_SLICES) {
        TeFail("find: owned time is 0x%llX, not every slice; the clock arming is wrong",
               (unsigned long long)states[kSrc].timeSlices);
    }
    FindReachableRegions(kSrc, reached, states);

    std::set<RandoCheckId> satisfiable;
    for (RandoRegionId region : reached) {
        for (auto& [check, logic] : Regions.at(region).checks) {
            (void)logic;
            if (RowHolds(states, region, check)) {
                satisfiable.insert(check);
            }
        }
    }
    CheckTraversal("find", reached, states, satisfiable);
}

void RunFindStay() {
    ArmSave();
    std::set<RandoRegionId> reached = { kStay };
    std::unordered_map<RandoRegionId, RegionTimeState> states;
    states[kStay] = { .timeSlices = TIME_BIT_ONE << TIME_DAY1_AM_06_00, .canStayOverTime = true };
    FindReachableRegions(kStay, reached, states);

    if (!reached.contains(kStayNight)) {
        TeFail("find d: the staying source's night-only target is not reached, so leg d probes nothing");
    }
    ExpectMask("find d staying source stays to every slice", MaskOf(states, kStay), TIME_ALL_SLICES,
               "ExpandTimeForward did not widen the Day 1 06:00 seed, so leg d does not test a stay");
    ExpectMask("find d IS_NIGHT2 edge out of a staying source carries Night 2 only", MaskOf(states, kStayNight),
               GetHalfDayTimeMask(3), "the night-only edge carried the stayed-to day slices into its target (#1453)");
}

/** The solo glitchless fill's region sweep, as GlitchlessLogic.cpp runs it at
 *  the top of each pass. */
void FillSweep(std::set<RandoRegionId>& regionsInLogic,
               std::unordered_map<RandoRegionId, RegionTimeState>& regionTimeStates) {
    for (RandoRegionId regionId : regionsInLogic) {
        FindReachableRegions(regionId, regionsInLogic, regionTimeStates);
    }
}

void RunFillClockGain() {
    const uint64_t day1 = GetHalfDayTimeMask(0);
    const uint64_t night2 = GetHalfDayTimeMask(3);

    ArmSaveOwning(1u << 3); // Night 2 only
    std::set<RandoRegionId> regionsInLogic = { RR_MAX };
    std::unordered_map<RandoRegionId, RegionTimeState> states = InitializeRegionTimeStates(RR_MAX);
    FillSweep(regionsInLogic, states);
    if (!regionsInLogic.contains(kNight) || MaskOf(states, kNight) != night2) {
        TeFail("fill e: before the clock, NIGHT is %s with mask 0x%llX, not reached at Night 2, so leg e probes "
               "nothing",
               regionsInLogic.contains(kNight) ? "reached" : "unreached", (unsigned long long)MaskOf(states, kNight));
    }

    // The clock: Day 1 is now owned, as GiveItem(ConvertItem(RI_TIME_DAY_1)) leaves it.
    Flags_SetRandoInf(RANDO_INF_OBTAINED_CLOCK_DAY_1);
    ResetRegionTimeStatesForTimeGain(regionsInLogic, states);
    ExpectBool("fill e1 reset leaves only the root in logic",
               regionsInLogic.size() == 1 && regionsInLogic.contains(RR_MAX) && states.size() == 1, true,
               "a region discovered before the clock kept its place or its state, so the sweep cannot rebuild it");
    ExpectMask("fill e1 root takes the new owned time", MaskOf(states, RR_MAX), day1 | night2,
               "the root was not reset to the owned time after the clock, so the gain reaches nothing");

    FillSweep(regionsInLogic, states);
    ExpectMask("fill e2 time-free source takes the gained half-day", MaskOf(states, kSrc), day1 | night2,
               "the rebuilt source did not gain Day 1");
    ExpectMask("fill e2 time-free target takes the gained half-day", MaskOf(states, kFree), day1 | night2,
               "the rebuilt time-free target did not gain Day 1");
    ExpectBool("fill e2 day row behind the time-free edge is satisfied", RowHolds(states, kFree, kFreeDayRow), true,
               "the gained Day 1 did not reach a time-free target");
    ExpectMask("fill e3 IS_NIGHT2 edge still carries Night 2 only after the clock", MaskOf(states, kNight), night2,
               "the clock gain widened the night-only target in place (#1509)");
    ExpectBool("fill e3 day row behind the night-only edge stays unsatisfied", RowHolds(states, kNight, kNightDayRow),
               false, "an IS_DAY1 row passed behind an IS_NIGHT2 edge after a clock gain (#1509)");
}

void RunUnit() {
    ArmSave();
    int calls = 0;
    const std::function<bool()> counted = [&calls] {
        calls++;
        return true;
    };

    const uint64_t night2 = GetHalfDayTimeMask(3);
    calls = 0;
    ExpectMask("u1 one-half-day source is returned as is", NarrowEdgeTime(night2, counted), night2,
               "a source inside one half-day was narrowed");
    ExpectBool("u1 one-half-day source costs no evaluation", calls == 0, true,
               "NarrowEdgeTime re-evaluated an edge whose source sits in one half-day");

    calls = 0;
    ExpectMask("u2 time-free condition keeps every slice", NarrowEdgeTime(TIME_ALL_SLICES, counted), TIME_ALL_SLICES,
               "a time-free condition was narrowed");
    ExpectBool("u2 every slice costs six evaluations", calls == 6, true,
               "NarrowEdgeTime did not evaluate exactly once per spanned half-day");

    const std::function<bool()> acrossHalfDays = [] { return IS_DAY1() && IS_NIGHT2(); };
    const uint64_t day1Night2 = GetHalfDayTimeMask(0) | night2;
    gCurrentRegionTime = day1Night2;
    ExpectMask("u3 no single half-day holds: whole mask kept", NarrowEdgeTime(day1Night2, acrossHalfDays), day1Night2,
               "the fallback closed an edge the whole mask opened");

    gCurrentRegionTime = 0x1234;
    (void)NarrowEdgeTime(TIME_ALL_SLICES, counted);
    ExpectBool("u4 gCurrentRegionTime restored", gCurrentRegionTime == 0x1234, true,
               "NarrowEdgeTime left gCurrentRegionTime restricted");
}

} // namespace

extern "C" int MM_TimeEdges_RunHeadless(void) {
    printf("[TEST] mm-time-edges: a time-gated MM logic edge carries only the half-days its condition holds on, in "
           "the crawl and in the glitchless fill, also after a clock gain (#1453, #1509)\n");
    sFailures = 0;

    // Heap, not stack: the port's SaveContext is tens of kilobytes.
    auto saved = std::make_unique<SaveContext>();
    memcpy(saved.get(), &gSaveContext, sizeof(SaveContext));
    const uint64_t savedTime = Rando::Logic::gCurrentRegionTime;

    std::map<RandoRegionId, Rando::Logic::RandoRegion> graph = BuildGraph();
    std::swap(Rando::Logic::Regions, graph);
    Rando::Logic::InvalidateEntranceRegionCache();

    RunCrawl();
    RunFind();
    RunFindStay();
    RunFillClockGain();
    RunUnit();

    std::swap(Rando::Logic::Regions, graph);
    Rando::Logic::InvalidateEntranceRegionCache();
    memcpy(&gSaveContext, saved.get(), sizeof(SaveContext));
    Rando::Logic::gCurrentRegionTime = savedTime;

    if (sFailures == 0) {
        printf("[TEST] PASS: mm-time-edges\n");
    } else {
        printf("[TEST] FAIL: mm-time-edges: %d leg(s) failed\n", sFailures);
    }
    return sFailures;
}

#endif // RSBS_SINGLE_EXECUTABLE
