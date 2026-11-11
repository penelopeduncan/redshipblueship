/**
 * @file test_mm_warp_song_play.c
 * @brief Locks for #1169 lane XW7: Ocarina of Time's six warp songs PLAYED in
 *        Majora's Mask (src/common/mm_warp_song_play.{h,c}; MM's call sites in
 *        code_8019AF00.c, z_message.c, z_player.c, z_en_test7.c and
 *        SkipSoaringCutscene.cpp; the creation's record ComboOoTWarpRecord,
 *        context.h, ADR 0009 claim 22; OoT's arrival in randomizer_entrance.c).
 *
 * THREE ROM-FREE ROWS (redship tier, generating nothing; each brings up the
 * display-free shared context first, as mm-warp-songs-items does):
 *
 *   mm-warp-song-play (CTest MMWarpSongPlay) -- the game-neutral half, and every
 *   vendored branch by a source anchor scoped to the function (or switch case) it
 *   claims to sit in:
 *     P1 the record's layout (offset 1842, 20 bytes, sizeof(ComboContext) 1864,
 *        reserved[64] still at 939) and its validator over every malformed shape;
 *        a pre-XW7 record (the bytes before 1842 copied, zeros after) is UNSET;
 *     P2 the record is a creation record: a failed generation's rollback restores
 *        it, a retracted creation clears it, the file-create KEEP invalidation keeps
 *        it, a DROP clears it, and a seed package's world record carries it;
 *     P3 the pads (OoT's sWarpSongEntrances order) and their inverse; OoTMM's
 *        button lists, whose distinct buttons are XW5's can_play_tp_* sets;
 *     P4 the pure matcher: each song on its last note only; a usable mask; the
 *        recorded Requiem / Inverted Song of Time prefix steal; the wall helper
 *        (a steal before the last note, never a tie on it);
 *     P5 staff states and En_Test7's params (exactly 0x8000..0x8005);
 *     P6 the decision, gate by gate in order, every refusal shown as 0x1B95;
 *     P7 the texts: OoTMM's fixed text with hints off, OoT's RHT_WARP_SONG shape
 *        with them on ("a mysterious place" for an area with no name), nothing
 *        for an unset record or a short buffer;
 *     P8 OoT's frozen age over OoT's real layout (CHILD, ADULT, not started, not
 *        a live file, none, an odd value, unregistered);
 *     P9 MM-FIRST: with nothing but the frozen record, a child OoT half and a
 *        paired world, the gather offers, the prompt names the record's area, MM
 *        arms the vanilla pad, and OoT's arrival takes the record's destination;
 *        a grotto destination is NOT_READY until OoT's probe says ready;
 *     P10 the latches (pending, one-shot departure) and the writer's override
 *        resolution (the last row wins, no row is the pad);
 *     P11 source anchors (see MwpCheckAnchors).
 *
 *   mm-warp-song-matcher and mm-warp-song-departure (CTest MMWarpSongMatcher /
 *   MMWarpSongDeparture) drive MM's real code; their legs are
 *   games/mm/2s2h/mm_warp_song_play_test.cpp's.
 *
 * Linkage note: #included into test_runner.cpp at FILE SCOPE (compiled as C++),
 * like its neighbours; every symbol under test is C-linkage except entrance.h's
 * C++ API. Helpers carry the Mwp prefix because every included test file shares
 * one translation unit.
 */

#include "../combo_area.h"
#include "../context.h"
#include "../entrance.h"
#include "../foreign_items.h"
#include "../mm_warp_song_play.h"
#include "../test_runner.h"

#include <cstdio>
#include <cstring>
#include <cstddef>
#include <fstream>
#include <initializer_list>
#include <iterator>
#include <string>

#ifdef __cplusplus
extern "C" {
#endif
// games/mm/2s2h/mm_warp_song_play_test.cpp
int MM_WarpSongPlay_MatcherRunHeadless(void);
int MM_WarpSongPlay_DepartureRunHeadless(void);
// games/oot/soh/SaveManager.cpp: OoT's descriptors, the frozen age's among them.
void OoT_SlotMeta_Register(void);
// games/oot/soh/Enhancements/randomizer/WarpSongRecordOoT.cpp
uint16_t OoT_WarpSongRecord_TestResolve(const int16_t* index, const int16_t* override, int n, uint16_t pad);
void OoT_WarpSongRecord_TestFreezeOoTHalf(int linkAge, int started, int live);
// games/oot/soh/Enhancements/randomizer/SeedPackageSingleExe.cpp: a package's world
// record copied and applied again (WhCheckCreationRecord uses it the same way).
void OoT_SeedPackage_TestReapplyWorldRecord(void);
#ifdef __cplusplus
}
#endif

static int sMwpFailures;

#define MWP_CHECK(cond, ...)                                       \
    do {                                                           \
        if (!(cond)) {                                             \
            printf("[TEST] FAIL (mm-warp-song-play): ");           \
            printf(__VA_ARGS__);                                   \
            printf("  [%s] (%s:%d)\n", #cond, __FILE__, __LINE__); \
            sMwpFailures++;                                        \
        }                                                          \
    } while (0)

namespace {

const uint16_t kMwpPads[6] = { 0x0600u, 0x04F6u, 0x0604u, 0x01F1u, 0x0568u, 0x05F4u };
const uint32_t kMwpSeed = 0x1169A7A7u;
const uint32_t kMwpSettingsHash = 0x1169C8C8u;

/** Field by field (never memcmp). */
bool MwpSameRecord(const ComboOoTWarpRecord& a, const ComboOoTWarpRecord& b) {
    if (a.formatVersion != b.formatVersion || a.flags != b.flags) {
        return false;
    }
    for (size_t i = 0; i < sizeof(a.destLE); i++) {
        if (a.destLE[i] != b.destLE[i]) {
            return false;
        }
    }
    for (size_t i = 0; i < sizeof(a.area); i++) {
        if (a.area[i] != b.area[i]) {
            return false;
        }
    }
    return true;
}

/** A shuffled, hinted sample: Minuet to Kakariko's front gate (0x00DB) naming
 *  Kakariko Village; the rest to their own pads with no area. */
ComboOoTWarpRecord MwpSampleRecord() {
    uint16_t dest[6];
    uint8_t area[6] = { 0, 0, 0, 0, 0, 0 };
    for (int i = 0; i < 6; i++) {
        dest[i] = kMwpPads[i];
    }
    dest[0] = 0x00DBu;
    area[0] = (uint8_t)COMBO_AREA_OOT_KAKARIKO_VILLAGE;
    ComboOoTWarpRecord r;
    (void)Combo_OoTWarpRecord_Build(&r, true, true, dest, area);
    return r;
}

void MwpPair() {
    gComboCtx.sourceIsRando = true;
    gComboCtx.sharedRandoSeed = kMwpSeed;
    gComboCtx.sharedRandoSettingsHash = kMwpSettingsHash;
}

void MwpSetDest(ComboOoTWarpRecord* r, int song, uint16_t d) {
    r->destLE[song * 2] = (uint8_t)(d & 0xFFu);
    r->destLE[song * 2 + 1] = (uint8_t)(d >> 8);
}

int MwpProbeReady(void) {
    return 1;
}
int MwpProbeNotReady(void) {
    return 0;
}

// ---- P1 ---------------------------------------------------------------------------
void MwpCheckRecordLayoutAndValidator() {
    MWP_CHECK(offsetof(ComboContext, comboOoTWarps) == 1842u && sizeof(ComboOoTWarpRecord) == 20u &&
                  sizeof(ComboContext) == 1864u && offsetof(ComboContext, reserved) == 939u &&
                  sizeof(((ComboContext*)0)->reserved) == 64u,
              "P1: the record is %u bytes at %u (sizeof %u, reserved[] %u at %u); want 20 at 1842, 1864, 64 at 939\n",
              (unsigned)sizeof(ComboOoTWarpRecord), (unsigned)offsetof(ComboContext, comboOoTWarps),
              (unsigned)sizeof(ComboContext), (unsigned)sizeof(((ComboContext*)0)->reserved),
              (unsigned)offsetof(ComboContext, reserved));
    MWP_CHECK(RSBS_OOT_WARP_REC_VERSION == 1u && RSBS_OOT_WARP_REC_SHUFFLED == 0x01u &&
                  RSBS_OOT_WARP_REC_HINTS == 0x02u && RSBS_OOT_WARP_REC_SONGS == 6u,
              "P1: the record's pinned values moved\n");

    ComboOoTWarpRecord r;
    Combo_OoTWarpRecord_Clear(&r);
    MWP_CHECK(Combo_OoTWarpRecord_StatusOf(&r) == COMBO_OOT_WARP_REC_UNSET &&
                  Combo_OoTWarpRecord_StatusOf(NULL) == COMBO_OOT_WARP_REC_UNSET,
              "P1: an all-zero record is not UNSET\n");
    MWP_CHECK(Combo_OoTWarpRecord_Build(&r, false, false, NULL, NULL) == COMBO_OOT_WARP_REC_PRESENT && r.flags == 0u,
              "P1: an unshuffled record does not build PRESENT\n");
    for (int i = 0; i < 6; i++) {
        MWP_CHECK((uint16_t)(r.destLE[i * 2] | (r.destLE[i * 2 + 1] << 8)) == kMwpPads[i] && r.area[i] == 0u,
                  "P1: an unshuffled record's song %d is not its vanilla pad with no area\n", i);
    }
    const ComboOoTWarpRecord sample = MwpSampleRecord();
    MWP_CHECK(Combo_OoTWarpRecord_StatusOf(&sample) == COMBO_OOT_WARP_REC_PRESENT && sample.flags == 3u,
              "P1: the shuffled, hinted sample does not build PRESENT with flags 3\n");

    struct Bad {
        const char* name;
        ComboOoTWarpRecord rec;
    };
    Bad bad[9];
    for (Bad& b : bad) {
        b.rec = sample;
    }
    bad[0].name = "format 2";
    bad[0].rec.formatVersion = 2;
    bad[1].name = "flag bit 2";
    bad[1].rec.flags = (uint8_t)(bad[1].rec.flags | 0x04u);
    bad[2].name = "hints without shuffle";
    bad[2].rec.flags = RSBS_OOT_WARP_REC_HINTS;
    bad[3].name = "unshuffled with a moved destination";
    (void)Combo_OoTWarpRecord_Build(&bad[3].rec, false, false, NULL, NULL);
    MwpSetDest(&bad[3].rec, 1, 0x00DBu);
    bad[4].name = "unshuffled with an area";
    (void)Combo_OoTWarpRecord_Build(&bad[4].rec, false, false, NULL, NULL);
    bad[4].rec.area[2] = 1;
    bad[5].name = "shuffled with destination 0";
    MwpSetDest(&bad[5].rec, 4, 0u);
    bad[6].name = "an area past the OoT half";
    bad[6].rec.area[3] = (uint8_t)(Combo_Area_Count((uint8_t)GAME_OOT) + 1);
    bad[7].name = "Link's Pocket";
    bad[7].rec.area[5] = (uint8_t)COMBO_AREA_POCKET;
    bad[8].name = "shuffled without hints but with an area";
    bad[8].rec.flags = RSBS_OOT_WARP_REC_SHUFFLED;
    for (const Bad& b : bad) {
        MWP_CHECK(Combo_OoTWarpRecord_StatusOf(&b.rec) == COMBO_OOT_WARP_REC_INVALID,
                  "P1: the malformed record '%s' does not read INVALID\n", b.name);
    }
    // A hinted record may name no area for a song (its region has none): OoT says
    // "a mysterious place" then, and so does MM.
    ComboOoTWarpRecord unnamed = sample;
    unnamed.area[0] = 0;
    MWP_CHECK(Combo_OoTWarpRecord_StatusOf(&unnamed) == COMBO_OOT_WARP_REC_PRESENT,
              "P1: a hinted record with an unnamed area is refused\n");

    // A pre-XW7 Tier-1 record: every byte before 1842 as written, zeros after.
    ComboContext_Init();
    gComboCtx.comboOoTWarps = sample;
    ComboContext* legacy = new ComboContext;
    memset(legacy, 0, sizeof(ComboContext));
    memcpy(legacy, &gComboCtx, offsetof(ComboContext, comboOoTWarps));
    MWP_CHECK(Combo_OoTWarpRecord_StatusOf(&legacy->comboOoTWarps) == COMBO_OOT_WARP_REC_UNSET,
              "P1: a record written before XW7 does not zero-extend to UNSET\n");
    delete legacy;
    ComboContext_Init();
}

// ---- P2 ---------------------------------------------------------------------------
void MwpCheckCreationRecord() {
    const ComboOoTWarpRecord sample = MwpSampleRecord();
    ComboOoTWarpRecord zero;
    Combo_OoTWarpRecord_Clear(&zero);

    ComboContext_Init();
    gComboCtx.comboOoTWarps = sample;
    ComboCreationRecords prior;
    Combo_SnapshotCreationRecords(&prior);
    Combo_OoTWarpRecord_Clear(&gComboCtx.comboOoTWarps);
    Combo_RestoreCreationRecords(&prior);
    const bool restored = MwpSameRecord(gComboCtx.comboOoTWarps, sample);
    Combo_RetractCreationRecords();
    const bool retracted = MwpSameRecord(gComboCtx.comboOoTWarps, zero);

    ComboContext_Init();
    gComboCtx.comboOoTWarps = sample;
    Context_InvalidateSessionState(RSBS_SEED_STAMP_KEEP);
    const bool kept = MwpSameRecord(gComboCtx.comboOoTWarps, sample);
    Context_InvalidateSessionState(RSBS_SEED_STAMP_DROP);
    const bool dropped = MwpSameRecord(gComboCtx.comboOoTWarps, zero);

    ComboContext_Init();
    gComboCtx.comboOoTWarps = sample;
    OoT_SeedPackage_TestReapplyWorldRecord();
    const bool packaged = MwpSameRecord(gComboCtx.comboOoTWarps, sample);
    printf("[TEST] mm-warp-song-play P2: rollback-restored %d, retracted %d, KEEP %d, DROP cleared %d, package %d "
           "(want 1 1 1 1 1)\n",
           restored ? 1 : 0, retracted ? 1 : 0, kept ? 1 : 0, dropped ? 1 : 0, packaged ? 1 : 0);
    MWP_CHECK(restored, "P2: a failed generation's rollback does not restore the warp-song record\n");
    MWP_CHECK(retracted, "P2: a retracted creation leaves its warp-song record behind\n");
    MWP_CHECK(kept, "P2: the file-create invalidation drops the record the creation just wrote\n");
    MWP_CHECK(dropped, "P2: a session drop keeps a dead world's warp-song record\n");
    MWP_CHECK(packaged, "P2: a seed package's world record does not carry the warp-song record\n");
    ComboContext_Init();
}

// ---- P3 ---------------------------------------------------------------------------
void MwpCheckPadsAndButtons() {
    for (int i = 0; i < 6; i++) {
        MWP_CHECK(Combo_MMWarpPlay_PadEntrance(i) == kMwpPads[i] && Combo_MMWarpPlay_SongOfPad(kMwpPads[i]) == i,
                  "P3: song %d's pad is 0x%04X (want 0x%04X) or does not invert\n", i,
                  (unsigned)Combo_MMWarpPlay_PadEntrance(i), (unsigned)kMwpPads[i]);
    }
    MWP_CHECK(Combo_MMWarpPlay_PadEntrance(-1) == 0u && Combo_MMWarpPlay_PadEntrance(6) == 0u &&
                  Combo_MMWarpPlay_SongOfPad(0x0282u) == -1 && Combo_MMWarpPlay_SongOfPad(0u) == -1,
              "P3: an out-of-range song has a pad, or a non-pad entrance has a song\n");
    // OoTMM sCustomSongs, as MM's OCARINA_BTN (A 0, C-down 1, C-right 2, C-left 3, C-up 4).
    const uint8_t kOoTMM[6][8] = { { 0, 4, 3, 2, 3, 2 }, { 1, 0, 1, 0, 2, 1, 2, 1 }, { 0, 1, 2, 2, 3 },
                                   { 0, 1, 0, 2, 1, 0 }, { 3, 2, 2, 0, 3, 2, 1 },    { 4, 2, 4, 2, 3, 4 } };
    const uint8_t kCounts[6] = { 6, 8, 5, 6, 7, 6 };
    // XW5's can_play_tp_* (test_warp_songs_mm.c kWsmSongs): bit 0 A, 1 C-up, 2 C-down, 3 C-left, 4 C-right.
    const uint32_t kLogic[6] = { 0x1Bu, 0x15u, 0x1Du, 0x15u, 0x1Du, 0x1Au };
    for (int i = 0; i < 6; i++) {
        uint8_t n = 0;
        const uint8_t* b = Combo_MMWarpPlay_Buttons(i, &n);
        bool same = b != NULL && n == kCounts[i];
        uint32_t set = 0;
        for (int j = 0; same && j < n; j++) {
            same = b[j] == kOoTMM[i][j];
            const uint32_t bitOf[5] = { 0x01u, 0x04u, 0x10u, 0x08u, 0x02u };
            set |= b[j] < 5 ? bitOf[b[j]] : 0u;
        }
        MWP_CHECK(same, "P3: %s's buttons are not OoTMM's\n", Combo_MMWarpPlay_SongName(i));
        MWP_CHECK(set == kLogic[i], "P3: %s's buttons use %02X, the logic's can_play_tp set is %02X\n",
                  Combo_MMWarpPlay_SongName(i), (unsigned)set, (unsigned)kLogic[i]);
    }
    uint8_t n = 9;
    MWP_CHECK(Combo_MMWarpPlay_Buttons(6, &n) == NULL && n == 0, "P3: song 6 has buttons\n");
}

// ---- P4 ---------------------------------------------------------------------------
void MwpCheckPureMatcher() {
    // MM's sButtonToPitchMap: D4 2, F4 5, A4 9, B4 11, D5 14.
    const uint8_t pitch[5] = { 2, 5, 9, 11, 14 };
    for (int i = 0; i < 6; i++) {
        uint8_t n = 0;
        const uint8_t* b = Combo_MMWarpPlay_Buttons(i, &n);
        uint8_t buf[8];
        for (int j = 0; j < n; j++) {
            buf[j] = pitch[b[j]];
        }
        MWP_CHECK(Combo_MMWarpPlay_MatchSong(buf, n, pitch, 0x3Fu) == i, "P4: %s does not match on its last note\n",
                  Combo_MMWarpPlay_SongName(i));
        MWP_CHECK(Combo_MMWarpPlay_MatchSong(buf, (uint8_t)(n - 1), pitch, 0x3Fu) != i, "P4: %s matched a note early\n",
                  Combo_MMWarpPlay_SongName(i));
        MWP_CHECK(Combo_MMWarpPlay_MatchSong(buf, n, pitch, (uint8_t)(0x3Fu & ~(1u << i))) != i,
                  "P4: %s matched although not usable\n", Combo_MMWarpPlay_SongName(i));
    }
    MWP_CHECK(!Combo_MMWarpPlay_TailMatches(NULL, 6, Combo_MMWarpPlay_Buttons(0, NULL), 6, pitch) &&
                  !Combo_MMWarpPlay_TailMatches(pitch, 4, Combo_MMWarpPlay_Buttons(0, NULL), 6, pitch),
              "P4: the tail check matched a NULL buffer or a buffer shorter than the song\n");
    // The recorded steal: a stray A, then the Inverted Song of Time's first five notes,
    // is the Requiem of Spirit (OoTMM accepts it; the matcher row records it live).
    const uint8_t steal[6] = { pitch[0], pitch[1], pitch[0], pitch[2], pitch[1], pitch[0] };
    MWP_CHECK(Combo_MMWarpPlay_MatchSong(steal, 6, pitch, 0x3Fu) == 3,
              "P4: the recorded Requiem / Inverted Song of Time prefix steal changed\n");
    // The wall helper: Serenade ending at note 5 of an 8-note wall steals it; on the
    // last note of a 5-note wall it ties; with Serenade unusable nothing steals.
    const uint8_t wall[8] = { 0, 1, 2, 2, 3, 4, 4, 4 };
    MWP_CHECK(Combo_MMWarpPlay_WallStealsBeforeEnd(wall, 8, 1u << 2),
              "P4: a wall with Serenade before its last note does not read as stolen\n");
    MWP_CHECK(!Combo_MMWarpPlay_WallStealsBeforeEnd(wall, 5, 1u << 2),
              "P4: a Serenade ending on the wall's last note read as a steal (vanilla wins the tie)\n");
    MWP_CHECK(!Combo_MMWarpPlay_WallStealsBeforeEnd(wall, 8, 0x3Fu & ~(1u << 2)),
              "P4: an unusable song stole the wall\n");
}

// ---- P5 ---------------------------------------------------------------------------
void MwpCheckStatesAndParams() {
    for (int i = 0; i < 6; i++) {
        const uint8_t s = Combo_MMWarpPlay_StaffState(i);
        MWP_CHECK(s == 0x80u + (unsigned)i && Combo_MMWarpPlay_IsStaffState(s) &&
                      Combo_MMWarpPlay_SongOfStaffState(s) == i,
                  "P5: song %d's staff state is 0x%02X\n", i, (unsigned)s);
        const uint16_t p = RSBS_MMWP_ENTEST7_PARAMS(i);
        MWP_CHECK(Combo_MMWarpPlay_SongOfEnTest7Params((int16_t)p) == i,
                  "P5: song %d's En_Test7 params do not invert\n", i);
    }
    bool vanillaStates = true;
    for (unsigned s = 0; s <= 0xFFu; s++) {
        if (s < 0x80u || s > 0x85u) {
            vanillaStates =
                vanillaStates && !Combo_MMWarpPlay_IsStaffState(s) && Combo_MMWarpPlay_SongOfStaffState(s) < 0;
        }
    }
    MWP_CHECK(vanillaStates, "P5: a state outside 0x80..0x85 (a vanilla song, 0xFE, 0xFF) reads as a warp song\n");
    int accepted = 0;
    for (int p = -32768; p <= 32767; p++) {
        accepted += Combo_MMWarpPlay_IsEnTest7Params((int16_t)p) ? 1 : 0;
    }
    bool owls = !Combo_MMWarpPlay_IsEnTest7Params((int16_t)-1);
    for (int m = 0x1C; m <= 0x26; m++) {
        owls = owls && !Combo_MMWarpPlay_IsEnTest7Params((int16_t)m);
    }
    MWP_CHECK(accepted == 6 && owls, "P5: En_Test7's predicate takes %d params (want 6) or an owl mode / arrival\n",
              accepted);
}

// ---- P6 ---------------------------------------------------------------------------
void MwpCheckDecide() {
    ComboMMWarpPlayInputs good;
    good.song = 0;
    good.paired = 1;
    good.ootPlayable = 1;
    good.switchPending = 0;
    good.soaringRestricted = 0;
    good.ootAge = RSBS_MMWP_AGE_CHILD;
    good.recordStatus = COMBO_OOT_WARP_REC_PRESENT;
    good.destResolvable = 1;
    MWP_CHECK(Combo_MMWarpPlay_Decide(&good) == COMBO_MMWP_OFFER, "P6: every gate holding does not offer\n");
    struct Flip {
        const char* name;
        ComboMMWarpPlayInputs in;
        ComboMMWarpPlayGate want;
    };
    Flip flips[10];
    for (Flip& f : flips) {
        f.in = good;
    }
    flips[0] = { "unpaired", good, COMBO_MMWP_OOT_UNAVAILABLE };
    flips[0].in.paired = 0;
    flips[1] = { "OoT unplayable", good, COMBO_MMWP_OOT_UNAVAILABLE };
    flips[1].in.ootPlayable = 0;
    flips[2] = { "a switch pending", good, COMBO_MMWP_OOT_UNAVAILABLE };
    flips[2].in.switchPending = 1;
    flips[3] = { "restricted", good, COMBO_MMWP_RESTRICTED };
    flips[3].in.soaringRestricted = 1;
    flips[4] = { "unset record", good, COMBO_MMWP_NOT_RECORDED };
    flips[4].in.recordStatus = COMBO_OOT_WARP_REC_UNSET;
    flips[5] = { "invalid record", good, COMBO_MMWP_NOT_RECORDED };
    flips[5].in.recordStatus = COMBO_OOT_WARP_REC_INVALID;
    flips[6] = { "adult", good, COMBO_MMWP_NOT_CHILD };
    flips[6].in.ootAge = RSBS_MMWP_AGE_ADULT;
    flips[7] = { "unknown age", good, COMBO_MMWP_NOT_CHILD };
    flips[7].in.ootAge = RSBS_MMWP_AGE_UNKNOWN;
    flips[8] = { "grotto not ready", good, COMBO_MMWP_NOT_READY };
    flips[8].in.destResolvable = 0;
    flips[9] = { "restricted AND adult (order)", good, COMBO_MMWP_RESTRICTED };
    flips[9].in.soaringRestricted = 1;
    flips[9].in.ootAge = RSBS_MMWP_AGE_ADULT;
    for (const Flip& f : flips) {
        const ComboMMWarpPlayGate got = Combo_MMWarpPlay_Decide(&f.in);
        MWP_CHECK(got == f.want, "P6: %s decides %d (%s); want %d\n", f.name, (int)got, Combo_MMWarpPlay_GateName(got),
                  (int)f.want);
    }
    ComboMMWarpPlayInputs badSong = good;
    badSong.song = 6;
    MWP_CHECK(Combo_MMWarpPlay_Decide(&badSong) == COMBO_MMWP_BAD_SONG &&
                  Combo_MMWarpPlay_Decide(NULL) == COMBO_MMWP_BAD_SONG,
              "P6: a song out of range or no inputs offered\n");
}

// ---- P7 ---------------------------------------------------------------------------
void MwpCheckTexts() {
    char out[128];
    ComboContext_Init();
    MWP_CHECK(Combo_MMWarpPlay_PromptText(0, out, sizeof(out)) == -1 && out[0] == '\0',
              "P7: an unset record gave a prompt\n");
    (void)Combo_OoTWarpRecord_Build(&gComboCtx.comboOoTWarps, false, false, NULL, NULL);
    const char* kFixed[6] = { "%gWarp to the Lost Woods?%w", "%rWarp to the Death Mountain\nCrater?%w",
                              "%bWarp to Lake Hylia?%w",     "%yWarp to the Desert Colossus?%w",
                              "%pWarp to the graveyard?%w",  "%tWarp to the Temple of Time?%w" };
    for (int i = 0; i < 6; i++) {
        const int form = Combo_MMWarpPlay_PromptText(i, out, sizeof(out));
        MWP_CHECK(form == 0 && std::string(out) == kFixed[i], "P7: hints off, song %d says \"%s\" (form %d)\n", i, out,
                  form);
    }
    gComboCtx.comboOoTWarps = MwpSampleRecord();
    const ComboArea kak = { (uint8_t)GAME_OOT, (uint8_t)COMBO_AREA_OOT_KAKARIKO_VILLAGE };
    const std::string want = std::string("Warp to\n%r") + Combo_Area_Name(kak) + "%w?";
    int form = Combo_MMWarpPlay_PromptText(0, out, sizeof(out));
    MWP_CHECK(form == 1 && std::string(out) == want, "P7: hints on, Minuet says \"%s\" (form %d); want \"%s\"\n", out,
              form, want.c_str());
    form = Combo_MMWarpPlay_PromptText(1, out, sizeof(out));
    MWP_CHECK(form == 1 && std::string(out) == "Warp to\n%ra mysterious place%w?",
              "P7: hints on, an unnamed area says \"%s\" (form %d)\n", out, form);
    MWP_CHECK(Combo_MMWarpPlay_PromptText(0, out, 8) == -1 && out[0] == '\0' &&
                  Combo_MMWarpPlay_PromptText(6, out, sizeof(out)) == -1,
              "P7: a short buffer or song 6 gave a prompt\n");
    MWP_CHECK(Combo_MMWarpPlay_PlayedText(0, out, sizeof(out)) > 0 &&
                  std::string(out) == "You played the %gMinuet of Forest%w!",
              "P7: the played text is \"%s\"\n", out);
    MWP_CHECK(Combo_MMWarpPlay_LineCount("a") == 1 && Combo_MMWarpPlay_LineCount("a\nb") == 2,
              "P7: the line count is wrong\n");
    ComboContext_Init();
}

// ---- P8 ---------------------------------------------------------------------------
void MwpCheckAge() {
    OoT_SlotMeta_Register();
    MWP_CHECK(Combo_MMWarpPlay_OoTDescRegistered(), "P8: OoT_SlotMeta_Register registered no age descriptor\n");
    struct AgeCase {
        const char* name;
        int linkAge, started, live, freeze;
        int want;
    };
    const AgeCase cases[] = {
        { "child", 1, 1, 1, 1, RSBS_MMWP_AGE_CHILD },
        { "adult", 0, 1, 1, 1, RSBS_MMWP_AGE_ADULT },
        { "title-screen save", 1, 0, 1, 1, RSBS_MMWP_AGE_UNKNOWN },
        { "not a live file", 1, 1, 0, 1, RSBS_MMWP_AGE_UNKNOWN },
        { "no OoT half", 1, 1, 1, 0, RSBS_MMWP_AGE_UNKNOWN },
        { "linkAge 7", 7, 1, 1, 1, RSBS_MMWP_AGE_UNKNOWN },
    };
    for (const AgeCase& c : cases) {
        Context_ClearAllFrozenStates();
        if (c.freeze) {
            OoT_WarpSongRecord_TestFreezeOoTHalf(c.linkAge, c.started, c.live);
        }
        const int got = Combo_MMWarpPlay_OoTFrozenAge();
        MWP_CHECK(got == c.want, "P8: OoT's frozen age for %s reads %d; want %d\n", c.name, got, c.want);
    }
    Context_ClearAllFrozenStates();
    OoT_WarpSongRecord_TestFreezeOoTHalf(1, 1, 1);
    Combo_MMWarpPlay_RegisterOoTDesc(NULL);
    MWP_CHECK(Combo_MMWarpPlay_OoTFrozenAge() == RSBS_MMWP_AGE_UNKNOWN, "P8: an unregistered descriptor read an age\n");
    OoT_SlotMeta_Register();
    MWP_CHECK(Combo_MMWarpPlay_OoTFrozenAge() == RSBS_MMWP_AGE_CHILD, "P8: re-registering did not read the child\n");
    Context_ClearAllFrozenStates();
}

// ---- P9 ---------------------------------------------------------------------------
void MwpCheckMMFirst() {
    ComboContext_Init();
    MwpPair();
    gComboCtx.comboOoTWarps = MwpSampleRecord();
    Context_ClearAllFrozenStates();
    OoT_SlotMeta_Register();
    OoT_WarpSongRecord_TestFreezeOoTHalf(1, 1, 1);
    Switch_SetTargetStatusForTest(SWITCH_TARGET_PLAYABLE);
    ComboEntrance_Init();

    ComboMMWarpPlayInputs in;
    Combo_MMWarpPlay_GatherInputs(0, false, &in);
    const ComboMMWarpPlayGate gate = Combo_MMWarpPlay_Decide(&in);
    char out[128];
    const int form = Combo_MMWarpPlay_PromptText(0, out, sizeof(out));
    uint16_t dest = 0;
    const int arrival = Combo_OoTWarpRecord_ArrivalFor(0x0600u, &dest);
    printf("[TEST] mm-warp-song-play P9: MM-first gate %d (%s), prompt form %d \"%s\", pad 0x%04X, arrival %d -> "
           "0x%04X\n",
           (int)gate, Combo_MMWarpPlay_GateName(gate), form, out, (unsigned)Combo_MMWarpPlay_PadEntrance(0), arrival,
           (unsigned)dest);
    MWP_CHECK(gate == COMBO_MMWP_OFFER, "P9: the frozen record, a child half and a paired world do not offer\n");
    MWP_CHECK(form == 1 && strstr(out, "Kakariko") != NULL, "P9: the prompt does not name the record's area\n");
    MWP_CHECK(arrival == RSBS_OOT_WARP_ARRIVAL_DEST && dest == 0x00DBu,
              "P9: OoT's arrival does not take the record's destination\n");

    // The gather's switch questions: an armed warp is a pending switch.
    MWP_CHECK(ComboWarp_Arm(GAME_MM, 0xD810u, GAME_OOT, 0x0600u, COMBO_SWITCH_KIND_WARP_SONG),
              "P9 setup: the warp target did not arm\n");
    Combo_MMWarpPlay_GatherInputs(0, false, &in);
    MWP_CHECK(in.switchPending && Combo_MMWarpPlay_Decide(&in) == COMBO_MMWP_OOT_UNAVAILABLE,
              "P9: an armed warp target is not a pending switch to the gather\n");
    ComboEntrance_Init();

    // Arrival answers.
    MWP_CHECK(Combo_OoTWarpRecord_ArrivalFor(0x1234u, &dest) == RSBS_OOT_WARP_ARRIVAL_NONE,
              "P9: a non-pad arrival took a record answer\n");
    (void)Combo_OoTWarpRecord_Build(&gComboCtx.comboOoTWarps, false, false, NULL, NULL);
    MWP_CHECK(Combo_OoTWarpRecord_ArrivalFor(0x0600u, &dest) == RSBS_OOT_WARP_ARRIVAL_PAD,
              "P9: an unshuffled record does not keep the pad\n");
    Combo_OoTWarpRecord_Clear(&gComboCtx.comboOoTWarps);
    MWP_CHECK(Combo_OoTWarpRecord_ArrivalFor(0x0600u, &dest) == RSBS_OOT_WARP_ARRIVAL_NONE,
              "P9: an unset record gave an answer (XW3's body must run)\n");
    Combo_MMWarpPlay_GatherInputs(0, false, &in);
    MWP_CHECK(Combo_MMWarpPlay_Decide(&in) == COMBO_MMWP_NOT_RECORDED, "P9: an unset record does not refuse\n");

    // A grotto destination: NOT_READY until OoT's probe says ready.
    ComboOoTWarpRecord grotto = MwpSampleRecord();
    MwpSetDest(&grotto, 0, 0x0800u);
    gComboCtx.comboOoTWarps = grotto;
    Combo_MMWarpPlay_RegisterOoTReadyProbe(NULL);
    Combo_MMWarpPlay_GatherInputs(0, false, &in);
    const ComboMMWarpPlayGate unregistered = Combo_MMWarpPlay_Decide(&in);
    Combo_MMWarpPlay_RegisterOoTReadyProbe(MwpProbeNotReady);
    Combo_MMWarpPlay_GatherInputs(0, false, &in);
    const ComboMMWarpPlayGate notReady = Combo_MMWarpPlay_Decide(&in);
    Combo_MMWarpPlay_RegisterOoTReadyProbe(MwpProbeReady);
    Combo_MMWarpPlay_GatherInputs(0, false, &in);
    const ComboMMWarpPlayGate ready = Combo_MMWarpPlay_Decide(&in);
    MWP_CHECK(unregistered == COMBO_MMWP_NOT_READY && notReady == COMBO_MMWP_NOT_READY && ready == COMBO_MMWP_OFFER,
              "P9: a grotto destination decides %d / %d / %d (unregistered, not ready, ready); want 5 5 0\n",
              (int)unregistered, (int)notReady, (int)ready);
    OoT_SlotMeta_Register(); // the real probe back

    Switch_SetTargetStatusForTest(-1);
    Context_ClearAllFrozenStates();
    ComboContext_Init();
}

// ---- P10 --------------------------------------------------------------------------
void MwpCheckLatchesAndWriter() {
    Combo_MMWarpPlay_ResetLatches();
    uint8_t song = 0xFF;
    MWP_CHECK(!Combo_MMWarpPlay_HasPending() && !Combo_MMWarpPlay_TakePending(&song) && !Combo_MMWarpPlay_DepartOnce(),
              "P10: fresh latches are not empty\n");
    Combo_MMWarpPlay_SetPending(2);
    MWP_CHECK(Combo_MMWarpPlay_HasPending() && Combo_MMWarpPlay_PendingSong() == 2 && !Combo_MMWarpPlay_DepartOnce(),
              "P10: a prompt's pending song is not held, or departs before it is taken\n");
    MWP_CHECK(Combo_MMWarpPlay_TakePending(&song) && song == 2 && !Combo_MMWarpPlay_HasPending(),
              "P10: taking the pending song did not hand it over and clear it\n");
    MWP_CHECK(Combo_MMWarpPlay_DepartOnce() && !Combo_MMWarpPlay_DepartOnce(),
              "P10: the departure latch is not exactly once per take\n");
    Combo_MMWarpPlay_SetPending(1);
    Combo_MMWarpPlay_ClearPending();
    MWP_CHECK(!Combo_MMWarpPlay_HasPending(), "P10: the ocarina's end does not clear the pending song\n");
    Combo_MMWarpPlay_SetPending(7);
    MWP_CHECK(!Combo_MMWarpPlay_HasPending(), "P10: an out-of-range song is pending\n");
    Combo_MMWarpPlay_ResetLatches();

    const int16_t idx[3] = { 0x0600, 0x0604, 0x0600 };
    const int16_t ov[3] = { 0x00DB, 0x0800, 0x0123 };
    MWP_CHECK(OoT_WarpSongRecord_TestResolve(idx, ov, 3, 0x0600u) == 0x0123u,
              "P10: the writer does not take the last row for a pad (Entrance_Init's last write wins)\n");
    MWP_CHECK(OoT_WarpSongRecord_TestResolve(idx, ov, 3, 0x0604u) == 0x0800u,
              "P10: the writer drops a grotto destination\n");
    MWP_CHECK(OoT_WarpSongRecord_TestResolve(idx, ov, 3, 0x05F4u) == 0x05F4u,
              "P10: a pad with no row does not resolve to itself\n");
}

// ---- P11 --------------------------------------------------------------------------
std::string MwpReadSource(const char* rel) {
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

/** The body of the function whose definition starts with @p signature (the first
 *  occurrence followed by '{', not a prototype), braces matched; "" if absent. */
std::string MwpFunctionBody(const std::string& src, const char* signature) {
    size_t at = 0;
    while ((at = src.find(signature, at)) != std::string::npos) {
        size_t p = at + strlen(signature);
        while (p < src.size() && (src[p] == ' ' || src[p] == '\n' || src[p] == '\r' || src[p] == '\t')) {
            p++;
        }
        if (p < src.size() && src[p] == '{') {
            int depth = 0;
            for (size_t q = p; q < src.size(); q++) {
                if (src[q] == '{') {
                    depth++;
                } else if (src[q] == '}') {
                    depth--;
                    if (depth == 0) {
                        return src.substr(p, q - p + 1);
                    }
                }
            }
            return std::string();
        }
        at = p;
    }
    return std::string();
}

/** The text from @p from to the next @p to after it (both inside @p src); "" if
 *  either is absent. For a switch case, scoped to that case. */
std::string MwpRegion(const std::string& src, const char* from, const char* to) {
    const size_t a = src.find(from);
    if (a == std::string::npos) {
        return std::string();
    }
    const size_t b = src.find(to, a + strlen(from));
    return b == std::string::npos ? std::string() : src.substr(a, b - a);
}

/** Every needle in order inside @p hay. */
bool MwpInOrder(const std::string& hay, std::initializer_list<const char*> needles) {
    size_t at = 0;
    for (const char* n : needles) {
        const size_t f = hay.find(n, at);
        if (hay.empty() || f == std::string::npos) {
            return false;
        }
        at = f + strlen(n);
    }
    return true;
}

void MwpCheckAnchors() {
    const std::string audio = MwpReadSource("games/mm/src/audio/code_8019AF00.c");
    const std::string message = MwpReadSource("games/mm/src/code/z_message.c");
    const std::string player = MwpReadSource("games/mm/src/overlays/actors/ovl_player_actor/z_player.c");
    const std::string test7 = MwpReadSource("games/mm/src/overlays/actors/ovl_En_Test7/z_en_test7.c");
    const std::string skip = MwpReadSource("games/mm/2s2h/Enhancements/Songs/SkipSoaringCutscene.cpp");
    const std::string mm = MwpReadSource("games/mm/2s2h/Rando/WarpSongsMM.cpp");
    const std::string arrival = MwpReadSource("games/oot/soh/Enhancements/randomizer/randomizer_entrance.c");
    const std::string creation = MwpReadSource("games/oot/soh/Enhancements/randomizer/ForeignItemsSingleExe.cpp");
    if (audio.empty() || message.empty() || player.empty() || test7.empty() || skip.empty() || mm.empty() ||
        arrival.empty() || creation.empty()) {
        MWP_CHECK(false, "P11: a source file under test could not be read (RSBS_SOURCE_DIR)\n");
        return;
    }
    const std::string checker = MwpFunctionBody(audio, "void AudioOcarina_CheckSongsWithoutMusicStaff(void)");
    MWP_CHECK(MwpInOrder(checker, { "sPlayedOcarinaSongIndexPlusOne = songIndex + 1;", "if (sOcarinaFlags != 0) {",
                                    "MM_WarpSongMM_NoteMatched(sCurOcarinaSongWithoutMusicStaff",
                                    "sPlayedOcarinaSongIndexPlusOne = rsbsWarpState + 1;" }),
              "P11: the matcher is not called after the vanilla loop, behind 'no vanilla match on this note', in "
              "AudioOcarina_CheckSongsWithoutMusicStaff\n");
    const std::string wall = MwpFunctionBody(audio, "void AudioOcarina_TerminaWallGenerateNotes(void)");
    MWP_CHECK(MwpInOrder(wall, { "(sOcarinaWallCounter++ % 2) || MM_WarpSongMM_WallRandomOnly()",
                                 "MM_WarpSongMM_WallRejected(" }),
              "P11: the Termina wall's gates are not in AudioOcarina_TerminaWallGenerateNotes\n");
    // E1-E3 are MM_Message_DrawMain's cases, E4 MM_Message_Update's: each region is
    // cut from inside the function it claims (both functions have a TEXT_CLOSING and
    // an OCARINA_PLAYING case).
    const std::string drawMain = MwpFunctionBody(message, "void MM_Message_DrawMain(PlayState* play, Gfx** gfxP)");
    const std::string update = MwpFunctionBody(message, "void MM_Message_Update(PlayState* play)");
    const std::string e1 = MwpRegion(drawMain, "case MSGMODE_OCARINA_PLAYING:", "case MSGMODE_E:");
    MWP_CHECK(MwpInOrder(e1, { "MM_WarpSongMM_IsStaffState(msgCtx->ocarinaStaff->state)", "MSGMODE_SONG_PLAYED",
                               "bool vanillaOwnedSongCheck" }),
              "P11: E1 does not take a warp state before the quest-item check in MSGMODE_OCARINA_PLAYING\n");
    const std::string e2 =
        MwpRegion(drawMain, "case MSGMODE_SETUP_DISPLAY_SONG_PLAYED:", "case MSGMODE_DISPLAY_SONG_PLAYED_TEXT_BEGIN:");
    MWP_CHECK(
        MwpInOrder(e2, { "MM_WarpSongMM_IsStaffState(msgCtx->songPlayed)", "AudioOcarina_SetPlaybackSong(1, 1);",
                         "MM_WarpSongMM_PointPlaybackAtSong(msgCtx->songPlayed);", "#endif",
                         "sOcarinaSongFanfares[msgCtx->songPlayed]", "play->msgCtx.ocarinaMode = OCARINA_MODE_ACTIVE;",
                         "msgCtx->ocarinaAction = OCARINA_ACTION_FREE_PLAY_DONE;",
                         "msgCtx->msgMode = MSGMODE_DISPLAY_SONG_PLAYED;" }),
        "P11: E2 does not guard only the playback and fanfare of SETUP_DISPLAY_SONG_PLAYED, leaving its tail "
        "(ACTIVE, FREE_PLAY_DONE, DISPLAY_SONG_PLAYED) to every song\n");
    const std::string e3 = MwpRegion(drawMain, "case MSGMODE_DISPLAY_SONG_PLAYED_TEXT_BEGIN:", "case MSGMODE_16:");
    MWP_CHECK(MwpInOrder(e3, { "MM_WarpSongMM_PlayedTextId(msgCtx->songPlayed)", "0x1B72 + msgCtx->songPlayed" }),
              "P11: E3 does not replace the played text before the vanilla 0x1B72 + song\n");
    const std::string e4 =
        MwpRegion(update, "case MSGMODE_TEXT_CLOSING:", "if (MM_sLastPlayedSong == OCARINA_SONG_SOARING)");
    MWP_CHECK(MwpInOrder(e4, { "MM_WarpSongMM_IsStaffState((unsigned)MM_sLastPlayedSong)", "MM_sLastPlayedSong = 0xFF;",
                               "MM_WarpSongMM_OnSongPlayed(play, rsbsWarpState);", "break;" }),
              "P11: E4 is not at TEXT_CLOSING before the Soaring branch\n");
    const std::string effect = MwpFunctionBody(message, "void Message_SpawnSongEffect(PlayState* play)");
    MWP_CHECK(
        MwpInOrder(effect, { "MM_WarpSongMM_SpawnSongEffect(play, msgCtx->songPlayed)", "sOcarinaEffectActorIds" }),
        "P11: the warp song's effect is not spawned in Message_SpawnSongEffect\n");
    const std::string act63 = MwpFunctionBody(player, "void Player_Action_63(Player* this, PlayState* play)");
    MWP_CHECK(MwpInOrder(act63, { "OCARINA_MODE_END", "MM_WarpSongMM_ClearPending();",
                                  "(play->msgCtx.ocarinaMode == OCARINA_MODE_WARP) && MM_WarpSongMM_HasPending(play)",
                                  "MM_WarpSongMM_TakePending(play, &rsbsWarpSong)",
                                  "playerCsIds[PLAYER_CS_ID_SONG_WARP] != CS_ID_NONE", "ACTOR_EN_TEST7",
                                  "MM_WarpSongMM_Depart(play, rsbsWarpSong);", "s32 var_v1" }),
              "P11: Player_Action_63 does not clear the prompt at END and spawn En_Test7 (or fade) for OK before "
              "vanilla's warp chain\n");
    const std::string warp = MwpFunctionBody(test7, "void EnTest7_WarpCsWarp(EnTest7* this, PlayState* play)");
    MWP_CHECK(MwpInOrder(warp, { "MM_WarpSongMM_IsEnTest7Params(", "MM_WarpSongMM_Depart(",
                                 "EnTest7_SetupAction(this, EnTest7_WarpCsXGameHold);", "return;", "SCENE_SECOM",
                                 "sOwlWarpEntrances[" }),
              "P11: EnTest7_WarpCsWarp does not depart once and leave before the owl table\n");
    const std::string skipBody = MwpFunctionBody(skip, "static void SkipSoaringCutscene(Actor* actor, bool* should)");
    MWP_CHECK(MwpInOrder(skipBody, { "ENTEST7_ARRIVE", "MM_WarpSongMM_IsEnTest7Params(ocarinaMode)",
                                     "MM_WarpSongMM_Depart(", "return;", "sOwlWarpEntrancesForMods[" }),
              "P11: SkipSoaringCutscene does not depart a warp song before the owl table\n");
    const std::string depart = MwpFunctionBody(mm, "extern \"C\" void MM_WarpSongMM_Depart(PlayState* play, int song)");
    MWP_CHECK(MwpInOrder(depart, { "Combo_MMWarpPlay_DepartOnce()", "Combo_MMWarpPlay_Decide(", "ComboWarp_Arm(",
                                   "play->nextEntrance = marker;" }),
              "P11: MM_WarpSongMM_Depart does not take its one-shot latch before re-asking and arming\n");
    const std::string arrive = MwpFunctionBody(arrival, "s16 Entrance_OverrideWarpArrival(s16 entranceIndex)");
    MWP_CHECK(MwpInOrder(arrive, { "Combo_OoTWarpRecord_ArrivalFor(", "the record wins", "IS_RANDO" }),
              "P11: Entrance_OverrideWarpArrival does not ask the frozen record before XW3's body\n");
    const std::string finish =
        MwpFunctionBody(creation, "extern \"C\" int OoT_Creation_FinishPairedHalf(int writeSpoiler)");
    MWP_CHECK(MwpInOrder(finish, { "Combo_OoTWarpRecord_Clear(&gComboCtx.comboOoTWarps);",
                                   "Combo_Crossings_CaptureFromCoordinator()", "OoT_ComboLogic_FinishGeneralPass(",
                                   "OoT_Creation_FreezeWarpSongRecord()" }),
              "P11: OoT_Creation_FinishPairedHalf does not clear the record first and freeze it after the remainder\n");
}

} // namespace

TestResult MMWarpSongPlay_Run(void) {
    sMwpFailures = 0;
    ComboContext* saved = new ComboContext;
    memcpy(saved, &gComboCtx, sizeof(ComboContext));

    MwpCheckRecordLayoutAndValidator();
    MwpCheckCreationRecord();
    MwpCheckPadsAndButtons();
    MwpCheckPureMatcher();
    MwpCheckStatesAndParams();
    MwpCheckDecide();
    MwpCheckTexts();
    MwpCheckAge();
    MwpCheckMMFirst();
    MwpCheckLatchesAndWriter();
    MwpCheckAnchors();

    memcpy(&gComboCtx, saved, sizeof(ComboContext));
    delete saved;
    Context_ClearAllFrozenStates();
    Switch_SetTargetStatusForTest(-1);
    ComboEntrance_Init();
    Combo_MMWarpPlay_ResetLatches();
    if (sMwpFailures != 0) {
        printf("[TEST] FAIL (mm-warp-song-play): %d failure(s)\n", sMwpFailures);
        return TEST_FAIL;
    }
    printf("[TEST] PASS: mm-warp-song-play\n");
    return TEST_PASS;
}
