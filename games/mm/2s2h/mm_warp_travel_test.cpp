/**
 * @file mm_warp_travel_test.cpp
 * @brief MM's half of the travel-fact locks (#1169 lane XW9, combo_logic.h ABI 10).
 *        CTest row `MMWarpTravelEdges` (label `rando`), test name
 *        `mm-warp-travel-edges`, registered in src/common/test_runner.cpp; and,
 *        at the end of the file, CTest row `ComboWarpTravelRounds` (label
 *        `rando`, one paired generation), test name `combo-warp-travel-rounds`:
 *        the facts through a coordinator round over both real engines.
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
#include <map>
#include <memory>
#include <set>
#include <utility>
#include <vector>

#include "Rando/Rando.h"
#include "Rando/Types.h"
#include "Rando/Logic/Logic.h"
#include "Rando/Logic/ResetTimeGuard.h"
#include "Rando/WarpSongsMM.h"

#include <libultraship/bridge/consolevariablebridge.h>

#include "combo_logic.h"         // src/common: Combo_Logic_RunRound and the placement tables
#include "combo_single_bag.h"    // src/common: the creation's commit (Combo_SingleBag_Forget)
#include "context.h"             // src/common: ComboContext_Init
#include "foreign_items.h"       // src/common: the second rules word and its freeze
#include "combo_settings_view.h" // src/common: the warp-song keys (#1169 XW12, combo-warp-song-world)
#include "mm_warp_song_play.h"   // src/common: the creation's warp-song record
#include "shared_items.h"        // src/common: RSBS_FILL_ARM_SONGS_ROAM (D5)

extern "C" {
#include "variables.h"
}

extern "C" void MM_Rando_InitCore(void);
// Rando/ComboLogicEngineSingleExe.cpp: the accessor's seam and the facts decision.
extern "C" void MM_ComboLogic_TestSoaringTravelSeam(int armed, int value);
extern "C" uint32_t MM_ComboLogic_TestTravelFactsNow(void);
extern "C" int MM_ComboLogic_TestSoaringTravelHits(int reset);
extern "C" {
// combo-warp-travel-rounds: the paired creation (3drando/menu.cpp, mm_rando_gen_test.cpp)
// and the OoT engine's bridges (ComboLogicEngineOoT.cpp).
int Rando_HeadlessSeedTest(const char* seedStr);
int OoT_ComboLogic_GeneralPassDeferred(void);
int MM_Rando_HeadlessPairedHalf(void);
uint32_t OoT_ComboLogic_TestTravelEdgeSeen(int reset);
uint16_t OoT_ComboLogic_TestTravelItemId(int which);
int OoT_ComboLogic_TestHostsHolding(uint16_t rg, uint16_t* out, int cap);
// #1169 XW12 (T6): close OoT's crossing for the rounds that follow (ComboLogicEngineOoT.cpp).
int OoT_ComboLogic_TestCloseCrossing(int on);
// #1169 XW12 (combo-warp-song-world): OoT's side of the creation tail
// (ForeignItemsSingleExe.cpp) and the confinement bits OoT's settings arm
// (ComboLogicEngineOoT.cpp).
int OoT_Creation_FinishPairedHalf(int writeSpoiler);
uint32_t OoT_ComboLogic_ConfinementArmed(void);
}

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
    // Only "includes Day 1 06:00" is observable here, not "exactly Day 1 06:00": the
    // crawl stores a region's slices AFTER ExpandTimeForward (a region that can stay
    // over time carries every later slice its stay restrictions allow) and after the
    // join with every region that leads back into it. What the edge itself carries
    // is pinned by E2-E4 (open at Day 1 06:00, closed at Night 1 and Day 2) and N1
    // (exactly the owned Day 1 slices); without Clock Shuffle the root is seeded with
    // Day 1 06:00 alone (InitializeRegionTimeStates), so that is the carry.
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

// ============================================================================
// combo-warp-travel-rounds (rando tier, one paired generation; #1169 XW9)
// ============================================================================
//
// The travel facts through Combo_Logic_RunRound over the two REAL engines, on a
// real paired world. The world is created with every warp-song rule off (what
// Generate accepts today); the rules are then frozen on by writing the frozen
// second word directly (Combo_ComboItemRules2Store over gComboCtx, the only
// writer), never through Generate, and put back after every round. Each round
// assumes the two songs the world does not hold (OoT's Song of Soaring and MM's
// Minuet of Forest) and forbids every host holding the NATIVE song that would
// short-circuit the edge (OoT's Minuet of Forest, MM's Song of Soaring), so the
// root exits can only open through a delivered fact.
//
//   S  the created world's exit round proves the goal with both crossings open
//      and credits nothing; the native songs are on hosts (not starting items),
//      so withholding them is real (vacuity guards).
//   T1 rules on, both songs assumed: MM's Minuet fact reaches OoT (travelToOoT ==
//      Minuet's bit), OoT's Soaring reaches MM (travelToMM == the Soaring bit),
//      and each engine's NEXT expand read its delivered fact: OoT's search found
//      Minuet's root exit open through it (and no other song's), MM's crawl read
//      the Soaring fact at the root's owl landings. Catches: a fact not read from
//      the round's own search or save, an accept the next expand never sees, the
//      round's delivery removed.
//   T2 each rule alone: only its own direction is credited. Catches the rule bits
//      wired to the wrong engine or song.
//   T3 rules off, both songs assumed: nothing credited, no edge read. Catches a
//      fact not behind its frozen rule (every world a player can make today).
//   T4 rules on, no song assumed, against the same round with the rules off:
//      every result field equal and nothing credited. Catches the rule bits alone
//      costing an alternation or moving any answer.
//
//   T5 (#1169 XW12) MM's crossing CLOSED (graph surgery: the one authored edge
//      into the Clock Tower's interior removed), both rules on, both songs
//      assumed: OoT's crossing open, MM's closed; Minuet still reaches OoT
//      (only OoT's crossing gates it) and Soaring does not reach MM. Vacuity:
//      OoT's raw Soaring fact WAS reported (Combo_Logic_TestLastTravelFacts), so
//      the gate, not a missing fact, dropped it. Catches the crossMM term dropped
//      from the Soaring gate (combo_logic.c, Combo_Logic_TravelGate).
//   T6 (#1169 XW12) OoT's crossing CLOSED (the Market's exit to the Happy Mask
//      Shop held false, OoT_ComboLogic_TestCloseCrossing), the same assumptions:
//      nothing credited in either direction, no edge read. Vacuity: both raw
//      facts were reported and MM's crossing stayed open, so T6 alone catches
//      the crossOoT term dropped from EITHER gate.
//   T5b, T6b the same closures with the rules off: nothing credited (a closure
//      that changes the rules-off answer is caught).
//
// THE TRAVEL GATES' closed-crossing behaviour is also combo-logic-travel-facts'
// stub legs T2 and T3; T5 and T6 are the same claims over the two real engines.

namespace {

int sMwrFailures = 0;

void MwrCheck(bool ok, const char* leg, const char* why) {
    if (!ok) {
        sMwrFailures++;
        fprintf(stderr, "[COMBO-WARP-TRAVEL-ROUNDS] FAIL: %s: %s\n", leg, why);
        return;
    }
    printf("[TEST]   ok: %s\n", leg);
}

/** One round over the created world's tables with the frozen second word set to
 *  @p rules2 for the round only (put back to @p restore2 afterwards). */
int MwrRound(uint32_t rules2, uint32_t restore2, const std::vector<ComboLogicBagItem>& assumed,
             const std::vector<ComboLogicHostRef>& forbidden, ComboLogicRoundResult* out) {
    Combo_ComboItemRules2Store(&gComboCtx.comboItemRules2, rules2);
    ComboLogicRoundRequest req;
    std::memset(&req, 0, sizeof(req));
    req.assumed = assumed.empty() ? nullptr : assumed.data();
    req.assumedCount = (int)assumed.size();
    req.goal = gComboCtx.comboSettings.goal;
    req.forbidden = forbidden.empty() ? nullptr : forbidden.data();
    req.forbiddenCount = (int)forbidden.size();
    std::memset(out, 0, sizeof(*out));
    const int status = Combo_Logic_RunRound(&req, out);
    Combo_ComboItemRules2Store(&gComboCtx.comboItemRules2, restore2);
    return status;
}

/** Field by field, never memcmp. */
bool MwrSameResult(const ComboLogicRoundResult& a, const ComboLogicRoundResult& b) {
    return a.status == b.status && a.iterations == b.iterations && a.crossingOpenOoT == b.crossingOpenOoT &&
           a.crossingOpenMM == b.crossingOpenMM && a.goalOoT == b.goalOoT && a.goalMM == b.goalMM &&
           a.goalExpression == b.goalExpression && a.candidatesOoT == b.candidatesOoT &&
           a.candidatesMM == b.candidatesMM && a.exchanged == b.exchanged && a.allHostsReached == b.allHostsReached &&
           a.triforcePiecesOoT == b.triforcePiecesOoT && a.triforcePiecesMM == b.triforcePiecesMM &&
           a.triforcePieces == b.triforcePieces && a.allLocationsReached == b.allLocationsReached &&
           a.locationsUnreached == b.locationsUnreached && a.travelToOoT == b.travelToOoT &&
           a.travelToMM == b.travelToMM;
}

void MwrPrint(const char* leg, const ComboLogicRoundResult& r) {
    printf("[TEST] combo-warp-travel-rounds: %s: status %d, iterations %d, crossings OoT %d MM %d, goal %d, "
           "candidates %d/%d, travel to OoT %02X, to MM %02X, OoT edges seen %02X, MM Soaring reads %d\n",
           leg, r.status, r.iterations, r.crossingOpenOoT, r.crossingOpenMM, r.goalExpression, r.candidatesOoT,
           r.candidatesMM, (unsigned)r.travelToOoT, (unsigned)r.travelToMM,
           (unsigned)OoT_ComboLogic_TestTravelEdgeSeen(0), MM_ComboLogic_TestSoaringTravelHits(0));
}

/** The frozen word put back, the probes cleared, OoT's crossing reopened and the
 *  created world forgotten however the row ends. */
struct MwrRestore {
    uint32_t word = 0u;
    ~MwrRestore() {
        Combo_ComboItemRules2Store(&gComboCtx.comboItemRules2, word);
        (void)OoT_ComboLogic_TestTravelEdgeSeen(1);
        (void)MM_ComboLogic_TestSoaringTravelHits(1);
        (void)OoT_ComboLogic_TestCloseCrossing(0);
        Combo_SingleBag_Forget();
    }
};

/** T5's surgery (#1169 XW12): one MM region's exits, put back however the scope is
 *  left (mm_combo_logic_engine_test.cpp's RegionExitsGuard, under this row's name). */
struct MwrRegionExitsGuard {
    RandoRegionId regionId;
    std::map<s32, Rando::Logic::RandoRegionExit> saved;
    explicit MwrRegionExitsGuard(RandoRegionId id) : regionId(id), saved(Rando::Logic::Regions[id].exits) {
    }
    ~MwrRegionExitsGuard() {
        Rando::Logic::Regions[regionId].exits = saved;
    }
    MwrRegionExitsGuard(const MwrRegionExitsGuard&) = delete;
    MwrRegionExitsGuard& operator=(const MwrRegionExitsGuard&) = delete;
};

} // namespace

extern "C" int MM_WarpTravelRounds_RunHeadless(void) {
    printf("[TEST] combo-warp-travel-rounds: over a real paired world and both real engines, MM's Minuet of Forest "
           "reaches OoT's root exit and OoT's Song of Soaring reaches MM's owl landings through a coordinator round "
           "only with their frozen rules on, and the rule bits alone change no answer (#1169 XW9)\n");
    sMwrFailures = 0;
    MwrRestore restore;
    // Before this row's generation places anything: a previous world is committed,
    // never rolled back over this one (mm-warp-songs-pooled-only-when-rule-on).
    Combo_SingleBag_Forget();
    ComboContext_Init();
    if (Rando_HeadlessSeedTest("RSBSXW9TRAVEL1") != 0 || !Combo_ForeignPairingActive() ||
        !Combo_ComboSettingsFrozen() || !OoT_ComboLogic_GeneralPassDeferred()) {
        fprintf(stderr, "[COMBO-WARP-TRAVEL-ROUNDS] FAIL: the paired OoT generation did not leave a frozen paired "
                        "world at its general pass\n");
        return 1;
    }
    if (MM_Rando_HeadlessPairedHalf() != 0) {
        fprintf(stderr, "[COMBO-WARP-TRAVEL-ROUNDS] FAIL: MM's creation-time half did not produce a rando world\n");
        return 1;
    }
    restore.word = Combo_ComboItemRules2Word(&gComboCtx.comboItemRules2);
    const uint32_t created = restore.word;
    MwrCheck(created == 0u, "S1 the world was created with every warp-song rule off",
             "the creation froze a second rules word, so the control rounds are not rules-off worlds");
    (void)OoT_ComboLogic_TestTravelEdgeSeen(1);
    (void)MM_ComboLogic_TestSoaringTravelHits(1);

    const std::vector<ComboLogicBagItem> none;
    const std::vector<ComboLogicHostRef> nothingForbidden;
    ComboLogicRoundResult r0;
    const int s0 = MwrRound(0u, created, none, nothingForbidden, &r0);
    MwrPrint("S2 rules off, nothing assumed", r0);
    MwrCheck(s0 == RSBS_COMBO_LOGIC_OK && r0.goalExpression == 1 && r0.crossingOpenOoT == 1 && r0.crossingOpenMM == 1,
             "S2 the created world's exit round proves the goal with both crossings open",
             "the created world's exit round did not run, prove the goal or open both crossings");
    MwrCheck(r0.travelToOoT == 0u && r0.travelToMM == 0u, "S2 rules off: nothing credited",
             "a rules-off round credited travel");

    // The hosts holding the native songs, in both games and on both tables.
    const uint16_t ootSoaring = OoT_ComboLogic_TestTravelItemId(0);
    const uint16_t ootMinuet = OoT_ComboLogic_TestTravelItemId(1);
    std::set<std::pair<uint8_t, uint16_t>> forbid;
    uint16_t hosts[64];
    const int ootMinuetHosts = OoT_ComboLogic_TestHostsHolding(ootMinuet, hosts, 64);
    for (int i = 0; i < ootMinuetHosts && i < 64; ++i) {
        forbid.insert({ (uint8_t)GAME_OOT, hosts[i] });
    }
    int mmSoaringHosts = 0;
    for (int rc = (int)RC_UNKNOWN + 1; rc < (int)RC_MAX; ++rc) {
        if (RANDO_SAVE_CHECKS[rc].randoItemId == RI_SONG_SOARING) {
            forbid.insert({ (uint8_t)GAME_MM, (uint16_t)rc });
            mmSoaringHosts++;
        }
    }
    int onTables = 0;
    // OoT's Minuet where the single-bag fill put it on the coordinator's tables: in
    // either game, since a native song may cross (it does in this row's world since
    // #1404's settings moved it). OoT's own table of locations
    // (OoT_ComboLogic_TestHostsHolding) answers 0 for a copy that crossed.
    int ootMinuetOnTables = 0;
    ComboLogicPlacement p;
    for (int g = 0; g < 2; ++g) {
        const GameId host = g == 0 ? GAME_OOT : GAME_MM;
        for (int i = 0; i < Combo_Logic_PlacementCount(host); ++i) {
            if (!Combo_Logic_PlacementAt(host, i, &p)) {
                continue;
            }
            const bool nativeMinuet = p.item.originGame == (uint8_t)GAME_OOT && p.item.id == ootMinuet;
            const bool nativeSoaring = p.item.originGame == (uint8_t)GAME_MM && p.item.id == (uint16_t)RI_SONG_SOARING;
            if (nativeMinuet || nativeSoaring) {
                forbid.insert({ (uint8_t)host, p.hostCheck });
                onTables++;
                ootMinuetOnTables += nativeMinuet ? 1 : 0;
                mmSoaringHosts += (nativeSoaring && host == GAME_OOT) ? 1 : 0;
            }
        }
    }
    std::vector<ComboLogicHostRef> forbidden;
    for (const auto& h : forbid) {
        ComboLogicHostRef ref;
        ref.hostGame = h.first;
        ref.hostCheck = h.second;
        forbidden.push_back(ref);
    }
    printf("[TEST] combo-warp-travel-rounds: forbidding %d host(s): OoT's Minuet of Forest on %d OoT location(s) and "
           "%d coordinator placement(s), MM's Song of Soaring on %d host(s), %d native song placement(s) on the "
           "coordinator's tables\n",
           (int)forbidden.size(), ootMinuetHosts, ootMinuetOnTables, mmSoaringHosts, onTables);
    // Vacuity guards: a song the player STARTS with cannot be withheld, and then
    // the native disjunct short-circuits the edge this row probes. Where the fill
    // put the song is the seed's business: an OoT location, or a host in either game
    // on the coordinator's tables; every one of them is forbidden above.
    MwrCheck(ootSoaring != 0u && ootMinuet != 0u, "S3 the OoT bridge names both songs", "an OoT item id is 0");
    MwrCheck(ootMinuetHosts + ootMinuetOnTables >= 1,
             "S3 OoT's Minuet of Forest is on a host in one of the games (not a starting item)",
             "OoT's Minuet of Forest is on no OoT location and no coordinator placement, so its native disjunct "
             "cannot be withheld");
    MwrCheck(mmSoaringHosts >= 1 && !CHECK_QUEST_ITEM(QUEST_SONG_SOARING),
             "S3 MM's Song of Soaring is on a host and not in the starting save",
             "MM's Song of Soaring is a starting item or on no host, so MM's own Soaring short-circuits the landing");

    std::vector<ComboLogicBagItem> songs(2);
    std::memset(songs.data(), 0, sizeof(ComboLogicBagItem) * songs.size());
    songs[0].item.originGame = (uint8_t)GAME_OOT;
    songs[0].item.id = ootSoaring;
    songs[1].item.originGame = (uint8_t)GAME_MM;
    songs[1].item.id = (uint16_t)RI_SONG_MINUET;
    const uint32_t both = RSBS_XITEM2_SONG_SOARING_OOT | RSBS_XITEM2_SONG_MINUET_MM;

    // ---- T1 ----------------------------------------------------------------------
    (void)OoT_ComboLogic_TestTravelEdgeSeen(1);
    (void)MM_ComboLogic_TestSoaringTravelHits(1);
    ComboLogicRoundResult r1;
    const int s1 = MwrRound(both, created, songs, forbidden, &r1);
    MwrPrint("T1 both rules on, both songs assumed", r1);
    const uint32_t seen1 = OoT_ComboLogic_TestTravelEdgeSeen(1);
    const int hits1 = MM_ComboLogic_TestSoaringTravelHits(1);
    MwrCheck(s1 == RSBS_COMBO_LOGIC_OK && r1.crossingOpenOoT == 1 && r1.crossingOpenMM == 1,
             "T1 the round ran with both crossings open", "the round failed or a crossing stayed closed");
    MwrCheck(r1.travelToOoT == RSBS_XITEM2_SONG_MINUET_MM, "T1 MM's Minuet of Forest is credited to OoT",
             "travelToOoT is not exactly Minuet's bit: MM's fact was not read from the round's save, or not handed "
             "across with OoT's crossing open");
    MwrCheck(r1.travelToMM == RSBS_COMBO_TRAVEL_INTO_MM, "T1 OoT's Song of Soaring is credited to MM",
             "travelToMM is not the Soaring bit: OoT's fact was not read from the round's own search, or not handed "
             "across with both crossings open");
    MwrCheck(seen1 == 0x01u, "T1 OoT's next search opened Minuet's root exit through the delivered fact",
             "OoT's search never found Minuet's root exit open through the fact (the accept never reached the next "
             "expand), or found another song's open");
    MwrCheck(hits1 > 0, "T1 MM's next crawl read the delivered Soaring fact at the root's owl landings",
             "MM's crawl never read the Soaring fact (the accept never reached the next expand)");

    // ---- T2 ----------------------------------------------------------------------
    ComboLogicRoundResult r2a;
    const int s2a = MwrRound(RSBS_XITEM2_SONG_SOARING_OOT, created, songs, forbidden, &r2a);
    MwrPrint("T2 Soaring's rule alone", r2a);
    const uint32_t seen2a = OoT_ComboLogic_TestTravelEdgeSeen(1);
    (void)MM_ComboLogic_TestSoaringTravelHits(1);
    MwrCheck(s2a == RSBS_COMBO_LOGIC_OK && r2a.travelToMM == RSBS_COMBO_TRAVEL_INTO_MM && r2a.travelToOoT == 0u &&
                 seen2a == 0u,
             "T2 Soaring's rule alone credits only Soaring",
             "with Minuet's rule off MM's Minuet was credited, or Soaring's rule did not credit Soaring");
    ComboLogicRoundResult r2b;
    const int s2b = MwrRound(RSBS_XITEM2_SONG_MINUET_MM, created, songs, forbidden, &r2b);
    MwrPrint("T2 Minuet's rule alone", r2b);
    (void)OoT_ComboLogic_TestTravelEdgeSeen(1);
    const int hits2b = MM_ComboLogic_TestSoaringTravelHits(1);
    MwrCheck(s2b == RSBS_COMBO_LOGIC_OK && r2b.travelToOoT == RSBS_XITEM2_SONG_MINUET_MM && r2b.travelToMM == 0u &&
                 hits2b == 0,
             "T2 Minuet's rule alone credits only Minuet",
             "with Soaring's rule off OoT's Soaring was credited, or Minuet's rule did not credit Minuet");

    // ---- T3 ----------------------------------------------------------------------
    ComboLogicRoundResult r3;
    const int s3 = MwrRound(0u, created, songs, forbidden, &r3);
    MwrPrint("T3 rules off, both songs assumed", r3);
    const uint32_t seen3 = OoT_ComboLogic_TestTravelEdgeSeen(1);
    const int hits3 = MM_ComboLogic_TestSoaringTravelHits(1);
    MwrCheck(s3 == RSBS_COMBO_LOGIC_OK && r3.travelToOoT == 0u && r3.travelToMM == 0u && seen3 == 0u && hits3 == 0,
             "T3 rules off: nothing credited and no edge read",
             "a travel fact was credited or read with every warp-song rule off");

    // ---- T4 ----------------------------------------------------------------------
    ComboLogicRoundResult r4off;
    ComboLogicRoundResult r4on;
    const int s4off = MwrRound(0u, created, none, forbidden, &r4off);
    const int s4on = MwrRound(both, created, none, forbidden, &r4on);
    MwrPrint("T4 rules off, no song", r4off);
    MwrPrint("T4 rules on, no song", r4on);
    const uint32_t seen4 = OoT_ComboLogic_TestTravelEdgeSeen(1);
    const int hits4 = MM_ComboLogic_TestSoaringTravelHits(1);
    MwrCheck(s4off == RSBS_COMBO_LOGIC_OK && s4on == RSBS_COMBO_LOGIC_OK && MwrSameResult(r4off, r4on) &&
                 r4on.travelToOoT == 0u && r4on.travelToMM == 0u && seen4 == 0u && hits4 == 0,
             "T4 the rule bits alone change no field of the round",
             "with no song to play, freezing the rules on changed the round's answer or cost");

    // ---- T5: MM's crossing closed (#1169 XW12) -----------------------------------
    {
        MwrRegionExitsGuard southGuard(RR_CLOCK_TOWN_SOUTH);
        const size_t erased = Rando::Logic::Regions[RR_CLOCK_TOWN_SOUTH].exits.erase(ENTRANCE(CLOCK_TOWER_INTERIOR, 1));
        MwrCheck(erased == 1, "T5 the surgery removed MM's one crossing edge",
                 "RR_CLOCK_TOWN_SOUTH carries no EXIT to ENTRANCE(CLOCK_TOWER_INTERIOR, 1), so T5 cannot close MM's "
                 "crossing");
        (void)OoT_ComboLogic_TestTravelEdgeSeen(1);
        (void)MM_ComboLogic_TestSoaringTravelHits(1);
        ComboLogicRoundResult r5;
        const int s5 = MwrRound(both, created, songs, forbidden, &r5);
        MwrPrint("T5 MM's crossing closed, both rules on, both songs assumed", r5);
        uint32_t factsOoT5 = 0u;
        uint32_t factsMM5 = 0u;
        Combo_Logic_TestLastTravelFacts(&factsOoT5, &factsMM5);
        const uint32_t seen5 = OoT_ComboLogic_TestTravelEdgeSeen(1);
        const int hits5 = MM_ComboLogic_TestSoaringTravelHits(1);
        printf("[TEST] combo-warp-travel-rounds: T5 raw facts OoT %02X MM %02X\n", (unsigned)factsOoT5,
               (unsigned)factsMM5);
        MwrCheck(s5 == RSBS_COMBO_LOGIC_OK && r5.crossingOpenMM == 0 && r5.crossingOpenOoT == 1,
                 "T5 the round ran with OoT's crossing open and MM's closed",
                 "the round failed, or the surgery did not close MM's crossing alone");
        MwrCheck((factsOoT5 & RSBS_COMBO_TRAVEL_INTO_MM) != 0u, "T5 OoT reported its Soaring fact (vacuity guard)",
                 "OoT's raw Soaring fact was 0, so T5 cannot tell the closed gate from a missing fact");
        MwrCheck(r5.travelToMM == 0u && hits5 == 0, "T5 Soaring does not reach MM with MM's crossing closed",
                 "THE SOARING GATE IGNORED MM'S CROSSING: OoT's Soaring was handed to MM, or MM's crawl read it");
        MwrCheck(r5.travelToOoT == RSBS_XITEM2_SONG_MINUET_MM && seen5 == 0x01u,
                 "T5 Minuet still reaches OoT (only OoT's crossing gates it)",
                 "MM's Minuet was not credited, or OoT's search did not open its root exit, with OoT's crossing open");
        ComboLogicRoundResult r5b;
        const int s5b = MwrRound(0u, created, songs, forbidden, &r5b);
        MwrPrint("T5b MM's crossing closed, rules off", r5b);
        const uint32_t seen5b = OoT_ComboLogic_TestTravelEdgeSeen(1);
        const int hits5b = MM_ComboLogic_TestSoaringTravelHits(1);
        MwrCheck(s5b == RSBS_COMBO_LOGIC_OK && r5b.travelToOoT == 0u && r5b.travelToMM == 0u && seen5b == 0u &&
                     hits5b == 0,
                 "T5b rules off with MM's crossing closed: nothing credited",
                 "a closed MM crossing credited travel with every warp-song rule off");
    }

    // ---- T6: OoT's crossing closed (#1169 XW12) ----------------------------------
    {
        MwrCheck(OoT_ComboLogic_TestCloseCrossing(1) == 1, "T6 the Market's exit to the Happy Mask Shop is held false",
                 "OoT's RR_THE_MARKET has no exit to RR_MARKET_MASK_SHOP, so T6 cannot close OoT's crossing");
        (void)OoT_ComboLogic_TestTravelEdgeSeen(1);
        (void)MM_ComboLogic_TestSoaringTravelHits(1);
        ComboLogicRoundResult r6;
        const int s6 = MwrRound(both, created, songs, forbidden, &r6);
        MwrPrint("T6 OoT's crossing closed, both rules on, both songs assumed", r6);
        uint32_t factsOoT6 = 0u;
        uint32_t factsMM6 = 0u;
        Combo_Logic_TestLastTravelFacts(&factsOoT6, &factsMM6);
        const uint32_t seen6 = OoT_ComboLogic_TestTravelEdgeSeen(1);
        const int hits6 = MM_ComboLogic_TestSoaringTravelHits(1);
        printf("[TEST] combo-warp-travel-rounds: T6 raw facts OoT %02X MM %02X\n", (unsigned)factsOoT6,
               (unsigned)factsMM6);
        MwrCheck(s6 == RSBS_COMBO_LOGIC_OK && r6.crossingOpenOoT == 0 && r6.crossingOpenMM == 1,
                 "T6 the round ran with OoT's crossing closed and MM's open",
                 "the round failed, or holding the Mask Shop exit false did not close OoT's crossing alone");
        MwrCheck((factsMM6 & RSBS_XITEM2_SONG_MINUET_MM) != 0u && (factsOoT6 & RSBS_COMBO_TRAVEL_INTO_MM) != 0u,
                 "T6 both engines reported their raw facts (vacuity guard)",
                 "a raw travel fact was 0 with OoT's crossing closed, so T6 cannot claim both gates' crossOoT term");
        MwrCheck(r6.travelToOoT == 0u && r6.travelToMM == 0u && seen6 == 0u && hits6 == 0,
                 "T6 nothing crosses with OoT's crossing closed",
                 "A GATE IGNORED OoT'S CROSSING: a warp fact was handed across, or an engine read one, while the "
                 "player could never enter Termina");
        ComboLogicRoundResult r6b;
        const int s6b = MwrRound(0u, created, songs, forbidden, &r6b);
        MwrPrint("T6b OoT's crossing closed, rules off", r6b);
        const uint32_t seen6b = OoT_ComboLogic_TestTravelEdgeSeen(1);
        const int hits6b = MM_ComboLogic_TestSoaringTravelHits(1);
        MwrCheck(s6b == RSBS_COMBO_LOGIC_OK && r6b.travelToOoT == 0u && r6b.travelToMM == 0u && seen6b == 0u &&
                     hits6b == 0,
                 "T6b rules off with OoT's crossing closed: nothing credited",
                 "a closed OoT crossing credited travel with every warp-song rule off");
        (void)OoT_ComboLogic_TestCloseCrossing(0);
    }
    // And both crossings are back: the surgeries did not leak into the world.
    {
        ComboLogicRoundResult back;
        const int sBack = MwrRound(0u, created, none, nothingForbidden, &back);
        MwrCheck(sBack == RSBS_COMBO_LOGIC_OK && back.crossingOpenOoT == 1 && back.crossingOpenMM == 1,
                 "T7 both crossings reopen after the surgeries",
                 "a crossing stayed closed after T5's guard and T6's reopen");
    }

    printf("[TEST] %s: combo-warp-travel-rounds (%d failure(s))\n", sMwrFailures == 0 ? "PASS" : "FAIL", sMwrFailures);
    return sMwrFailures;
}

// ============================================================================
// combo-warp-song-world (rando tier, #1169 XW12)
// ============================================================================
//
// A REAL paired creation with one warp-song rule ON, set by its key with the
// PRODUCTION mask (the rule is asserted offered with the test seam closed), and
// a world the logic proves beatable with the song in it:
//
//   F1 Minuet of Forest (MM), Shuffle Songs: Song Locations (the shared song pool),
//      Crossing Direction Both: the frozen word is exactly the Minuet's bit; the
//      world holds exactly one Minuet of Forest (MM) on an MM check or crossed
//      onto an OoT host; the creation's own fill held its every-location term
//      (All Locations, the default since #1367; read from the fill's report,
//      because a plain round is never asked that term); the created world's exit
//      round, nothing assumed, proves the goal and credits the Minuet's travel to
//      OoT; OoT's side of the tail freezes a PRESENT warp-song record (D3's
//      refusal not taken).
//   F2 Song of Soaring (OoT), the same settings: exactly one copy in the world,
//      the fill held its every-location term, and the exit round credits Soaring
//      into MM. A second round with every host of MM's own Song of Soaring
//      forbidden shows MM's crawl READING the delivered fact at the root's owl
//      landings: with MM's own song in reach the landing's native disjunct
//      short-circuits the read, so the plain exit round cannot show it.
//   F3 Song of Soaring (OoT), Shuffle Songs: Off, no shared song (D5): the world
//      creates, OoT arms its songs to roam (RSBS_FILL_ARM_SONGS_ROAM) and holds
//      exactly one copy.
//
// Catches: a pool missing the song or holding two copies, a rule bit the fill's
// proof never turns into travel, a world the logic cannot finish with the rule on,
// a Majora's Mask rule frozen with no record, and D5's arm missing.
//
// LIMITS, stated: "placed where the fill needs it" cannot be asserted without
// pinning a seed-specific route, so this row proves the song is a placed,
// proof-credited row and the proof is beatable with it; need-based placement is
// the coordinator's own business, locked by combo-logic-travel-facts. The song
// is not asserted to be on a SONG check when it crossed onto an OoT host (the
// OoT check's type is not visible from this MM TU), and its bag disposition
// (REQUIRED vs surplus) is not read: no bridge exposes a placed row's disposition.

namespace {

int sMwwFailures = 0;

void MwwCheck(bool ok, const char* leg, const char* why) {
    if (!ok) {
        sMwwFailures++;
        fprintf(stderr, "[COMBO-WARP-SONG-WORLD] FAIL: %s: %s\n", leg, why);
        return;
    }
    printf("[TEST]   ok: %s\n", leg);
}

/** Every key a leg set cleared and the last world forgotten however the row ends. */
struct MwwRestore {
    ~MwwRestore() {
        CVarClear(Combo_ComboSettingKey(COMBO_SETTING_SONG_MINUET_MM));
        CVarClear(Combo_ComboSettingKey(COMBO_SETTING_SONG_SOARING_OOT));
        CVarClear("gRandoSettings.ShuffleSongs");
        Combo_ComboItemRules2RevealForTest(0);
        Combo_SingleBag_Forget();
    }
};

/** MM's eleven song checks (combo_song_pool.h's RSBS_SONG_CHECK_MM_*). */
bool MwwIsMMSongCheck(int rc) {
    static const int kMMSongChecks[] = {
        RC_STARTING_ITEM_SONG_OF_HEALING,
        RC_CLOCK_TOWER_ROOF_SONG_OF_TIME,
        RC_ROMANI_RANCH_EPONAS_SONG,
        RC_SOUTHERN_SWAMP_SONG_OF_SOARING,
        RC_DEKU_KINGS_CHAMBER_MONKEY,
        RC_PATH_TO_GORON_VILLAGE_LULLABY_INTRO,
        RC_GORON_SHRINE_FULL_LULLABY,
        RC_GREAT_BAY_COAST_NEW_WAVE_BOSSA_NOVA,
        RC_BENEATH_THE_GRAVEYARD_SONG_OF_STORMS,
        RC_ANCIENT_CASTLE_OF_IKANA_BOSS,
        RC_GIANTS_CHAMBER_OATH_TO_ORDER,
    };
    for (int c : kMMSongChecks) {
        if (c == rc) {
            return true;
        }
    }
    return false;
}

/** Copies of item (@p origin, @p id) in the created world: on an MM check (MM's
 *  own save for an MM item), on an OoT location (OoT's world, for an OoT item), or
 *  on the coordinator's tables of the other game. @p onMMSongCheck counts the MM
 *  hosts among them that are song checks. */
int MwwPlaced(uint8_t origin, uint16_t id, int* onMMSongCheck) {
    int placed = 0;
    int songChecks = 0;
    if (origin == (uint8_t)GAME_MM) {
        for (int rc = (int)RC_UNKNOWN + 1; rc < (int)RC_MAX; ++rc) {
            const RandoSaveCheck& check = RANDO_SAVE_CHECKS[rc];
            if (check.shuffled && !check.skipped && check.randoItemId == (RandoItemId)id) {
                placed++;
                songChecks += MwwIsMMSongCheck(rc) ? 1 : 0;
            }
        }
    } else {
        uint16_t hosts[64];
        placed += OoT_ComboLogic_TestHostsHolding(id, hosts, 64);
    }
    const GameId other = origin == (uint8_t)GAME_MM ? GAME_OOT : GAME_MM;
    ComboLogicPlacement p;
    for (int i = 0; i < Combo_Logic_PlacementCount(other); ++i) {
        if (Combo_Logic_PlacementAt(other, i, &p) && p.item.originGame == origin && p.item.id == id) {
            placed++;
            songChecks += (other == GAME_MM && MwwIsMMSongCheck((int)p.hostCheck)) ? 1 : 0;
        }
    }
    if (onMMSongCheck != nullptr) {
        *onMMSongCheck = songChecks;
    }
    return placed;
}

/** One leg: a real paired creation with @p key on and Shuffle Songs @p shuffleSongs
 *  (-1: OoT's shipped default), up to MM's half. 1 when the world exists. */
int MwwCreate(const char* leg, const char* seed, ComboSettingId key, uint32_t bit, int shuffleSongs) {
    Combo_SingleBag_Forget();
    ComboContext_Init();
    CVarClear(Combo_ComboSettingKey(COMBO_SETTING_SONG_MINUET_MM));
    CVarClear(Combo_ComboSettingKey(COMBO_SETTING_SONG_SOARING_OOT));
    CVarClear("gRandoSettings.ShuffleSongs");
    CVarSetInteger(Combo_ComboSettingKey(key), 1);
    if (shuffleSongs >= 0) {
        CVarSetInteger("gRandoSettings.ShuffleSongs", shuffleSongs);
    }
    // The production mask for every rule: the Song of Soaring (OoT) is offered
    // since XW12, the Majora's Mask extensions since XW13 (IntMMWarpSongMinuet).
    Combo_ComboItemRules2RevealForTest(RSBS_XITEM2_REVEAL_NONE);
    MwwCheck(Combo_ComboItemRule2Offered(bit), leg,
             "the rule is not offered by the production mask (XW12 offers the Song of Soaring (OoT), XW13 the "
             "Majora's Mask extensions)");
    if (Rando_HeadlessSeedTest(seed) != 0 || !Combo_ForeignPairingActive() || !Combo_ComboSettingsFrozen() ||
        !OoT_ComboLogic_GeneralPassDeferred()) {
        MwwCheck(false, leg, "the paired OoT generation did not leave a frozen paired world at its general pass");
        return 0;
    }
    MwwCheck(Combo_ComboCrossItemFlags2() == bit, leg, "the frozen second word is not exactly the leg's rule");
    if (MM_Rando_HeadlessPairedHalf() != 0) {
        MwwCheck(false, leg, "MM's creation-time half did not produce a rando world");
        return 0;
    }
    return 1;
}

/** Did the creation's own fill hold its every-location term? Combo_Logic_RunRound
 *  is never asked that term (allLocationsReached stays -1), so the verdict is the
 *  fill report's. */
bool MwwFillReachedEveryLocation() {
    const ComboSingleBagReport* rep = Combo_SingleBag_LastReport();
    printf("[TEST] combo-warp-song-world: fill report %s, status %d, rung %d, every location %d (%d checked, %d not "
           "reached)\n",
           rep != nullptr ? "present" : "ABSENT", rep != nullptr ? rep->status : -1,
           rep != nullptr ? (int)rep->rung : -1, (rep != nullptr && rep->fill.allLocationsReached) ? 1 : 0,
           rep != nullptr ? rep->fill.locationsChecked : 0, rep != nullptr ? rep->fill.locationsUnreachable : 0);
    return rep != nullptr && rep->status == RSBS_COMBO_LOGIC_OK && rep->fill.allLocationsReached &&
           rep->fill.locationsChecked > 0 && rep->fill.locationsUnreachable == 0;
}

/** The created world's exit round, nothing assumed, nothing forbidden. */
int MwwExitRound(ComboLogicRoundResult* out) {
    const std::vector<ComboLogicBagItem> none;
    const std::vector<ComboLogicHostRef> nothing;
    const uint32_t word = Combo_ComboItemRules2Word(&gComboCtx.comboItemRules2);
    return MwrRound(word, word, none, nothing, out);
}

} // namespace

extern "C" int MM_WarpSongWorld_RunHeadless(void) {
    printf("[TEST] combo-warp-song-world: real paired creations with one warp-song rule on through its key (the "
           "production mask) place exactly one copy, prove the world beatable with its "
           "travel credited, and freeze the warp-song record (#1169 XW12)\n");
    sMwwFailures = 0;
    MwwRestore restore;
    (void)OoT_ComboLogic_TestTravelEdgeSeen(1);
    (void)MM_ComboLogic_TestSoaringTravelHits(1);

    // ---- F1: Minuet of Forest (MM), Song Locations ----------------------------------
    if (MwwCreate("F1 Minuet of Forest (MM)", "RSBSXW12MINUET", COMBO_SETTING_SONG_MINUET_MM,
                  RSBS_XITEM2_SONG_MINUET_MM, -1) != 0) {
        int onSongCheck = 0;
        const int placed = MwwPlaced((uint8_t)GAME_MM, (uint16_t)RI_SONG_MINUET, &onSongCheck);
        const bool everyLocation = MwwFillReachedEveryLocation();
        ComboLogicRoundResult r;
        const int s = MwwExitRound(&r);
        MwrPrint("F1 exit round", r);
        printf("[TEST] combo-warp-song-world: F1 copies placed %d (%d on an MM song check)\n", placed, onSongCheck);
        MwwCheck(placed == 1, "F1 exactly one Minuet of Forest (MM) in the world",
                 "the world holds no copy or more than one");
        MwwCheck(everyLocation, "F1 the creation's fill reached every location",
                 "the fill's report is absent, failed, or did not hold its every-location term with the Minuet on");
        MwwCheck(s == RSBS_COMBO_LOGIC_OK && r.goalExpression == 1, "F1 the created world's exit round proves the goal",
                 "the exit round did not prove the goal with the Minuet on");
        MwwCheck(r.travelToOoT == RSBS_XITEM2_SONG_MINUET_MM, "F1 the exit round credits the Minuet's travel to OoT",
                 "the placed Minuet never became a travel fact in the proof");
        const int crossings = OoT_Creation_FinishPairedHalf(0);
        MwwCheck(crossings >= 0 && Combo_OoTWarpRecord_Status() == COMBO_OOT_WARP_REC_PRESENT,
                 "F1 OoT's tail froze a PRESENT warp-song record",
                 "the tail failed (D3's -3) or left the record unset with the Minuet on");
    }
    Combo_SingleBag_Forget();
    (void)OoT_ComboLogic_TestTravelEdgeSeen(1);
    (void)MM_ComboLogic_TestSoaringTravelHits(1);

    // ---- F2: Song of Soaring (OoT), Song Locations ----------------------------------
    const uint16_t ootSoaring = OoT_ComboLogic_TestTravelItemId(0);
    if (MwwCreate("F2 Song of Soaring (OoT)", "RSBSXW12SOARING", COMBO_SETTING_SONG_SOARING_OOT,
                  RSBS_XITEM2_SONG_SOARING_OOT, -1) != 0) {
        const int placed = MwwPlaced((uint8_t)GAME_OOT, ootSoaring, nullptr);
        const bool everyLocation = MwwFillReachedEveryLocation();
        (void)MM_ComboLogic_TestSoaringTravelHits(1);
        ComboLogicRoundResult r;
        const int s = MwwExitRound(&r);
        MwrPrint("F2 exit round", r);
        (void)MM_ComboLogic_TestSoaringTravelHits(1);
        MwwCheck(placed == 1, "F2 exactly one Song of Soaring (OoT) in the world",
                 "the world holds no copy or more than one");
        MwwCheck(everyLocation, "F2 the creation's fill reached every location",
                 "the fill's report is absent, failed, or did not hold its every-location term with Soaring on");
        MwwCheck(s == RSBS_COMBO_LOGIC_OK && r.goalExpression == 1, "F2 the created world's exit round proves the goal",
                 "the exit round did not prove the goal with Soaring on");
        MwwCheck(r.travelToMM == RSBS_COMBO_TRAVEL_INTO_MM, "F2 the exit round credits Soaring into MM",
                 "the placed Song of Soaring (OoT) never became a travel fact handed to MM");

        // MM's crawl reads the fact: only visible with MM's own Song of Soaring
        // out of reach, or the landing's native disjunct answers first.
        std::vector<ComboLogicHostRef> noMMSoaring;
        for (int rc = (int)RC_UNKNOWN + 1; rc < (int)RC_MAX; ++rc) {
            if (RANDO_SAVE_CHECKS[rc].randoItemId == RI_SONG_SOARING) {
                ComboLogicHostRef ref;
                ref.hostGame = (uint8_t)GAME_MM;
                ref.hostCheck = (uint16_t)rc;
                noMMSoaring.push_back(ref);
            }
        }
        ComboLogicPlacement p;
        for (int i = 0; i < Combo_Logic_PlacementCount(GAME_OOT); ++i) {
            if (Combo_Logic_PlacementAt(GAME_OOT, i, &p) && p.item.originGame == (uint8_t)GAME_MM &&
                p.item.id == (uint16_t)RI_SONG_SOARING) {
                ComboLogicHostRef ref;
                ref.hostGame = (uint8_t)GAME_OOT;
                ref.hostCheck = p.hostCheck;
                noMMSoaring.push_back(ref);
            }
        }
        MwwCheck(!noMMSoaring.empty() && !CHECK_QUEST_ITEM(QUEST_SONG_SOARING),
                 "F2 MM's own Song of Soaring is on a host and not in the starting save (vacuity guard)",
                 "MM's own Song of Soaring cannot be withheld, so the landing's native disjunct always answers first");
        const std::vector<ComboLogicBagItem> none;
        const uint32_t word = Combo_ComboItemRules2Word(&gComboCtx.comboItemRules2);
        ComboLogicRoundResult rb;
        const int sb = MwrRound(word, word, none, noMMSoaring, &rb);
        MwrPrint("F2 MM's own Song of Soaring forbidden", rb);
        const int hits = MM_ComboLogic_TestSoaringTravelHits(1);
        MwwCheck(sb == RSBS_COMBO_LOGIC_OK && rb.travelToMM == RSBS_COMBO_TRAVEL_INTO_MM && hits > 0,
                 "F2 with MM's own song out of reach, MM's crawl reads the delivered Soaring fact",
                 "the placed Song of Soaring (OoT) never became a travel fact MM's crawl read at the owl landings");
    }
    Combo_SingleBag_Forget();

    // ---- F3: Song of Soaring (OoT), Shuffle Songs Off (D5) --------------------------
    if (MwwCreate("F3 Song of Soaring (OoT), Shuffle Songs Off", "RSBSXW12SOAROFF", COMBO_SETTING_SONG_SOARING_OOT,
                  RSBS_XITEM2_SONG_SOARING_OOT, 0) != 0) {
        const uint32_t armed = OoT_ComboLogic_ConfinementArmed();
        const int placed = MwwPlaced((uint8_t)GAME_OOT, ootSoaring, nullptr);
        printf("[TEST] combo-warp-song-world: F3 OoT arm word %08X, copies placed %d\n", (unsigned)armed, placed);
        MwwCheck((armed & RSBS_FILL_ARM_SONGS_ROAM) != 0u,
                 "F3 under Off the Song of Soaring (OoT) arms OoT's songs to roam (D5)",
                 "the arm word lacks SONGS_ROAM: the song's domain still depends on an unrelated held-back song");
        MwwCheck(placed == 1, "F3 exactly one Song of Soaring (OoT) in the world", "no copy or more than one");
    }
    Combo_SingleBag_Forget();

    printf("[TEST] %s: combo-warp-song-world (%d failure(s))\n", sMwwFailures == 0 ? "PASS" : "FAIL", sMwwFailures);
    return sMwwFailures;
}

#endif // RSBS_SINGLE_EXECUTABLE
