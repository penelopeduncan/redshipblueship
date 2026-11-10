/**
 * @file test_warp_song_soaring.c
 * @brief Locks for Ocarina of Time's Song of Soaring to a Termina owl (#1169 lane
 *        XW6; the game-neutral half is src/common/warp_song_soaring.c, OoT's is
 *        games/oot/soh/Enhancements/randomizer/SongOfSoaringOoT.cpp and the
 *        guarded branches in games/oot/src/code/code_800EC960.c and
 *        z_message_PAL.c).
 *
 * Three rows, all ROM-free (redship tier), none generating:
 *
 *   soaring-owl-table (CTest SoaringOwlTable) -- THE OWLS AND THE FROZEN COPY.
 *   The ten owl entrances and names equal MM's own table (read in MM's TU, where
 *   ENTRANCE() is in scope); MM_SlotMeta_Register registers the owl-flag layout
 *   (so the --test rows, which bypass main.cpp, see the production layout);
 *   Combo_SoaringOwlMask reads exactly the owls of MM's frozen half, frozen from
 *   MM's REAL SaveContext the way a departure freezes it, and reads 0 for a
 *   shadow that is not frozen, an unmarked half, day 0, MM's hidden owl alone,
 *   or an unregistered layout.
 *
 *   oot-soaring-gates (CTest OoTSoaringGates) -- THE GATES AND THE CHOOSER, the
 *   pure decisions the z_message_PAL.c branches and SongOfSoaringOoT.cpp act on:
 *   every restriction refuses (ruling 5), an unplayable MM refuses, no owl shows
 *   the no-statues text, every refusal and every chooser exit ends in
 *   OCARINA_MODE_04 and none in 02; only A selects; the chooser cycles and
 *   wraps. From source: the branches call those decisions, the chooser opens
 *   with ContinueTextbox (never StartTextbox), and MM's arrival skips the
 *   pause-save spot for a Soaring arrival.
 *
 *   oot-soaring-matcher (CTest OoTSoaringMatcher) -- THE MATCH, over OoT's REAL
 *   free-play checker (func_800ED200) driven note by note
 *   (games/oot/soh/oot_soaring_matcher_test.cpp): nothing with the rule off,
 *   unowned, adult, unpaired or under check-no-warp; every vanilla song still
 *   answers itself with every gate on; Soaring answers RSBS_OOT_SONG_SOARING;
 *   the tail rule (junk first, five notes, a sharpened note); Pierre's song wins
 *   only when both finish on the same note.
 *
 * Linkage note: #included into test_runner.cpp at FILE SCOPE (compiled as C++),
 * like its neighbours; every symbol under test is C-linkage. Helpers carry the
 * Xw6 prefix because every included test file shares one translation unit.
 */

#include "../context.h"
#include "../entrance.h"
#include "../foreign_items.h"
#include "../game.h"
#include "../shared_resources.h"
#include "../test_runner.h"
#include "../warp_song_soaring.h"

#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#ifdef __cplusplus
extern "C" {
#endif
// games/mm/2s2h/GameExports_SingleExe.cpp
void MM_SlotMeta_Register(void);
uint16_t MM_SoaringTest_OwlEntrance(int owl);
void MM_SoaringTest_FreezeHalf(uint16_t owls, int32_t day, int marked);
// games/oot/soh/oot_soaring_matcher_test.cpp
int OoT_SoaringMatcherTest_Play(const uint8_t* notes, int count, int child, int owned, int freePlay);
int OoT_SoaringMatcherTest_PlayVanilla(int song);
int OoT_SoaringMatcherTest_PlayWithPierre(const uint8_t* pierre, const uint8_t* notes, int count);
// games/oot/soh/Enhancements/randomizer/SongOfSoaringOoT.cpp
bool OoT_SongOfSoaringOoTOn(void);
#ifdef __cplusplus
}
#endif

static_assert(RSBS_SOARING_SWITCH_KIND == COMBO_SWITCH_KIND_SOARING,
              "warp_song_soaring.h's switch kind must be entrance.h's COMBO_SWITCH_KIND_SOARING (#1169 XW6)");

static int sXw6Failures;

#define XW6_CHECK(cond, ...)                          \
    do {                                              \
        if (!(cond)) {                                \
            printf("[TEST] FAIL: ");                  \
            printf(__VA_ARGS__);                      \
            printf(" (%s:%d)\n", __FILE__, __LINE__); \
            sXw6Failures++;                           \
        }                                             \
    } while (0)

namespace {

const uint32_t kXw6Seed = 0x1169A6A6u;
const uint32_t kXw6SettingsHash = 0x1169CA56u;
const uint32_t kXw6ProfileDigest = 0x1169D166u;

// OoT note values: D4 2, F4 5, A4 9, B4 11, D5 14 (sOcarinaNoteValues).
const uint8_t kXw6Soaring[6] = { 5, 11, 14, 5, 11, 14 };
// The checker's answer for Pierre's song: OCARINA_SONG_SCARECROW (12) + 1.
const int kXw6ScarecrowAnswer = 13;

/** A paired identity and the shipped combo record frozen with @p rules2 as the
 *  second word (raw: the hidden-bit refusal is Generate's), as XW4's rows do. */
void Xw6FreezeWorld(uint32_t rules2) {
    ComboContext_Init();
    Combo_ResetSharedResourceWatermarks();
    gComboCtx.sourceIsRando = true;
    gComboCtx.sharedRandoSeed = kXw6Seed;
    gComboCtx.sharedRandoSettingsHash = kXw6SettingsHash;
    gComboCtx.mmProfileDigest = kXw6ProfileDigest;
    ComboSettingsRecord rec;
    Combo_ComboSettingsDefaults(&rec);
    Combo_FreezeComboSettingsEx2(&rec, NULL, rules2);
}

std::string Xw6ReadSource(const char* rel) {
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

int Xw6Count(const std::string& hay, const char* needle) {
    int n = 0;
    const size_t len = strlen(needle);
    for (size_t at = hay.find(needle); at != std::string::npos; at = hay.find(needle, at + len)) {
        n++;
    }
    return n;
}

} // namespace

// ============================================================================
// soaring-owl-table
// ============================================================================

TestResult Test_SoaringOwlTable(void) {
    printf("[TEST] soaring-owl-table: the Song of Soaring's owls are MM's own, and the owl mask is exactly MM's "
           "frozen half's (#1169 XW6)\n");
    sXw6Failures = 0;

    // ---- (1) the table: MM's own entrances, OoTMM's names -----------------------------
    static const char* const kNames[RSBS_SOARING_OWL_COUNT] = {
        "Great Bay Coast", "Zora Cape", "Snowhead",       "Mountain Village", "Clock Town",
        "Milk Road",       "Woodfall",  "Southern Swamp", "Ikana Canyon",     "Stone Tower",
    };
    for (int owl = 0; owl < RSBS_SOARING_OWL_COUNT; owl++) {
        XW6_CHECK(Combo_SoaringOwlEntrance(owl) != 0 &&
                      Combo_SoaringOwlEntrance(owl) == MM_SoaringTest_OwlEntrance(owl),
                  "owl %d lands on 0x%04X, MM's own table says 0x%04X", owl, Combo_SoaringOwlEntrance(owl),
                  MM_SoaringTest_OwlEntrance(owl));
        XW6_CHECK(Combo_SoaringOwlName(owl) != NULL && strcmp(Combo_SoaringOwlName(owl), kNames[owl]) == 0,
                  "owl %d is named '%s', OoTMM's kSoarNames says '%s'", owl,
                  Combo_SoaringOwlName(owl) ? Combo_SoaringOwlName(owl) : "(null)", kNames[owl]);
    }
    XW6_CHECK(Combo_SoaringOwlEntrance(-1) == 0 && Combo_SoaringOwlEntrance(RSBS_SOARING_OWL_COUNT) == 0 &&
                  Combo_SoaringOwlName(RSBS_SOARING_OWL_COUNT) == NULL,
              "an out-of-range owl has an entrance or a name");
    // The UNCLEARED variants: MM's Play_Init resolves the cleared ones.
    XW6_CHECK(Combo_SoaringOwlEntrance(7) == 0x84A0 && Combo_SoaringOwlEntrance(3) == 0x9A80,
              "the swamp and mountain owls must be the poisoned and winter entrances (MM's Play_Init remaps)");

    // ---- (2) the layout is registered by MM_SlotMeta_Register ------------------------
    Context_ClearAllFrozenStates();
    Combo_Soaring_RegisterMMDesc(NULL);
    MM_SoaringTest_FreezeHalf(0x0081u, 1, 1);
    XW6_CHECK(Combo_SoaringOwlMask() == 0, "an unregistered layout read owls 0x%03X", Combo_SoaringOwlMask());
    MM_SlotMeta_Register();
    XW6_CHECK(Combo_Soaring_MMDescRegistered(),
              "MM_SlotMeta_Register did not register the Soaring layout: production would always say no statues");

    // ---- (3) exactly the frozen half's owls --------------------------------------------
    MM_SoaringTest_FreezeHalf(0x0081u, 1, 1);
    XW6_CHECK(Combo_SoaringOwlMask() == 0x0081u, "Great Bay Coast and the swamp frozen, read 0x%03X",
              Combo_SoaringOwlMask());
    MM_SoaringTest_FreezeHalf(0x0010u, 3, 1);
    XW6_CHECK(Combo_SoaringOwlMask() == 0x0010u, "Clock Town on day 3 frozen, read 0x%03X", Combo_SoaringOwlMask());
    MM_SoaringTest_FreezeHalf(0xFFFFu, 2, 1);
    XW6_CHECK(Combo_SoaringOwlMask() == RSBS_SOARING_OWL_BITS, "every bit frozen, read 0x%04X (want the ten owls)",
              Combo_SoaringOwlMask());

    // ---- (4) the zero cases --------------------------------------------------------------
    MM_SoaringTest_FreezeHalf(0x8000u, 1, 1);
    XW6_CHECK(Combo_SoaringOwlMask() == 0, "MM's hidden owl alone offered owls 0x%03X", Combo_SoaringOwlMask());
    MM_SoaringTest_FreezeHalf(0x0081u, 0, 1);
    XW6_CHECK(Combo_SoaringOwlMask() == 0, "a day-0 half (never entered) offered owls 0x%03X", Combo_SoaringOwlMask());
    MM_SoaringTest_FreezeHalf(0x0081u, 1, 0);
    XW6_CHECK(Combo_SoaringOwlMask() == 0, "an unmarked half offered owls 0x%03X", Combo_SoaringOwlMask());
    {
        // A marked shadow that is NOT frozen (MM never left): no frozen copy.
        MM_SoaringTest_FreezeHalf(0x0081u, 1, 1);
        std::vector<uint8_t> shadow(MM_SAVE_CONTEXT_SIZE, 0);
        const void* frozen = Context_GetMMSaveContext();
        XW6_CHECK(frozen != NULL, "no MM shadow after a freeze (setup)");
        if (frozen != NULL) {
            std::memcpy(shadow.data(), frozen, shadow.size());
        }
        XW6_CHECK(Combo_SoaringOwlMaskFromBlob(shadow.data(), shadow.size()) == 0x0081u,
                  "the copied blob does not read as frozen (setup)");
        Context_ClearFrozenState(GAME_MM);
        Context_UpdateShadowCopy(GAME_MM, shadow.data(), shadow.size());
        XW6_CHECK(!Context_HasFrozenState(GAME_MM), "the shadow copy armed a frozen state (setup)");
        XW6_CHECK(Combo_SoaringOwlMask() == 0, "a shadow that is not frozen offered owls 0x%03X",
                  Combo_SoaringOwlMask());
        XW6_CHECK(Combo_SoaringOwlMaskFromBlob(shadow.data(), 16) == 0, "a blob too small for the layout was read");
        XW6_CHECK(Combo_SoaringOwlMaskFromBlob(NULL, shadow.size()) == 0, "a NULL blob was read");
    }

    Context_ClearAllFrozenStates();
    printf("[TEST] soaring-owl-table: %s\n", sXw6Failures == 0 ? "PASS" : "FAIL");
    return sXw6Failures == 0 ? TEST_PASS : TEST_FAIL;
}

// ============================================================================
// oot-soaring-gates
// ============================================================================

TestResult Test_OoTSoaringGates(void) {
    printf("[TEST] oot-soaring-gates: the Song of Soaring's gates, its chooser and every exit's ocarina mode "
           "(#1169 XW6)\n");
    sXw6Failures = 0;
    const uint16_t kSwamp = 0x0080u;

    // ---- (1) the match gates --------------------------------------------------------------
    XW6_CHECK(Combo_SoaringMatchArmed(true, true, true, true, true), "every gate on did not arm the match");
    XW6_CHECK(!Combo_SoaringMatchArmed(false, true, true, true, true), "the rule off armed the match");
    XW6_CHECK(!Combo_SoaringMatchArmed(true, false, true, true, true), "an unpaired world armed the match");
    XW6_CHECK(!Combo_SoaringMatchArmed(true, true, false, true, true), "an unowned song armed the match");
    XW6_CHECK(!Combo_SoaringMatchArmed(true, true, true, false, true),
              "an adult armed the match (OoTMM: child only, no ageless Soaring)");
    XW6_CHECK(!Combo_SoaringMatchArmed(true, true, true, true, false), "an actor's ocarina check armed the match");
    XW6_CHECK(Combo_SoaringOnPlayed(true) == COMBO_SOARING_PLAYED_ACCEPT &&
                  Combo_SoaringOnPlayed(false) == COMBO_SOARING_PLAYED_FAIL,
              "OCARINA_PLAYING's Soaring arm does not accept free play and fail the rest");

    // ---- (2) SONG_PLAYED_ACT's gates ---------------------------------------------------
    XW6_CHECK(Combo_SoaringDecide(false, 0, true, kSwamp) == COMBO_SOARING_GATE_OFFER, "an open scene did not offer");
    // Restriction 3 is the reachable one (Gerudo Training Ground, the Windmill and
    // Dampe's Grave, Ganon's Castle: flags2 0x03, ocarina allowed), where the
    // randomizer lets vanilla warp songs through. 1 and 2 are defensive: every
    // such scene also forbids the ocarina (z_parameter.c's restriction table).
    for (uint8_t restriction = 1; restriction <= 3; restriction++) {
        XW6_CHECK(Combo_SoaringDecide(false, restriction, true, kSwamp) == COMBO_SOARING_GATE_RESTRICTED,
                  "warp-song restriction %u did not refuse (ruling 5: any nonzero restriction)", (unsigned)restriction);
    }
    XW6_CHECK(Combo_SoaringDecide(true, 0, true, kSwamp) == COMBO_SOARING_GATE_RESTRICTED,
              "disableWarpSongs did not refuse");
    XW6_CHECK(Combo_SoaringDecide(false, 0, false, kSwamp) == COMBO_SOARING_GATE_MM_UNPLAYABLE,
              "an unplayable Majora's Mask did not refuse");
    XW6_CHECK(Combo_SoaringDecide(false, 0, true, 0) == COMBO_SOARING_GATE_NO_STATUES, "no owl did not refuse");
    XW6_CHECK(Combo_SoaringDecide(false, 0, true, 0x8000u) == COMBO_SOARING_GATE_NO_STATUES,
              "MM's hidden owl alone was offered");
    XW6_CHECK(Combo_SoaringDecide(false, 3, false, 0) == COMBO_SOARING_GATE_RESTRICTED &&
                  Combo_SoaringDecide(false, 0, false, 0) == COMBO_SOARING_GATE_MM_UNPLAYABLE,
              "the gates are not asked in order (restriction, then MM, then the owls)");
    static const ComboSoaringGate kGates[4] = { COMBO_SOARING_GATE_OFFER, COMBO_SOARING_GATE_RESTRICTED,
                                                COMBO_SOARING_GATE_MM_UNPLAYABLE, COMBO_SOARING_GATE_NO_STATUES };
    for (int g = 0; g < 4; g++) {
        const uint8_t mode = Combo_SoaringOcarinaModeAfterGate(kGates[g]);
        XW6_CHECK(mode != 2u, "gate %d leaves OCARINA_MODE_02, which would send Player to sWarpSongEntrances", g);
        XW6_CHECK(mode == (g == 0 ? RSBS_OOT_OCARINA_MODE_ACTIVE : RSBS_OOT_OCARINA_MODE_END),
                  "gate %d leaves ocarina mode %u (a refusal must end in 04, the chooser stays in 01)", g,
                  (unsigned)mode);
    }

    // ---- (3) the chooser -------------------------------------------------------------------
    XW6_CHECK(Combo_SoaringFirstOwl(0) == -1 && Combo_SoaringFirstOwl(0x8000u) == -1, "the chooser opened on no owl");
    XW6_CHECK(Combo_SoaringFirstOwl(0x0091u) == RSBS_SOARING_OWL_CLOCK_TOWN,
              "the chooser did not open on Clock Town (OoTMM's cursor)");
    XW6_CHECK(Combo_SoaringFirstOwl(0x0081u) == 0, "without Clock Town the chooser did not open on the lowest owl");
    XW6_CHECK(Combo_SoaringNextOwl(0x0081u, 0) == 7 && Combo_SoaringNextOwl(0x0081u, 7) == 0,
              "Next statue did not cycle Great Bay Coast -> swamp -> Great Bay Coast (got %d, %d)",
              Combo_SoaringNextOwl(0x0081u, 0), Combo_SoaringNextOwl(0x0081u, 7));
    XW6_CHECK(Combo_SoaringNextOwl(kSwamp, 7) == 7, "one owl did not stay on itself");
    XW6_CHECK(Combo_SoaringNextOwl(0, 0) == -1, "Next statue found an owl in an empty mask");
    XW6_CHECK(Combo_SoaringChooserInput(false, false, 0) == COMBO_SOARING_CHOICE_WAIT,
              "the chooser acted on a frame with no input");
    XW6_CHECK(Combo_SoaringChooserInput(true, true, 0) == COMBO_SOARING_CHOICE_SOAR, "A on Soar did not soar");
    XW6_CHECK(Combo_SoaringChooserInput(true, true, 1) == COMBO_SOARING_CHOICE_NEXT, "A on Next statue did not step");
    XW6_CHECK(Combo_SoaringChooserInput(true, true, 2) == COMBO_SOARING_CHOICE_CANCEL, "A on No did not cancel");
    XW6_CHECK(Combo_SoaringChooserInput(true, false, 0) == COMBO_SOARING_CHOICE_CANCEL,
              "B (or C-up) on Soar soared: the cursor opens on Soar, and OoTMM's picker cancels on B");
    XW6_CHECK(Combo_SoaringChooserInput(true, false, 1) == COMBO_SOARING_CHOICE_CANCEL, "B on Next statue stepped");
    XW6_CHECK(Combo_SoaringChooserInput(true, true, 7) == COMBO_SOARING_CHOICE_CANCEL, "an unknown choice acted");
    // Open the chooser, then close it without Soar: Link must get control back.
    {
        uint8_t mode = Combo_SoaringOcarinaModeAfterGate(COMBO_SOARING_GATE_OFFER);
        XW6_CHECK(mode == RSBS_OOT_OCARINA_MODE_ACTIVE, "opening the chooser ended the ocarina (mode %u)",
                  (unsigned)mode);
        mode = Combo_SoaringOcarinaModeAfterChoice(Combo_SoaringChooserInput(false, false, 0));
        XW6_CHECK(mode == RSBS_OOT_OCARINA_MODE_ACTIVE, "waiting ended the ocarina (mode %u)", (unsigned)mode);
        mode = Combo_SoaringOcarinaModeAfterChoice(Combo_SoaringChooserInput(true, true, 1));
        XW6_CHECK(mode == RSBS_OOT_OCARINA_MODE_ACTIVE, "stepping ended the ocarina (mode %u)", (unsigned)mode);
        mode = Combo_SoaringOcarinaModeAfterChoice(Combo_SoaringChooserInput(true, false, 0));
        XW6_CHECK(mode == RSBS_OOT_OCARINA_MODE_END, "closing the chooser without Soar left ocarina mode %u, not 04",
                  (unsigned)mode);
        XW6_CHECK(Combo_SoaringOcarinaModeAfterChoice(COMBO_SOARING_CHOICE_SOAR) == RSBS_OOT_OCARINA_MODE_END,
                  "a departure did not end the ocarina");
    }

    // ---- (4) from source: the branches act on these decisions ---------------------------
    const std::string msg = Xw6ReadSource("games/oot/src/code/z_message_PAL.c");
    const std::string oot = Xw6ReadSource("games/oot/soh/Enhancements/randomizer/SongOfSoaringOoT.cpp");
    const std::string mmPlay = Xw6ReadSource("games/mm/src/code/z_play.c");
    if (msg.empty() || oot.empty() || mmPlay.empty()) {
        XW6_CHECK(false, "the sources are not readable under RSBS_SOURCE_DIR");
    } else {
        XW6_CHECK(Xw6Count(msg, "msgCtx->ocarinaStaff->state == RSBS_OOT_SONG_SOARING") == 1 &&
                      Xw6Count(msg, "Combo_SoaringOnPlayed(msgCtx->ocarinaAction == OCARINA_ACTION_FREE_PLAY)") == 1,
                  "E1: OCARINA_PLAYING has no Soaring arm (the state would fail the ocarina a frame later)");
        XW6_CHECK(Xw6Count(msg, "OoT_SoaringOoT_StartPlayback();") == 1 &&
                      Xw6Count(msg, "msgCtx->lastPlayedSong != RSBS_OOT_SONG_SOARING") == 1,
                  "E2: the playback or the fanfare would index a song table with the Soaring state");
        XW6_CHECK(Xw6Count(msg, "OoT_Message_ContinueTextbox(play, RSBS_OOT_TEXT_SOARING_PLAYED);") == 1,
                  "E3: \"You played\" would open text 0x893 + the Soaring state");
        XW6_CHECK(Xw6Count(msg, "OoT_SoaringOoT_OnSongPlayed(play);") == 1,
                  "E4: SONG_PLAYED_ACT does not hand the Soaring state to its gates");
        XW6_CHECK(Xw6Count(oot, "Combo_SoaringDecide(") == 1 && Xw6Count(oot, "Combo_SoaringChooserInput(") == 1 &&
                      Xw6Count(oot, "Combo_SoaringOcarinaModeAfterGate(") >= 2 &&
                      Xw6Count(oot, "Combo_SoaringOcarinaModeAfterChoice(") >= 2,
                  "SongOfSoaringOoT.cpp does not act on the gates and the chooser decided here");
        XW6_CHECK(Xw6Count(oot, "OoT_Message_ContinueTextbox(play, RSBS_OOT_TEXT_SOARING_CHOOSER);") == 2 &&
                      Xw6Count(oot, "OoT_Message_StartTextbox(play, RSBS_OOT_TEXT_SOARING_CHOOSER") == 0,
                  "the chooser must open and step with ContinueTextbox: StartTextbox drops FREE_PLAY_DONE and the "
                  "vanilla close never hands control back");
        XW6_CHECK(Xw6Count(oot, "OCARINA_MODE_02") == 0, "SongOfSoaringOoT.cpp sets OCARINA_MODE_02");
        XW6_CHECK(Xw6Count(oot, "CHECK_BTN_ALL(input->press.button, BTN_A)") == 1,
                  "the chooser does not read A itself (ShouldAdvance alone would soar on B)");
        XW6_CHECK(
            Xw6Count(oot, "ComboWarp_Arm(GAME_OOT, marker, GAME_MM, target, (uint8_t)RSBS_SOARING_SWITCH_KIND)") == 1,
            "the departure does not arm the one-shot target with the Soaring kind");
        XW6_CHECK(Xw6Count(mmPlay, "hadFrozenState && !soaringArrival && gSaveContext.save.isOwlSave") == 1,
                  "MM's arrival would land a Soaring arrival on a pause-save spot, not the chosen owl");
    }

    printf("[TEST] oot-soaring-gates: %s\n", sXw6Failures == 0 ? "PASS" : "FAIL");
    return sXw6Failures == 0 ? TEST_PASS : TEST_FAIL;
}

// ============================================================================
// oot-soaring-matcher
// ============================================================================

TestResult Test_OoTSoaringMatcher(void) {
    printf("[TEST] oot-soaring-matcher: OoT's real free-play checker matches the Song of Soaring only behind every "
           "gate and never instead of a vanilla song (#1169 XW6)\n");
    sXw6Failures = 0;
    const int kSoaringAnswer = (int)RSBS_OOT_SONG_SOARING + 1;

    // ---- (1) every gate ----------------------------------------------------------------------
    ComboContext_Init();
    XW6_CHECK(OoT_SoaringMatcherTest_Play(kXw6Soaring, 6, 1, 1, 1) == 0,
              "with nothing frozen (the rule off) the Song of Soaring matched");
    Xw6FreezeWorld(0u);
    XW6_CHECK(OoT_SoaringMatcherTest_Play(kXw6Soaring, 6, 1, 1, 1) == 0,
              "with the rule frozen off the Song of Soaring matched");
    Xw6FreezeWorld(RSBS_XITEM2_SONG_SOARING_OOT);
    XW6_CHECK(OoT_SongOfSoaringOoTOn(), "the rule did not freeze on (setup)");
    XW6_CHECK(OoT_SoaringMatcherTest_Play(kXw6Soaring, 6, 1, 0, 1) == 0, "the Song of Soaring matched unowned");
    XW6_CHECK(OoT_SoaringMatcherTest_Play(kXw6Soaring, 6, 0, 1, 1) == 0,
              "the Song of Soaring matched for an adult (crossing is child-only)");
    XW6_CHECK(OoT_SoaringMatcherTest_Play(kXw6Soaring, 6, 1, 1, 0) == 0,
              "the Song of Soaring matched under an actor's check-no-warp (the frogs)");
    gComboCtx.sharedRandoSettingsHash = 0u;
    XW6_CHECK(OoT_SongOfSoaringOoTOn(), "clearing the pairing cleared the rule (setup)");
    XW6_CHECK(OoT_SoaringMatcherTest_Play(kXw6Soaring, 6, 1, 1, 1) == 0,
              "the Song of Soaring matched without a paired world");

    // ---- (2) the song itself, and every vanilla song unchanged -------------------------------
    Xw6FreezeWorld(RSBS_XITEM2_SONG_SOARING_OOT);
    XW6_CHECK(OoT_SoaringMatcherTest_Play(kXw6Soaring, 6, 1, 1, 1) == kSoaringAnswer,
              "a child who owns it played the Song of Soaring and the checker answered %d, not %d",
              OoT_SoaringMatcherTest_Play(kXw6Soaring, 6, 1, 1, 1), kSoaringAnswer);
    for (int song = 0; song < 12; song++) {
        const int answer = OoT_SoaringMatcherTest_PlayVanilla(song);
        XW6_CHECK(answer == song + 1, "vanilla song %d answered %d with every Soaring gate on, not %d", song, answer,
                  song + 1);
    }

    // ---- (3) the tail rule ---------------------------------------------------------------------
    {
        const uint8_t junkFirst[8] = { 9, 2, 5, 11, 14, 5, 11, 14 };
        XW6_CHECK(OoT_SoaringMatcherTest_Play(junkFirst, 8, 1, 1, 1) == kSoaringAnswer,
                  "two other notes before the song stopped it matching");
        const uint8_t longJunk[16] = { 2, 9, 2, 9, 2, 9, 2, 9, 2, 9, 5, 11, 14, 5, 11, 14 };
        XW6_CHECK(OoT_SoaringMatcherTest_Play(longJunk, 16, 1, 1, 1) == kSoaringAnswer,
                  "the song after the 8-note buffer rolled over did not match");
        XW6_CHECK(OoT_SoaringMatcherTest_Play(kXw6Soaring, 5, 1, 1, 1) == 0, "five of its six notes matched");
        const uint8_t sharp[6] = { 5, 11, 15, 5, 11, 14 };
        XW6_CHECK(OoT_SoaringMatcherTest_Play(sharp, 6, 1, 1, 1) == 0, "a sharpened third note matched");
        const uint8_t bent[6] = { 5, 11, 14, 5, 11, 0xFF };
        XW6_CHECK(OoT_SoaringMatcherTest_Play(bent, 6, 1, 1, 1) == 0, "a bent (unpitched) last note matched");
    }

    // ---- (4) Pierre's song (decision 8: as OoTMM) ---------------------------------------------
    {
        // Pierre's song ENDS with the Song of Soaring: both finish on the same
        // note, and the vanilla loop's answer (the scarecrow) stands.
        const uint8_t pierreEnds[8] = { 9, 9, 5, 11, 14, 5, 11, 14 };
        XW6_CHECK(OoT_SoaringMatcherTest_PlayWithPierre(pierreEnds, pierreEnds, 8) == kXw6ScarecrowAnswer,
                  "Pierre's song and the Song of Soaring finishing on the same note did not answer Pierre");
        // Pierre's song BEGINS with it: Soaring finishes first and wins.
        const uint8_t pierreBegins[8] = { 5, 11, 14, 5, 11, 14, 9, 9 };
        XW6_CHECK(OoT_SoaringMatcherTest_PlayWithPierre(pierreBegins, pierreBegins, 8) == kSoaringAnswer,
                  "the Song of Soaring finishing before Pierre's song did not answer Soaring");
        XW6_CHECK(OoT_SoaringMatcherTest_Play(kXw6Soaring, 6, 1, 1, 1) == kSoaringAnswer,
                  "Pierre's song was not restored (setup)");
    }

    // ---- (5) from source: one guarded call, after the vanilla loop ---------------------------
    const std::string audio = Xw6ReadSource("games/oot/src/code/code_800EC960.c");
    if (audio.empty()) {
        XW6_CHECK(false, "code_800EC960.c is not readable under RSBS_SOURCE_DIR");
    } else {
        const size_t call = audio.find("D_80131878 == 0 && OoT_SoaringOoT_NoteMatched(sCurOcarinaSong");
        const size_t vanilla = audio.find("D_80131878 = i + 1;");
        XW6_CHECK(Xw6Count(audio, "OoT_SoaringOoT_NoteMatched(") == 2 && call != std::string::npos &&
                      vanilla != std::string::npos && vanilla < call,
                  "the Soaring check is not one guarded call after func_800ED200's vanilla loop");
        XW6_CHECK(Xw6Count(audio, "sOcarinaAvailSongs = flg & 0x3FFF;") == 1,
                  "the vanilla song mask was widened (OoTMM never widens it)");
    }

    ComboContext_Init();
    printf("[TEST] oot-soaring-matcher: %s\n", sXw6Failures == 0 ? "PASS" : "FAIL");
    return sXw6Failures == 0 ? TEST_PASS : TEST_FAIL;
}
