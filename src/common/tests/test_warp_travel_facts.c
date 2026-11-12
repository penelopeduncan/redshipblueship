/**
 * @file test_warp_travel_facts.c
 * @brief #1169 lane XW9: the travel facts' game-side locks (combo_logic.h ABI 10;
 *        the coordinator's own half is combo-logic-travel-facts in
 *        test_combo_logic.c).
 *
 *   oot-warp-travel-edges (redship tier, ROM-free, no generation). OoT's REAL
 *   region graph (Rando_InitRegionGraphForTest: a Context, RegionTable_Init,
 *   SetAllEntrancesData) and its real logic:
 *     P. XW7's destination rule split in two, pure: a grave is credited, a grotto
 *        pseudo index and 0 are not, and the edge predicate needs a round, the
 *        song's accepted bit and a credited destination.
 *     D. Each song's destination as the OoT engine reads it from the entrance
 *        graph is its vanilla pad in an unshuffled graph (what XW7's record holds
 *        for an unshuffled world), and still is with the exit's index unstamped
 *        (D2: the pad table, not the index SetAllEntrancesData writes).
 *     E. The six RR_ROOT_EXITS warp exits (location_access/root.cpp), evaluated
 *        with Entrance::ConditionsMet from one age and time each: the MM disjunct
 *        opens for a child at day only, only in a round, only with the song's
 *        accepted fact and a credited destination; the native song opens it for
 *        an adult (the control that the probe evaluates the real lambda).
 *     T. OoT's `travelFacts` decision: the Soaring fact exactly for the rule, the
 *        song, an ocarina and C-down, C-up and C-left with RR_ROOT child-day
 *        access, the four age and time flags put back.
 *     R. RR_ROOT as a round's Regions::AccessReset seeds it: child-day only under
 *        a child start, adult-day only under an adult start, so the travelFacts
 *        gate is a real input.
 *
 *   mm-warp-travel-edges (rando tier, no generation): MM's half, body in
 *   games/mm/2s2h/mm_warp_travel_test.cpp; its wrapper is in test_runner.cpp.
 *
 * FILE SCOPE (compiled as C++, #included into test_runner.cpp).
 */

#include "../combo_logic.h"
#include "../context.h"
#include "../foreign_items.h"
#include "../mm_warp_song_play.h"
#include "../test_runner.h"

#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>

extern "C" {
int Rando_InitRegionGraphForTest(void);
int OoT_ComboLogic_TestEnsureItemTable(void);
void OoT_ComboLogic_TestTravelSeam(int armed, int inRound, uint32_t accepted, const uint16_t* dest);
uint16_t OoT_ComboLogic_TestTravelGraphDest(int song);
uint16_t OoT_ComboLogic_TestTravelGraphDestUnstamped(int song);
int OoT_ComboLogic_TestRootWarpEdge(int song, int child, int day, int native);
int OoT_ComboLogic_TestRootAccessAfterReset(int adultStart, int* outChildDay, int* outAdultDay);
int OoT_ComboLogic_TestTravelFacts(int held, int ocarina, uint32_t buttons, int rootChildDay, int* outRestored);
}

namespace {

int sXw9Failures = 0;

#define XW9_CHECK(cond, ...)                                     \
    do {                                                         \
        if (!(cond)) {                                           \
            printf("[TEST] FAIL (%s:%d): ", __FILE__, __LINE__); \
            printf(__VA_ARGS__);                                 \
            printf("\n");                                        \
            sXw9Failures++;                                      \
        }                                                        \
    } while (0)

const uint16_t kXw9Grave = 0x031Cu;  // ENTR_REDEAD_GRAVE_0 (entrance_table.h): a real entrance
const uint16_t kXw9Grotto = 0x0700u; // ENTRANCE_GROTTO_LOAD(0): a grotto pseudo index

// The ocarina buttons the probe takes: bit 0 A, 1 C-up, 2 C-down, 3 C-left, 4 C-right.
const uint32_t kXw9Up = 0x02u;
const uint32_t kXw9Down = 0x04u;
const uint32_t kXw9Left = 0x08u;
const uint32_t kXw9Soaring = kXw9Up | kXw9Down | kXw9Left;

/** Freeze the shipped combo record with @p rules2 as the second rules word (the
 *  freeze is raw: the hidden-bit refusal is Generate's). */
void Xw9FreezeWorld(uint32_t rules2) {
    ComboContext_Init();
    ComboSettingsRecord rec;
    Combo_ComboSettingsDefaults(&rec);
    Combo_FreezeComboSettingsEx2(&rec, NULL, rules2);
}

/** The text of @p rel under the checkout, or "" when it cannot be read. */
std::string Xw9ReadSource(const char* rel) {
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

/** The body of the function (or initialiser) that starts at @p head in @p text:
 *  from @p head to the first line that is exactly "}" or "});" after it; "" when
 *  @p head is absent. So an anchor searches inside the function it names. */
std::string Xw9Body(const std::string& text, const char* head) {
    const size_t at = text.find(head);
    if (at == std::string::npos) {
        return std::string();
    }
    size_t end = text.find("\n}\n", at);
    const size_t init = text.find("\n}, {});", at);
    if (init != std::string::npos && (end == std::string::npos || init < end)) {
        end = init;
    }
    return text.substr(at, end == std::string::npos ? std::string::npos : end - at);
}

/** The text of @p text from @p from up to the first @p to after it; "" when
 *  either is absent. */
std::string Xw9Between(const std::string& text, const char* from, const char* to) {
    const size_t at = text.find(from);
    if (at == std::string::npos) {
        return std::string();
    }
    const size_t end = text.find(to, at + std::strlen(from));
    return end == std::string::npos ? std::string() : text.substr(at, end - at);
}

int Xw9Count(const std::string& text, const char* needle) {
    int n = 0;
    for (size_t at = text.find(needle); at != std::string::npos; at = text.find(needle, at + 1)) {
        n++;
    }
    return n;
}

/** Every song's pad, as the seam's destinations. */
void Xw9Pads(uint16_t* out) {
    for (int song = 0; song < RSBS_MMWP_SONG_COUNT; ++song) {
        out[song] = Combo_MMWarpPlay_PadEntrance(song);
    }
}

} // namespace

TestResult Test_OoTWarpTravelEdges(void) {
    printf("[TEST] oot-warp-travel-edges: OoT's six warp-song root exits open for an MM warp only for a child at day "
           "in a round with the song's fact and a destination play always lands, OoT's Soaring fact is "
           "can_play_soaring behind RR_ROOT's child-day access, and XW7's destination rule is shared (#1169 XW9)\n");
    sXw9Failures = 0;

    // ---- P. the pure destination rule ------------------------------------------------
    // Catches: graves dropped (an entrance-type test), grotto pseudo indices or 0
    // credited, or the edge predicate missing a term.
    XW9_CHECK(Combo_MMWarpPlay_DestCreditedByLogic(kXw9Grave) && !Combo_MMWarpPlay_DestNeedsOoTReady(kXw9Grave),
              "P1: a grave (a real entrance below 0x0700) must be credited: XW7 lands it in every session");
    XW9_CHECK(!Combo_MMWarpPlay_DestCreditedByLogic(kXw9Grotto) && Combo_MMWarpPlay_DestNeedsOoTReady(kXw9Grotto),
              "P2: a grotto pseudo index depends on the session's probe and must not be credited");
    XW9_CHECK(!Combo_MMWarpPlay_DestCreditedByLogic(0u), "P3: destination 0 must not be credited");
    XW9_CHECK(Combo_MMWarpPlay_DestCreditedByLogic(Combo_MMWarpPlay_PadEntrance(0)), "P4: Minuet's pad is credited");
    XW9_CHECK(Combo_MMWarpPlay_DestResolvable(kXw9Grave) && !Combo_MMWarpPlay_DestResolvable(0u),
              "P5: DestResolvable's behaviour moved with the split");
    XW9_CHECK(Combo_MMWarpPlay_TravelEdgeOpen(true, 0x02u, 0, 0x0600u), "P6: round, Minuet's bit, the pad: open");
    XW9_CHECK(!Combo_MMWarpPlay_TravelEdgeOpen(false, 0x02u, 0, 0x0600u), "P7: no round: closed");
    XW9_CHECK(!Combo_MMWarpPlay_TravelEdgeOpen(true, 0x04u, 0, 0x0600u), "P8: another song's bit: closed");
    XW9_CHECK(!Combo_MMWarpPlay_TravelEdgeOpen(true, 0x7Eu, 0, kXw9Grotto), "P9: a grotto destination: closed");
    XW9_CHECK(!Combo_MMWarpPlay_TravelEdgeOpen(true, 0x7Eu, 6, 0x0600u) &&
                  !Combo_MMWarpPlay_TravelEdgeOpen(true, 0x7Eu, -1, 0x0600u),
              "P10: a song out of range: closed");

    if (Rando_InitRegionGraphForTest() != 0 || OoT_ComboLogic_TestEnsureItemTable() != 0) {
        printf("[TEST] FAIL: oot-warp-travel-edges: no OoT region graph or item table\n");
        return TEST_FAIL;
    }

    // ---- D. destinations from the entrance graph ------------------------------------------
    // Catches: the engine reading the wrong exit, or an unshuffled song resolved
    // to anything but its pad (XW7's record holds the pad there). The bring-up ran
    // SetAllEntrancesData, which stamps every warp exit's index with its pad, so D
    // alone cannot tell the pad-table fallback from exit.GetIndex(); D2 asks again
    // with the index unstamped (-1, a creation with no entrance shuffle at all) and
    // catches exactly that replacement.
    for (int song = 0; song < RSBS_MMWP_SONG_COUNT; ++song) {
        const uint16_t dest = OoT_ComboLogic_TestTravelGraphDest(song);
        XW9_CHECK(dest == Combo_MMWarpPlay_PadEntrance(song), "D: song %d reads %04X from the graph, its pad is %04X",
                  song, (unsigned)dest, (unsigned)Combo_MMWarpPlay_PadEntrance(song));
        const uint16_t unstamped = OoT_ComboLogic_TestTravelGraphDestUnstamped(song);
        XW9_CHECK(unstamped == Combo_MMWarpPlay_PadEntrance(song),
                  "D2: song %d reads %04X with its exit's index unstamped, its pad is %04X (the unshuffled "
                  "destination must come from the pad table, not the exit's index)",
                  song, (unsigned)unstamped, (unsigned)Combo_MMWarpPlay_PadEntrance(song));
        XW9_CHECK(OoT_ComboLogic_TestTravelGraphDest(song) == dest, "D2: song %d's exit index was not put back", song);
    }

    // ---- E. the six real root exits ------------------------------------------------------
    uint16_t pads[RSBS_MMWP_SONG_COUNT];
    Xw9Pads(pads);
    Xw9FreezeWorld(0u);
    XW9_CHECK(OoT_ComboLogic_TestRootWarpEdge(0, 1, 1, 0) >= 0,
              "E0: the probe could not evaluate Minuet's root exit (%d: -1 no exit, -2 not glitchless)",
              OoT_ComboLogic_TestRootWarpEdge(0, 1, 1, 0));
    for (int song = 0; song < RSBS_MMWP_SONG_COUNT; ++song) {
        const uint32_t bit = 0x02u << song;
        OoT_ComboLogic_TestTravelSeam(0, 0, 0u, NULL);
        XW9_CHECK(OoT_ComboLogic_TestRootWarpEdge(song, 1, 1, 0) == 0,
                  "E1 song %d: outside a round the exit opened with nothing held", song);
        XW9_CHECK(OoT_ComboLogic_TestRootWarpEdge(song, 0, 1, 1) == 1,
                  "E2 song %d: control: the native song no longer opens the exit for an adult, so E3-E9 probe "
                  "a broken lambda",
                  song);
        OoT_ComboLogic_TestTravelSeam(1, 1, bit, pads);
        XW9_CHECK(OoT_ComboLogic_TestRootWarpEdge(song, 1, 1, 0) == 1,
                  "E3 song %d: a child at day with MM's accepted fact must take the exit", song);
        XW9_CHECK(OoT_ComboLogic_TestRootWarpEdge(song, 0, 1, 0) == 0,
                  "E4 song %d: an adult took MM's warp (the arrival is forced child)", song);
        XW9_CHECK(OoT_ComboLogic_TestRootWarpEdge(song, 1, 0, 0) == 0,
                  "E5 song %d: a child at night took MM's warp (credited by day only)", song);
        OoT_ComboLogic_TestTravelSeam(1, 1, 0x7Eu & ~bit, pads);
        XW9_CHECK(OoT_ComboLogic_TestRootWarpEdge(song, 1, 1, 0) == 0,
                  "E6 song %d: the exit opened on another song's fact", song);
        OoT_ComboLogic_TestTravelSeam(1, 0, bit, pads);
        XW9_CHECK(OoT_ComboLogic_TestRootWarpEdge(song, 1, 1, 0) == 0,
                  "E7 song %d: the exit opened outside a round (OoT's fill, tracker and validation would see it)",
                  song);
    }
    {
        uint16_t dest[RSBS_MMWP_SONG_COUNT];
        Xw9Pads(dest);
        dest[3] = kXw9Grotto;
        dest[4] = kXw9Grave;
        dest[5] = 0u;
        OoT_ComboLogic_TestTravelSeam(1, 1, 0x7Eu, dest);
        XW9_CHECK(OoT_ComboLogic_TestRootWarpEdge(3, 1, 1, 0) == 0,
                  "E8: Requiem to a grotto pseudo index was credited (play refuses it in a session without OoT's "
                  "grotto tables)");
        XW9_CHECK(OoT_ComboLogic_TestRootWarpEdge(4, 1, 1, 0) == 1,
                  "E9: Nocturne to a grave was not credited (XW7 lands a grave in every session)");
        XW9_CHECK(OoT_ComboLogic_TestRootWarpEdge(5, 1, 1, 0) == 0, "E10: Prelude to destination 0 was credited");
    }
    OoT_ComboLogic_TestTravelSeam(0, 0, 0u, NULL);

    // ---- T. OoT's travel facts -------------------------------------------------------------
    // Catches: the fact without its rule, the song, the ocarina or one of Soaring's
    // three buttons; the root child-day gate ignored; the probe's flags not put back.
    {
        int restored = 0;
        Xw9FreezeWorld(RSBS_XITEM2_SONG_SOARING_OOT);
        XW9_CHECK(OoT_ComboLogic_TestTravelFacts(1, 1, kXw9Soaring, 1, &restored) == (int)RSBS_COMBO_TRAVEL_INTO_MM &&
                      restored == 1,
                  "T1: rule on, the song, an ocarina and C-down/C-up/C-left: the Soaring fact, flags put back");
        XW9_CHECK(OoT_ComboLogic_TestTravelFacts(0, 1, kXw9Soaring, 1, NULL) == 0, "T2: without the song");
        XW9_CHECK(OoT_ComboLogic_TestTravelFacts(1, 0, kXw9Soaring, 1, NULL) == 0, "T3: without an ocarina");
        const uint32_t needed[3] = { kXw9Down, kXw9Up, kXw9Left };
        for (int b = 0; b < 3; ++b) {
            XW9_CHECK(OoT_ComboLogic_TestTravelFacts(1, 1, kXw9Soaring & ~needed[b], 1, NULL) == 0,
                      "T4: without button %u (Ocarina Button Shuffle must apply)", (unsigned)needed[b]);
        }
        XW9_CHECK(OoT_ComboLogic_TestTravelFacts(1, 1, kXw9Soaring, 0, &restored) == 0 && restored == 1,
                  "T5: RR_ROOT without child-day access (an adult start before time travel) gave the fact");
        Xw9FreezeWorld(0u);
        XW9_CHECK(OoT_ComboLogic_TestTravelFacts(1, 1, 0x1Fu, 1, &restored) == 0 && restored == 1,
                  "T6: rule off (every world a player can make today) gave the fact");
    }

    // ---- R. RR_ROOT as a round seeds it ------------------------------------------------------
    // Catches: a travelFacts gate that is vacuous because the root always holds
    // child-day access.
    {
        int childDay = -1;
        int adultDay = -1;
        XW9_CHECK(
            OoT_ComboLogic_TestRootAccessAfterReset(1, &childDay, &adultDay) == 0 && childDay == 0 && adultDay == 1,
            "R1: an adult start seeds RR_ROOT with adult-day only (child-day %d, adult-day %d)", childDay, adultDay);
        XW9_CHECK(
            OoT_ComboLogic_TestRootAccessAfterReset(0, &childDay, &adultDay) == 0 && childDay == 1 && adultDay == 0,
            "R2: a child start seeds RR_ROOT with child-day only (child-day %d, adult-day %d)", childDay, adultDay);
        Xw9FreezeWorld(RSBS_XITEM2_SONG_SOARING_OOT);
        XW9_CHECK(OoT_ComboLogic_TestTravelFacts(1, 1, kXw9Soaring, -1, NULL) == (int)RSBS_COMBO_TRAVEL_INTO_MM,
                  "R3: travelFacts' own read of RR_ROOT after a child-start reset must give the fact");
    }

    // ---- S. the wiring, by function-scoped source anchors ------------------------------
    // Each anchor reads inside the function or initialiser it names. Catches: the
    // coordinator not running the gate or not delivering, an engine not resetting
    // or not reading its facts, a disjunct dropped from a region file, or a
    // vtable that does not install the entries (each would leave the rows above
    // green over a seam while production never credits a warp).
    {
        const std::string root = Xw9ReadSource("games/oot/soh/Enhancements/randomizer/location_access/root.cpp");
        const std::string mmLogic = Xw9ReadSource("games/mm/2s2h/Rando/Logic/Logic.cpp");
        const std::string coord = Xw9ReadSource("src/common/combo_logic.c");
        const std::string oot = Xw9ReadSource("games/oot/soh/Enhancements/randomizer/ComboLogicEngineOoT.cpp");
        const std::string mm = Xw9ReadSource("games/mm/2s2h/Rando/ComboLogicEngineSingleExe.cpp");
        XW9_CHECK(!root.empty() && !mmLogic.empty() && !coord.empty() && !oot.empty() && !mm.empty(),
                  "S0: a source file could not be read under RSBS_SOURCE_DIR");
        const std::string rootExits = Xw9Between(root, "areaTable[RR_ROOT_EXITS]", "areaTable[RR_CHILD_SPAWN]");
        XW9_CHECK(Xw9Count(rootExits, "logic->IsChild && logic->AtDay && RSBS_MM_WARP_TRAVEL(") == 6,
                  "S1: RR_ROOT_EXITS does not carry the child-at-day MM disjunct on all six warp exits");
        const std::string mmRoot = Xw9Between(mmLogic, "Regions[RR_MAX] = RandoRegion{", "}, {});");
        XW9_CHECK(Xw9Count(mmRoot, "(CAN_PLAY_SONG(SOARING) || CAN_OOT_SOAR_TO_OWL) && CAN_OWL_WARP(") == 10,
                  "S2: RR_MAX does not carry OoT's Soaring on all ten owl landings");
        const std::string round = Xw9Body(coord, "static int ComboLogicRoundRun(");
        XW9_CHECK(Xw9Count(round, "Combo_Logic_TravelGate(") == 1 && Xw9Count(round, "->acceptTravelFacts(") == 2 &&
                      Xw9Count(round, "->travelFacts(") == 2,
                  "S3: the round does not read both engines' facts, gate them and deliver them");
        const std::string begin = Xw9Body(oot, "int OoT_ComboLogic_BeginQuery(void* self) {");
        const std::string end = Xw9Body(oot, "void OoT_ComboLogic_EndQuery(void* self) {");
        const std::string facts = Xw9Body(oot, "uint32_t OoT_ComboLogic_TravelFacts(void* self) {");
        const std::string access = Xw9Body(oot, "extern \"C\" bool OoT_ComboLogic_MMWarpTravel(int song) {");
        XW9_CHECK(Xw9Count(begin, "sOoTTravelAccepted = 0u;") == 1 &&
                      Xw9Count(begin, "sOoTTravelDest[song] = OoTComboTravelGraphDest(song);") == 1 &&
                      Xw9Count(end, "sOoTTravelAccepted = 0u;") == 1,
                  "S4: OoT's beginQuery / endQuery do not reset the facts and read the destinations");
        XW9_CHECK(Xw9Count(facts, "root->childDay") == 1 && Xw9Count(facts, "OoTComboTravelFactsOf(") == 1,
                  "S5: OoT's travelFacts does not ask OoTComboTravelFactsOf behind RR_ROOT's child-day access");
        XW9_CHECK(Xw9Count(access, "Combo_MMWarpPlay_TravelEdgeOpen(sInQuery, sOoTTravelAccepted, song, "
                                   "sOoTTravelDest[song])") == 1,
                  "S6: OoT's accessor does not read the round's state through the shared predicate");
        const std::string ootVt = Xw9Between(oot, "const ComboLogicEngine kOoTComboLogicEngine = {", "\n};");
        const std::string mmVt = Xw9Between(mm, "const ComboLogicEngine kMmEngine = {", "\n};");
        XW9_CHECK(Xw9Count(ootVt, "OoT_ComboLogic_TravelFacts,") == 1 &&
                      Xw9Count(ootVt, "OoT_ComboLogic_AcceptTravelFacts,") == 1 && Xw9Count(mmVt, "TravelFacts,") == 2,
                  "S7: a real engine's vtable does not install both travel entries");
        const std::string mmAccess = Xw9Body(mm, "extern \"C\" bool MM_ComboLogic_OoTSoaringTravel(void) {");
        const std::string mmFacts = Xw9Body(mm, "uint32_t MmComboTravelFactsNow() {");
        XW9_CHECK(Xw9Count(mmAccess, "sRound.inRound && (sRound.travelAccepted & RSBS_COMBO_TRAVEL_INTO_MM)") == 1 &&
                      Xw9Count(mmFacts, "CAN_PLAY_OOT_WARP_SONG(") == 1,
                  "S8: MM's accessor or its facts do not read the round's state and can_play_tp_*");
    }

    ComboContext_Init();
    printf("[TEST] oot-warp-travel-edges: %s\n", sXw9Failures == 0 ? "PASS" : "FAIL");
    return sXw9Failures == 0 ? TEST_PASS : TEST_FAIL;
}
