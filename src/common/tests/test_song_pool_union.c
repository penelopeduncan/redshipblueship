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

#include "../combo_hint_view.h"
#include "../combo_logic.h"
#include "../combo_settings_view.h"
#include "../combo_single_bag.h"
#include "../combo_song_pool.h"
#include "../context.h"
#include "../critical_renewables.h"
#include "../crossing_store.h"
#include "../foreign_items.h"
#include "../test_runner.h"
#include "test_named_items.h"

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
int MM_ComboLogic_TestShuffledItems(uint16_t* outItems, uint16_t* outChecks, int cap);
void MM_ComboLogic_TestSongCheckContents(int* outSongs, int* outCrossedSongs, int* outJunk, int* outOther,
                                         int* outSkipped, int* outNotShuffled);
void MM_ComboLogic_TestSongGroupCensus(int* outSongItems, int* outWideItems, int* outMixed, int* outWideSongChecks,
                                       int* outWideOtherChecks);
int OoT_SharedSongsTest_WorldCopies(int rg);
int MM_PlacementConfinement_WorldCopies(uint16_t id);
int OoT_SharedWarpSongsTest_LinksPocket(void);
int OoT_Fill_TestSongHeldForBag(uint16_t rg);
int OoT_SharedWarpSongsTest_UnsharedSongId(void);
void MM_IntSongHost_PinIds(uint16_t* healingCheck, uint16_t* sonataItem);
int OoT_Combo_SetHintingSuspendedWorld(int on);
int Rando_TestCheckLoadedCrossingHostHints(int* outProbes, int* outStones);
int MM_HintSources_TestLegacyFixedText(uint16_t source, char* out, int cap);
int MM_HintArea_TestItemLocationText(uint16_t randoItemId, const char* tmpl, const char* key, char* out, int cap);
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
        CVarClear(Combo_ComboSettingKey(COMBO_SETTING_DIRECTION));
        CVarClear("gRandoSettings.LinksPocket");
        CVarClear("gRando.Options.RO_SHUFFLE_SHOPS");
        CVarClear("gRando.ExcludedChecks");
        Combo_Logic_TestSetForcedPlacements(nullptr, 0);
        OoT_Combo_SetHintingSuspendedWorld(0);
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

/**
 * Census of all 22 distinct songs of the profile across both games: asserts every
 * OoT song (12) and every MM song (10) has exactly one placed copy total over
 * both games' checks and the crossing store. Returns 22 on success, or a negative
 * code on failure.
 */
int SpuCensusPlacedSongs(void) {
    int ootSongCount = 0;
    for (uint16_t id = 1; id < 5000; ++id) {
        if (OoT_ComboLogic_TestIsSongItem(id) == 0) {
            continue;
        }
        ootSongCount++;
        int copies = OoT_SharedSongsTest_WorldCopies((int)id);
        for (int i = 0; i < Combo_Crossings_Count(GAME_MM); ++i) {
            ComboCrossing c;
            if (Combo_Crossings_At(GAME_MM, i, &c) && c.item.originGame == (uint8_t)GAME_OOT && c.item.id == id) {
                copies++;
            }
        }
        if (copies != 1) {
            printf("[TEST]   OoT song %u has %d copies across both games\n", (unsigned)id, copies);
            return -1;
        }
    }
    if (ootSongCount != 12) {
        printf("[TEST]   found %d OoT songs in item table, want 12\n", ootSongCount);
        return -2;
    }

    int mmSongCount = 0;
    for (uint16_t id = 1; id < 5000; ++id) {
        if (!MM_ComboLogic_IsPairSongItem(id)) {
            continue;
        }
        const int mmCopies = MM_PlacementConfinement_WorldCopies(id);
        int ootCrossCopies = 0;
        for (int i = 0; i < Combo_Crossings_Count(GAME_OOT); ++i) {
            ComboCrossing c;
            if (Combo_Crossings_At(GAME_OOT, i, &c) && c.item.originGame == (uint8_t)GAME_MM && c.item.id == id) {
                ootCrossCopies++;
            }
        }
        const int total = mmCopies + ootCrossCopies;
        if (total > 0) {
            mmSongCount++;
            if (total != 1) {
                printf("[TEST]   MM song %u has %d copies across both games (%d MM, %d OoT crossing)\n", (unsigned)id,
                       total, mmCopies, ootCrossCopies);
                return -3;
            }
        }
    }
    if (mmSongCount != 10) {
        printf("[TEST]   found %d placed MM songs, want 10\n", mmSongCount);
        return -4;
    }
    return 22;
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

TestResult SongPoolCrossesBothWays_Run(void) {
    printf("[TEST] song-pool-crosses-both-ways: under Song Locations crossings occur in both directions across seeds, "
           "and every song is placed exactly once (#1549 owed from #1546)\n");
    sSpuFailures = 0;
    SpuKeys keys;
    const char* seeds[] = {
        "RSBSSONGPOOLCBW1",
        "RSBSSONGPOOLCBW2",
        "RSBSSONGPOOLCBW3",
    };
    int totalCrossedOoT = 0;
    int totalCrossedMM = 0;
    for (const char* seed : seeds) {
        const SpuWorld w = SpuGenerate(seed, -1, -1, -1, true);
        SPU_CHECK(w.generated, "%s: the paired world did not finish\n", seed);
        if (!w.generated) {
            continue;
        }
        SPU_CHECK(w.unionArmed && w.bag.exclusiveGroups == RSBS_COMBO_GROUP_SONG_LOCATIONS,
                  "%s: the shared song pool was not armed (union %d, exclusive 0x%08X)\n", seed,
                  w.unionArmed ? 1 : 0, (unsigned)w.bag.exclusiveGroups);
        SPU_CHECK(w.bag.status == RSBS_COMBO_LOGIC_OK && w.bag.fill.goalProven,
                  "%s: bag status %s, goalProven %d\n", seed, Combo_Logic_StatusName(w.bag.status),
                  w.bag.fill.goalProven ? 1 : 0);
        SPU_CHECK(w.ootOther == 0 && w.mmOther == 0,
                  "%s: %d OoT and %d MM song checks hold neither a song nor junk\n", seed, w.ootOther, w.mmOther);
        const int songsOnSongChecks = w.ootSongs + w.ootCrossed + w.mmSongs + w.mmCrossed;
        const int junk = w.ootJunk + w.mmJunk;
        SPU_CHECK(songsOnSongChecks == 22 && junk == 1,
                  "%s: %d songs on 23 song checks and %d junk, want 22 and 1\n", seed, songsOnSongChecks, junk);
        SPU_CHECK(w.pocketSong == 0, "%s: Link's Pocket holds a song\n", seed);
        const int census = SpuCensusPlacedSongs();
        SPU_CHECK(census == 22, "%s: census of placed songs returned %d, want 22 distinct songs\n", seed, census);
        totalCrossedOoT += w.ootCrossed;
        totalCrossedMM += w.mmCrossed;
    }
    SPU_CHECK(totalCrossedOoT > 0, "no seed placed an MM song on an OoT song check (total %d)\n", totalCrossedOoT);
    SPU_CHECK(totalCrossedMM > 0, "no seed placed an OoT song on an MM song check (total %d)\n", totalCrossedMM);
    printf("[TEST] song-pool-crosses-both-ways: %d crossed into OoT, %d crossed into MM; %s\n",
           totalCrossedOoT, totalCrossedMM, sSpuFailures == 0 ? "PASS" : "FAIL");
    return sSpuFailures == 0 ? TEST_PASS : TEST_FAIL;
}

TestResult SongPoolPocketAdvancement_Run(void) {
    printf("[TEST] song-pool-pocket-advancement: with Link's Pocket set to Advancement, songs are held for the bag, "
           "pocket holds no song, and row/host counts still agree (#1549 owed from #1546)\n");
    sSpuFailures = 0;
    SpuKeys keys;
    CVarSetInteger("gRandoSettings.LinksPocket", 1);
    const char* seed = "RSBSSONGPOCKETADV";
    const SpuWorld w = SpuGenerate(seed, -1, -1, -1, true);
    SPU_CHECK(w.generated, "%s: the paired world did not finish\n", seed);
    if (w.generated) {
        SPU_CHECK(w.unionArmed && w.bag.exclusiveGroups == RSBS_COMBO_GROUP_SONG_LOCATIONS,
                  "%s: the shared song pool was not armed (union %d, exclusive 0x%08X)\n", seed,
                  w.unionArmed ? 1 : 0, (unsigned)w.bag.exclusiveGroups);
        SPU_CHECK(w.bag.status == RSBS_COMBO_LOGIC_OK && w.bag.fill.goalProven,
                  "%s: bag status %s, goalProven %d\n", seed, Combo_Logic_StatusName(w.bag.status),
                  w.bag.fill.goalProven ? 1 : 0);
        SPU_CHECK(w.pocketSong == 0, "%s: Link's Pocket holds a song\n", seed);
        const int pocketItem = OoT_SharedWarpSongsTest_LinksPocket();
        SPU_CHECK(pocketItem > 0 && OoT_ComboLogic_TestIsSongItem((uint16_t)pocketItem) == 0,
                  "%s: Link's Pocket item %d is not an advancement item or is a song\n", seed, pocketItem);
        const int unshared = OoT_SharedWarpSongsTest_UnsharedSongId();
        SPU_CHECK(OoT_Fill_TestSongHeldForBag((uint16_t)unshared) == 1,
                  "Link's Pocket song exclusion did not hold unshared song %d for bag\n", unshared);
        SPU_CHECK(w.ootOther == 0 && w.mmOther == 0,
                  "%s: %d OoT and %d MM song checks hold neither a song nor junk\n", seed, w.ootOther, w.mmOther);
        const int songsOnSongChecks = w.ootSongs + w.ootCrossed + w.mmSongs + w.mmCrossed;
        const int junk = w.ootJunk + w.mmJunk;
        SPU_CHECK(songsOnSongChecks == 22 && junk == 1,
                  "%s: %d songs on 23 song checks and %d junk, want 22 and 1\n", seed, songsOnSongChecks, junk);
        SPU_CHECK(w.ootJunked + w.mmJunked == 1 && w.ootPadded == 0 && w.mmPadded == 0,
                  "%s: remainders junked %d + %d song checks (padded %d + %d)\n", seed, w.ootJunked, w.mmJunked,
                  w.ootPadded, w.mmPadded);
        const int census = SpuCensusPlacedSongs();
        SPU_CHECK(census == 22, "%s: census of placed songs returned %d, want 22 distinct songs\n", seed, census);
    }
    printf("[TEST] song-pool-pocket-advancement: %s\n", sSpuFailures == 0 ? "PASS" : "FAIL");
    return sSpuFailures == 0 ? TEST_PASS : TEST_FAIL;
}

TestResult SongPoolAllLocations_Run(void) {
    printf("[TEST] song-pool-all-locations: under All Locations critical renewables rule is armed, and the leftover "
           "song check never holds a pending critical renewable first copy (#1549 owed from #1546)\n");
    sSpuFailures = 0;
    SpuKeys keys;

    // Leg (a) — Shipped profile under All Locations (RSBSSONGALR1):
    // Real paired world under RSBS_COMBO_RUNG_ALL_REACHABLE with default settings.
    {
        const char* seed = "RSBSSONGALR1";
        const SpuWorld w = SpuGenerate(seed, -1, (int)RSBS_COMBO_RUNG_ALL_REACHABLE, -1, true);
        SPU_CHECK(w.generated, "%s: the paired world did not finish\n", seed);
        if (w.generated) {
            SPU_CHECK(w.unionArmed && w.bag.exclusiveGroups == RSBS_COMBO_GROUP_SONG_LOCATIONS,
                      "%s: the shared song pool was not armed (union %d, exclusive 0x%08X)\n", seed,
                      w.unionArmed ? 1 : 0, (unsigned)w.bag.exclusiveGroups);
            SPU_CHECK(w.bag.status == RSBS_COMBO_LOGIC_OK && w.bag.fill.goalProven,
                      "%s: bag status %s, goalProven %d\n", seed, Combo_Logic_StatusName(w.bag.status),
                      w.bag.fill.goalProven ? 1 : 0);
            SPU_CHECK(Combo_CriticalRenewablesArmed((uint8_t)RSBS_COMBO_RUNG_ALL_REACHABLE),
                      "critical renewables rule was not armed under All Locations\n");
            SPU_CHECK(w.ootOther == 0 && w.mmOther == 0,
                      "%s: %d OoT and %d MM song checks hold neither a song nor junk\n", seed, w.ootOther, w.mmOther);
            const int songsOnSongChecks = w.ootSongs + w.ootCrossed + w.mmSongs + w.mmCrossed;
            const int junk = w.ootJunk + w.mmJunk;
            SPU_CHECK(songsOnSongChecks == 22 && junk == 1,
                      "%s: %d songs on 23 song checks and %d junk, want 22 and 1\n", seed, songsOnSongChecks, junk);
            SPU_CHECK(w.ootJunked + w.mmJunked == 1 && w.ootPadded == 0 && w.mmPadded == 0,
                      "%s: remainders junked %d + %d song checks (padded %d + %d)\n", seed, w.ootJunked, w.mmJunked,
                      w.ootPadded, w.mmPadded);
            const int census = SpuCensusPlacedSongs();
            SPU_CHECK(census == 22, "%s: census of placed songs returned %d, want 22 distinct songs\n", seed, census);
        }
    }

    // Leg (b) — Pending MM critical renewable first-copy guard (RSBSSONGALR2):
    // With Shuffle Shops on, crossings off (guaranteeing MM has the leftover song check),
    // and pre-Bomb-Shop RITYPE_JUNK checks 4, 5, 10, 12, 13 excluded, 10 Bombchus from
    // Bomb Shop check 14 is the first RENEWABLE item in rest. Line 3441 must skip it so
    // that Combo_CriticalRenewablesPlanFirstCopies places it on a renewing host.
    {
        SPU_CHECK(Combo_CriticalRenewableHostRenews((uint8_t)GAME_MM, 13) &&
                      Combo_CriticalRenewableHostRenews((uint8_t)GAME_MM, 14) &&
                      !Combo_CriticalRenewableHostRenews((uint8_t)GAME_MM, 15),
                  "Bomb Shop check renewable predicates do not match Types.h\n");

        CVarSetInteger(Combo_ComboSettingKey(COMBO_SETTING_DIRECTION), (int32_t)RSBS_COMBO_DIR_OFF);
        CVarSetInteger("gRando.Options.RO_SHUFFLE_SHOPS", 1);
        CVarSetString("gRando.ExcludedChecks", "4,5,10,12,13,");

        const char* seed = "RSBSSONGALR2";
        const SpuWorld w = SpuGenerate(seed, -1, (int)RSBS_COMBO_RUNG_ALL_REACHABLE, -1, true);

        CVarClear(Combo_ComboSettingKey(COMBO_SETTING_DIRECTION));
        CVarClear("gRando.Options.RO_SHUFFLE_SHOPS");
        CVarClear("gRando.ExcludedChecks");

        SPU_CHECK(w.generated, "%s: the paired world did not finish\n", seed);
        if (w.generated) {
            SPU_CHECK(w.unionArmed, "%s: shared song pool was not armed\n", seed);
            SPU_CHECK(w.bag.status == RSBS_COMBO_LOGIC_OK && w.bag.fill.goalProven,
                      "%s: bag status %s, goalProven %d\n", seed, Combo_Logic_StatusName(w.bag.status),
                      w.bag.fill.goalProven ? 1 : 0);
            SPU_CHECK(w.ootJunked == 0 && w.mmJunked == 1 && w.mmPadded == 0,
                      "%s: expected 0 OoT and 1 MM junked song checks (got %d + %d, padded %d + %d)\n", seed,
                      w.ootJunked, w.mmJunked, w.ootPadded, w.mmPadded);
            SPU_CHECK(w.ootOther == 0 && w.mmOther == 0,
                      "%s: %d OoT and %d MM song checks hold neither a song nor junk\n", seed, w.ootOther, w.mmOther);
            const int songsOnSongChecks = w.ootSongs + w.ootCrossed + w.mmSongs + w.mmCrossed;
            const int junk = w.ootJunk + w.mmJunk;
            SPU_CHECK(songsOnSongChecks == 22 && junk == 1,
                      "%s: %d songs on 23 song checks and %d junk, want 22 and 1\n", seed, songsOnSongChecks, junk);
            const int census = SpuCensusPlacedSongs();
            SPU_CHECK(census == 22, "%s: census of placed songs returned %d, want 22 distinct songs\n", seed, census);

            uint32_t songRenewed[RSBS_FOREIGN_POOL_ORIGIN_COUNT] = {};
            Combo_CriticalRenewablesRenewedNow(songRenewed);
            SPU_CHECK((songRenewed[GAME_MM] & (1u << RSBS_CRIT_MM_BOMBCHU_10)) == 0u,
                      "%s: 10 Bombchus were renewed before remainder fill\n", seed);

            const int shuffledTotal = MM_ComboLogic_TestShuffledItems(nullptr, nullptr, 0);
            std::vector<uint16_t> items((size_t)(shuffledTotal > 0 ? shuffledTotal : 1));
            std::vector<uint16_t> checks((size_t)(shuffledTotal > 0 ? shuffledTotal : 1));
            const int count = MM_ComboLogic_TestShuffledItems(items.data(), checks.data(), (int)items.size());

            int mmLeftoversChecked = 0;
            int bombchuOnRenewing = 0;
            for (int i = 0; i < count; ++i) {
                const uint16_t rc = checks[(size_t)i];
                const uint16_t itemId = items[(size_t)i];
                const SharedItem itemRow = { (uint8_t)GAME_MM, 0, itemId };

                if (Combo_CriticalRenewableKind(itemRow) == RSBS_CRIT_MM_BOMBCHU_10 &&
                    Combo_CriticalRenewableHostRenews((uint8_t)GAME_MM, rc)) {
                    bombchuOnRenewing++;
                }

                if (MM_Rando_Foreign_TestSongCheckIndex(rc) < 0) {
                    continue;
                }
                const SharedItem* crossing = Combo_Crossings_Lookup(GAME_MM, rc);
                if (crossing != nullptr) {
                    continue;
                }
                if (MM_ComboLogic_IsPairSongItem(itemId)) {
                    continue;
                }

                mmLeftoversChecked++;
                SPU_CHECK(!Combo_CriticalRenewablePending(itemRow, songRenewed) && Combo_CriticalRenewableKind(itemRow) < 0,
                          "%s: MM leftover song check %u holds critical renewable %s (%u)\n", seed, (unsigned)rc,
                          Combo_CriticalRenewableKindName(GAME_MM, Combo_CriticalRenewableKind(itemRow)),
                          (unsigned)itemId);
            }
            SPU_CHECK(mmLeftoversChecked == 1, "%s: inspected %d MM leftover song checks, want 1\n", seed,
                      mmLeftoversChecked);
            SPU_CHECK(bombchuOnRenewing > 0,
                      "%s: 10 Bombchus has %d copies on a renewing MM host, want >= 1\n", seed, bombchuOnRenewing);
        }
    }

    printf("[TEST] song-pool-all-locations: %s\n", sSpuFailures == 0 ? "PASS" : "FAIL");
    return sSpuFailures == 0 ? TEST_PASS : TEST_FAIL;
}

TestResult SongPoolCrossedHints_Run(void) {
    printf("[TEST] song-pool-crossed-hints: hint text for a crossed song names the right game's check, and Song of "
           "Time reveal text names the crossed song (#1549 owed from #1546)\n");
    sSpuFailures = 0;
    SpuKeys keys;
    SPU_CHECK(OoT_ComboLogic_TestEnsureItemTableTransient() == 0, "OoT's item table is not available\n");

    uint16_t rcSongOfTime = 0;
    uint16_t rcSheikForest = 0;
    for (int rc = 1; rc < OoT_ComboLogic_TestCheckIdSpace(); ++rc) {
        const int idx = OoT_ComboLogic_TestSongCheckIndex((uint16_t)rc);
        if (idx == RSBS_SONG_CHECK_OOT_OCARINA_OF_TIME) {
            rcSongOfTime = (uint16_t)rc;
        } else if (idx == RSBS_SONG_CHECK_OOT_SHEIK_FOREST) {
            rcSheikForest = (uint16_t)rc;
        }
    }
    SPU_CHECK(rcSongOfTime != 0 && rcSheikForest != 0, "could not find OoT song check IDs\n");

    SharedItem mmSonata;
    SharedItem mmOath;
    SharedItem ootLullaby;
    SPU_CHECK(TestNamedItem((uint8_t)GAME_MM, "Sonata of Awakening", &mmSonata), "MM Sonata of Awakening not found\n");
    SPU_CHECK(TestNamedItem((uint8_t)GAME_MM, "Oath to Order", &mmOath), "MM Oath to Order not found\n");
    SPU_CHECK(TestNamedItem((uint8_t)GAME_OOT, "Zelda's Lullaby", &ootLullaby), "OoT Zelda's Lullaby not found\n");

    uint16_t mmHealingCheck = 0;
    uint16_t mmSonataId = 0;
    MM_IntSongHost_PinIds(&mmHealingCheck, &mmSonataId);

    ComboLogicForcedPlacement forced[3];
    std::memset(forced, 0, sizeof(forced));
    // 0: Sonata of Awakening on OoT's Song of Time check
    forced[0].hostGame = (uint8_t)GAME_OOT;
    forced[0].hostCheck = rcSongOfTime;
    forced[0].item.originGame = (uint8_t)GAME_MM;
    forced[0].item.id = mmSonata.id;
    // 1: Oath to Order on OoT's Sacred Forest Meadow check
    forced[1].hostGame = (uint8_t)GAME_OOT;
    forced[1].hostCheck = rcSheikForest;
    forced[1].item.originGame = (uint8_t)GAME_MM;
    forced[1].item.id = mmOath.id;
    // 2: Zelda's Lullaby on MM's starting Song of Healing check
    forced[2].hostGame = (uint8_t)GAME_MM;
    forced[2].hostCheck = mmHealingCheck;
    forced[2].item.originGame = (uint8_t)GAME_OOT;
    forced[2].item.id = ootLullaby.id;

    SPU_CHECK(Combo_Logic_TestSetForcedPlacements(forced, 3) == 3, "failed to set forced placements\n");

    const char* seed = "RSBSSONGHINTS1";
    const SpuWorld w = SpuGenerate(seed, -1, -1, -1, true);
    SPU_CHECK(w.generated, "%s: the paired world did not finish\n", seed);
    if (w.generated) {
        // Arm the loaded-file hinting gate for the completed world
        const int wasSuspended = OoT_Combo_SetHintingSuspendedWorld(1);

        // (a) OoT crossing host hints: gossip stones and loaded hints (including Song of Time)
        int probes = 0;
        int stones = 0;
        const int hintFailures = Rando_TestCheckLoadedCrossingHostHints(&probes, &stones);
        SPU_CHECK(hintFailures == 0 && probes > 0 && stones > 0,
                  "OoT crossing host hints failed: %d failures, %d probes, %d loaded hints\n", hintFailures, probes,
                  stones);

        // (b) MM Skull Kid Oath to Order hint names OoT's Sacred Forest Meadow area
        char oathBuf[512];
        std::memset(oathBuf, 0, sizeof(oathBuf));
        const int oathLen = MM_HintSources_TestLegacyFixedText(RSBS_MM_HINT_SRC_OATH, oathBuf, (int)sizeof(oathBuf));
        SPU_CHECK(oathLen > 0, "MM Oath hint returned length %d\n", oathLen);
        SPU_CHECK(std::strstr(oathBuf, "Sacred Forest Meadow") != nullptr,
                  "MM Oath hint text does not name Sacred Forest Meadow: \"%s\"\n", oathBuf);

        // (c) MM HintArea location text helper names Sacred Forest Meadow for Oath to Order
        char areaBuf[256];
        std::memset(areaBuf, 0, sizeof(areaBuf));
        const int areaLen = MM_HintArea_TestItemLocationText(mmOath.id, "Giants at {{location}}", "{{location}}",
                                                            areaBuf, (int)sizeof(areaBuf));
        SPU_CHECK(areaLen > 0 && std::strstr(areaBuf, "Sacred Forest Meadow") != nullptr,
                  "MM HintArea did not name Sacred Forest Meadow for crossed Oath: \"%s\"\n", areaBuf);

        OoT_Combo_SetHintingSuspendedWorld(wasSuspended);
    }
    Combo_Logic_TestSetForcedPlacements(nullptr, 0);
    printf("[TEST] song-pool-crossed-hints: %s\n", sSpuFailures == 0 ? "PASS" : "FAIL");
    return sSpuFailures == 0 ? TEST_PASS : TEST_FAIL;
}
