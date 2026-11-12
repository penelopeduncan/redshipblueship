/**
 * @file test_song_pool_union.c
 * @brief THE SHARED SONG POOL (#1542 S3, rulings D1 and D5; OoTMM `songLocations`)
 *        over real paired generations.
 *
 * ============================================================================
 * song-pool-union-world (CTest SongPoolUnionWorld, rando label, generates)
 * ============================================================================
 *
 * The shipped profile (OoT Shuffle Songs: Song Locations, so MM follows) over two
 * seeds, then one seed under No Logic and one under Beatable Only. Each world is
 * generated and FINISHED the way a creation finishes it (MM's half, whose OnSaveInit
 * chain runs the single bag, then OoT's crossings and remainder, then the commit;
 * oot_butterfly_fairy_test.cpp's shape). Asserts, per world:
 *  (1) the union is armed (OoT_ComboLogic_SongPoolUnion) and the fill ran with the
 *      exclusive song group (red if a producer or the bag stopped reading it);
 *  (2) every song check of both games holds a song of either game (its own, or a
 *      crossing in the crossing store) or an item of the junk set, never a heart
 *      piece, a map, a trap or progression (red on a non-song placed on a song
 *      check, OoTMM fillJunk's rule, or a leftover filled by either remainder's
 *      ordinary deal);
 *  (3) the 22 songs of the profile (OoT's 12, MM's 10) are all on song checks, so
 *      exactly one song check is left to junk (red on a song placed off the song
 *      checks, or a song dropped or doubled);
 *  (4) no MM song check is `skipped` or left unshuffled, so the starting Song of
 *      Healing is a host (red if GeneratePools' surplus step runs under the pool);
 *  (5) Link's Pocket holds no song (red if the pocket draws a song the bag places);
 *  (6) the leftover junk came from the pools (no fresh pad in either game) and the
 *      two remainders' counts equal the song checks found holding junk (red if a
 *      remainder's junk step is skipped and the ordinary deal fills the leftover);
 *  (7) under the proving rungs the goal is proven; under No Logic the proof is
 *      skipped.
 *
 * ============================================================================
 * song-pool-anywhere-unconfined (CTest SongPoolAnywhereUnconfined, rando, generates)
 * ============================================================================
 *
 *  (i) OoT Shuffle Songs Anywhere (MM Anywhere), with the song-host override
 *      cleared and again set to 1: the union is off, the bag's exclusive group is
 *      0, no MM pool row carries a combo-wide bit, and no song check of either game
 *      answers a host group (red if the override, which arms host acceptance in any
 *      mode, arms the shared song pool outside Song Locations, or if a producer
 *      reads the override directly);
 *  (ii) a Song Locations world with the override cleared against the same seed
 *      with the override 0: the union and the exclusive group differ, and so do
 *      the placements (the positive control: the switch moves something, so (i)'s
 *      zeros are not vacuous).
 *
 * Linkage note: #included into test_runner.cpp at FILE SCOPE (compiled as C++),
 * like its neighbours. Helpers carry the Spu prefix.
 */

#include "../combo_logic.h"
#include "../combo_settings_view.h"
#include "../combo_single_bag.h"
#include "../combo_song_pool.h"
#include "../context.h"
#include "../crossing_store.h"
#include "../foreign_items.h"
#include "../test_runner.h"

#include <cstdio>
#include <cstring>
#include <vector>

#include <libultraship/bridge/consolevariablebridge.h>

extern "C" {
int Rando_HeadlessSeedTest(const char* seedStr);
int MM_Rando_HeadlessPairedHalf(void);
int OoT_Creation_FinishPairedHalf(int writeSpoiler);
int OoT_ComboLogic_TestSongCheckIndex(uint16_t rc);
int OoT_ComboLogic_TestCheckIdSpace(void);
int OoT_ComboLogic_TestIsSongItem(uint16_t id);
int OoT_ComboLogic_TestLastSongChecksJunked(int* outPadded);
int OoT_ComboLogic_TestEnsureItemTableTransient(void);
void OoT_ComboLogic_TestSongRowMasks(int expectUnion, int* outSongs, int* outWide, int* outWrong);
void OoT_ComboLogic_TestSongCheckContents(int* outSongs, int* outCrossedSongs, int* outJunk, int* outOther,
                                          int* outPocketSong);
int MM_Rando_Foreign_TestSongCheckIndex(uint16_t randoCheckId);
int MM_ComboLogic_TestLastSongChecksJunked(int* outPadded);
int MM_ComboLogic_TestLastPoolConfinement(uint16_t* outItems, uint32_t* outMasks, int cap);
void MM_ComboLogic_TestSongCheckContents(int* outSongs, int* outCrossedSongs, int* outJunk, int* outOther,
                                         int* outSkipped, int* outNotShuffled);
void MM_ComboLogic_TestSongGroupCensus(int* outSongItems, int* outWideItems, int* outMixed, int* outWideSongChecks,
                                       int* outWideOtherChecks);
}

namespace {

int sSpuFailures = 0;

#define SPU_CHECK(cond, ...)                                       \
    do {                                                           \
        if (!(cond)) {                                             \
            printf("[TEST] FAIL (song-pool): ");                   \
            printf(__VA_ARGS__);                                   \
            printf("  [%s] (%s:%d)\n", #cond, __FILE__, __LINE__); \
            sSpuFailures++;                                        \
        }                                                          \
    } while (0)

/** The keys a leg sets, cleared on every return path. */
struct SpuKeys {
    ~SpuKeys() {
        CVarClear(RSBS_OOT_SHUFFLE_SONGS_CVAR);
        CVarClear(Combo_ComboSettingKey(COMBO_SETTING_LOGIC));
        Combo_SongHostsForceForTest(-1);
        Combo_SingleBag_Forget();
        ComboContext_Init();
    }
};

/** What one finished world did with its songs. */
struct SpuWorld {
    bool generated;
    bool unionArmed;
    ComboSingleBagReport bag;
    uint32_t placementDigest;
    int ootSongs, ootCrossed, ootJunk, ootOther, pocketSong;
    int mmSongs, mmCrossed, mmJunk, mmOther, mmSkipped, mmNotShuffled;
    int ootJunked, ootPadded, mmJunked, mmPadded;
    int mmWideRows;
};

/**
 * Generate @p seed with OoT Shuffle Songs @p songs (-1: the default) under the
 * [Both Games] Logic @p rung (-1: the default) and the song-host override
 * @p forced, then finish the pair. @p finish false stops after the bag (the
 * coordinator tables are read and committed) without OoT's remainder.
 */
SpuWorld SpuGenerate(const char* seed, int songs, int rung, int forced, bool finish) {
    SpuWorld w;
    std::memset(&w, 0, sizeof(w));
    Combo_SingleBag_Forget();
    ComboContext_Init();
    CVarClear(RSBS_OOT_SHUFFLE_SONGS_CVAR);
    CVarClear(Combo_ComboSettingKey(COMBO_SETTING_LOGIC));
    if (songs >= 0) {
        CVarSetInteger(RSBS_OOT_SHUFFLE_SONGS_CVAR, songs);
    }
    if (rung >= 0) {
        CVarSetInteger(Combo_ComboSettingKey(COMBO_SETTING_LOGIC), rung);
    }
    Combo_SongHostsForceForTest(forced);
    const int gen = Rando_HeadlessSeedTest(seed);
    w.unionArmed = gen == 0 && OoT_ComboLogic_SongPoolUnion();
    const int mmHalf = gen == 0 ? MM_Rando_HeadlessPairedHalf() : -1;
    w.bag = *Combo_SingleBag_LastReport();
    w.placementDigest = w.bag.fill.placementDigest;
    w.generated = gen == 0 && mmHalf == 0 && w.bag.status == RSBS_COMBO_LOGIC_OK;
    {
        const int total = MM_ComboLogic_TestLastPoolConfinement(nullptr, nullptr, 0);
        std::vector<uint32_t> masks((size_t)(total > 0 ? total : 1), 0u);
        MM_ComboLogic_TestLastPoolConfinement(nullptr, masks.data(), total);
        for (int i = 0; i < total; ++i) {
            w.mmWideRows += (masks[(size_t)i] & RSBS_COMBO_GROUP_WIDE_MASK) != 0u ? 1 : 0;
        }
    }
    if (w.generated && finish) {
        const int crossings = OoT_Creation_FinishPairedHalf(0);
        w.generated = crossings >= 0;
        OoT_ComboLogic_TestSongCheckContents(&w.ootSongs, &w.ootCrossed, &w.ootJunk, &w.ootOther, &w.pocketSong);
        MM_ComboLogic_TestSongCheckContents(&w.mmSongs, &w.mmCrossed, &w.mmJunk, &w.mmOther, &w.mmSkipped,
                                            &w.mmNotShuffled);
        w.ootJunked = OoT_ComboLogic_TestLastSongChecksJunked(&w.ootPadded);
        w.mmJunked = MM_ComboLogic_TestLastSongChecksJunked(&w.mmPadded);
    }
    Combo_SingleBag_Forget();
    Combo_SongHostsForceForTest(-1);
    printf("[TEST] song-pool: %s (songs %d, rung %d, override %d): generated %d, union %d, bag %s, exclusive "
           "0x%08X, goal proven %d / skipped %d, digest %08X, %d MM wide rows\n",
           seed, songs, rung, forced, w.generated ? 1 : 0, w.unionArmed ? 1 : 0, Combo_Logic_StatusName(w.bag.status),
           (unsigned)w.bag.exclusiveGroups, w.bag.fill.goalProven ? 1 : 0, w.bag.fill.proofSkipped ? 1 : 0,
           (unsigned)w.placementDigest, w.mmWideRows);
    if (finish) {
        printf("[TEST] song-pool: %s: OoT song checks %d songs / %d crossed MM songs / %d junk / %d other, pocket "
               "song %d; MM song checks %d songs / %d crossed OoT songs / %d junk / %d other, %d skipped, %d "
               "unshuffled; junked OoT %d (padded %d), MM %d (padded %d)\n",
               seed, w.ootSongs, w.ootCrossed, w.ootJunk, w.ootOther, w.pocketSong, w.mmSongs, w.mmCrossed, w.mmJunk,
               w.mmOther, w.mmSkipped, w.mmNotShuffled, w.ootJunked, w.ootPadded, w.mmJunked, w.mmPadded);
    }
    return w;
}

} // namespace

TestResult SongPoolUnionWorld_Run(void) {
    printf("[TEST] song-pool-union-world: under Song Locations the song checks of both games are one pool: every "
           "song of either game is on one, a leftover holds junk, Healing is a host and Link's Pocket holds no song "
           "(#1542 S3, rulings D1 and D5)\n");
    sSpuFailures = 0;
    SpuKeys keys;
    struct Leg {
        const char* seed;
        int rung;
    };
    const Leg legs[] = {
        { "RSBSSONGUNION1", -1 },
        { "RSBSSONGUNION2", -1 },
        { "RSBSSONGUNIONNL", (int)RSBS_COMBO_RUNG_NONE },
        { "RSBSSONGUNIONBO", (int)RSBS_COMBO_RUNG_BEATABLE },
    };
    for (const Leg& leg : legs) {
        const SpuWorld w = SpuGenerate(leg.seed, -1, leg.rung, -1, true);
        SPU_CHECK(w.generated, "%s: the paired world did not finish\n", leg.seed);
        if (!w.generated) {
            continue;
        }
        SPU_CHECK(w.unionArmed && w.bag.exclusiveGroups == RSBS_COMBO_GROUP_SONG_LOCATIONS,
                  "%s: (1) the shipped profile did not arm the shared song pool (union %d, exclusive 0x%08X)\n",
                  leg.seed, w.unionArmed ? 1 : 0, (unsigned)w.bag.exclusiveGroups);
        SPU_CHECK(w.ootOther == 0 && w.mmOther == 0,
                  "%s: (2) %d OoT and %d MM song checks hold neither a song nor junk\n", leg.seed, w.ootOther,
                  w.mmOther);
        const int songsOnSongChecks = w.ootSongs + w.ootCrossed + w.mmSongs + w.mmCrossed;
        const int junk = w.ootJunk + w.mmJunk;
        SPU_CHECK(songsOnSongChecks == 22 && junk == 1,
                  "%s: (3) %d songs on the 23 song checks and %d junk, want OoTMM's 22 and 1\n", leg.seed,
                  songsOnSongChecks, junk);
        SPU_CHECK(w.mmSkipped == 0 && w.mmNotShuffled == 0,
                  "%s: (4) %d MM song checks skipped and %d unshuffled: the surplus step ran under the pool\n",
                  leg.seed, w.mmSkipped, w.mmNotShuffled);
        SPU_CHECK(w.pocketSong == 0, "%s: (5) Link's Pocket holds a song the bag places\n", leg.seed);
        SPU_CHECK(w.ootJunked + w.mmJunked == junk && w.ootPadded == 0 && w.mmPadded == 0,
                  "%s: (6) the remainders junked %d + %d song checks (padded %d + %d), the checks hold %d junk\n",
                  leg.seed, w.ootJunked, w.mmJunked, w.ootPadded, w.mmPadded, junk);
        if (leg.rung == (int)RSBS_COMBO_RUNG_NONE) {
            SPU_CHECK(w.bag.fill.proofSkipped, "%s: (7) No Logic ran a proof\n", leg.seed);
        } else {
            SPU_CHECK(w.bag.fill.goalProven, "%s: (7) the goal was not proven\n", leg.seed);
        }
    }
    printf("[TEST] song-pool-union-world: %s\n", sSpuFailures == 0 ? "PASS" : "FAIL");
    return sSpuFailures == 0 ? TEST_PASS : TEST_FAIL;
}

TestResult SongPoolAnywhereUnconfined_Run(void) {
    printf("[TEST] song-pool-anywhere-unconfined: under Shuffle Songs Anywhere nothing of the shared song pool runs, "
           "even with the song-host override on; at Song Locations the switch moves the world (#1542 S3)\n");
    sSpuFailures = 0;
    SpuKeys keys;
    SPU_CHECK(OoT_ComboLogic_TestEnsureItemTableTransient() == 0, "OoT's item table is not available\n");

    // (i) Anywhere, override cleared and 1.
    for (int forced = -1; forced <= 1; forced += 2) {
        const SpuWorld w = SpuGenerate("RSBSSONGANYWHERE", (int)RSBS_OOT_SONGS_ANYWHERE, -1, forced, false);
        SPU_CHECK(w.generated, "(i) override %d: the Anywhere world did not generate\n", forced);
        // confinedRows: the shipped profile confines no MM row locally (the keys,
        // boss keys and fairies GoldenSeedDigestMMConfined places in their dungeons are
        // not the defaults), so under Anywhere no bag row carries a mask at all.
        SPU_CHECK(!w.unionArmed && w.bag.exclusiveGroups == 0u && w.mmWideRows == 0 && w.bag.confinedRows == 0,
                  "(i) override %d: under Anywhere the union read %d, the exclusive group 0x%08X, %d MM rows wide, "
                  "%d bag rows confined\n",
                  forced, w.unionArmed ? 1 : 0, (unsigned)w.bag.exclusiveGroups, w.mmWideRows, w.bag.confinedRows);
        // The engines' answers in that world, still held.
        Combo_SongHostsForceForTest(forced);
        int ootSongs = 0;
        int ootWide = 0;
        int ootWrong = 0;
        OoT_ComboLogic_TestSongRowMasks(0, &ootSongs, &ootWide, &ootWrong);
        int mmSongItems = 0;
        int mmWideItems = 0;
        int mmMixed = 0;
        int mmWideSong = 0;
        int mmWideOther = 0;
        MM_ComboLogic_TestSongGroupCensus(&mmSongItems, &mmWideItems, &mmMixed, &mmWideSong, &mmWideOther);
        const ComboLogicEngine* oot = Combo_Logic_GetEngine(GAME_OOT);
        int ootGrouped = 0;
        for (int rc = 1; oot != NULL && oot->hostGroups != NULL && rc < OoT_ComboLogic_TestCheckIdSpace(); rc++) {
            if (OoT_ComboLogic_TestSongCheckIndex((uint16_t)rc) >= 0) {
                ootGrouped += oot->hostGroups(oot->self, (uint16_t)rc) != 0u ? 1 : 0;
            }
        }
        Combo_SongHostsForceForTest(-1);
        SPU_CHECK(ootWide == 0 && ootWrong == 0 && mmWideItems == 0 && mmWideSong == 0 && mmWideOther == 0 &&
                      ootGrouped == 0,
                  "(i) override %d: under Anywhere OoT gave %d rows the song group (%d wrong), MM %d rows and %d song "
                  "checks, OoT %d song checks a group\n",
                  forced, ootWide, ootWrong, mmWideItems, mmWideSong, ootGrouped);
    }

    // (ii) the positive control at Song Locations.
    const SpuWorld on = SpuGenerate("RSBSSONGCONTROL", (int)RSBS_OOT_SONGS_SONG_LOCATIONS, -1, -1, false);
    const SpuWorld off = SpuGenerate("RSBSSONGCONTROL", (int)RSBS_OOT_SONGS_SONG_LOCATIONS, -1, 0, false);
    SPU_CHECK(on.generated && off.generated, "(ii) a Song Locations control world did not generate\n");
    SPU_CHECK(on.unionArmed && !off.unionArmed && on.bag.exclusiveGroups != off.bag.exclusiveGroups &&
                  on.placementDigest != off.placementDigest,
              "(ii) the override 0 did not move the Song Locations world (union %d / %d, exclusive 0x%08X / 0x%08X, "
              "digest %08X / %08X)\n",
              on.unionArmed ? 1 : 0, off.unionArmed ? 1 : 0, (unsigned)on.bag.exclusiveGroups,
              (unsigned)off.bag.exclusiveGroups, (unsigned)on.placementDigest, (unsigned)off.placementDigest);
    printf("[TEST] song-pool-anywhere-unconfined: %s\n", sSpuFailures == 0 ? "PASS" : "FAIL");
    return sSpuFailures == 0 ? TEST_PASS : TEST_FAIL;
}
