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
 *   the tail rule (junk first, five notes, a sharpened note, a bent last note);
 *   Pierre's song wins only when both finish on the same note, and its
 *   recorded row does not outlive the leg.
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
#include "../warp_song_soaring_map.h"

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
void MM_SoaringTest_FreezeHalfClouds(uint16_t owls, int32_t day, int marked, uint32_t clouds);
// games/oot/soh/oot_soaring_matcher_test.cpp
int OoT_SoaringMatcherTest_Play(const uint8_t* notes, int count, int child, int owned, int freePlay);
int OoT_SoaringMatcherTest_PlayVanilla(int song);
int OoT_SoaringMatcherTest_PlayWithPierre(const uint8_t* pierre, const uint8_t* notes, int count);
int OoT_SoaringMatcherTest_PlayBentLast(const uint8_t* notes, int count);
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

/** The body of the function whose definition starts with @p signature, up to
 *  the first closing brace at column 0 ("" when absent): the XW8 anchors search
 *  inside the function they claim to check, never from the top of the file. */
std::string Xw6FnBody(const std::string& src, const char* signature) {
    const size_t at = src.find(signature);
    if (at == std::string::npos) {
        return std::string();
    }
    const size_t end = src.find("\n}\n", at);
    return end == std::string::npos ? std::string() : src.substr(at, end - at + 2);
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

    // ---- (5) #1169 XW8: the owl map's clouds, from the same half under the same guard ------
    MM_SoaringTest_FreezeHalfClouds(0x0081u, 1, 1, 0x1E03u);
    XW6_CHECK(Combo_SoaringCloudMask() == 0x1E03u,
              "Clock Town's and Great Bay's clouds cleared in the frozen half, the map reads 0x%04X (is the cloud "
              "offset registered by MM_SlotMeta_Register?)",
              Combo_SoaringCloudMask());
    MM_SoaringTest_FreezeHalfClouds(0x0081u, 2, 1, 0xFFFFFFFFu);
    XW6_CHECK(Combo_SoaringCloudMask() == 0x7FFFu, "every cloud bit set, the map reads 0x%04X (want MM's fifteen)",
              Combo_SoaringCloudMask());
    MM_SoaringTest_FreezeHalfClouds(0x0081u, 0, 1, 0x1E03u);
    XW6_CHECK(Combo_SoaringCloudMask() == 0, "a day-0 half's clouds were read (0x%04X)", Combo_SoaringCloudMask());
    MM_SoaringTest_FreezeHalfClouds(0x0081u, 1, 0, 0x1E03u);
    XW6_CHECK(Combo_SoaringCloudMask() == 0, "an unmarked half's clouds were read (0x%04X)", Combo_SoaringCloudMask());
    MM_SoaringTest_FreezeHalfClouds(0x0081u, 1, 1, 0x1E03u);
    Context_ClearFrozenState(GAME_MM);
    XW6_CHECK(Combo_SoaringCloudMask() == 0, "an unfrozen half's clouds were read (0x%04X)", Combo_SoaringCloudMask());

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

    // ---- (5) #1169 XW8: the owl map's guarded kaleido arms and its wiring -----------------
    // Each anchor searches inside the function it names. The pure decisions
    // they act on are soaring-map-picker's.
    const std::string call = Xw6ReadSource("games/oot/src/code/z_kaleido_scope_call.c");
    const std::string pal = Xw6ReadSource("games/oot/src/overlays/misc/ovl_kaleido_scope/z_kaleido_scope_PAL.c");
    const std::string mapOoT = Xw6ReadSource("games/oot/soh/Enhancements/randomizer/SoaringMapOoT.c");
    if (call.empty() || pal.empty() || mapOoT.empty() || oot.empty()) {
        XW6_CHECK(false, "the kaleido sources or SoaringMapOoT.c are not readable under RSBS_SOURCE_DIR");
    } else {
        // K1: the waiting states, before the call layer's catch-all.
        const std::string callUpdate = Xw6FnBody(call, "void OoT_KaleidoScopeCall_Update(PlayState* play) {");
        const size_t k1 = callUpdate.find("} else if (OoT_SoaringMap_CallLayerOwns(pauseCtx->state)) {");
        const size_t catchAll = callUpdate.find("} else if (pauseCtx->state != 0) {");
        XW6_CHECK(k1 != std::string::npos && catchAll != std::string::npos && k1 < catchAll &&
                      Xw6Count(callUpdate, "OoT_SoaringMap_CallLayerUpdate(play);") == 1,
                  "K1: the call layer does not hand SOARING_0/_1 to the map before its catch-all");
        // K2: the draw gate.
        const std::string callDraw = Xw6FnBody(call, "void OoT_KaleidoScopeCall_Draw(PlayState* play) {");
        const size_t k2 = callDraw.find("if (OoT_SoaringMap_CallLayerDraws(play->pauseCtx.state)) {");
        const size_t drawGate = callDraw.find("((play->pauseCtx.state >= 11) && (play->pauseCtx.state <= 18))");
        XW6_CHECK(Xw6Count(callDraw, "if (OoT_SoaringMap_CallLayerDraws(play->pauseCtx.state)) {") == 1 &&
                      k2 != std::string::npos && drawGate != std::string::npos && k2 < drawGate,
                  "K2: the call layer does not draw the map's states before its vanilla draw gate (the close's "
                  "0x12 frame would reach kaleido's page draw)");
        // K3: the update, before the vanilla head block and switch.
        const std::string palUpdate = Xw6FnBody(pal, "void OoT_KaleidoScope_Update(PlayState* play) {");
        const size_t k3 = palUpdate.find("if (OoT_SoaringMap_OwnsUpdate(pauseCtx->state)) {");
        const size_t head = palUpdate.find("if ((R_PAUSE_MENU_MODE >= 3)");
        XW6_CHECK(k3 != std::string::npos && head != std::string::npos && k3 < head &&
                      Xw6Count(palUpdate, "if (OoT_SoaringMap_TakeButtonSave()) {") == 1 &&
                      Xw6Count(palUpdate, "OoT_SoaringMap_Update(play);") == 1,
                  "K3: kaleido's update does not hand the map's states (and its 0x12 frame) to the map before its "
                  "head block, or does not save the buttons 0x13 restores");
        // K4: the draw, before any vanilla page draw.
        const std::string palDraw = Xw6FnBody(pal, "void OoT_KaleidoScope_Draw(PlayState* play) {");
        const size_t k4 = palDraw.find("if (OoT_SoaringMap_OwnsDraw(pauseCtx->state)) {");
        const size_t open = palDraw.find("OPEN_DISPS(");
        XW6_CHECK(k4 != std::string::npos && open != std::string::npos && k4 < open &&
                      Xw6Count(palDraw, "OoT_SoaringMap_Draw(play);") == 1,
                  "K4: kaleido's draw runs InitVertices and the pages over the map's states (stale segments)");

        // The route: OnSongPlayed switches on it and keeps the fallback chooser.
        const std::string played = Xw6FnBody(oot, "extern \"C\" void OoT_SoaringOoT_OnSongPlayed(PlayState* play) {");
        XW6_CHECK(Xw6Count(played, "switch (Combo_SoaringMap_Route(gate, assets, pause)) {") == 1 &&
                      Xw6Count(played, "OoT_SoaringMap_Begin(play, owlMask)") == 1 &&
                      Xw6Count(played, "OoT_SoaringOoT_BeginChooser(play, owlMask);") == 2 &&
                      Xw6Count(played, "OoT_SoaringMap_AssetsPresent()") == 1 &&
                      Xw6Count(played, "OoT_SoaringMap_PauseCanOpen(play)") == 1,
                  "OnSongPlayed does not route the song to the map with XW6's chooser as the fallback");
        XW6_CHECK(Xw6Count(oot, "COND_ID_HOOK(OnOpenText, RSBS_OOT_TEXT_SOARING_CONFIRM, true, "
                                "SoaringOoTBuildConfirm);") == 1 &&
                      Xw6Count(oot, "COND_HOOK(OnKaleidoscopeUpdate, true, SoaringOoTMapOnKaleidoscopeUpdate);") == 1 &&
                      Xw6Count(oot, "COND_HOOK(OnPlayerUpdate, true, SoaringOoTMapBackstop);") == 1,
                  "the map's prompt text, its close's end or its backstop is not registered");

        // The map file: no cutscene predicate (the ocarina sets the flag it
        // reads), no 02, the HUD only through unk_13EE (one backstop call).
        XW6_CHECK(Xw6Count(mapOoT, "Play_InCsMode") == 0 && Xw6Count(mapOoT, "Player_InCsMode") == 0,
                  "SoaringMapOoT.c asks a cutscene predicate the ocarina itself makes true: the map would never open");
        XW6_CHECK(Xw6Count(mapOoT, "OCARINA_MODE_02") == 0, "SoaringMapOoT.c sets OCARINA_MODE_02");
        const std::string backstop = Xw6FnBody(mapOoT, "void OoT_SoaringMap_Backstop(PlayState* play) {");
        XW6_CHECK(Xw6Count(mapOoT, "Interface_ChangeAlpha(") == 1 &&
                      Xw6Count(backstop, "Interface_ChangeAlpha(RSBS_OOT_HUD_RESTORE_ALL);") == 1,
                  "the map changes the HUD mode outside its backstop (the close restores it through unk_13EE)");
        // The backstop never reaches 0x13, so it undoes what 0x13 would.
        XW6_CHECK(Xw6Count(backstop, "if (state == 0) {") == 1 && Xw6Count(backstop, "R_UPDATE_RATE = 3;") == 1 &&
                      Xw6Count(backstop, "R_PAUSE_MENU_MODE = 0;") == 1 &&
                      Xw6Count(backstop, "func_800F64E0(0);") == 1 &&
                      Xw6Count(backstop, "Interface_SetDoAction(play, DO_ACTION_NONE);") == 1 &&
                      Xw6Count(backstop, "gSaveContext.buttonStatus[i] = buttonSave[i];") == 1,
                  "the backstop leaves the pause's update rate, menu mode, sound duck, labels or buttons set");
        const std::string close = Xw6FnBody(mapOoT, "static void SoaringMapEnterClose(PlayState* play, "
                                                    "ComboSoaringMapEvent ev) {");
        XW6_CHECK(Xw6Count(close, "gSaveContext.unk_13EE = Combo_SoaringMap_HudRestoreOnEvent(ev);") == 1 &&
                      Xw6Count(close, "func_800F64E0(0);") == 1 &&
                      Xw6Count(close, "Interface_SetDoAction(play, DO_ACTION_NONE);") == 1 &&
                      Xw6Count(close, "play->pauseCtx.state = RSBS_OOT_PAUSE_SOARING_6;") == 1,
                  "the map's close does not restore the HUD, play the close sound, clear A and roll out");
        const std::string labels = Xw6FnBody(mapOoT, "static void SoaringMapSetLabels(PlayState* play) {");
        XW6_CHECK(Xw6Count(labels, "Interface_SetDoAction(play, DO_ACTION_DECIDE);") == 1 &&
                      Xw6Count(labels, "Interface_LoadActionLabelB(play, DO_ACTION_STOP);") == 1 &&
                      Xw6Count(mapOoT, "SoaringMapSetLabels(play);") == 2,
                  "SOARING_2 and every BACK do not set A to Decide and B to Stop (OoTMM)");
        const std::string handOver = Xw6FnBody(mapOoT, "static void SoaringMapHandToUnpause(PlayState* play) {");
        XW6_CHECK(Xw6Count(handOver, "pauseCtx->state = RSBS_OOT_PAUSE_UNPAUSE_SETUP;") == 1 &&
                      Xw6Count(handOver, "RSBS_OOT_PAUSE_UNPAUSE_CLOSE") == 0 &&
                      Xw6Count(mapOoT, "SoaringMapHandToUnpause(play);") == 1,
                  "SOARING_6 must hand to vanilla 0x12 (PauseBufferInputs' state 18), not straight to 0x13");
        const std::string picker = Xw6FnBody(mapOoT, "static void SoaringMapUpdatePicker(PlayState* play) {");
        XW6_CHECK(Xw6Count(picker, "OoT_Message_StartTextbox(play, RSBS_OOT_TEXT_SOARING_CONFIRM, NULL);") == 1 &&
                      Xw6Count(picker, "Combo_SoaringMap_Step(picker, &in)") == 1 &&
                      Xw6Count(picker, "if (wasConfirm && picker->phase == COMBO_SOARING_MAP_CONFIRM) {") == 1 &&
                      Xw6Count(mapOoT, "OoT_Message_Update(play);") == 1,
                  "the prompt is not opened by the picker's event, or Message_Update runs outside a kept prompt");
        const size_t msgUpdate = picker.find("OoT_Message_Update(play);");
        const size_t keepDecide = picker.find("Interface_SetDoAction(play, DO_ACTION_DECIDE);");
        XW6_CHECK(msgUpdate != std::string::npos && keepDecide != std::string::npos && msgUpdate < keepDecide,
                  "the prompt leaves A reading Next (Message_Update's TEXT_STARTING frame) instead of Decide");
        const std::string end = Xw6FnBody(mapOoT, "void OoT_SoaringMap_OnKaleidoscopeUpdate(PlayState* play) {");
        XW6_CHECK(Xw6Count(mapOoT, "OoT_SoaringOoT_Depart(") == 1 &&
                      Xw6Count(end, "OoT_SoaringOoT_Depart(play, owl);") == 1 &&
                      Xw6Count(end, "play->pauseCtx.state != 0") == 1 &&
                      Xw6Count(end, "play->msgCtx.ocarinaMode = Combo_SoaringMap_OcarinaModeOnHandBack();") == 1,
                  "the map departs outside the frame its pause closed, or a cancel does not hand back 04");
    }

    printf("[TEST] oot-soaring-gates: %s\n", sXw6Failures == 0 ? "PASS" : "FAIL");
    return sXw6Failures == 0 ? TEST_PASS : TEST_FAIL;
}

// ============================================================================
// oot-soaring-matcher
// ============================================================================

// The row's body. The checker reads a CVar on every note (func_800ED200,
// CustomOcarina.Enabled), so the runner brings up the display-free shared context
// first (Test_OoTSoaringMatcher, test_runner.cpp), as the XW5 rows do.
static TestResult OoTSoaringMatcher_Run(void) {
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
        // The song's own six notes with the last one bent: func_800ED200 stores
        // a bent note as 0xFF, so a matcher that read the live button (or
        // skipped unpitched notes) instead of the buffer would match here.
        XW6_CHECK(OoT_SoaringMatcherTest_PlayBentLast(kXw6Soaring, 6) == 0, "a bent (unpitched) last note matched");
    }

    // ---- (4) Pierre's song (decision 8: as OoTMM) ---------------------------------------------
    {
        const uint8_t pierreEnds[8] = { 9, 9, 5, 11, 14, 5, 11, 14 };
        XW6_CHECK(OoT_SoaringMatcherTest_Play(pierreEnds, 8, 1, 1, 1) == kSoaringAnswer,
                  "eight notes ending in the Song of Soaring did not answer Soaring before any Pierre's song was "
                  "recorded (setup: the process started with one)");
        // Pierre's song BEGINS with it: Soaring finishes first and wins.
        const uint8_t pierreBegins[8] = { 5, 11, 14, 5, 11, 14, 9, 9 };
        XW6_CHECK(OoT_SoaringMatcherTest_PlayWithPierre(pierreBegins, pierreBegins, 8) == kSoaringAnswer,
                  "the Song of Soaring finishing before Pierre's song did not answer Soaring");
        // Pierre's song ENDS with the Song of Soaring: both finish on the same
        // note, and the vanilla loop's answer (the scarecrow) stands.
        XW6_CHECK(OoT_SoaringMatcherTest_PlayWithPierre(pierreEnds, pierreEnds, 8) == kXw6ScarecrowAnswer,
                  "Pierre's song and the Song of Soaring finishing on the same note did not answer Pierre");
        // The same eight notes with no Pierre's song: a restored scarecrow row
        // leaves Soaring's answer; a leaked pierreEnds row (the restore in
        // OoT_SoaringMatcherTest_PlayWithPierre dropped) would answer Pierre.
        XW6_CHECK(OoT_SoaringMatcherTest_Play(pierreEnds, 8, 1, 1, 1) == kSoaringAnswer,
                  "Pierre's song was not restored: the recorded row outlived its leg");
    }

    // ---- (5) from source: one guarded call, after the vanilla loop ---------------------------
    const std::string audio = Xw6ReadSource("games/oot/src/code/code_800EC960.c");
    if (audio.empty()) {
        XW6_CHECK(false, "code_800EC960.c is not readable under RSBS_SOURCE_DIR");
    } else {
        // Bounded to func_800ED200 itself: func_800ECDF8, above it, also sets
        // D_80131878 = i + 1, so an unbounded search would pass a call placed
        // anywhere after that function, including above func_800ED200's loop.
        const size_t fn = audio.find("void func_800ED200(void) {");
        const size_t fnEnd = fn == std::string::npos ? std::string::npos : audio.find("\nvoid ", fn + 1);
        const size_t vanilla = fn == std::string::npos ? std::string::npos : audio.find("D_80131878 = i + 1;", fn);
        const size_t call = fn == std::string::npos
                                ? std::string::npos
                                : audio.find("D_80131878 == 0 && OoT_SoaringOoT_NoteMatched(sCurOcarinaSong", fn);
        XW6_CHECK(Xw6Count(audio, "OoT_SoaringOoT_NoteMatched(") == 2 && fn != std::string::npos &&
                      fnEnd != std::string::npos && vanilla != std::string::npos && call != std::string::npos &&
                      fn < vanilla && vanilla < call && call < fnEnd,
                  "the Soaring check is not one guarded call after func_800ED200's vanilla loop");
        XW6_CHECK(Xw6Count(audio, "sOcarinaAvailSongs = flg & 0x3FFF;") == 1,
                  "the vanilla song mask was widened (OoTMM never widens it)");
    }

    ComboContext_Init();
    printf("[TEST] oot-soaring-matcher: %s\n", sXw6Failures == 0 ? "PASS" : "FAIL");
    return sXw6Failures == 0 ? TEST_PASS : TEST_FAIL;
}
