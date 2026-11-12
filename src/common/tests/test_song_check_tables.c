/**
 * @file test_song_check_tables.c
 * @brief THE SONG CHECKS AS CROSS-GAME HOSTS (#1542 S0): combo_song_pool.h's
 *        verdict table and its one switch, both engines' song-host answers, and
 *        today's host tables with the switch off.
 *
 * ============================================================================
 * song-check-tables (CTest SongCheckTables, redship tier, ROM-free, generates
 * nothing)
 * ============================================================================
 *
 *  (a) OoT's song checks, from OoT's static table: the RCTYPE_SONG_LOCATION rows
 *      are exactly the twelve names spelled here (ShuffleSongs.cpp's names, in
 *      OoTMM's order), and OoT_SongCheckIndex maps each onto its position and
 *      every other row onto -1. Red on a song row added or removed upstream, or
 *      on a wrong case in the index switch.
 *  (b) MM's, the same way: the RCTYPE_SONG rows are exactly the eleven
 *      RandoCheckId names spelled here, indexed 12..22; every other row -1.
 *  (c) Combo_SongCheckAcceptsForeign against LITERAL expectations over the 23
 *      checks x OoT's four Shuffle Songs values x Skip Child Zelda x the override:
 *      cleared and 0 refuse everywhere (red if S0's production answer is
 *      anything but "dormant"); 1 accepts every check but Impa, which it accepts
 *      only with Skip Child Zelda off (red on a wrong rule, or on the Impa rule
 *      lost). Out-of-range indices refuse.
 *  (c2) The verdict table's game, rule and give path, row by row, against a
 *      literal list (red on any table edit that this row does not also make).
 *  (d) The two engines under the override, with a Rando context whose frozen
 *      options are set (OoT_ComboLogic_TestHoldSongContext): OoT's REAL predicate
 *      answers (c)'s literal for the frozen Skip Child Zelda; MM's REAL host class
 *      accepts all eleven and agrees with the pure answer for the frozen value;
 *      OoT_Combo_FrozenShuffleSongs reads the context's option; and a creation
 *      capture of each value, then none, leaves every MM answer unchanged. In S0
 *      the switch ignores the songs value, so this loop catches only an explicit
 *      capture-gated return; an indirect read of the capture or of MM's own songs
 *      option is caught by (h)'s ban on Combo_PairSongs*, ResolvedPairedSongs and
 *      RO_SHUFFLE_SONGS inside the host class, not here.
 *  (e) For every NON-song row of both games the answer is the same with the
 *      override on as with it cleared (red on a song branch keyed on a type or a
 *      flag instead of the check id: the Tier B trap).
 *  (f) THE BASE TABLES. With the override cleared and with it 0, every real row
 *      of both games answers what the host rule at the base commit (0c9277362)
 *      answers, restated here from static facts the predicates do not share:
 *      OoT's shelves (RCTYPE_SHOP drawn by EN_GIRLA), scrubs (RCTYPE_SCRUB drawn
 *      by EN_DNS), its four merchants, and EN_BOX chests that are no shop,
 *      scrub, merchant, chest-game row or shop-scene row, in the chest-game room
 *      only its reward; MM's Tingle maps, cycle chests (RCTYPE_CHEST with
 *      FLAG_CYCL_SCENE_CHEST) and shops. All 23 song checks refuse. Red on any
 *      host-acceptance change with the switch off.
 *  (h) Source anchors, each searched INSIDE the function it names: the OoT
 *      predicate asks the song index and the pure answer with OoT's frozen two
 *      options before its actor test; MM's host class asks them first, reads OoT's
 *      frozen value and never the creation capture (no Combo_PairSongs*,
 *      ResolvedPairedSongs or RO_SHUFFLE_SONGS in its body); CheckQueue consumes a
 *      delivered song host inside `eligible`, before the outbox hold and the
 *      queue; the switch's production body is "false" and the pool reads no
 *      CVar; MM's spoiler-load path is reachable only unpaired; StartingItemGive
 *      has no crossing branch (why Impa refuses under Skip Child Zelda); the two
 *      CheckQueue drives refuse a song check.
 *  (i) After the scope guard: the override is cleared, the capture is what it was
 *      on entry, and no Rando context is left that was not there on entry.
 *
 * Linkage note: #included into test_runner.cpp at FILE SCOPE (compiled as C++),
 * like its neighbours; every symbol under test is C-linkage. Helpers carry the
 * Sct prefix because every included test file shares one translation unit.
 */

#include "../combo_song_pool.h"
#include "../test_runner.h"
#include "test_named_items.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <initializer_list>
#include <iterator>
#include <string>

extern "C" {
// ComboLogicEngineOoT.cpp (#1542 S0 bridges).
int OoT_ComboLogic_TestCheckIdSpace(void);
int OoT_ComboLogic_TestEnsureLocationTable(void);
int OoT_ComboLogic_TestSongCheckIndex(uint16_t rc);
int OoT_ComboLogic_TestHostAcceptsForeign(uint16_t rc);
int OoT_ComboLogic_TestHostFacts(uint16_t hostCheck, int* outFacts);
const char* OoT_ComboLogic_TestCheckName(uint16_t hostCheck);
int OoT_ComboLogic_TestHoldSongContext(int songs, int skipChildZelda);
void OoT_ComboLogic_TestReleaseSongContext(void);
int OoT_ComboLogic_TestRandoContextExists(void);
// Rando/Foreign.cpp.
int MM_Rando_Foreign_TestCheckIdMax(void);
int MM_Rando_Foreign_TestIsForeignHostClass(uint16_t randoCheckId);
int MM_Rando_Foreign_TestCheckClass(uint16_t randoCheckId, int* outIsChestType, int* outHasChestFlag);
int MM_Rando_Foreign_TestCheckShopClass(uint16_t randoCheckId, int* outIsShop, int* outIsTingleShop);
int MM_Rando_Foreign_TestSongCheckIndex(uint16_t randoCheckId);
int MM_Rando_Foreign_TestIsSongRow(uint16_t randoCheckId);
const char* MM_Rando_Foreign_TestCheckRowName(uint16_t randoCheckId);
// games/mm/2s2h/mm_song_host_guard_test.cpp.
int MM_SongHostGuard_Run(uint8_t ootOrigin, uint16_t ootItemId);
}

namespace {

int sSctFailures = 0;

#define SCT_CHECK(cond, ...)                                       \
    do {                                                           \
        if (!(cond)) {                                             \
            printf("[TEST] FAIL (song-check-tables): ");           \
            printf(__VA_ARGS__);                                   \
            printf("  [%s] (%s:%d)\n", #cond, __FILE__, __LINE__); \
            sSctFailures++;                                        \
        }                                                          \
    } while (0)

/** OoT's twelve song checks by their location-table names (ShuffleSongs.cpp),
 *  in OoTMM's order: RsbsSongCheck 0..11. */
const char* const kSctOoTSongNames[RSBS_SONG_CHECK_OOT_COUNT] = {
    "Song from Saria",           "Sheik in Forest",   "Song from Malon",
    "Song from Ocarina of Time", "Song from Impa",    "Song from Royal Family's Tomb",
    "Song from Windmill",        "Sheik in Kakariko", "Sheik in Crater",
    "Sheik in Ice Cavern",       "Sheik at Colossus", "Sheik at Temple",
};

/** MM's eleven by their RandoCheckId names (Checks.cpp stringifies the id):
 *  RsbsSongCheck 12..22. */
const char* const kSctMMSongNames[RSBS_SONG_CHECK_MM_COUNT] = {
    "RC_STARTING_ITEM_SONG_OF_HEALING",
    "RC_CLOCK_TOWER_ROOF_SONG_OF_TIME",
    "RC_ROMANI_RANCH_EPONAS_SONG",
    "RC_SOUTHERN_SWAMP_SONG_OF_SOARING",
    "RC_DEKU_KINGS_CHAMBER_MONKEY",
    "RC_PATH_TO_GORON_VILLAGE_LULLABY_INTRO",
    "RC_GORON_SHRINE_FULL_LULLABY",
    "RC_GREAT_BAY_COAST_NEW_WAVE_BOSSA_NOVA",
    "RC_BENEATH_THE_GRAVEYARD_SONG_OF_STORMS",
    "RC_ANCIENT_CASTLE_OF_IKANA_BOSS",
    "RC_GIANTS_CHAMBER_OATH_TO_ORDER",
};

/** Index of Impa, the one rule that is not a plain ALLOW. */
constexpr int kSctImpa = 4;

/** (c)'s literal: with the switch armed, every check but Impa; Impa only with
 *  Skip Child Zelda off. */
bool SctWantArmed(int songCheck, bool skipChildZelda) {
    return songCheck != kSctImpa || !skipChildZelda;
}

/** Restores both hooks on every return path (the assert macros of other rows
 *  return early; this row's do not, but its legs still nest). */
struct SctHookGuard {
    int16_t capture;
    SctHookGuard() : capture(Combo_PairSongsSnapshot()) {
    }
    ~SctHookGuard() {
        Combo_SongHostsForceForTest(-1);
        Combo_PairSongsRestore(capture);
        OoT_ComboLogic_TestReleaseSongContext();
    }
};

std::string SctReadSource(const char* rel) {
#ifdef RSBS_SOURCE_DIR
    std::ifstream in(std::string(RSBS_SOURCE_DIR) + "/" + rel, std::ios::binary);
    if (!in.good()) {
        return std::string();
    }
    std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    std::string out;
    out.reserve(text.size());
    for (const char c : text) {
        if (c != '\r') {
            out.push_back(c);
        }
    }
    return out;
#else
    (void)rel;
    return std::string();
#endif
}

/** The body of the function whose definition starts with @p signature (the first
 *  occurrence followed by '{', not a prototype), braces matched; "" if absent. */
std::string SctFunctionBody(const std::string& src, const char* signature) {
    size_t at = 0;
    while ((at = src.find(signature, at)) != std::string::npos) {
        size_t p = at + strlen(signature);
        while (p < src.size() && (src[p] == ' ' || src[p] == '\n' || src[p] == '\t')) {
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

/** The body of the TOP-LEVEL function whose definition starts with @p signature:
 *  from its opening '{' to the first closing brace at column 0 after it. For
 *  vendored functions whose `#ifdef ... #else` branches each open the same brace,
 *  so a raw brace count never closes (CheckQueue, OnFileCreate); "" if absent. */
std::string SctTopLevelBody(const std::string& src, const char* signature) {
    size_t at = 0;
    while ((at = src.find(signature, at)) != std::string::npos) {
        size_t p = at + strlen(signature);
        while (p < src.size() && (src[p] == ' ' || src[p] == '\n' || src[p] == '\t')) {
            p++;
        }
        if (p < src.size() && src[p] == '{') {
            const size_t end = src.find("\n}", p);
            if (end == std::string::npos) {
                return std::string();
            }
            return src.substr(p, end + 2 - p);
        }
        at = p;
    }
    return std::string();
}

/** Every needle in order inside @p hay (false for an empty @p hay). */
bool SctInOrder(const std::string& hay, std::initializer_list<const char*> needles) {
    if (hay.empty()) {
        return false;
    }
    size_t at = 0;
    for (const char* n : needles) {
        const size_t f = hay.find(n, at);
        if (f == std::string::npos) {
            return false;
        }
        at = f + strlen(n);
    }
    return true;
}

/** OoT's check for each RsbsSongCheck 0..11 and MM's for 12..22, found by (a)
 *  and (b); 0 when not found. */
uint16_t sSctHost[RSBS_SONG_CHECK_COUNT];

// ---- (a) ------------------------------------------------------------------------
void SctCheckOoTSongRows() {
    int seen[RSBS_SONG_CHECK_OOT_COUNT] = { 0 };
    int songRows = 0;
    int otherIndexed = 0;
    const int rcMax = OoT_ComboLogic_TestCheckIdSpace();
    for (int rc = 1; rc < rcMax; rc++) {
        int facts[4] = { 0, 0, 0, 0 };
        const int index = OoT_ComboLogic_TestSongCheckIndex((uint16_t)rc);
        if (OoT_ComboLogic_TestHostFacts((uint16_t)rc, facts) == 0) {
            SCT_CHECK(index == -1, "(a) OoT check %d is not a real row but has song index %d\n", rc, index);
            continue;
        }
        if (facts[0] != 5) {
            otherIndexed += index != -1 ? 1 : 0;
            SCT_CHECK(index == -1, "(a) OoT check %d (%s) is no song location but has song index %d\n", rc,
                      OoT_ComboLogic_TestCheckName((uint16_t)rc), index);
            continue;
        }
        songRows++;
        const char* name = OoT_ComboLogic_TestCheckName((uint16_t)rc);
        int listed = -1;
        for (int i = 0; i < RSBS_SONG_CHECK_OOT_COUNT; i++) {
            if (name != NULL && strcmp(name, kSctOoTSongNames[i]) == 0) {
                listed = i;
            }
        }
        SCT_CHECK(listed >= 0, "(a) OoT song location '%s' (check %d) is not one of OoTMM's twelve\n",
                  name != NULL ? name : "?", rc);
        SCT_CHECK(listed < 0 || index == listed, "(a) OoT song location '%s' has song index %d, want %d\n",
                  name != NULL ? name : "?", index, listed);
        if (listed >= 0) {
            seen[listed]++;
            sSctHost[listed] = (uint16_t)rc;
        }
    }
    SCT_CHECK(songRows == RSBS_SONG_CHECK_OOT_COUNT, "(a) OoT has %d song locations, want OoTMM's 12\n", songRows);
    SCT_CHECK(otherIndexed == 0, "(a) %d OoT rows that are no song location carry a song index\n", otherIndexed);
    for (int i = 0; i < RSBS_SONG_CHECK_OOT_COUNT; i++) {
        SCT_CHECK(seen[i] == 1, "(a) '%s' was seen %d times among OoT's song locations, want once\n",
                  kSctOoTSongNames[i], seen[i]);
    }
}

// ---- (b) ------------------------------------------------------------------------
void SctCheckMMSongRows() {
    int seen[RSBS_SONG_CHECK_MM_COUNT] = { 0 };
    int songRows = 0;
    const int idMax = MM_Rando_Foreign_TestCheckIdMax();
    for (int id = 1; id < idMax; id++) {
        const int index = MM_Rando_Foreign_TestSongCheckIndex((uint16_t)id);
        const int isSong = MM_Rando_Foreign_TestIsSongRow((uint16_t)id);
        if (isSong != 1) {
            SCT_CHECK(index == -1, "(b) MM check %d (%s) is no song row but has song index %d\n", id,
                      isSong < 0 ? "no row" : MM_Rando_Foreign_TestCheckRowName((uint16_t)id), index);
            continue;
        }
        songRows++;
        const char* name = MM_Rando_Foreign_TestCheckRowName((uint16_t)id);
        int listed = -1;
        for (int i = 0; i < RSBS_SONG_CHECK_MM_COUNT; i++) {
            if (name != NULL && strcmp(name, kSctMMSongNames[i]) == 0) {
                listed = i;
            }
        }
        SCT_CHECK(listed >= 0, "(b) MM song row %s (check %d) is not one of OoTMM's eleven\n",
                  name != NULL ? name : "?", id);
        SCT_CHECK(listed < 0 || index == RSBS_SONG_CHECK_FIRST_MM + listed,
                  "(b) MM song row %s has song index %d, want %d\n", name != NULL ? name : "?", index,
                  RSBS_SONG_CHECK_FIRST_MM + listed);
        if (listed >= 0) {
            seen[listed]++;
            sSctHost[RSBS_SONG_CHECK_FIRST_MM + listed] = (uint16_t)id;
        }
    }
    SCT_CHECK(songRows == RSBS_SONG_CHECK_MM_COUNT, "(b) MM has %d song rows, want OoTMM's 11\n", songRows);
    for (int i = 0; i < RSBS_SONG_CHECK_MM_COUNT; i++) {
        SCT_CHECK(seen[i] == 1, "(b) %s was seen %d times among MM's song rows, want once\n", kSctMMSongNames[i],
                  seen[i]);
    }
}

// ---- (c), (c2) --------------------------------------------------------------------
void SctCheckPureAnswer() {
    for (int forced = -1; forced <= 1; forced++) {
        Combo_SongHostsForceForTest(forced);
        SCT_CHECK(Combo_SongHostsTestOverride() == forced, "(c) the override reads %d after setting %d\n",
                  Combo_SongHostsTestOverride(), forced);
        for (uint32_t songs = 0; songs < 4; songs++) {
            SCT_CHECK(Combo_SongHostsArmed(songs) == (forced == 1),
                      "(c) the switch reads %d for OoT Shuffle Songs %u with the override %d\n",
                      Combo_SongHostsArmed(songs) ? 1 : 0, (unsigned)songs, forced);
            for (int skip = 0; skip < 2; skip++) {
                for (int i = 0; i < RSBS_SONG_CHECK_COUNT; i++) {
                    const bool want = forced == 1 && SctWantArmed(i, skip != 0);
                    const bool got = Combo_SongCheckAcceptsForeign(i, songs, skip != 0);
                    SCT_CHECK(
                        got == want,
                        "(c) song check %d, OoT Shuffle Songs %u, Skip Child Zelda %d, override %d: %d, want %d\n", i,
                        (unsigned)songs, skip, forced, got ? 1 : 0, want ? 1 : 0);
                }
                SCT_CHECK(!Combo_SongCheckAcceptsForeign(-1, songs, skip != 0) &&
                              !Combo_SongCheckAcceptsForeign(RSBS_SONG_CHECK_COUNT, songs, skip != 0),
                          "(c) an index outside 0..22 is accepted (override %d)\n", forced);
            }
        }
    }
    Combo_SongHostsForceForTest(-1);

    // (c2) literal: games, rules, give paths.
    static const uint8_t kPath[RSBS_SONG_CHECK_COUNT] = {
        RSBS_SONG_GIVE_OOT_ACTOR_FLAG,   RSBS_SONG_GIVE_OOT_ACTOR_FLAG,   RSBS_SONG_GIVE_OOT_ACTOR_FLAG,
        RSBS_SONG_GIVE_OOT_TRIGGER_FLAG, RSBS_SONG_GIVE_OOT_ACTOR_FLAG,   RSBS_SONG_GIVE_OOT_ACTOR_FLAG,
        RSBS_SONG_GIVE_OOT_ACTOR_FLAG,   RSBS_SONG_GIVE_OOT_TRIGGER_FLAG, RSBS_SONG_GIVE_OOT_ACTOR_FLAG,
        RSBS_SONG_GIVE_OOT_ACTOR_FLAG,   RSBS_SONG_GIVE_OOT_TRIGGER_FLAG, RSBS_SONG_GIVE_OOT_ACTOR_FLAG,
        RSBS_SONG_GIVE_MM_CREATION,      RSBS_SONG_GIVE_MM_REARMS,        RSBS_SONG_GIVE_MM_ONCE,
        RSBS_SONG_GIVE_MM_REARMS,        RSBS_SONG_GIVE_MM_REARMS,        RSBS_SONG_GIVE_MM_REARMS,
        RSBS_SONG_GIVE_MM_REARMS,        RSBS_SONG_GIVE_MM_REARMS,        RSBS_SONG_GIVE_MM_REARMS,
        RSBS_SONG_GIVE_MM_REARMS,        RSBS_SONG_GIVE_MM_REARMS,
    };
    for (int i = 0; i < RSBS_SONG_CHECK_COUNT; i++) {
        const RsbsSongCheckInfo* info = Combo_SongCheckInfo(i);
        SCT_CHECK(info != NULL, "(c2) song check %d has no table row\n", i);
        if (info == NULL) {
            continue;
        }
        const uint8_t wantGame = i < RSBS_SONG_CHECK_OOT_COUNT ? RSBS_SONG_CHECK_GAME_OOT : RSBS_SONG_CHECK_GAME_MM;
        const uint8_t wantRule = i == kSctImpa ? RSBS_SONG_HOST_ALLOW_UNLESS_SKIP_CHILD_ZELDA : RSBS_SONG_HOST_ALLOW;
        SCT_CHECK(info->game == wantGame && info->rule == wantRule && info->path == kPath[i],
                  "(c2) song check %d (%s): game %u rule %u path %u, want %u %u %u\n", i,
                  info->label != NULL ? info->label : "?", (unsigned)info->game, (unsigned)info->rule,
                  (unsigned)info->path, (unsigned)wantGame, (unsigned)wantRule, (unsigned)kPath[i]);
        SCT_CHECK(info->label != NULL && info->label[0] != '\0', "(c2) song check %d has no label\n", i);
    }
    SCT_CHECK(Combo_SongCheckInfo(-1) == NULL && Combo_SongCheckInfo(RSBS_SONG_CHECK_COUNT) == NULL,
              "(c2) an index outside 0..22 has a table row\n");
}

// ---- (d) -------------------------------------------------------------------------
void SctCheckEnginesArmed() {
    Combo_SongHostsForceForTest(1);
    for (int songs = 0; songs < 4; songs++) {
        for (int skip = 0; skip < 2; skip++) {
            SCT_CHECK(OoT_ComboLogic_TestHoldSongContext(songs, skip) == 0, "(d) no Rando context could be held\n");
            SCT_CHECK(OoT_Combo_FrozenShuffleSongs() == (uint32_t)songs,
                      "(d) OoT_Combo_FrozenShuffleSongs reads %u, the context holds %d\n",
                      (unsigned)OoT_Combo_FrozenShuffleSongs(), songs);
            for (int i = 0; i < RSBS_SONG_CHECK_OOT_COUNT; i++) {
                if (sSctHost[i] == 0) {
                    continue; // (a) reported it
                }
                const int want = SctWantArmed(i, skip != 0) ? 1 : 0;
                const int got = OoT_ComboLogic_TestHostAcceptsForeign(sSctHost[i]);
                SCT_CHECK(got == want, "(d) OoT's predicate on '%s' (songs %d, skip %d, armed): %d, want %d\n",
                          kSctOoTSongNames[i], songs, skip, got, want);
            }
            for (int i = RSBS_SONG_CHECK_FIRST_MM; i < RSBS_SONG_CHECK_COUNT; i++) {
                if (sSctHost[i] == 0) {
                    continue; // (b) reported it
                }
                const int pure = Combo_SongCheckAcceptsForeign(i, OoT_Combo_FrozenShuffleSongs(), false) ? 1 : 0;
                const int got = MM_Rando_Foreign_TestIsForeignHostClass(sSctHost[i]);
                SCT_CHECK(got == 1 && got == pure,
                          "(d) MM's host class on %s (songs %d, armed): %d (pure %d), want 1\n",
                          kSctMMSongNames[i - RSBS_SONG_CHECK_FIRST_MM], songs, got, pure);
                for (uint32_t captured = 0; captured < 4; captured++) {
                    Combo_PairSongsCapture(captured);
                    const int withCapture = MM_Rando_Foreign_TestIsForeignHostClass(sSctHost[i]);
                    Combo_PairSongsClear();
                    const int withoutCapture = MM_Rando_Foreign_TestIsForeignHostClass(sSctHost[i]);
                    SCT_CHECK(withCapture == got && withoutCapture == got,
                              "(d) MM's answer on %s moves with the creation capture (%d with capture %u, %d with "
                              "none, %d before): the capture is not a host-acceptance input\n",
                              kSctMMSongNames[i - RSBS_SONG_CHECK_FIRST_MM], withCapture, (unsigned)captured,
                              withoutCapture, got);
                }
            }
            OoT_ComboLogic_TestReleaseSongContext();
        }
    }
    Combo_SongHostsForceForTest(-1);
}

// ---- (e), (f) ---------------------------------------------------------------------

/** The OoT host rule at the base commit, from facts (see the file comment). */
int SctOoTBaseAnswer(const int* facts, const char* name) {
    const int type = facts[0];
    const int actor = facts[1];
    const bool isShopRow = facts[2] != 0;
    const bool chestGameRoom = facts[3] != 0;
    if (type == 1 && actor == 1) {
        return 1; // a plain shop shelf
    }
    if (type == 2 && actor == 2) {
        return 1; // a Business Scrub
    }
    if (type == 3) {
        return 1; // one of the four traced merchants (counted to four below)
    }
    if (actor != 3 || type == 1 || type == 2 || type == 3 || type == 4 || isShopRow) {
        return 0;
    }
    if (chestGameRoom) {
        return (name != NULL && strcmp(name, "Treasure Chest Game Reward") == 0) ? 1 : 0;
    }
    return 1;
}

/** MM's host rule at the base commit: Tingle, cycle chests and shops. */
int SctMMBaseAnswer(int isChest, int hasChestFlag, int isShop, int isTingle) {
    return (isTingle || (isChest && hasChestFlag) || isShop) ? 1 : 0;
}

void SctCheckBaseTables(int forced) {
    Combo_SongHostsForceForTest(forced);
    int ootAccepted = 0;
    int ootMerchants = 0;
    int ootMismatch = 0;
    const int rcMax = OoT_ComboLogic_TestCheckIdSpace();
    for (int rc = 1; rc < rcMax; rc++) {
        int facts[4] = { 0, 0, 0, 0 };
        if (OoT_ComboLogic_TestHostFacts((uint16_t)rc, facts) == 0) {
            SCT_CHECK(OoT_ComboLogic_TestHostAcceptsForeign((uint16_t)rc) == 0,
                      "(f) OoT check %d is no real row but is accepted\n", rc);
            continue;
        }
        const char* name = OoT_ComboLogic_TestCheckName((uint16_t)rc);
        const int want = SctOoTBaseAnswer(facts, name);
        const int got = OoT_ComboLogic_TestHostAcceptsForeign((uint16_t)rc);
        ootAccepted += got;
        ootMerchants += facts[0] == 3 ? 1 : 0;
        if (got != want) {
            ootMismatch++;
            printf("[TEST] FAIL (song-check-tables): (f) override %d: OoT '%s' (check %d) answers %d, the base "
                   "rule %d\n",
                   forced, name != NULL ? name : "?", rc, got, want);
        }
    }
    sSctFailures += ootMismatch;
    // The base rule names four merchants one by one; a fifth upstream row would be
    // refused there and accepted by this restatement, so pin the count.
    SCT_CHECK(ootMerchants == 4, "(f) OoT has %d merchant rows; the base rule traced four\n", ootMerchants);
    SCT_CHECK(ootAccepted > 0, "(f) no OoT row is accepted, so the comparison proves nothing\n");

    int mmAccepted = 0;
    int mmMismatch = 0;
    const int idMax = MM_Rando_Foreign_TestCheckIdMax();
    for (int id = 1; id < idMax; id++) {
        int isChest = 0;
        int hasChestFlag = 0;
        int isShop = 0;
        int isTingle = 0;
        const int got = MM_Rando_Foreign_TestIsForeignHostClass((uint16_t)id);
        if (MM_Rando_Foreign_TestCheckClass((uint16_t)id, &isChest, &hasChestFlag) == 0) {
            SCT_CHECK(got == 0, "(f) MM check %d is no real row but is accepted\n", id);
            continue;
        }
        (void)MM_Rando_Foreign_TestCheckShopClass((uint16_t)id, &isShop, &isTingle);
        const int want = SctMMBaseAnswer(isChest, hasChestFlag, isShop, isTingle);
        mmAccepted += got;
        if (got != want) {
            mmMismatch++;
            printf("[TEST] FAIL (song-check-tables): (f) override %d: MM %s (check %d) answers %d, the base rule "
                   "%d\n",
                   forced, MM_Rando_Foreign_TestCheckRowName((uint16_t)id), id, got, want);
        }
    }
    sSctFailures += mmMismatch;
    SCT_CHECK(mmAccepted > 0, "(f) no MM row is accepted, so the comparison proves nothing\n");
    for (int i = 0; i < RSBS_SONG_CHECK_COUNT; i++) {
        if (sSctHost[i] == 0) {
            continue;
        }
        const int got = i < RSBS_SONG_CHECK_OOT_COUNT ? OoT_ComboLogic_TestHostAcceptsForeign(sSctHost[i])
                                                      : MM_Rando_Foreign_TestIsForeignHostClass(sSctHost[i]);
        SCT_CHECK(got == 0, "(f) override %d: song check %d accepts a foreign item with the switch off\n", forced, i);
    }
    printf("[TEST] song-check-tables: (f) override %d: %d OoT and %d MM rows accepted, %d and %d against the base "
           "rule\n",
           forced, ootAccepted, mmAccepted, ootMismatch, mmMismatch);
    Combo_SongHostsForceForTest(-1);
}

void SctCheckNonSongUnmoved() {
    // With a context holding Song Locations and Skip Child Zelda off, the values
    // S3 arms under, so a context read on a non-song path would show.
    SCT_CHECK(OoT_ComboLogic_TestHoldSongContext((int)RSBS_OOT_SONGS_SONG_LOCATIONS, 0) == 0,
              "(e) no Rando context could be held\n");
    int moved = 0;
    const int rcMax = OoT_ComboLogic_TestCheckIdSpace();
    for (int rc = 1; rc < rcMax; rc++) {
        if (OoT_ComboLogic_TestSongCheckIndex((uint16_t)rc) >= 0) {
            continue;
        }
        Combo_SongHostsForceForTest(-1);
        const int off = OoT_ComboLogic_TestHostAcceptsForeign((uint16_t)rc);
        Combo_SongHostsForceForTest(1);
        const int on = OoT_ComboLogic_TestHostAcceptsForeign((uint16_t)rc);
        if (off != on) {
            moved++;
            printf("[TEST] FAIL (song-check-tables): (e) OoT check %d (%s) answers %d armed, %d off\n", rc,
                   OoT_ComboLogic_TestCheckName((uint16_t)rc), on, off);
        }
    }
    const int idMax = MM_Rando_Foreign_TestCheckIdMax();
    for (int id = 1; id < idMax; id++) {
        if (MM_Rando_Foreign_TestSongCheckIndex((uint16_t)id) >= 0) {
            continue;
        }
        Combo_SongHostsForceForTest(-1);
        const int off = MM_Rando_Foreign_TestIsForeignHostClass((uint16_t)id);
        Combo_SongHostsForceForTest(1);
        const int on = MM_Rando_Foreign_TestIsForeignHostClass((uint16_t)id);
        if (off != on) {
            moved++;
            printf("[TEST] FAIL (song-check-tables): (e) MM check %d answers %d armed, %d off\n", id, on, off);
        }
    }
    sSctFailures += moved;
    Combo_SongHostsForceForTest(-1);
    OoT_ComboLogic_TestReleaseSongContext();
}

// ---- (h) -------------------------------------------------------------------------
void SctCheckAnchors() {
#ifndef RSBS_SOURCE_DIR
    printf("[TEST] FAIL (song-check-tables): (h) RSBS_SOURCE_DIR is not defined, so no anchor can be read\n");
    sSctFailures++;
#else
    const std::string oot = SctReadSource("games/oot/soh/Enhancements/randomizer/ComboLogicEngineOoT.cpp");
    const std::string ootHost =
        SctFunctionBody(oot, "int OoT_ComboLogic_HostAcceptsForeign(void* self, uint16_t hostCheck)");
    SCT_CHECK(SctInOrder(ootHost, { "OoTComboLogicIsRealCheck(rc)", "OoT_SongCheckIndex(rc)", "if (song >= 0)",
                                    "Combo_SongCheckAcceptsForeign(song, OoT_Combo_FrozenShuffleSongs()",
                                    "GetOption(RSK_SKIP_CHILD_ZELDA)", "ACTOR_EN_BOX" }),
              "(h) OoT's predicate does not ask the song index and the pure answer with OoT's frozen Shuffle Songs and "
              "Skip Child Zelda before its actor test\n");
    const std::string frozen = SctFunctionBody(oot, "uint32_t OoT_Combo_FrozenShuffleSongs(void)");
    SCT_CHECK(SctInOrder(frozen, { "GetOption(RSK_SHUFFLE_SONGS)", "RSBS_OOT_SONGS_OFF" }) &&
                  frozen.find("Captured") == std::string::npos && frozen.find("CVarGet") == std::string::npos,
              "(h) OoT_Combo_FrozenShuffleSongs does not read the context's frozen option alone\n");

    const std::string mm = SctReadSource("games/mm/2s2h/Rando/Foreign.cpp");
    const std::string mmClass = SctFunctionBody(
        mm, "static bool IsAllowedHostClass(const Rando::StaticData::RandoStaticCheck& randoStaticCheck)");
    SCT_CHECK(SctInOrder(mmClass, { "MM_SongCheckIndex(randoStaticCheck.randoCheckId)", "if (song >= 0)",
                                    "Combo_SongCheckAcceptsForeign(song, OoT_Combo_FrozenShuffleSongs(), false)",
                                    "RCTYPE_TINGLE_SHOP" }),
              "(h) MM's host class does not answer the song checks first from OoT's frozen Shuffle Songs\n");
    SCT_CHECK(!mmClass.empty() && mmClass.find("Combo_PairSongs") == std::string::npos &&
                  mmClass.find("ResolvedPairedSongs") == std::string::npos &&
                  mmClass.find("RO_SHUFFLE_SONGS") == std::string::npos,
              "(h) MM's host class reads the creation capture or MM's own songs option, directly or through a "
              "helper\n");
    const std::string guardFn = SctFunctionBody(mm, "bool IsDeliveredForeignSongHost(RandoCheckId randoCheckId)");
    SCT_CHECK(SctInOrder(guardFn, { "MM_SongCheckIndex(randoCheckId) >= 0", "IsDeliveredForeignHost(randoCheckId)" }) &&
                  guardFn.find("Combo_SongHostsArmed") == std::string::npos &&
                  guardFn.find("Captured") == std::string::npos,
              "(h) IsDeliveredForeignSongHost is not 'a song check that delivered its placement' alone\n");

    const std::string queue = SctReadSource("games/mm/2s2h/Rando/MiscBehavior/CheckQueue.cpp");
    const std::string queueFn = SctTopLevelBody(queue, "void Rando::MiscBehavior::CheckQueue()");
    SCT_CHECK(!queueFn.empty() && queueFn.find("void Rando::MiscBehavior::CheckQueueReset()") == std::string::npos,
              "(h) CheckQueue's body could not be read (signature changed, or the read ran past it)\n");
    SCT_CHECK(SctInOrder(queueFn, { "if (randoSaveCheck.eligible) {", "IsDeliveredForeignSongHost(randoCheckId)",
                                    "RANDO_SAVE_CHECKS[randoCheckId].eligible = false;",
                                    "RANDO_SAVE_CHECKS[randoCheckId].cycleObtained = true;", "continue;",
                                    "ForeignOutboxFullFor(randoCheckId)", "queued = true;" }),
              "(h) CheckQueue does not consume a delivered song host inside `eligible`, through the array, before "
              "the outbox hold and the queue\n");

    const std::string pool = SctReadSource("src/common/combo_song_pool.cpp");
    const std::string armed = SctFunctionBody(pool, "bool Combo_SongHostsArmed(uint32_t frozenOoTSongs)");
    SCT_CHECK(SctInOrder(armed, { "sSongHostsOverride.load", "return false;" }) &&
                  armed.find("RSBS_OOT_SONGS_SONG_LOCATIONS") == std::string::npos,
              "(h) the switch's production body is not 'false' (S0 arms nothing)\n");
    SCT_CHECK(!pool.empty() && pool.find("CVarGet") == std::string::npos && pool.find("CVarSet") == std::string::npos,
              "(h) combo_song_pool.cpp reads or writes a CVar\n");

    const std::string create = SctReadSource("games/mm/2s2h/Rando/MiscBehavior/OnFileCreate.cpp");
    const std::string createFn = SctTopLevelBody(create, "void Rando::MiscBehavior::OnFileCreate(s16 fileNum)");
    SCT_CHECK(!createFn.empty(), "(h) OnFileCreate's body could not be read (signature changed)\n");
    SCT_CHECK(SctInOrder(createFn, { "== 0 || rsbsPaired) {", "Rando::Spoiler::ApplyToSaveContext(" }),
              "(h) MM's spoiler-load branch (the one path whose host-class gate runs outside a creation) is no "
              "longer behind the paired world's always-generate condition\n");

    const std::string save = SctReadSource("games/oot/soh/Enhancements/randomizer/savefile.cpp");
    const std::string give =
        SctFunctionBody(save, "void StartingItemGive(GetItemEntry getItemEntry, RandomizerCheck randomizerCheck)");
    SCT_CHECK(!give.empty() && give.find("Combo_GetForeignPlacementForOoTCheck") == std::string::npos &&
                  give.find("OoT_Rando_Foreign_RecordPickup") == std::string::npos,
              "(h) StartingItemGive gained a crossing branch: Impa's Skip Child Zelda refusal may be revisited\n");
    const std::string init = SctFunctionBody(save, "void Randomizer_InitSaveFile()");
    SCT_CHECK(SctInOrder(init, { "RSK_SKIP_CHILD_ZELDA", "RC_SONG_FROM_IMPA",
                                 "StartingItemGive(getItemEntry, "
                                 "RC_SONG_FROM_IMPA)" }),
              "(h) Skip Child Zelda no longer gives Impa's check at creation through StartingItemGive\n");

    const std::string model = SctReadSource("games/mm/2s2h/Rando/ForeignModelSingleExe.cpp");
    const std::string icon = SctReadSource("games/mm/2s2h/Rando/ForeignTextboxIconSingleExe.cpp");
    SCT_CHECK(SctInOrder(SctFunctionBody(model, "bool RunCheckQueueDraw(uint16_t mmCheckId, const ComboModel* want)"),
                         { "MM_Rando_Foreign_TestSongCheckIndex(mmCheckId) < 0", ".obtained = true;" }) &&
                  SctInOrder(SctFunctionBody(icon, "bool RunCheckQueueGive(uint16_t mmCheckId, const char* "
                                                   "wantTexture, uint8_t wantItemId)"),
                             { "MM_Rando_Foreign_TestSongCheckIndex(mmCheckId) < 0", ".obtained = true;" }),
              "(h) a CheckQueue drive that pre-sets `obtained` does not refuse a song check first\n");
#endif
}

} // namespace

TestResult SongCheckTables_Run(void) {
    printf("[TEST] song-check-tables: both games' song checks are cross-game hosts only behind one dormant "
           "switch, Impa refuses under Skip Child Zelda, and with the switch off every host answer is the base "
           "commit's (#1542 S0)\n");
    sSctFailures = 0;
    std::memset(sSctHost, 0, sizeof(sSctHost));
    const int contextBefore = OoT_ComboLogic_TestRandoContextExists();
    const int16_t captureBefore = Combo_PairSongsSnapshot();
    SCT_CHECK(Combo_SongHostsTestOverride() == -1, "the override is %d on entry, not cleared\n",
              Combo_SongHostsTestOverride());
    if (OoT_ComboLogic_TestEnsureLocationTable() != 0) {
        printf("[TEST] FAIL (song-check-tables): OoT's location table is not available\n");
        return TEST_FAIL;
    }
    {
        SctHookGuard guard;
        SctCheckOoTSongRows();
        SctCheckMMSongRows();
        SctCheckPureAnswer();
        SctCheckEnginesArmed();
        SctCheckNonSongUnmoved();
        SctCheckBaseTables(-1);
        SctCheckBaseTables(0);
        SctCheckAnchors();
    }
    // (i) the guard restored both hooks and dropped the context it held.
    SCT_CHECK(Combo_SongHostsTestOverride() == -1, "(i) the override reads %d after the row, not cleared\n",
              Combo_SongHostsTestOverride());
    SCT_CHECK(Combo_PairSongsSnapshot() == captureBefore, "(i) the creation capture is %d after the row, was %d\n",
              (int)Combo_PairSongsSnapshot(), (int)captureBefore);
    SCT_CHECK(contextBefore != 0 || OoT_ComboLogic_TestRandoContextExists() == 0,
              "(i) the row left a Rando context behind that was not there before it\n");
    if (sSctFailures != 0) {
        printf("[TEST] FAIL: song-check-tables (%d failure(s))\n", sSctFailures);
        return TEST_FAIL;
    }
    printf("[TEST] PASS: song-check-tables\n");
    return TEST_PASS;
}

// ============================================================================
// mm-song-host-guard (CTest MMSongHostGuard, redship tier, ROM-free, generates
// nothing): the legs are MM's (games/mm/2s2h/mm_song_host_guard_test.cpp); this
// hands them a real OoT item that crosses (the Megaton Hammer: no counted pack
// and no capacity tier, so the record is a crossing, #1061 / #1120).
// ============================================================================
TestResult MMSongHostGuard_Run(void) {
    SharedItem hammer;
    if (!TestNamedItem((uint8_t)GAME_OOT, "Megaton Hammer", &hammer)) {
        printf("[TEST] FAIL (mm-song-host-guard): OoT has no item named \"Megaton Hammer\"\n");
        return TEST_FAIL;
    }
    const int16_t captureBefore = Combo_PairSongsSnapshot();
    const int rc = MM_SongHostGuard_Run(hammer.originGame, hammer.id);
    if (Combo_SongHostsTestOverride() != -1 || Combo_PairSongsSnapshot() != captureBefore) {
        printf("[TEST] FAIL (mm-song-host-guard): the row moved the song-host override or the creation capture\n");
        return TEST_FAIL;
    }
    return rc == 0 ? TEST_PASS : TEST_FAIL;
}
