/**
 * @file test_song_check_tables.c
 * @brief THE SONG CHECKS AS CROSS-GAME HOSTS (#1542 S0, armed by S3):
 *        combo_song_pool.h's verdict table, its two switches (host acceptance and
 *        the shared song pool), both engines' song-host answers and groups, and
 *        the host tables with the switch off.
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
 *      cleared arms exactly Song Locations (red if S3's production body arms any
 *      other value, or none), 0 refuses everywhere, 1 everywhere; armed, every
 *      check but Impa accepts, Impa only with Skip Child Zelda off (red on a wrong
 *      rule, or on the Impa rule lost). Out-of-range indices refuse. The shared
 *      song pool's rule (Combo_SongPoolUnionRule) against its literal truth
 *      table: true only paired-and-frozen at Song Locations with the override
 *      cleared or 1, never with 0, never outside Song Locations (red if the
 *      override arms the union under Anywhere: ComboSingleBag leg E4's world).
 *      Combo_SongPoolCapacityRefusal's four wordings against literals.
 *  (c2) The verdict table's game, rule and give path, row by row, against a
 *      literal list (red on any table edit that this row does not also make).
 *  (d) The two engines with the override 1 and cleared, a Rando context whose
 *      frozen options are set (OoT_ComboLogic_TestHoldSongContext) and the pair
 *      active (gComboCtx.sourceIsRando and a settings hash): OoT's REAL predicate
 *      answers (c)'s literal for the frozen values; MM's REAL host class answers
 *      the pure answer for OoT's frozen value, so at override -1 both engines are
 *      armed exactly at Song Locations (red if either engine's arming disagrees
 *      with the pure switch); OoT_Combo_FrozenShuffleSongs reads the context's
 *      option; and a creation capture of each value, then none, leaves every MM
 *      answer unchanged (red on a capture-gated return; now that the switch reads
 *      the songs value, a capture read in place of the frozen value moves an
 *      answer at override -1 too).
 *  (d-ctl) With the pair NOT active, songs Song Locations and the override 1,
 *      MM's host class refuses all eleven: the lock on MM's pairing term (red if
 *      the unpaired spoiler-load gate could admit a song check).
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
 *      FLAG_CYCL_SCENE_CHEST) and shops. All 23 song checks refuse. Run with the
 *      override cleared under a held context at Anywhere (production, the switch
 *      unarmed), and with the override 0 under a held context at Song Locations.
 *      Red on any host-acceptance change with the switch off.
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
 *  (j) THE SHARED SONG POOL'S GROUPS, with a held context at Song Locations, the
 *      pair active and the combo settings frozen (gComboCtx), and MM's Shuffle
 *      Songs at Song Locations: the union is on in both engines; OoT's REAL
 *      `hostGroups` answers the wide song group on exactly the twelve OoT song
 *      checks and 0 elsewhere; MM's answers it on exactly its eleven and no other
 *      check carries a wide bit; both declare it (`wideGroups`); OoT's row masks
 *      give it to exactly the songs, MM's to exactly OoTMM's song set, and no
 *      mask mixes namespaces. At Dungeon Rewards nothing is wide and OoT's masks
 *      are 0. Red on any producer that bypasses the union, or on the union
 *      armed outside Song Locations.
 *  (k) The union on but MM's own Shuffle Songs at Anywhere: MM declares no wide
 *      group, and a fill request carrying the wide group is refused BAD_REQUEST
 *      by the real coordinator before any attempt (red if a half-armed pair could
 *      reach the fill).
 *  (i) After the scope guard: the override is cleared, the capture is what it was
 *      on entry, the pairing fields and MM's songs value are restored, and no Rando
 *      context is left that was not there on entry.
 *
 * Linkage note: #included into test_runner.cpp at FILE SCOPE (compiled as C++),
 * like its neighbours; every symbol under test is C-linkage. Helpers carry the
 * Sct prefix because every included test file shares one translation unit.
 */

#include "../combo_logic.h"
#include "../combo_song_pool.h"
#include "../context.h"
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
// #1542 S3 bridges (ComboLogicEngineOoT.cpp, ComboLogicEngineSingleExe.cpp).
int OoT_ComboLogic_TestEnsureItemTableTransient(void);
void OoT_ComboLogic_TestSongRowMasks(int expectUnion, int* outSongs, int* outWide, int* outWrong);
int MM_ComboLogic_TestSetShuffleSongs(int value);
void MM_ComboLogic_TestSongGroupCensus(int* outSongItems, int* outWideItems, int* outMixed, int* outWideSongChecks,
                                       int* outWideOtherChecks);
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
    bool sourceIsRando;
    uint32_t settingsHash;
    uint8_t formatVersion;
    int mmSongs;
    SctHookGuard()
        : capture(Combo_PairSongsSnapshot()), sourceIsRando(gComboCtx.sourceIsRando),
          settingsHash(gComboCtx.sharedRandoSettingsHash), formatVersion(gComboCtx.comboSettings.formatVersion),
          mmSongs(MM_ComboLogic_TestSetShuffleSongs(0)) {
        (void)MM_ComboLogic_TestSetShuffleSongs(mmSongs);
    }
    ~SctHookGuard() {
        Combo_SongHostsForceForTest(-1);
        Combo_PairSongsRestore(capture);
        OoT_ComboLogic_TestReleaseSongContext();
        gComboCtx.sourceIsRando = sourceIsRando;
        gComboCtx.sharedRandoSettingsHash = settingsHash;
        gComboCtx.comboSettings.formatVersion = formatVersion;
        (void)MM_ComboLogic_TestSetShuffleSongs(mmSongs);
    }
};

/** The pair active (Combo_ForeignPairingActive), as test_foreign_items.c sets it,
 *  and optionally the combo settings frozen (Combo_ComboSettingsFrozen reads the
 *  record's format version), so OoT_ComboLogic_DeferGeneralPassWanted holds. The
 *  guard puts all three back. */
void SctSetPairing(bool active, bool frozen) {
    gComboCtx.sourceIsRando = active;
    gComboCtx.sharedRandoSettingsHash = active ? 0x5EED5A5Du : 0u;
    gComboCtx.comboSettings.formatVersion = frozen ? (uint8_t)1u : (uint8_t)0u;
}

/** (c)'s literal for the switch: the override, else Song Locations. */
bool SctSwitchWant(int forced, uint32_t songs) {
    return forced >= 0 ? forced == 1 : songs == RSBS_OOT_SONGS_SONG_LOCATIONS;
}

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

/** The one source line holding @p anchor, without its line break, so needles
 *  searched in it cannot run on into a later line; "" when @p anchor is absent
 *  or occurs more than once. */
std::string SctLine(const std::string& src, const char* anchor) {
    const size_t f = src.find(anchor);
    if (f == std::string::npos || src.find(anchor, f + strlen(anchor)) != std::string::npos) {
        return std::string();
    }
    const size_t nl = src.rfind('\n', f);
    const size_t begin = nl == std::string::npos ? 0 : nl + 1;
    const size_t end = src.find('\n', f);
    return src.substr(begin, (end == std::string::npos ? src.size() : end) - begin);
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
            SCT_CHECK(Combo_SongHostsArmed(songs) == SctSwitchWant(forced, songs),
                      "(c) the switch reads %d for OoT Shuffle Songs %u with the override %d\n",
                      Combo_SongHostsArmed(songs) ? 1 : 0, (unsigned)songs, forced);
            // The shared song pool's literal truth table (S3 amendment, section C).
            const bool unionWant = songs == RSBS_OOT_SONGS_SONG_LOCATIONS && forced != 0;
            SCT_CHECK(Combo_SongPoolUnionRule(true, songs) == unionWant && !Combo_SongPoolUnionRule(false, songs),
                      "(c) the shared song pool reads %d paired / %d unpaired for OoT Shuffle Songs %u with the "
                      "override %d, want %d / 0\n",
                      Combo_SongPoolUnionRule(true, songs) ? 1 : 0, Combo_SongPoolUnionRule(false, songs) ? 1 : 0,
                      (unsigned)songs, forced, unionWant ? 1 : 0);
            for (int skip = 0; skip < 2; skip++) {
                for (int i = 0; i < RSBS_SONG_CHECK_COUNT; i++) {
                    const bool want = SctSwitchWant(forced, songs) && SctWantArmed(i, skip != 0);
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

    // The capacity refusal's four wordings (S3; OoTMM's sentence first).
    struct Wording {
        uint8_t side;
        uint32_t excluded;
        const char* want;
    };
    // #1169 XW14: the shared rows are offered on the production mask, so the
    // home-side texts advise sharing a song. Pinned literally below; this check
    // catches a gate that stops reading the shared bits as offered (the OoT and
    // MM home texts would then also lose "Share a song").
    SCT_CHECK(Combo_SongPoolShareOffered(), "(c) the shared warp-song rules do not read as offered\n");
    const char* const kOoTHome =
        "Not enough locations for songs/dungeon rewards: 13 Ocarina of Time songs for the 23 song checks they "
        "may use (Crossing Direction keeps them in Ocarina of Time). Share a song, start with an Ocarina of "
        "Time song, change the Crossing Direction, or shuffle songs Anywhere";
    const char* const kMMHome =
        "Not enough locations for songs/dungeon rewards: 16 Majora's Mask songs for the 23 song checks they "
        "may use (Crossing Direction keeps them in Majora's Mask). Share a song, start with a Majora's Mask "
        "song, change the Crossing Direction, or shuffle songs Anywhere";
    const Wording kWordings[] = {
        { RSBS_SONG_POOL_SHORT_BOTH, 0u,
          "Not enough locations for songs/dungeon rewards: 28 songs for the 23 song checks of both games (Shuffle "
          "Songs: Song Locations). Share a song or start with a song, or shuffle songs Anywhere" },
        { RSBS_SONG_POOL_SHORT_BOTH, 2u,
          "Not enough locations for songs/dungeon rewards: 28 songs for the 23 song checks of both games (Shuffle "
          "Songs: Song Locations). Share a song or start with a song, or include an excluded song check, or shuffle "
          "songs Anywhere" },
        { RSBS_SONG_POOL_SHORT_OOT, 0u, kOoTHome },
        { RSBS_SONG_POOL_SHORT_MM, 0u, kMMHome },
        { RSBS_SONG_POOL_SHORT_SOME, 0u,
          "Not enough locations for songs/dungeon rewards: 28 songs for the 23 song checks that can hold them "
          "(Shuffle Songs: Song Locations). Share a song or start with a song, or shuffle songs Anywhere" },
    };
    for (const Wording& w : kWordings) {
        char msg[512];
        std::memset(msg, 'x', sizeof(msg));
        (void)Combo_SongPoolCapacityRefusal(w.side, 28u, 23u, 13u, 16u, w.excluded, msg, sizeof(msg));
        SCT_CHECK(std::strcmp(msg, w.want) == 0, "(c) the song pool refusal for side %u (excluded %u) reads '%s'\n",
                  (unsigned)w.side, (unsigned)w.excluded, msg);
    }
    char tiny[8];
    std::memset(tiny, 'x', sizeof(tiny));
    (void)Combo_SongPoolCapacityRefusal(RSBS_SONG_POOL_SHORT_BOTH, 28u, 23u, 13u, 16u, 0u, tiny, sizeof(tiny));
    SCT_CHECK(tiny[sizeof(tiny) - 1] == '\0', "(c) a short buffer is not NUL-terminated\n");
}

// ---- (d) -------------------------------------------------------------------------
void SctCheckEnginesArmed(int forced) {
    Combo_SongHostsForceForTest(forced);
    // MM's song tier answers only while the pair is active (#1542 S3).
    SctSetPairing(true, false);
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
                const int want = (SctSwitchWant(forced, (uint32_t)songs) && SctWantArmed(i, skip != 0)) ? 1 : 0;
                const int got = OoT_ComboLogic_TestHostAcceptsForeign(sSctHost[i]);
                SCT_CHECK(got == want, "(d) OoT's predicate on '%s' (songs %d, skip %d, override %d): %d, want %d\n",
                          kSctOoTSongNames[i], songs, skip, forced, got, want);
            }
            for (int i = RSBS_SONG_CHECK_FIRST_MM; i < RSBS_SONG_CHECK_COUNT; i++) {
                if (sSctHost[i] == 0) {
                    continue; // (b) reported it
                }
                const int pure = Combo_SongCheckAcceptsForeign(i, OoT_Combo_FrozenShuffleSongs(), false) ? 1 : 0;
                const int want = SctSwitchWant(forced, (uint32_t)songs) ? 1 : 0;
                const int got = MM_Rando_Foreign_TestIsForeignHostClass(sSctHost[i]);
                SCT_CHECK(got == want && got == pure,
                          "(d) MM's host class on %s (songs %d, override %d): %d (pure %d), want %d\n",
                          kSctMMSongNames[i - RSBS_SONG_CHECK_FIRST_MM], songs, forced, got, pure, want);
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
    SctSetPairing(false, false);
}

// ---- (d-ctl) ---------------------------------------------------------------------
void SctCheckMMNeedsPairing() {
    Combo_SongHostsForceForTest(1);
    SctSetPairing(false, false);
    SCT_CHECK(OoT_ComboLogic_TestHoldSongContext((int)RSBS_OOT_SONGS_SONG_LOCATIONS, 0) == 0,
              "(d-ctl) no Rando context could be held\n");
    int admitted = 0;
    for (int i = RSBS_SONG_CHECK_FIRST_MM; i < RSBS_SONG_CHECK_COUNT; i++) {
        if (sSctHost[i] != 0) {
            admitted += MM_Rando_Foreign_TestIsForeignHostClass(sSctHost[i]);
        }
    }
    SCT_CHECK(admitted == 0,
              "(d-ctl) with no pair active MM's host class admitted %d song checks: the unpaired "
              "spoiler-load gate could take a crossing\n",
              admitted);
    OoT_ComboLogic_TestReleaseSongContext();
    Combo_SongHostsForceForTest(-1);
}

// ---- (j), (k) --------------------------------------------------------------------
void SctCheckPoolGroups() {
    SCT_CHECK(OoT_ComboLogic_TestEnsureItemTableTransient() == 0, "(j) OoT's item table is not available\n");
    const ComboLogicEngine* oot = Combo_Logic_GetEngine(GAME_OOT);
    const ComboLogicEngine* mm = Combo_Logic_GetEngine(GAME_MM);
    SCT_CHECK(oot != NULL && mm != NULL && oot->hostGroups != NULL && mm->hostGroups != NULL &&
                  oot->wideGroups != NULL && mm->wideGroups != NULL,
              "(j) an engine is missing, or lacks hostGroups or wideGroups\n");
    if (oot == NULL || mm == NULL || oot->hostGroups == NULL || mm->hostGroups == NULL || oot->wideGroups == NULL ||
        mm->wideGroups == NULL) {
        return;
    }
    const uint32_t kWide = RSBS_COMBO_GROUP_SONG_LOCATIONS;
    const int rcMax = OoT_ComboLogic_TestCheckIdSpace();
    for (int songs = 0; songs < 4; songs++) {
        Combo_SongHostsForceForTest(-1);
        SctSetPairing(true, true);
        (void)MM_ComboLogic_TestSetShuffleSongs((int)Combo_PairSongsMmValue((uint32_t)songs));
        SCT_CHECK(OoT_ComboLogic_TestHoldSongContext(songs, 0) == 0, "(j) no Rando context could be held\n");
        const bool on = songs == (int)RSBS_OOT_SONGS_SONG_LOCATIONS;
        SCT_CHECK(OoT_ComboLogic_SongPoolUnion() == on && MM_ComboLogic_SongPoolUnionLive() == on,
                  "(j) songs %d: the union reads OoT %d / MM %d, want %d\n", songs,
                  OoT_ComboLogic_SongPoolUnion() ? 1 : 0, MM_ComboLogic_SongPoolUnionLive() ? 1 : 0, on ? 1 : 0);
        SCT_CHECK(oot->wideGroups(oot->self) == (on ? kWide : 0u) && mm->wideGroups(mm->self) == (on ? kWide : 0u),
                  "(j) songs %d: the engines declare 0x%08X / 0x%08X\n", songs, (unsigned)oot->wideGroups(oot->self),
                  (unsigned)mm->wideGroups(mm->self));
        int ootWideSong = 0;
        int ootWideOther = 0;
        for (int rc = 1; rc < rcMax; rc++) {
            const uint32_t g = oot->hostGroups(oot->self, (uint16_t)rc);
            if (OoT_ComboLogic_TestSongCheckIndex((uint16_t)rc) >= 0) {
                ootWideSong += (g == kWide) ? 1 : 0;
            } else {
                ootWideOther += (g != 0u) ? 1 : 0;
            }
        }
        SCT_CHECK(ootWideSong == (on ? RSBS_SONG_CHECK_OOT_COUNT : 0) && ootWideOther == 0,
                  "(j) songs %d: OoT's hostGroups answers the song group on %d song checks and a group on %d other "
                  "checks\n",
                  songs, ootWideSong, ootWideOther);
        int ootSongs = 0;
        int ootWide = 0;
        int ootWrong = 0;
        OoT_ComboLogic_TestSongRowMasks(on ? 1 : 0, &ootSongs, &ootWide, &ootWrong);
        SCT_CHECK(ootSongs >= RSBS_SONG_CHECK_OOT_COUNT && ootWrong == 0 && ootWide == (on ? ootSongs : 0),
                  "(j) songs %d: OoT's row masks: %d songs, %d wide, %d wrong\n", songs, ootSongs, ootWide, ootWrong);
        int mmSongItems = 0;
        int mmWideItems = 0;
        int mmMixed = 0;
        int mmWideSong = 0;
        int mmWideOther = 0;
        MM_ComboLogic_TestSongGroupCensus(&mmSongItems, &mmWideItems, &mmMixed, &mmWideSong, &mmWideOther);
        SCT_CHECK(mmSongItems >= 16 && mmMixed == 0 && mmWideOther == 0 && mmWideItems == (on ? mmSongItems : 0) &&
                      mmWideSong == (on ? RSBS_SONG_CHECK_MM_COUNT : 0),
                  "(j) songs %d: MM: %d song items, %d wide, %d mixed; %d song checks wide, %d other checks wide\n",
                  songs, mmSongItems, mmWideItems, mmMixed, mmWideSong, mmWideOther);
        // The override 0 disarms the union at Song Locations.
        if (on) {
            Combo_SongHostsForceForTest(0);
            SCT_CHECK(!OoT_ComboLogic_SongPoolUnion() && oot->wideGroups(oot->self) == 0u &&
                          mm->wideGroups(mm->self) == 0u,
                      "(j) the override 0 left the union armed at Song Locations\n");
            Combo_SongHostsForceForTest(1);
            SCT_CHECK(OoT_ComboLogic_SongPoolUnion(), "(j) the override 1 disarmed the union at Song Locations\n");
            Combo_SongHostsForceForTest(-1);
            // Not paired-and-frozen: no union, whatever the songs value.
            SctSetPairing(true, false);
            SCT_CHECK(!OoT_ComboLogic_SongPoolUnion(), "(j) the union armed with the combo settings not frozen\n");
            SctSetPairing(true, true);
        } else {
            Combo_SongHostsForceForTest(1);
            SCT_CHECK(!OoT_ComboLogic_SongPoolUnion() && oot->wideGroups(oot->self) == 0u,
                      "(j) songs %d: the override 1 armed the union outside Song Locations\n", songs);
            Combo_SongHostsForceForTest(-1);
        }
        OoT_ComboLogic_TestReleaseSongContext();
    }

    // (k) the union on, MM's own songs at Anywhere: a half-armed pair.
    SctSetPairing(true, true);
    SCT_CHECK(OoT_ComboLogic_TestHoldSongContext((int)RSBS_OOT_SONGS_SONG_LOCATIONS, 0) == 0,
              "(k) no Rando context could be held\n");
    (void)MM_ComboLogic_TestSetShuffleSongs((int)RSBS_MM_SONGS_ANYWHERE);
    SCT_CHECK(OoT_ComboLogic_SongPoolUnion() && !MM_ComboLogic_SongPoolUnionLive() && mm->wideGroups(mm->self) == 0u &&
                  oot->wideGroups(oot->self) == kWide,
              "(k) with MM at Anywhere MM still declares the song group, or OoT does not\n");
    ComboLogicBagItem row;
    std::memset(&row, 0, sizeof(row));
    row.item.originGame = (uint8_t)GAME_OOT;
    row.item.id = 1u;
    row.itemClass = (uint16_t)RSBS_ITEMCLASS_PROGRESSION;
    const uint32_t mask = kWide;
    ComboLogicFillRequest req;
    std::memset(&req, 0, sizeof(req));
    req.bag = &row;
    req.bagCount = 1;
    req.goal = (uint8_t)RSBS_COMBO_GOAL_BEAT_BOTH;
    req.logicRung = (uint8_t)RSBS_COMBO_RUNG_NONE;
    req.confineMasks = &mask;
    req.exclusiveGroups = kWide;
    ComboLogicFillResult res;
    const int rc = Combo_Logic_RunFill(&req, &res);
    SCT_CHECK(rc == RSBS_COMBO_LOGIC_ERR_BAD_REQUEST && res.attempts == 0,
              "(k) a half-armed pair's wide request returned %s after %d attempt(s), want BAD_REQUEST before any\n",
              Combo_Logic_StatusName(rc), res.attempts);
    OoT_ComboLogic_TestReleaseSongContext();
    SctSetPairing(false, false);
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
    // The switch off two ways (#1542 S3): production (override cleared) under a
    // world at Anywhere, and the override 0 under a world at Song Locations.
    SCT_CHECK(OoT_ComboLogic_TestHoldSongContext(
                  forced < 0 ? (int)RSBS_OOT_SONGS_ANYWHERE : (int)RSBS_OOT_SONGS_SONG_LOCATIONS, 0) == 0,
              "(f) no Rando context could be held\n");
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
    OoT_ComboLogic_TestReleaseSongContext();
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
                                    "return Combo_ForeignPairingActive() &&",
                                    "Combo_SongCheckAcceptsForeign(song, OoT_Combo_FrozenShuffleSongs(), false)",
                                    "RCTYPE_TINGLE_SHOP" }),
              "(h) MM's host class does not answer the song checks first from OoT's frozen Shuffle Songs, behind the "
              "pairing\n");
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
    SCT_CHECK(SctInOrder(queueFn, { "if (randoSaveCheck.eligible) {", "IsDeliveredForeignHost(randoCheckId)",
                                    "RANDO_SAVE_CHECKS[randoCheckId].eligible = false;",
                                    "RANDO_SAVE_CHECKS[randoCheckId].cycleObtained = true;", "continue;",
                                    "ForeignOutboxFullFor(randoCheckId)", "queued = true;" }),
              "(h) CheckQueue does not consume a delivered foreign host inside `eligible`, through the array, "
              "before the outbox hold and the queue\n");
    SCT_CHECK(queueFn.find("IsDeliveredForeignSongHost(") == std::string::npos,
              "(h) CheckQueue's consume is still the song-only guard (#1600)\n");

    const std::string pool = SctReadSource("src/common/combo_song_pool.cpp");
    const std::string armed = SctFunctionBody(pool, "bool Combo_SongHostsArmed(uint32_t frozenOoTSongs)");
    SCT_CHECK(SctInOrder(armed, { "sSongHostsOverride.load", "return forced != 0;",
                                  "return frozenOoTSongs == RSBS_OOT_SONGS_SONG_LOCATIONS;" }),
              "(h) the switch's production body is not 'the override, else Song Locations' (#1542 S3)\n");
    const std::string unionRule = SctFunctionBody(pool, "bool Combo_SongPoolUnionRule(bool pairedFrozen, uint32_t "
                                                        "frozenOoTSongs)");
    SCT_CHECK(SctInOrder(unionRule, { "pairedFrozen && frozenOoTSongs == RSBS_OOT_SONGS_SONG_LOCATIONS && "
                                      "Combo_SongHostsArmed(frozenOoTSongs)" }),
              "(h) the shared song pool's rule is not 'paired, Song Locations, and acceptance armed'\n");
    const std::string ootUnion = SctFunctionBody(oot, "bool OoT_ComboLogic_SongPoolUnion(void)");
    SCT_CHECK(SctInOrder(ootUnion, { "Combo_SongPoolUnionRule(OoT_ComboLogic_DeferGeneralPassWanted() != 0, "
                                     "OoT_Combo_FrozenShuffleSongs())" }) &&
                  ootUnion.find("Captured") == std::string::npos,
              "(h) OoT_ComboLogic_SongPoolUnion does not read the deferral and the frozen songs value alone\n");
    const std::string mmEngine = SctReadSource("games/mm/2s2h/Rando/ComboLogicEngineSingleExe.cpp");
    const std::string mmUnion = SctFunctionBody(mmEngine, "bool MM_ComboLogic_SongPoolUnionLive(void)");
    SCT_CHECK(SctInOrder(mmUnion, { "OoT_ComboLogic_SongPoolUnion()",
                                    "RANDO_SAVE_OPTIONS[RO_SHUFFLE_SONGS] == RO_SONG_SHUFFLE_SONG_LOCATIONS" }),
              "(h) MM's half of the union is not 'OoT's union and MM's own songs at Song Locations'\n");
    // MM's leftover song checks take junk before the critical-renewables plan, so
    // under All Locations the junk step must pass over a pending first copy (OoTMM
    // solve.ts moves those out of the junk pool before fillJunk); column-0 close,
    // because the body's strings hold braces.
    SCT_CHECK(SctInOrder(SctTopLevelBody(mmEngine, "int ladderAttempt)"),
                         { "if (MM_ComboLogic_SongPoolUnionLive()) {",
                           "const bool critArmed = Combo_CriticalRenewablesArmed(report.rung);",
                           "Combo_CriticalRenewablesRenewedNow(songRenewed);", "if (!MmIsSongCheck(host)) {",
                           "if (critArmed && rest[r] != RI_JUNK && Combo_CriticalRenewablePending(row, songRenewed)) {",
                           "Combo_ItemClassOf(row) == RSBS_FILL_CLASS_RENEWABLE",
                           "Combo_CriticalRenewablesPlanFirstCopies(" }),
              "(h) MM's song-check junk step can take a pending critical renewable's first copy before the plan\n");
    const std::string fill = SctReadSource("games/oot/soh/Enhancements/randomizer/3drando/fill.cpp");
    const std::string fillFn = SctFunctionBody(fill, "int Fill()");
    SCT_CHECK(
        SctInOrder(fillFn, { "const bool rsbsSongUnion = OoT_ComboLogic_SongPoolUnion();",
                             "if (!rsbsSongUnion && ctx->GetOption(RSK_SHUFFLE_SONGS).IsNot(RO_SONG_SHUFFLE_ANYWHERE)",
                             "RandomizeDungeonItems();", "RandomizeLinksPocket();",
                             "OoT_ComboLogic_DeferGeneralPassWanted()" }),
        "(h) Fill() does not skip OoT's own song pass under the shared song pool before the seam\n");
    SCT_CHECK(SctInOrder(SctFunctionBody(fill, "static void RandomizeLinksPocket()"),
                         { "RO_LINKS_POCKET_ADVANCEMENT", "RsbsSongHeldForBag(i)", "RandomElement(advancementItems" }),
              "(h) Link's Pocket does not set the bag's songs aside before it draws\n");
    SCT_CHECK(SctInOrder(SctFunctionBody(fill, "static void RandomizeDungeonItems()"),
                         { "RsbsDropReservedSongChecks(anyDungeonLocations);",
                           "RsbsDropReservedSongChecks(rsbsOverworldLocations);",
                           "AssumedFill(anyDungeonItems, anyDungeonLocations, true);",
                           "AssumedFill(overworldItems, RSBS_OVERWORLD_LOCATIONS, true);" }),
              "(h) the restricted dungeon-item passes do not leave the song checks to the bag\n");
    SCT_CHECK(SctInOrder(SctFunctionBody(fill, "int RsbsFinishPairedGeneralPass(std::vector<RandomizerGet>& "
                                               "remainingPool)"),
                         { "if (OoT_ComboLogic_SongPoolUnion())", "OoT_ComboLogic_SongCheckJunkRow(",
                           "PlaceItemInLocation(rc, junk);", "FastFill(traps, GetAllEmptyLocations(), true);" }),
              "(h) OoT's remainder does not junk the leftover song checks before it deals traps and the rest\n");
    const std::string bag = SctReadSource("src/common/combo_single_bag.c");
    SCT_CHECK(SctInOrder(bag, { "OoT_ComboLogic_PoolRowConfineMasks(sOoTItems, ootTotal, sOoTConfine);",
                                "freq.exclusiveGroups = OoT_ComboLogic_SongPoolUnion() ? "
                                "RSBS_COMBO_GROUP_SONG_LOCATIONS : 0u;",
                                "Combo_Logic_RunFill(&freq" }),
              "(h) the single bag does not mask OoT's songs and set the exclusive song group before the fill\n");
    const std::string pools = SctReadSource("games/mm/2s2h/Rando/Logic/GeneratePools.cpp");
    SCT_CHECK(SctInOrder(SctFunctionBody(pools, "void GeneratePools(RandoSaveInfo& saveInfo, "
                                                "std::vector<RandoCheckId>& checkPool, std::vector<RandoItemId>& "
                                                "itemPool)"),
                         { "RsbsSongPoolUnion(saveInfo) && MM_ComboLogic_IsPairSongItem(",
                           "const bool rsbsSongUnion = RsbsSongPoolUnion(saveInfo);",
                           "if (!rsbsSongUnion && saveInfo.randoSaveOptions[RO_SHUFFLE_SONGS] == "
                           "RO_SONG_SHUFFLE_SONG_LOCATIONS)",
                           "randoSaveCheck.skipped = true;" }),
              "(h) GeneratePools still junks and skips a song check (or adds a Plentiful song) under the shared song "
              "pool\n");
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
    // #1404 (SoH #6938): an unshuffled starting letter skips child Zelda too, so the
    // creation give and OoT's host predicate must both read it.
    SCT_CHECK(SctInOrder(init, { "RSK_STARTING_ZELDAS_LETTER", "RSK_SHUFFLE_ZELDAS_LETTER",
                                 "StartingItemGive(getItemEntry, RC_SONG_FROM_IMPA)" }) &&
                  SctInOrder(ootHost, { "GetOption(RSK_SKIP_CHILD_ZELDA)", "GetOption(RSK_STARTING_ZELDAS_LETTER)",
                                        "GetOption(RSK_SHUFFLE_ZELDAS_LETTER)", "ACTOR_EN_BOX" }),
              "(h) Start with Zelda's Letter gives Impa's check at creation, but OoT's host predicate does not refuse "
              "Impa for it (#1404)\n");
    // #1404 QC: logic credits Talon's and Malon's return under the legacy Skip
    // Child Zelda (root.cpp), and a seed package restores that key without
    // FinalizeSettings' mapping, so the creation must wake Talon under it too.
    // Each root event is searched on its OWN line: a search over the whole file
    // would find both terms again on the RC_HC_MALON_EGG line below them.
    const std::string rootRegion = SctReadSource("games/oot/soh/Enhancements/randomizer/location_access/root.cpp");
    const std::string rootTalon = SctLine(rootRegion, "EventAccess(LOGIC_TALON_RETURNED_FROM_CASTLE,");
    const std::string rootMalon = SctLine(rootRegion, "EventAccess(LOGIC_MALON_RETURNED_FROM_CASTLE,");
    SCT_CHECK(SctInOrder(init, { "RO_WEIRD_EGG_SKIP_TALON ||", "Randomizer_GetSettingValue(RSK_SKIP_CHILD_ZELDA)) {",
                                 "EVENTCHKINF_TALON_RETURNED_FROM_CASTLE" }) &&
                  SctInOrder(rootTalon, { "GetOption(RSK_SHUFFLE_WEIRD_EGG).Is(RO_WEIRD_EGG_SKIP_TALON) ||",
                                          "GetOption(RSK_SKIP_CHILD_ZELDA)" }),
              "(h) logic credits Talon's return under the legacy Skip Child Zelda, but the creation no longer wakes "
              "him under it, or root.cpp's Talon event lost a term (#1404)\n");
    SCT_CHECK(SctInOrder(rootMalon, { "GetOption(RSK_SHUFFLE_WEIRD_EGG).Is(RO_WEIRD_EGG_SKIP_TALON) ||",
                                      "GetOption(RSK_SKIP_CHILD_ZELDA)" }),
              "(h) root.cpp's Malon event no longer reads both Skip Waking Talon and the legacy Skip Child Zelda, "
              "so Malon's song leaves logic on a file whose creation sent her home (#1404)\n");

    const std::string model = SctReadSource("games/mm/2s2h/Rando/ForeignModelSingleExe.cpp");
    const std::string icon = SctReadSource("games/mm/2s2h/Rando/ForeignTextboxIconSingleExe.cpp");
    const std::string drawDrive =
        SctFunctionBody(model, "bool RunCheckQueueDraw(uint16_t mmCheckId, const ComboModel* want)");
    const std::string giveDrive = SctFunctionBody(
        icon, "bool RunCheckQueueGive(uint16_t mmCheckId, const char* wantTexture, uint8_t wantItemId)");
    SCT_CHECK(!drawDrive.empty() && drawDrive.find("gComboCtx.sourceIsRando = false;") != std::string::npos &&
                  drawDrive.find("].obtained = true;") == std::string::npos && !giveDrive.empty() &&
                  giveDrive.find("gComboCtx.sourceIsRando = false;") != std::string::npos &&
                  giveDrive.find("].obtained = true;") == std::string::npos,
              "(h) a CheckQueue drive pre-sets `obtained` (the delivered-host consume would take its host) or does "
              "not suspend the pairing (#1600)\n");
#endif
}

} // namespace

TestResult SongCheckTables_Run(void) {
    printf("[TEST] song-check-tables: both games' song checks are cross-game hosts exactly under Song Locations in "
           "a paired world, the shared song pool's groups and masks follow one predicate in both engines, Impa "
           "refuses under Skip Child Zelda, and with the switch off every host answer is the base commit's (#1542 "
           "S0, S3)\n");
    sSctFailures = 0;
    std::memset(sSctHost, 0, sizeof(sSctHost));
    const int contextBefore = OoT_ComboLogic_TestRandoContextExists();
    const int16_t captureBefore = Combo_PairSongsSnapshot();
    const bool sourceBefore = gComboCtx.sourceIsRando;
    const uint32_t hashBefore = gComboCtx.sharedRandoSettingsHash;
    const uint8_t formatBefore = gComboCtx.comboSettings.formatVersion;
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
        SctCheckEnginesArmed(1);
        SctCheckEnginesArmed(-1);
        SctCheckMMNeedsPairing();
        SctCheckNonSongUnmoved();
        SctCheckBaseTables(-1);
        SctCheckBaseTables(0);
        SctCheckPoolGroups();
        SctCheckAnchors();
    }
    // (i) the guard restored both hooks and dropped the context it held.
    SCT_CHECK(Combo_SongHostsTestOverride() == -1, "(i) the override reads %d after the row, not cleared\n",
              Combo_SongHostsTestOverride());
    SCT_CHECK(Combo_PairSongsSnapshot() == captureBefore, "(i) the creation capture is %d after the row, was %d\n",
              (int)Combo_PairSongsSnapshot(), (int)captureBefore);
    SCT_CHECK(contextBefore != 0 || OoT_ComboLogic_TestRandoContextExists() == 0,
              "(i) the row left a Rando context behind that was not there before it\n");
    SCT_CHECK(gComboCtx.sourceIsRando == sourceBefore && gComboCtx.sharedRandoSettingsHash == hashBefore &&
                  gComboCtx.comboSettings.formatVersion == formatBefore,
              "(i) the row left the pairing fields moved\n");
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
