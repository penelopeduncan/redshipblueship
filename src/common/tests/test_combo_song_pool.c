/**
 * @file test_combo_song_pool.c
 * @brief THE PAIR'S ONE SONGS SETTING (#1542 D2; OoTMM `songs`, one setting for
 *        both games): combo_song_pool.h's mapping and its creation capture.
 *
 * ============================================================================
 * pair-songs-mapping (CTest PairSongsMapping, redship tier, generates nothing)
 * ============================================================================
 *
 *  (1) THE MAPPING, spelled here independently of combo_song_pool.cpp: OoT Off,
 *      Song Locations, Dungeon Rewards, Anywhere give MM Vanilla, Song Locations,
 *      Song Locations, Anywhere, and a value outside OoT's four gives OoT's
 *      default's (Song Locations). Red on any drift of the table.
 *  (2) THE CAPTURE. None at first; a capture is held and read back; the capture
 *      wins over OoT's key value (Combo_PairSongsOoTValue: red if the completion
 *      could follow a key changed after the freeze); a clear drops it; the
 *      snapshot restores "none" and a held value exactly (Playthrough_Init's
 *      rollback); a value above 255 is held as 255 and maps as the default.
 *  (3) THE INVALIDATION. Context_InvalidateSessionState keeps a capture under
 *      RSBS_SEED_STAMP_KEEP (a test process completes MM's half after the
 *      file-create invalidation: red if it is dropped there) and clears it under
 *      DROP (red if a dead session's capture survives to the next creation).
 *  (4) THE CALL SITES, from source (RSBS_SOURCE_DIR), each searched INSIDE the
 *      function it claims: Playthrough_Init captures OoT's finalized option after
 *      the hidden-rules gate and before the MM profile stamp, and its rollback
 *      restores the snapshot; OoT_PairedGen_CompleteWorld clears it when no world
 *      waits, refuses before MM's half when MM's resolution disagrees with OoT's
 *      frozen option, and clears it right after MM's half; it checks the half MM
 *      authored against OoT's frozen option while MM's bytes are still in view
 *      and refuses after OoT's are put back; RetractPairedWorld and the
 *      package's ApplyComboWorld clear it; MM's paired resolution sets
 *      RO_SHUFFLE_SONGS inside its paired branch, before the authored-logic early
 *      return, from the capture-aware helper and never from MM's key; the load
 *      restore skips the row; and both frozen rows (Shuffle Songs, Logic) search
 *      through the per-digest memo, the songs row after the resident half.
 *
 * Linkage note: #included into test_runner.cpp at FILE SCOPE (compiled as C++),
 * like its neighbours; every symbol under test is C-linkage.
 */

#include "../combo_song_pool.h"
#include "../context.h"
#include "../test_runner.h"

#include <cstdio>
#include <cstring>
#include <fstream>
#include <initializer_list>
#include <iterator>
#include <string>

namespace {

int sCspFailures = 0;

#define CSP_CHECK(cond, ...)                                       \
    do {                                                           \
        if (!(cond)) {                                             \
            printf("[TEST] FAIL (pair-songs-mapping): ");          \
            printf(__VA_ARGS__);                                   \
            printf("  [%s] (%s:%d)\n", #cond, __FILE__, __LINE__); \
            sCspFailures++;                                        \
        }                                                          \
    } while (0)

std::string CspReadSource(const char* rel) {
#ifdef RSBS_SOURCE_DIR
    std::ifstream in(std::string(RSBS_SOURCE_DIR) + "/" + rel, std::ios::binary);
    if (!in.good()) {
        return std::string();
    }
    std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    // Line endings as LF, so a needle spanning lines reads the same on Windows.
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
std::string CspFunctionBody(const std::string& src, const char* signature) {
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

/** Every needle in order inside @p hay (false for an empty @p hay). */
bool CspInOrder(const std::string& hay, std::initializer_list<const char*> needles) {
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

void CspCheckMapping() {
    // OoT's values (randomizerTypes.h RO_SONG_SHUFFLE_*) and MM's (Rando/Types.h),
    // as integers: Off 0, Song Locations 1, Dungeon Rewards 2, Anywhere 3; MM
    // Anywhere 0, Song Locations 1, Vanilla 2.
    static const uint32_t kWant[4] = { 2u, 1u, 1u, 0u };
    for (uint32_t oot = 0; oot < 4; ++oot) {
        CSP_CHECK(Combo_PairSongsMmValue(oot) == kWant[oot], "(1) OoT Shuffle Songs %u maps to MM %u, want %u\n",
                  (unsigned)oot, (unsigned)Combo_PairSongsMmValue(oot), (unsigned)kWant[oot]);
    }
    CSP_CHECK(Combo_PairSongsMmValue(4u) == 1u && Combo_PairSongsMmValue(255u) == 1u,
              "(1) a value outside OoT's four must map as OoT's default (Song Locations)\n");
    CSP_CHECK(RSBS_OOT_SHUFFLE_SONGS_DEFAULT == 1 &&
                  strcmp(RSBS_OOT_SHUFFLE_SONGS_CVAR, "gRandoSettings.ShuffleSongs") == 0,
              "(1) OoT's Shuffle Songs key or default is not SoH's (settings.cpp RSK_SHUFFLE_SONGS)\n");
    printf("[TEST] (1) the mapping is OoTMM's one setting over OoT's four values\n");
}

void CspCheckCapture() {
    uint32_t out = 0xAAu;
    Combo_PairSongsClear();
    CSP_CHECK(!Combo_PairSongsCaptured(&out) && out == 0xAAu, "(2) a cleared capture reads as held, or wrote out\n");
    CSP_CHECK(Combo_PairSongsOoTValue(2u) == 2u, "(2) with no capture OoT's key value must be the answer\n");
    CSP_CHECK(Combo_PairSongsSnapshot() == -1, "(2) the snapshot of no capture must be -1\n");

    Combo_PairSongsCapture(3u);
    CSP_CHECK(Combo_PairSongsCaptured(&out) && out == 3u, "(2) a capture of Anywhere reads back as %u\n",
              (unsigned)out);
    CSP_CHECK(Combo_PairSongsCaptured(nullptr), "(2) a held capture must read as held with a NULL out\n");
    CSP_CHECK(Combo_PairSongsOoTValue(1u) == 3u,
              "(2) the capture must win over OoT's key value (the freeze, not a key changed since)\n");
    const int16_t held = Combo_PairSongsSnapshot();

    Combo_PairSongsClear();
    CSP_CHECK(!Combo_PairSongsCaptured(nullptr), "(2) a clear did not drop the capture\n");
    Combo_PairSongsRestore(held);
    CSP_CHECK(Combo_PairSongsCaptured(&out) && out == 3u, "(2) the rollback did not restore the held capture\n");
    Combo_PairSongsRestore(-1);
    CSP_CHECK(!Combo_PairSongsCaptured(nullptr), "(2) the rollback did not restore \"no capture\"\n");

    Combo_PairSongsCapture(1000u);
    CSP_CHECK(Combo_PairSongsCaptured(&out) && out == 255u && Combo_PairSongsMmValue(out) == 1u,
              "(2) an out-of-range capture must be held as 255 and map as the default\n");
    Combo_PairSongsClear();
    printf("[TEST] (2) the capture is held, wins over the key, clears and rolls back\n");
}

void CspCheckInvalidation() {
    ComboContext_Init();
    Combo_PairSongsCapture(0u);
    Context_InvalidateSessionState(RSBS_SEED_STAMP_KEEP);
    uint32_t out = 0;
    CSP_CHECK(Combo_PairSongsCaptured(&out) && out == 0u,
              "(3) the file-create (KEEP) invalidation dropped the songs capture: a test process's completion would "
              "then read OoT's live key\n");
    Context_InvalidateSessionState(RSBS_SEED_STAMP_DROP);
    CSP_CHECK(!Combo_PairSongsCaptured(nullptr),
              "(3) a DROP invalidation kept the songs capture: a dead session's value would reach the next world\n");
    ComboContext_Init();
    printf("[TEST] (3) KEEP keeps the capture and DROP clears it\n");
}

void CspCheckAnchors() {
    const std::string play = CspReadSource("games/oot/soh/Enhancements/randomizer/3drando/playthrough.cpp");
    const std::string creation = CspReadSource("games/oot/soh/Enhancements/randomizer/ForeignItemsSingleExe.cpp");
    const std::string package = CspReadSource("games/oot/soh/Enhancements/randomizer/SeedPackageSingleExe.cpp");
    const std::string foreign = CspReadSource("games/mm/2s2h/Rando/Foreign.cpp");
    if (play.empty() || creation.empty() || package.empty() || foreign.empty()) {
        CSP_CHECK(false, "(4) a source file under test could not be read (RSBS_SOURCE_DIR)\n");
        return;
    }
    const std::string init = CspFunctionBody(play, "std::set<RandomizerTrick> enabledTricks)");
    CSP_CHECK(CspInOrder(init, { "Combo_ComboItemRules2Refused(",
                                 "Combo_PairSongsCapture((uint32_t)ctx->GetOption(RSK_SHUFFLE_SONGS).Get());",
                                 "gComboCtx.mmProfileDigest = MM_Rando_ComputeProfileStamp();" }),
              "(4) Playthrough_Init must capture OoT's Shuffle Songs after the hidden-rules gate and before the MM "
              "profile stamp\n");
    CSP_CHECK(
        CspInOrder(init, { "const int16_t rsbsPriorSongsCapture = Combo_PairSongsSnapshot();",
                           "auto rsbsRollbackFreeze = [&]() {", "Combo_PairSongsRestore(rsbsPriorSongsCapture);" }),
        "(4) Playthrough_Init's rollback must restore the songs capture it snapshotted\n");

    const std::string complete = CspFunctionBody(creation, "extern \"C\" int OoT_PairedGen_CompleteWorld(void)");
    CSP_CHECK(CspInOrder(complete, { "Combo_PairSongsClear();", "return RSBS_SEEDPKG_ERR_NO_WORLD;",
                                     "if (!OoTPairSongsAgree()) {", "RetractPairedWorld();",
                                     "MM_Rando_AuthorHalfAtCreation(", "Combo_PairSongsClear();" }),
              "(4) OoT_PairedGen_CompleteWorld must clear the capture when no world waits, refuse before MM's half "
              "on a songs disagreement, and clear the capture right after MM's half\n");
    CSP_CHECK(CspInOrder(complete, { "MM_Rando_AuthorHalfAtCreation(",
                                     "const bool songsAuthoredAgree = mmRc != 0 || OoTPairHalfSongsAgree();",
                                     "Combo_PairSongsClear();", "memcpy(&gSaveContext, sOoTSaveSnapshot,",
                                     "if (!songsAuthoredAgree) {", "RetractPairedWorld();",
                                     "return RSBS_SEEDPKG_ERR_GENERATION;" }),
              "(4) OoT_PairedGen_CompleteWorld must check the authored half's Shuffle Songs while MM's bytes are in "
              "view, and refuse after OoT's are put back\n");
    CSP_CHECK(CspInOrder(CspFunctionBody(creation, "static bool OoTPairHalfSongsAgree(void)"),
                         { "GetOption(RSK_SHUFFLE_SONGS).Get()", "Combo_PairSongsMmValue(",
                           "MM_Rando_AuthoredHalfSongs()", "return false;" }),
              "(4) the authored-half lock must compare the half's option with the mapping of OoT's frozen option\n");
    CSP_CHECK(CspInOrder(CspFunctionBody(foreign, "extern \"C\" int MM_Rando_AuthoredHalfSongs(void)"),
                         { "RANDO_SAVE_OPTIONS[RO_SHUFFLE_SONGS]" }),
              "(4) MM_Rando_AuthoredHalfSongs must read the live half's frozen Shuffle Songs\n");
    CSP_CHECK(CspInOrder(CspFunctionBody(foreign, "extern \"C\" int MM_Rando_PairedSongsShown(void)"),
                         { "Combo_MMProfileFrozen()", "ResidentPairHalfSongs(&songs)", "MemoFrozenStampCandidate(",
                           "ResolvedPairedSongs()" }),
              "(4) the frozen songs row must read the resident half, then the memoised search, then the fallback\n");
    CSP_CHECK(CspInOrder(CspFunctionBody(foreign, "extern \"C\" int MM_Rando_PairedLogicShown(void)"),
                         { "MemoFrozenStampCandidate(" }),
              "(4) the frozen Logic row must search through the per-digest memo\n");
    const std::string agree = CspFunctionBody(creation, "static bool OoTPairSongsAgree(void)");
    CSP_CHECK(CspInOrder(agree, { "GetOption(RSK_SHUFFLE_SONGS).Get()", "Combo_PairSongsMmValue(",
                                  "MM_Rando_PairedSongsResolved()", "return false;" }),
              "(4) the completion's lock must compare MM's resolution with the mapping of OoT's frozen option\n");
    CSP_CHECK(
        CspInOrder(CspFunctionBody(creation, "static void RetractPairedWorld(void)"), { "Combo_PairSongsClear();" }),
        "(4) RetractPairedWorld must clear the songs capture\n");
    CSP_CHECK(CspInOrder(CspFunctionBody(package, "void ApplyComboWorld(const ComboContext& rec)"),
                         { "Combo_PairSongsClear();" }),
              "(4) applying a seed package's world must clear the songs capture\n");

    const std::string resolve = CspFunctionBody(foreign, "PairedLogicSource logic = PairedLogicSource::ComboRung)");
    CSP_CHECK(CspInOrder(resolve, { "if (!paired) {", "values[RO_SHUFFLE_SONGS] = ResolvedPairedSongs();",
                                    "if (logic == PairedLogicSource::Authored) {" }),
              "(4) MM's paired resolution must set Shuffle Songs in its paired branch, before the authored-logic "
              "early return\n");
    const std::string helper = CspFunctionBody(foreign, "static uint32_t ResolvedPairedSongs()");
    CSP_CHECK(
        CspInOrder(helper, { "RSBS_OOT_SHUFFLE_SONGS_CVAR", "Combo_PairSongsOoTValue(", "Combo_PairSongsMmValue(" }) &&
            helper.find("RO_SHUFFLE_SONGS") == std::string::npos,
        "(4) the paired songs helper must read the capture, then OoT's key, and never MM's own row\n");
    const std::string restore = CspFunctionBody(foreign, "size_t namesLen, int* outCount)");
    CSP_CHECK(CspInOrder(restore, { "if (randoOptionId == RO_SHUFFLE_SONGS) {", "continue;", "CVarSetInteger(" }),
              "(4) the load restore must skip MM's Shuffle Songs before it writes any option key\n");
    printf("[TEST] (4) the capture, the lock, the clears, the paired resolution and the restore skip are where they "
           "claim\n");
}

} // namespace

TestResult PairSongsMapping_Run(void) {
    printf("[TEST] pair-songs-mapping: Majora's Mask's Shuffle Songs follows Ocarina of Time's through one mapping, "
           "captured at the freeze and kept until MM's half is made (#1542)\n");
    sCspFailures = 0;
    const int16_t prior = Combo_PairSongsSnapshot();
    CspCheckMapping();
    CspCheckCapture();
    CspCheckInvalidation();
    CspCheckAnchors();
    Combo_PairSongsRestore(prior);
    printf("[TEST] pair-songs-mapping: %s\n", sCspFailures == 0 ? "PASS" : "FAIL");
    return sCspFailures == 0 ? TEST_PASS : TEST_FAIL;
}
