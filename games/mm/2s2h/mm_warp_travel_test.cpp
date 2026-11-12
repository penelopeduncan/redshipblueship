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
#include <memory>
#include <set>
#include <utility>
#include <vector>

#include "Rando/Rando.h"
#include "Rando/Types.h"
#include "Rando/Logic/Logic.h"
#include "Rando/Logic/ResetTimeGuard.h"
#include "Rando/WarpSongsMM.h"

#include "combo_logic.h"      // src/common: Combo_Logic_RunRound and the placement tables
#include "combo_single_bag.h" // src/common: the creation's commit (Combo_SingleBag_Forget)
#include "context.h"          // src/common: ComboContext_Init
#include "foreign_items.h"    // src/common: the second rules word and its freeze

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
// NOT covered here: a round whose crossing is CLOSED over the real engines (this
// world opens both from nothing assumed); THE TRAVEL GATES' closed-crossing
// behaviour is combo-logic-travel-facts' stub legs T2 and T3.

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

/** The frozen word put back, the probes cleared and the created world forgotten
 *  however the row ends. */
struct MwrRestore {
    uint32_t word = 0u;
    ~MwrRestore() {
        Combo_ComboItemRules2Store(&gComboCtx.comboItemRules2, word);
        (void)OoT_ComboLogic_TestTravelEdgeSeen(1);
        (void)MM_ComboLogic_TestSoaringTravelHits(1);
        Combo_SingleBag_Forget();
    }
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
    printf("[TEST] combo-warp-travel-rounds: forbidding %d host(s): OoT's Minuet of Forest on %d OoT location(s), "
           "MM's Song of Soaring on %d host(s), %d native song placement(s) on the coordinator's tables\n",
           (int)forbidden.size(), ootMinuetHosts, mmSoaringHosts, onTables);
    // Vacuity guards: a song the player STARTS with cannot be withheld, and then
    // the native disjunct short-circuits the edge this row probes.
    MwrCheck(ootSoaring != 0u && ootMinuet != 0u, "S3 the OoT bridge names both songs", "an OoT item id is 0");
    MwrCheck(ootMinuetHosts >= 1, "S3 OoT's Minuet of Forest is on a location (not a starting item)",
             "OoT's Minuet of Forest is on no location, so its native disjunct cannot be withheld");
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

    printf("[TEST] %s: combo-warp-travel-rounds (%d failure(s))\n", sMwrFailures == 0 ? "PASS" : "FAIL", sMwrFailures);
    return sMwrFailures;
}

#endif // RSBS_SINGLE_EXECUTABLE
