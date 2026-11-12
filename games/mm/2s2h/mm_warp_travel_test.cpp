/**
 * @file mm_warp_travel_test.cpp
 * @brief MM's half of the travel-fact locks (#1169 lane XW9, combo_logic.h ABI 10).
 *        CTest row `MMWarpTravelEdges` (label `rando`), test name
 *        `mm-warp-travel-edges`, registered in src/common/test_runner.cpp.
 *
 * WHAT IT LOCKS, over MM's REAL region graph after MM_Rando_InitCore (the
 * mm-logic-gates bring-up; nothing is generated):
 *
 *   E. The root's owl landings (Logic.cpp, RR_MAX's exits): with Ocarina of Time's
 *      Soaring fact (the accessor armed through its test seam) and NO Song of
 *      Soaring in MM, the Stone Tower landing opens for an ACTIVE owl at Day 1
 *      06:00 only: not with the fact absent, not with the owl inactive, not at
 *      Night 1 or Day 2. MM's own Soaring still opens it at night (the control
 *      that the probe evaluates the real lambda), with or without the fact.
 *   N. Under Clock Shuffle owning Day 1 and Day 2, NarrowEdgeTime carries exactly
 *      the Day 1 slices over the fact's edge and both half-days over MM's own
 *      Soaring.
 *   C. The crawl (CrawlReachableRegions, what the coordinator's `expand` runs):
 *      the Stone Tower landing is reached only with the fact and the owl active;
 *      the tower interior holds every owned Day 1 slice the root does under Clock
 *      Shuffle (the soundness argument: the player can leave MM from it at every
 *      slice the landing is credited); and with time not resettable the reset-time
 *      guard prunes the landing.
 *   F. MM's `travelFacts` decision (MmComboTravelFactsNow): Minuet's bit exactly
 *      for the Ocarina of Time, the song owned under its frozen rule and A, C-up,
 *      C-right and C-left; each removed in turn, the rule included, drops it.
 *
 * Restores the save image, gCurrentRegionTime, the frozen record and the seam.
 * Skipped by `--test all` (it rebinds the process's MM rando graph).
 */
#ifdef RSBS_SINGLE_EXECUTABLE

#include "global.h"

#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <set>

#include "Rando/Rando.h"
#include "Rando/Types.h"
#include "Rando/Logic/Logic.h"
#include "Rando/Logic/ResetTimeGuard.h"
#include "Rando/WarpSongsMM.h"

#include "context.h"       // src/common: ComboContext_Init
#include "foreign_items.h" // src/common: the second rules word and its freeze

extern "C" {
#include "variables.h"
}

extern "C" void MM_Rando_InitCore(void);
// Rando/ComboLogicEngineSingleExe.cpp: the accessor's seam and the facts decision.
extern "C" void MM_ComboLogic_TestSoaringTravelSeam(int armed, int value);
extern "C" uint32_t MM_ComboLogic_TestTravelFactsNow(void);

namespace {

int sMwtFailures = 0;

void MwtFail(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    fprintf(stderr, "[MM-WARP-TRAVEL] FAIL: ");
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");
    va_end(args);
    sMwtFailures++;
}

void MwtCheck(bool ok, const char* leg, const char* why) {
    if (!ok) {
        MwtFail("%s: %s", leg, why);
        return;
    }
    printf("[TEST]   ok: %s\n", leg);
}

/** Freeze a paired world whose second rules word is @p rules2. */
void MwtFreezeRules2(uint32_t rules2) {
    ComboContext_Init();
    ComboSettingsRecord rec;
    Combo_ComboSettingsDefaults(&rec);
    Combo_FreezeComboSettingsEx2(&rec, NULL, rules2);
}

/** An empty save: every slot ITEM_NONE, human form, no flags, no options. */
void MwtResetSave() {
    std::memset(&gSaveContext, 0, sizeof(gSaveContext));
    std::memset(gSaveContext.save.saveInfo.inventory.items, ITEM_NONE,
                sizeof(gSaveContext.save.saveInfo.inventory.items));
    gSaveContext.save.playerForm = PLAYER_FORM_HUMAN;
}

void MwtGiveAllButtons() {
    Flags_SetRandoInf(RANDO_INF_OBTAINED_OCARINA_BUTTON_A);
    Flags_SetRandoInf(RANDO_INF_OBTAINED_OCARINA_BUTTON_C_UP);
    Flags_SetRandoInf(RANDO_INF_OBTAINED_OCARINA_BUTTON_C_DOWN);
    Flags_SetRandoInf(RANDO_INF_OBTAINED_OCARINA_BUTTON_C_LEFT);
    Flags_SetRandoInf(RANDO_INF_OBTAINED_OCARINA_BUTTON_C_RIGHT);
}

void MwtGiveSoaring() {
    INV_CONTENT(ITEM_OCARINA_OF_TIME) = ITEM_OCARINA_OF_TIME;
    SET_QUEST_ITEM(QUEST_SONG_SOARING);
    MwtGiveAllButtons();
}

/** Clock Shuffle on, random mode, owning the half-days in @p owned (bit i). */
void MwtArmClocks(unsigned owned) {
    RANDO_SAVE_OPTIONS[RO_CLOCK_SHUFFLE] = RO_GENERIC_ON;
    RANDO_SAVE_OPTIONS[RO_CLOCK_SHUFFLE_PROGRESSIVE] = RO_CLOCK_SHUFFLE_RANDOM;
    for (int i = 0; i < 6; i++) {
        if ((owned & (1u << i)) != 0u) {
            Flags_SetRandoInf((RandoInf)(RANDO_INF_OBTAINED_CLOCK_DAY_1 + i));
        }
    }
}

const s32 kMwtStoneTowerLanding = ENTRANCE(STONE_TOWER, 3);

/** The root's exit to the Stone Tower owl landing, evaluated at @p time; *found
 *  false when the graph has no such exit. */
bool MwtRootOwlExit(uint64_t time, bool* found) {
    auto& root = Rando::Logic::Regions[RR_MAX];
    const auto it = root.exits.find(kMwtStoneTowerLanding);
    if (it == root.exits.end()) {
        *found = false;
        return false;
    }
    *found = true;
    const uint64_t saved = Rando::Logic::gCurrentRegionTime;
    Rando::Logic::gCurrentRegionTime = time;
    const bool open = it->second.condition();
    Rando::Logic::gCurrentRegionTime = saved;
    return open;
}

void MwtExpectExit(const char* leg, uint64_t time, bool expected, const char* why) {
    bool found = false;
    const bool got = MwtRootOwlExit(time, &found);
    if (!found) {
        MwtFail("%s: RR_MAX has no exit to the Stone Tower owl landing, so this leg probes nothing", leg);
        return;
    }
    MwtCheck(got == expected, leg, why);
}

/** Crawl from the arrival; *outTime the landing region's slices (0 unreached). */
bool MwtCrawlReachesLanding(uint64_t* outTime, uint64_t* outInteriorTime, bool guard) {
    Rando::Logic::ReachabilityCrawl crawl = Rando::Logic::CrawlReachableRegions(ENTRANCE(SOUTH_CLOCK_TOWN, 0));
    if (guard) {
        std::set<RandoCheckId> checks = Rando::Logic::EvaluateReachableChecks(crawl);
        (void)Rando::Logic::ApplyResetTimeGuard(crawl, checks);
    }
    const RandoRegionId landing = Rando::Logic::GetRegionIdFromEntrance(kMwtStoneTowerLanding);
    const bool reached = crawl.reachableRegions.count(landing) != 0;
    if (outTime != nullptr) {
        const auto it = crawl.regionTimeStates.find(landing);
        *outTime = (reached && it != crawl.regionTimeStates.end()) ? it->second.timeSlices : 0u;
    }
    if (outInteriorTime != nullptr) {
        const auto it = crawl.regionTimeStates.find(RR_CLOCK_TOWER_INTERIOR);
        *outInteriorTime = it != crawl.regionTimeStates.end() ? it->second.timeSlices : 0u;
    }
    return reached;
}

} // namespace

extern "C" int MM_WarpTravelEdges_RunHeadless(void) {
    printf("[TEST] mm-warp-travel-edges: Ocarina of Time's Soaring fact opens the root's owl landings only for an "
           "active owl on Day 1, MM's own Soaring is unchanged, the crawl and the reset-time guard agree, and MM's "
           "warp-song facts follow can_play_tp_* (#1169 XW9)\n");
    sMwtFailures = 0;
    MM_Rando_InitCore();
    if (gRegEditor == NULL) {
        static RegEditor sMwtRegEditor = {};
        gRegEditor = &sMwtRegEditor;
    }
    auto saved = std::make_unique<SaveContext>();
    std::memcpy(saved.get(), &gSaveContext, sizeof(SaveContext));
    const uint64_t savedTime = Rando::Logic::gCurrentRegionTime;
    using Rando::Logic::TIME_BIT_ONE;
    const uint64_t kDawn1 = TIME_BIT_ONE << Rando::Logic::TIME_DAY1_AM_06_00;
    const uint64_t kNight1 = TIME_BIT_ONE << Rando::Logic::TIME_NIGHT1_PM_06_00;
    const uint64_t kDawn2 = TIME_BIT_ONE << Rando::Logic::TIME_DAY2_AM_06_00;
    const uint64_t kDay1 = Rando::Logic::GetHalfDayTimeMask(0);
    const uint64_t kDay2 = Rando::Logic::GetHalfDayTimeMask(2);
    MwtFreezeRules2(RSBS_XITEM2_SONG_SOARING_OOT | RSBS_XITEM2_SONG_MINUET_MM);

    // ---- E. the root's owl landing -------------------------------------------------
    MwtResetSave();
    SET_OWL_WARP(OWL_WARP_STONE_TOWER);
    MM_ComboLogic_TestSoaringTravelSeam(0, 0);
    MwtExpectExit("E1 owl active, no fact, no Soaring", kDawn1, false,
                  "the landing opened with neither MM's Soaring nor OoT's fact (outside a round the fact is false)");
    MM_ComboLogic_TestSoaringTravelSeam(1, 1);
    MwtExpectExit("E2 owl active, OoT's fact, Day 1 06:00", kDawn1, true,
                  "OoT's Soaring fact must open an active owl's landing on Day 1 (OoTMM's mmTime |= 1)");
    MwtExpectExit("E3 owl active, OoT's fact, Night 1", kNight1, false,
                  "OoT's Soaring fact credited a Night 1 landing: MM arrival is credited on Day 1 only");
    MwtExpectExit("E4 owl active, OoT's fact, Day 2", kDawn2, false,
                  "OoT's Soaring fact credited a Day 2 landing: MM arrival is credited on Day 1 only");
    CLEAR_OWL_WARP(OWL_WARP_STONE_TOWER);
    MwtExpectExit("E5 owl inactive, OoT's fact", kDawn1, false,
                  "OoT's Soaring landed on an owl MM's frozen copy never activated");
    SET_OWL_WARP(OWL_WARP_STONE_TOWER);
    MwtGiveSoaring();
    MM_ComboLogic_TestSoaringTravelSeam(0, 0);
    MwtExpectExit("E6 control: MM's own Soaring at night, no fact", kNight1, true,
                  "MM's own Song of Soaring no longer opens the landing, so E1-E5 probe a broken edge");
    MM_ComboLogic_TestSoaringTravelSeam(1, 1);
    MwtExpectExit("E7 MM's own Soaring at night, with the fact", kNight1, true,
                  "the fact closed an edge MM's own Soaring opens (it may only add)");

    // ---- N. Clock Shuffle: the slices the edge carries ------------------------------
    MwtResetSave();
    SET_OWL_WARP(OWL_WARP_STONE_TOWER);
    MwtArmClocks(0x05u); // Day 1 and Day 2
    {
        auto& root = Rando::Logic::Regions[RR_MAX];
        const auto it = root.exits.find(kMwtStoneTowerLanding);
        if (it == root.exits.end()) {
            MwtFail("N: RR_MAX has no exit to the Stone Tower owl landing");
        } else {
            MM_ComboLogic_TestSoaringTravelSeam(1, 1);
            const uint64_t viaFact = Rando::Logic::NarrowEdgeTime(kDay1 | kDay2, it->second.condition);
            MwtCheck(viaFact == kDay1, "N1 the fact's edge carries exactly the owned Day 1 slices",
                     "under Clock Shuffle the fact's edge must carry every Day 1 slice and no Day 2 slice");
            MwtGiveSoaring();
            const uint64_t viaSoaring = Rando::Logic::NarrowEdgeTime(kDay1 | kDay2, it->second.condition);
            MwtCheck(viaSoaring == (kDay1 | kDay2), "N2 control: MM's own Soaring carries both half-days",
                     "MM's own Soaring edge was narrowed by the fact's Day 1 term");
        }
    }

    // ---- C. the crawl ----------------------------------------------------------------
    MwtResetSave();
    SET_OWL_WARP(OWL_WARP_STONE_TOWER);
    MM_ComboLogic_TestSoaringTravelSeam(0, 0);
    uint64_t landingTime = 0;
    uint64_t interiorTime = 0;
    MwtCheck(!MwtCrawlReachesLanding(nullptr, nullptr, false), "C1 no fact: the crawl does not reach the landing",
             "an empty save walked to Stone Tower, so C2 proves nothing");
    MM_ComboLogic_TestSoaringTravelSeam(1, 1);
    const bool c2 = MwtCrawlReachesLanding(&landingTime, nullptr, false);
    MwtCheck(c2 && (landingTime & kDawn1) != 0u, "C2 the fact: the crawl reaches the landing at Day 1 06:00",
             "the crawl did not carry OoT's Soaring to an active owl");
    CLEAR_OWL_WARP(OWL_WARP_STONE_TOWER);
    MwtCheck(!MwtCrawlReachesLanding(nullptr, nullptr, false), "C3 owl inactive: not reached",
             "the crawl landed on an inactive owl");
    SET_OWL_WARP(OWL_WARP_STONE_TOWER);
    MwtCheck(!Rando::Logic::MmCanResetTime(), "C4 precondition: an empty save cannot reset time",
             "the default frozen moon crash is New Cycle, so the guard leg below proves nothing");
    MwtCheck(!MwtCrawlReachesLanding(nullptr, nullptr, true), "C4 the reset-time guard prunes the landing",
             "with time not resettable the guard kept an owl landing outside the nook");
    MwtArmClocks(0x05u);
    const bool c5 = MwtCrawlReachesLanding(&landingTime, &interiorTime, false);
    MwtCheck(c5 && (landingTime & kDay1) != 0u && (interiorTime & kDay1) == kDay1,
             "C5 Clock Shuffle: the landing is reached on Day 1 and the interior holds every owned Day 1 slice",
             "the landing was credited a Day 1 slice at which the player cannot stand in the tower interior (the "
             "soundness argument of ADR 0010's travel-facts amendment)");

    // ---- F. MM's travel facts ----------------------------------------------------------
    MM_ComboLogic_TestSoaringTravelSeam(0, 0);
    MwtResetSave();
    INV_CONTENT(ITEM_OCARINA_OF_TIME) = ITEM_OCARINA_OF_TIME;
    MwtGiveAllButtons();
    MM_WarpSongMM_Give((uint16_t)RI_SONG_MINUET);
    MwtCheck(MM_ComboLogic_TestTravelFactsNow() == RSBS_XITEM2_SONG_MINUET_MM,
             "F1 Minuet owned, its rule on, the Ocarina and every button: exactly Minuet's bit",
             "MM's travel facts are not can_play_tp_forest");
    INV_CONTENT(ITEM_OCARINA_OF_TIME) = ITEM_NONE;
    MwtCheck(MM_ComboLogic_TestTravelFactsNow() == 0u, "F2 no Ocarina: no fact", "a fact without the Ocarina");
    INV_CONTENT(ITEM_OCARINA_OF_TIME) = ITEM_OCARINA_OF_TIME;
    static const RandoInf kMinuetButtons[4] = {
        RANDO_INF_OBTAINED_OCARINA_BUTTON_A,
        RANDO_INF_OBTAINED_OCARINA_BUTTON_C_UP,
        RANDO_INF_OBTAINED_OCARINA_BUTTON_C_RIGHT,
        RANDO_INF_OBTAINED_OCARINA_BUTTON_C_LEFT,
    };
    for (const RandoInf button : kMinuetButtons) {
        Flags_ClearRandoInf(button);
        if (MM_ComboLogic_TestTravelFactsNow() != 0u) {
            MwtFail("F3 a Minuet note's button removed: the fact remained (Ocarina Button Shuffle must apply)");
        }
        Flags_SetRandoInf(button);
    }
    printf("[TEST]   F3 each of Minuet's four buttons is required\n");
    const uint8_t owned = MM_WarpSongsMM_SnapshotOwned();
    MM_WarpSongsMM_RestoreOwned(0u);
    MwtCheck(MM_ComboLogic_TestTravelFactsNow() == 0u, "F4 not owned: no fact", "a fact for a song MM does not own");
    MM_WarpSongsMM_RestoreOwned(owned);
    MwtFreezeRules2(RSBS_XITEM2_SONG_SOARING_OOT);
    MM_WarpSongMM_Give((uint16_t)RI_SONG_MINUET);
    MwtCheck(MM_ComboLogic_TestTravelFactsNow() == 0u, "F5 Minuet's rule off: no fact",
             "a fact for a song whose frozen rule is off (every world a player can make today)");

    MM_ComboLogic_TestSoaringTravelSeam(0, 0);
    ComboContext_Init();
    Rando::Logic::gCurrentRegionTime = savedTime;
    std::memcpy(&gSaveContext, saved.get(), sizeof(SaveContext));
    printf("[TEST] %s: mm-warp-travel-edges (%d failure(s))\n", sMwtFailures == 0 ? "PASS" : "FAIL", sMwtFailures);
    return sMwtFailures;
}

#endif // RSBS_SINGLE_EXECUTABLE
