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
 * ============================================================================
 * The rows #1546 owed (#1549), each generating and finishing real paired worlds
 * (rando label). SpuCensusPlacedSongs, shared by the first three, checks the
 * finished world against the two pools the generation handed the single bag:
 * every OoT song and every MM pair song is placed (its own game's copies plus its
 * crossings in the other game) exactly as many times as its game's pool holds it
 * (MM's pool holds the progressive Goron Lullaby twice), every placed copy is on
 * a song check, and the pools have the profile's shape (OoT 12 copies of 12
 * songs, MM 10 copies of 9). Red on a song dropped, doubled past its pool count
 * or placed off the song checks; each mismatch names the game, song id and name,
 * copies found and copies wanted. SpuCheckLeftoverJunk: the song checks left to
 * junk are the song checks less the pooled song copies, and the two remainders
 * junked exactly those from their pools.
 * ============================================================================
 *
 * song-pool-crosses-both-ways (SongPoolCrossesBothWays, three worlds): per seed
 *  the pool shape of song-pool-union-world (armed, no non-song on a song check, no
 *  song in Link's Pocket) plus the census and the leftover junk; over the three
 *  seeds at least one MM song lands on an OoT song check and one OoT song on an
 *  MM song check (red if the shared pool stops crossing in either direction).
 *
 * song-pool-pocket-advancement (SongPoolPocketAdvancement, one world): Link's
 *  Pocket = Advancement, with dungeon rewards at Any Dungeon (FinalizeSettings
 *  resets the pocket to Dungeon Reward under End of Dungeons). The pocket holds an
 *  advancement item that is neither a dungeon reward nor a song; fill.cpp's
 *  RsbsSongHeldForBag answers 1 for an unshared song; the pool shape, the census
 *  and the leftover junk hold (red if the pocket draws a song the bag places).
 *
 * song-pool-all-locations (SongPoolAllLocations, two worlds): (a) the default
 *  world with All Locations set explicitly: the fill froze that rung, and the pool
 *  shape, census and leftover junk hold under it; (b) Crossings Off, Shuffle Shops
 *  on and five junk checks excluded: nothing crosses, each game's remainder junks
 *  its own leftover song checks (OoT none, MM exactly one), the census holds, the
 *  one MM leftover holds no critical renewable still pending (printed), and a 10
 *  Bombchus copy sits on a renewing MM host. (b) asserts those outcomes; it is not
 *  shown to lock the leftover step's pending-kind skip (see the leg's comment).
 *
 * song-pool-crossed-hints (SongPoolCrossedHints, one world): the OoT hint on and
 *  three forced crossings (MM's Sonata on OoT's Song of Time check, MM's Oath on
 *  Sacred Forest Meadow's, OoT's Lullaby on MM's Song of Healing check), each
 *  asserted in the store; every OoT hint naming a crossing host names the MM item
 *  (H3b's loaded bridge); the Song of Time reveal names "the Sonata of Awakening
 *  (MM)" and not the cover; MM's Oath hint and HintArea text name Sacred Forest
 *  Meadow.
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
int OoT_ComboLogic_TestFillAdvancement(uint16_t id);
void MM_IntSongHost_PinIds(uint16_t* healingCheck, uint16_t* sonataItem);
int OoT_Combo_SetHintingSuspendedWorld(int on);
int Rando_TestCheckLoadedCrossingHostHints(int* outProbes, int* outStones);
int MM_HintSources_TestLegacyFixedText(uint16_t source, char* out, int cap);
int MM_HintArea_TestItemLocationText(uint16_t randoItemId, const char* tmpl, const char* key, char* out, int cap);
int Rando_TestOoTHintRevealText(char* outText, int textCap, char* outExpected, int expectedCap, int* outNamesSongCheck,
                                int* outCoverNamed);
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
        CVarClear("gRandoSettings.ShuffleDungeonReward");
        CVarClear("gRandoSettings.OoTHint");
        CVarClear("gRando.Options.RO_SHUFFLE_SHOPS");
        CVarClear("gRando.Options.RO_PLACEMENT_SMALL_KEYS");
        CVarClear("gRando.Options.RO_PLACEMENT_BOSS_KEYS");
        CVarClear("gRando.Options.RO_PLACEMENT_STRAY_FAIRIES");
        CVarClear("gRando.Options.RO_PLACEMENT_STRAY_FAIRIES_OTHER");
        CVarClear("gRando.ExcludedChecks");
        Combo_Logic_TestSetForcedPlacements(nullptr, 0);
        OoT_Combo_SetHintingSuspendedWorld(0);
        Combo_SongHostsForceForTest(-1);
        Combo_SingleBag_Forget();
        ComboContext_Init();
    }
};

/** One song id and how many copies of it a pool holds. */
struct SpuSongCopies {
    uint16_t id;
    int copies;
};

/** Room for every song id of one game (OoT has 13 song items, MM 16 pair songs). */
#define SPU_SONG_ID_CAP 48

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
    // The song multisets of the two pools this generation handed the single bag
    // (Combo_SingleBag_Run's inputs): OoT's general-pass pool
    // (OoT_ComboLogic_ExportPool source 0, read before MM's half runs the bag) and
    // MM's GeneratePools pool (MM_ComboLogic_TestLastPoolConfinement). Rows -1
    // when the pool could not be read.
    int ootPoolRows, mmPoolRows;
    int ootPoolSongIds, mmPoolSongIds;
    SpuSongCopies ootPoolSongs[SPU_SONG_ID_CAP];
    SpuSongCopies mmPoolSongs[SPU_SONG_ID_CAP];
};

/** Count one more copy of @p id in @p t (@p n entries). False when full. */
bool SpuTallySong(SpuSongCopies* t, int* n, uint16_t id) {
    for (int i = 0; i < *n; ++i) {
        if (t[i].id == id) {
            t[i].copies++;
            return true;
        }
    }
    if (*n >= SPU_SONG_ID_CAP) {
        return false;
    }
    t[*n].id = id;
    t[*n].copies = 1;
    (*n)++;
    return true;
}

/** How many copies of @p id the tally holds (0 when none). */
int SpuPoolCopies(const SpuSongCopies* t, int n, uint16_t id) {
    for (int i = 0; i < n; ++i) {
        if (t[i].id == id) {
            return t[i].copies;
        }
    }
    return 0;
}

/** The describer's name for @p id of @p game, "?" when it has none. */
const char* SpuSongName(GameId game, uint16_t id) {
    SharedItem item;
    std::memset(&item, 0, sizeof(item));
    item.originGame = (uint8_t)game;
    item.id = id;
    const char* name = Combo_GetForeignItemName(item);
    return name != nullptr ? name : "?";
}

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
    w.ootPoolRows = -1;
    w.mmPoolRows = -1;
    if (gen == 0) {
        // OoT waits at its general pass: `itemPool` is exactly what the bag exports.
        const int total = OoT_ComboLogic_ExportPool(0, nullptr, nullptr, nullptr, 0);
        if (total >= 0) {
            std::vector<uint16_t> rows((size_t)(total > 0 ? total : 1), 0);
            const int got = OoT_ComboLogic_ExportPool(0, rows.data(), nullptr, nullptr, total);
            w.ootPoolRows = got < total ? got : total;
            for (int i = 0; i < w.ootPoolRows; ++i) {
                if (OoT_ComboLogic_TestIsSongItem(rows[(size_t)i]) != 0 &&
                    !SpuTallySong(w.ootPoolSongs, &w.ootPoolSongIds, rows[(size_t)i])) {
                    w.ootPoolRows = -1;
                    break;
                }
            }
        }
    }
    const int mmHalf = gen == 0 ? MM_Rando_HeadlessPairedHalf() : -1;
    w.bag = *Combo_SingleBag_LastReport();
    w.placementDigest = w.bag.fill.placementDigest;
    w.generated = gen == 0 && mmHalf == 0 && w.bag.status == RSBS_COMBO_LOGIC_OK;
    {
        const int total = MM_ComboLogic_TestLastPoolConfinement(nullptr, nullptr, 0);
        std::vector<uint16_t> items((size_t)(total > 0 ? total : 1), 0);
        std::vector<uint32_t> masks((size_t)(total > 0 ? total : 1), 0u);
        MM_ComboLogic_TestLastPoolConfinement(items.data(), masks.data(), total);
        for (int i = 0; i < total; ++i) {
            w.mmWideRows += (masks[(size_t)i] & RSBS_COMBO_GROUP_WIDE_MASK) != 0u ? 1 : 0;
        }
        // The last fill's pool is this world's only when MM's half ran.
        if (mmHalf == 0) {
            w.mmPoolRows = total;
            for (int i = 0; i < total; ++i) {
                if (MM_ComboLogic_IsPairSongItem(items[(size_t)i]) &&
                    !SpuTallySong(w.mmPoolSongs, &w.mmPoolSongIds, items[(size_t)i])) {
                    w.mmPoolRows = -1;
                    break;
                }
            }
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

/** What the census found: its failures and the two pools' song copies. */
struct SpuCensus {
    int failures;
    int ootPoolCopies; // song copies in OoT's general-pass pool
    int mmPoolCopies;  // ... and in MM's pool
};

/**
 * THE CENSUS OF PLACED SONGS, against the pools this generation built. For every
 * OoT song item (OoT_ComboLogic_TestIsSongItem) and every MM pair song item
 * (MM_ComboLogic_IsPairSongItem), the copies placed in the finished world (its
 * own game's checks plus its crossings hosted in the other game) must equal the
 * copies of it in its game's pool (SpuWorld's tallies): red on a song dropped, or
 * placed more often than its pool holds it. MM's pool holds the progressive Goron
 * Lullaby twice (both Lullaby song checks have it as their vanilla item,
 * GeneratePools pushes each), so a copy count, not "one copy per id", is the
 * rule. Every placed song copy must also be on a song check of either game (the
 * songs the four song-check counters see): red on a song placed off the song
 * checks the shared pool confines it to.
 *
 * The pools are the derived truth, so the census cannot see a song missing from
 * a pool. The profile's pools are therefore also asserted against their known
 * shape, with the pool's songs printed so a red run says what moved: OoT's 12
 * song checks' songs once each (12 copies of 12 songs; the separate Song of
 * Soaring (OoT) is pooled only under its hidden rule), and MM's 11 song checks'
 * songs less the starting Song of Time (10 copies of 9 songs; the six "(MM)" warp
 * songs are pooled only under their off-by-default rules).
 *
 * Every mismatch is its own FAIL line naming the seed, the game's half, the song
 * id and name, the copies found (split by where) and the copies expected.
 */
SpuCensus SpuCensusPlacedSongs(const SpuWorld& w, const char* seed) {
    SpuCensus c = { 0, 0, 0 };
    const int before = sSpuFailures;
    SPU_CHECK(w.ootPoolRows > 0 && w.mmPoolRows > 0,
              "%s: census: the pools the bag took could not be read (OoT %d rows, MM %d rows)\n", seed, w.ootPoolRows,
              w.mmPoolRows);
    if (w.ootPoolRows <= 0 || w.mmPoolRows <= 0) {
        c.failures = sSpuFailures - before;
        return c;
    }

    // OoT's half: each OoT song, on OoT's checks plus crossed onto MM hosts.
    int ootPlaced = 0;
    std::string ootPool;
    for (uint16_t id = 1; id < 5000; ++id) {
        if (OoT_ComboLogic_TestIsSongItem(id) == 0) {
            continue;
        }
        const int pool = SpuPoolCopies(w.ootPoolSongs, w.ootPoolSongIds, id);
        const int onOoT = OoT_SharedSongsTest_WorldCopies((int)id);
        int crossedIntoMM = 0;
        for (int i = 0; i < Combo_Crossings_Count(GAME_MM); ++i) {
            ComboCrossing x;
            if (Combo_Crossings_At(GAME_MM, i, &x) && x.item.originGame == (uint8_t)GAME_OOT && x.item.id == id) {
                crossedIntoMM++;
            }
        }
        const int placed = (onOoT > 0 ? onOoT : 0) + crossedIntoMM;
        ootPlaced += placed;
        c.ootPoolCopies += pool;
        if (pool > 0) {
            ootPool += " " + std::to_string((unsigned)id) + "x" + std::to_string(pool);
        }
        SPU_CHECK(onOoT >= 0 && placed == pool,
                  "%s: census OoT half: OoT song %u (%s) has %d copies placed (%d on OoT checks, %d crossed into MM), "
                  "want %d (OoT's pool)\n",
                  seed, (unsigned)id, SpuSongName(GAME_OOT, id), placed, onOoT, crossedIntoMM, pool);
    }

    // MM's half: each MM pair song, on MM's checks plus crossed onto OoT hosts.
    int mmPlaced = 0;
    std::string mmPool;
    for (uint16_t id = 1; id < 5000; ++id) {
        if (!MM_ComboLogic_IsPairSongItem(id)) {
            continue;
        }
        const int pool = SpuPoolCopies(w.mmPoolSongs, w.mmPoolSongIds, id);
        const int onMM = MM_PlacementConfinement_WorldCopies(id);
        int crossedIntoOoT = 0;
        for (int i = 0; i < Combo_Crossings_Count(GAME_OOT); ++i) {
            ComboCrossing x;
            if (Combo_Crossings_At(GAME_OOT, i, &x) && x.item.originGame == (uint8_t)GAME_MM && x.item.id == id) {
                crossedIntoOoT++;
            }
        }
        const int placed = onMM + crossedIntoOoT;
        mmPlaced += placed;
        c.mmPoolCopies += pool;
        if (pool > 0) {
            mmPool += " " + std::to_string((unsigned)id) + "x" + std::to_string(pool);
        }
        SPU_CHECK(placed == pool,
                  "%s: census MM half: MM song %u (%s) has %d copies placed (%d on MM checks, %d crossed into OoT), "
                  "want %d (MM's pool)\n",
                  seed, (unsigned)id, SpuSongName(GAME_MM, id), placed, onMM, crossedIntoOoT, pool);
    }

    const int onSongChecks = w.ootSongs + w.ootCrossed + w.mmSongs + w.mmCrossed;
    printf("[TEST] song-pool: %s: census: OoT pool %d song copies of %d songs [%s ], MM pool %d copies of %d songs "
           "[%s ]; placed %d + %d, %d on song checks\n",
           seed, c.ootPoolCopies, w.ootPoolSongIds, ootPool.c_str(), c.mmPoolCopies, w.mmPoolSongIds, mmPool.c_str(),
           ootPlaced, mmPlaced, onSongChecks);
    SPU_CHECK(ootPlaced + mmPlaced == onSongChecks,
              "%s: census: %d song copies placed in both games (OoT %d, MM %d) but %d on song checks: a song sits off "
              "the song checks the shared pool confines it to\n",
              seed, ootPlaced + mmPlaced, ootPlaced, mmPlaced, onSongChecks);
    SPU_CHECK(c.ootPoolCopies == 12 && w.ootPoolSongIds == 12,
              "%s: census OoT half: OoT's pool holds %d song copies of %d songs [%s ], the profile's holds 12 of 12 "
              "(its 12 song checks' songs): the pool moved\n",
              seed, c.ootPoolCopies, w.ootPoolSongIds, ootPool.c_str());
    SPU_CHECK(c.mmPoolCopies == 10 && w.mmPoolSongIds == 9,
              "%s: census MM half: MM's pool holds %d song copies of %d songs [%s ], the profile's holds 10 of 9 (its "
              "11 song checks' songs less the starting Song of Time, the Goron Lullaby twice): the pool moved\n",
              seed, c.mmPoolCopies, w.mmPoolSongIds, mmPool.c_str());
    c.failures = sSpuFailures - before;
    return c;
}

/** OoT's song checks (all twelve, whatever they hold) and MM's eleven, from the
 *  song-check counters. */
int SpuOoTSongChecks(const SpuWorld& w) {
    return w.ootSongs + w.ootCrossed + w.ootJunk + w.ootOther;
}
int SpuMMSongChecks(const SpuWorld& w) {
    return w.mmSongs + w.mmCrossed + w.mmJunk + w.mmOther + w.mmNotShuffled;
}

/**
 * The leftover song checks, derived: the song checks of both games less the song
 * copies the two pools hold are the ones left to junk (OoTMM fillJunk), and the
 * two remainders' junk steps junked exactly those, from the pools (no fresh pad).
 */
void SpuCheckLeftoverJunk(const SpuWorld& w, const SpuCensus& c, const char* seed) {
    const int songChecks = SpuOoTSongChecks(w) + SpuMMSongChecks(w);
    const int leftover = songChecks - (c.ootPoolCopies + c.mmPoolCopies);
    const int junk = w.ootJunk + w.mmJunk;
    SPU_CHECK(junk == leftover,
              "%s: %d song checks hold junk, want %d (%d song checks less %d + %d pooled song copies)\n", seed, junk,
              leftover, songChecks, c.ootPoolCopies, c.mmPoolCopies);
    SPU_CHECK(w.ootJunked + w.mmJunked == junk && w.ootPadded == 0 && w.mmPadded == 0,
              "%s: the remainders junked %d + %d song checks (padded %d + %d), the checks hold %d junk\n", seed,
              w.ootJunked, w.mmJunked, w.ootPadded, w.mmPadded, junk);
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

    // (i) Anywhere, override cleared and 1: first under shipped #1548 placement
    // defaults (9 small keys + 4 boss keys + 31 chest fairies = 44 locally
    // confined MM rows, 0 wide rows, union disarmed), then explicitly Anywhere
    // (0 confined rows).
    for (int anywherePlacement = 0; anywherePlacement <= 1; ++anywherePlacement) {
        if (anywherePlacement) {
            CVarSetInteger("gRando.Options.RO_PLACEMENT_SMALL_KEYS", 0);
            CVarSetInteger("gRando.Options.RO_PLACEMENT_BOSS_KEYS", 0);
            CVarSetInteger("gRando.Options.RO_PLACEMENT_STRAY_FAIRIES", 0);
            CVarSetInteger("gRando.Options.RO_PLACEMENT_STRAY_FAIRIES_OTHER", 0);
        } else {
            CVarClear("gRando.Options.RO_PLACEMENT_SMALL_KEYS");
            CVarClear("gRando.Options.RO_PLACEMENT_BOSS_KEYS");
            CVarClear("gRando.Options.RO_PLACEMENT_STRAY_FAIRIES");
            CVarClear("gRando.Options.RO_PLACEMENT_STRAY_FAIRIES_OTHER");
        }
        const int wantConfined = anywherePlacement ? 0 : 44;
        for (int forced = -1; forced <= 1; forced += 2) {
            const SpuWorld w = SpuGenerate("RSBSSONGANYWHERE", (int)RSBS_OOT_SONGS_ANYWHERE, -1, forced, false);
            SPU_CHECK(w.generated, "(i) anywhere=%d override %d: the Anywhere world did not generate\n",
                      anywherePlacement, forced);
            SPU_CHECK(!w.unionArmed && w.bag.exclusiveGroups == 0u && w.mmWideRows == 0 &&
                          w.bag.confinedRows == wantConfined,
                      "(i) anywhere=%d override %d: under Anywhere the union read %d, the exclusive group 0x%08X, %d "
                      "MM rows wide, %d bag rows confined (want %d)\n",
                      anywherePlacement, forced, w.unionArmed ? 1 : 0, (unsigned)w.bag.exclusiveGroups, w.mmWideRows,
                      w.bag.confinedRows, wantConfined);
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
                      "(i) anywhere=%d override %d: under Anywhere OoT gave %d rows the song group (%d wrong), MM %d "
                      "rows and %d song checks, OoT %d song checks a group\n",
                      anywherePlacement, forced, ootWide, ootWrong, mmWideItems, mmWideSong, ootGrouped);
        }
    }
    CVarClear("gRando.Options.RO_PLACEMENT_SMALL_KEYS");
    CVarClear("gRando.Options.RO_PLACEMENT_BOSS_KEYS");
    CVarClear("gRando.Options.RO_PLACEMENT_STRAY_FAIRIES");
    CVarClear("gRando.Options.RO_PLACEMENT_STRAY_FAIRIES_OTHER");

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
           "and every pooled song copy is placed once, on a song check (#1549 owed from #1546)\n");
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
                  "%s: the shared song pool was not armed (union %d, exclusive 0x%08X)\n", seed, w.unionArmed ? 1 : 0,
                  (unsigned)w.bag.exclusiveGroups);
        SPU_CHECK(w.bag.status == RSBS_COMBO_LOGIC_OK && w.bag.fill.goalProven, "%s: bag status %s, goalProven %d\n",
                  seed, Combo_Logic_StatusName(w.bag.status), w.bag.fill.goalProven ? 1 : 0);
        SPU_CHECK(w.ootOther == 0 && w.mmOther == 0, "%s: %d OoT and %d MM song checks hold neither a song nor junk\n",
                  seed, w.ootOther, w.mmOther);
        SPU_CHECK(w.pocketSong == 0, "%s: Link's Pocket holds a song\n", seed);
        const SpuCensus census = SpuCensusPlacedSongs(w, seed);
        SpuCheckLeftoverJunk(w, census, seed);
        totalCrossedOoT += w.ootCrossed;
        totalCrossedMM += w.mmCrossed;
    }
    SPU_CHECK(totalCrossedOoT > 0, "no seed placed an MM song on an OoT song check (total %d)\n", totalCrossedOoT);
    SPU_CHECK(totalCrossedMM > 0, "no seed placed an OoT song on an MM song check (total %d)\n", totalCrossedMM);
    printf("[TEST] song-pool-crosses-both-ways: %d crossed into OoT, %d crossed into MM; %s\n", totalCrossedOoT,
           totalCrossedMM, sSpuFailures == 0 ? "PASS" : "FAIL");
    return sSpuFailures == 0 ? TEST_PASS : TEST_FAIL;
}

TestResult SongPoolPocketAdvancement_Run(void) {
    printf("[TEST] song-pool-pocket-advancement: with Link's Pocket set to Advancement, songs are held for the bag, "
           "pocket holds no song, and row/host counts still agree (#1549 owed from #1546)\n");
    sSpuFailures = 0;
    SpuKeys keys;
    // Context::FinalizeSettings forces Link's Pocket back to Dungeon Reward while
    // dungeon rewards are End of Dungeons (the default), so Advancement takes effect
    // only with them elsewhere: Any Dungeon (2) keeps them in OoT's own dungeon pass.
    CVarSetInteger("gRandoSettings.ShuffleDungeonReward", 2); // RO_DUNGEON_REWARDS_ANY_DUNGEON
    CVarSetInteger("gRandoSettings.LinksPocket", 1);          // RO_LINKS_POCKET_ADVANCEMENT
    SharedItem kokiriEmerald = {};
    SharedItem lightMedallion = {};
    SPU_CHECK(TestNamedItem((uint8_t)GAME_OOT, "Kokiri's Emerald", &kokiriEmerald) &&
                  TestNamedItem((uint8_t)GAME_OOT, "Light Medallion", &lightMedallion),
              "OoT's dungeon rewards are not named\n");
    const char* seed = "RSBSSONGPOCKETADV";
    const SpuWorld w = SpuGenerate(seed, -1, -1, -1, true);
    SPU_CHECK(w.generated, "%s: the paired world did not finish\n", seed);
    if (w.generated) {
        SPU_CHECK(w.unionArmed && w.bag.exclusiveGroups == RSBS_COMBO_GROUP_SONG_LOCATIONS,
                  "%s: the shared song pool was not armed (union %d, exclusive 0x%08X)\n", seed, w.unionArmed ? 1 : 0,
                  (unsigned)w.bag.exclusiveGroups);
        SPU_CHECK(w.bag.status == RSBS_COMBO_LOGIC_OK && w.bag.fill.goalProven, "%s: bag status %s, goalProven %d\n",
                  seed, Combo_Logic_StatusName(w.bag.status), w.bag.fill.goalProven ? 1 : 0);
        SPU_CHECK(w.pocketSong == 0, "%s: Link's Pocket holds a song\n", seed);
        const int pocketItem = OoT_SharedWarpSongsTest_LinksPocket();
        // Advancement took effect: the pocket holds an advancement item that is no
        // dungeon reward (under Dungeon Reward it would hold one of the nine,
        // RG_KOKIRI_EMERALD .. RG_LIGHT_MEDALLION, contiguous) and no song.
        const bool pocketReward = pocketItem >= (int)kokiriEmerald.id && pocketItem <= (int)lightMedallion.id;
        printf("[TEST] song-pool: %s: Link's Pocket holds OoT item %d (advancement %d, dungeon reward %d)\n", seed,
               pocketItem, pocketItem > 0 ? OoT_ComboLogic_TestFillAdvancement((uint16_t)pocketItem) : -1,
               pocketReward ? 1 : 0);
        SPU_CHECK(pocketItem > 0 && OoT_ComboLogic_TestFillAdvancement((uint16_t)pocketItem) == 1 && !pocketReward,
                  "%s: Link's Pocket item %d is not an advancement item other than a dungeon reward: Advancement "
                  "did not take effect\n",
                  seed, pocketItem);
        SPU_CHECK(pocketItem > 0 && OoT_ComboLogic_TestIsSongItem((uint16_t)pocketItem) == 0,
                  "%s: Link's Pocket item %d is a song\n", seed, pocketItem);
        const int unshared = OoT_SharedWarpSongsTest_UnsharedSongId();
        SPU_CHECK(OoT_Fill_TestSongHeldForBag((uint16_t)unshared) == 1,
                  "Link's Pocket song exclusion did not hold unshared song %d for bag\n", unshared);
        SPU_CHECK(w.ootOther == 0 && w.mmOther == 0, "%s: %d OoT and %d MM song checks hold neither a song nor junk\n",
                  seed, w.ootOther, w.mmOther);
        const SpuCensus census = SpuCensusPlacedSongs(w, seed);
        SpuCheckLeftoverJunk(w, census, seed);
    }
    printf("[TEST] song-pool-pocket-advancement: %s\n", sSpuFailures == 0 ? "PASS" : "FAIL");
    return sSpuFailures == 0 ? TEST_PASS : TEST_FAIL;
}

TestResult SongPoolAllLocations_Run(void) {
    printf("[TEST] song-pool-all-locations: under All Locations critical renewables rule is armed, and the leftover "
           "song check never holds a pending critical renewable first copy (#1549 owed from #1546)\n");
    sSpuFailures = 0;
    SpuKeys keys;

    // Leg (a): the shipped profile with All Locations Reachable set explicitly
    // (RSBSSONGALR1). All Locations is already the default rung, so this is the
    // default world; it asserts the generation froze that rung (so the critical-
    // renewables rule was armed for its fill) and the song-pool shape under it. The
    // critical-renewables guard itself is leg (b)'s: here the leftover song check
    // can fall on either game.
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
            SPU_CHECK(w.bag.rung == (uint8_t)RSBS_COMBO_RUNG_ALL_REACHABLE && Combo_CriticalRenewablesArmed(w.bag.rung),
                      "%s: the fill froze rung %u, not All Locations, so critical renewables were not armed\n", seed,
                      (unsigned)w.bag.rung);
            SPU_CHECK(w.ootOther == 0 && w.mmOther == 0,
                      "%s: %d OoT and %d MM song checks hold neither a song nor junk\n", seed, w.ootOther, w.mmOther);
            const SpuCensus census = SpuCensusPlacedSongs(w, seed);
            SpuCheckLeftoverJunk(w, census, seed);
        }
    }

    // Leg (b): MM's leftover song check under Crossings Off (RSBSSONGALR2).
    // Crossings Off keeps every song in its own game, so OoT's 12 songs fill its 12
    // song checks and MM's 10 copies leave one of its 11 to MM's leftover step
    // (ComboLogicEngineSingleExe.cpp, THE SHARED SONG POOL'S LEFTOVER SONG CHECKS).
    // Shuffle Shops on pools the Bomb Shop, so no vanilla shelf renews 10 Bombchus
    // and its first copy is pending when that step runs. The leg asserts the
    // outcome: the leftover holds no kind still pending, and a 10 Bombchus copy
    // sits on a renewing host.
    //
    // What it does NOT lock: the step's pending-kind skip (`if (critArmed &&
    // rest[r] != RI_JUNK && Combo_CriticalRenewablePending(row, songRenewed)) {`).
    // The step takes the first RI_JUNK or RENEWABLE-class row of `rest`, the rows
    // the bag left to MM's own pass in GeneratePools' region-major pool order
    // (trimmed rows as RI_JUNK in place). Without the skip the result moves only
    // if a pending kind's row comes before the first eligible row the skip lets
    // through, and which rows the bag leaves, which rows the trim turns to junk and
    // so that order are not readable from the code. On the first observed run the
    // leftover held RI_JUNK. The excluded checks (4, 5, 10, 12: rupee chests; 13:
    // the Bomb Shop's 10 Bombs) are junk-vanilla, so they stay vanilla and leave the
    // pool; 13 also renews bombs, which the precondition below reads.
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
        CVarClear("gRando.Options.RO_PLACEMENT_SMALL_KEYS");
        CVarClear("gRando.Options.RO_PLACEMENT_BOSS_KEYS");
        CVarClear("gRando.Options.RO_PLACEMENT_STRAY_FAIRIES");
        CVarClear("gRando.Options.RO_PLACEMENT_STRAY_FAIRIES_OTHER");
        CVarClear("gRando.ExcludedChecks");

        SPU_CHECK(w.generated, "%s: the paired world did not finish\n", seed);
        if (w.generated) {
            SPU_CHECK(w.unionArmed, "%s: shared song pool was not armed\n", seed);
            SPU_CHECK(w.bag.status == RSBS_COMBO_LOGIC_OK && w.bag.fill.goalProven,
                      "%s: bag status %s, goalProven %d\n", seed, Combo_Logic_StatusName(w.bag.status),
                      w.bag.fill.goalProven ? 1 : 0);
            SPU_CHECK(w.ootOther == 0 && w.mmOther == 0,
                      "%s: %d OoT and %d MM song checks hold neither a song nor junk\n", seed, w.ootOther, w.mmOther);
            const SpuCensus census = SpuCensusPlacedSongs(w, seed);
            SpuCheckLeftoverJunk(w, census, seed);
            // Crossings Off: every song stays in its own game, so each game's
            // leftover song checks are its song checks less its own pool's songs
            // (OoT 12 - 12 = 0, MM 11 - 10 = 1 on the profile), each junked by its
            // own remainder.
            const int ootLeftover = SpuOoTSongChecks(w) - census.ootPoolCopies;
            const int mmLeftover = SpuMMSongChecks(w) - census.mmPoolCopies;
            SPU_CHECK(w.ootCrossed == 0 && w.mmCrossed == 0 && w.ootJunked == ootLeftover && w.mmJunked == mmLeftover &&
                          mmLeftover == 1,
                      "%s: Crossings Off: %d + %d songs crossed, the remainders junked OoT %d / MM %d song checks, "
                      "want %d / %d (MM must keep exactly one leftover song check for this leg)\n",
                      seed, w.ootCrossed, w.mmCrossed, w.ootJunked, w.mmJunked, ootLeftover, mmLeftover);

            // The kinds MM renews without the fill: vanilla shelves and counters
            // outside the host pool, cows and the three business scrubs. MM's
            // junk-class critical rows (10 Bombchus, potions, milk) are never the
            // bag's, so for them this is what the leftover step saw. A setup
            // precondition, not the guard: with shops pooled, 10 Bombchus is pending.
            uint32_t songRenewed[RSBS_FOREIGN_POOL_ORIGIN_COUNT] = {};
            Combo_CriticalRenewablesRenewedNow(songRenewed);
            SPU_CHECK((songRenewed[GAME_MM] & (1u << RSBS_CRIT_MM_BOMBCHU_10)) == 0u,
                      "%s: setup: 10 Bombchus is renewed outside the fill, so its first copy was never pending\n",
                      seed);

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
                const int leftoverKind = Combo_CriticalRenewableKind(itemRow);
                printf("[TEST] song-pool: %s: MM leftover song check %u holds MM item %u (critical kind %d%s%s)\n",
                       seed, (unsigned)rc, (unsigned)itemId, leftoverKind, leftoverKind >= 0 ? ", " : "",
                       leftoverKind >= 0 ? Combo_CriticalRenewableKindName((uint8_t)GAME_MM, leftoverKind) : "");
                // Production's rule: the leftover takes no kind still PENDING (a
                // renewed kind, such as milk the cows give again, may sit there).
                SPU_CHECK(!Combo_CriticalRenewablePending(itemRow, songRenewed),
                          "%s: MM leftover song check %u holds the pending first copy of %s (%u)\n", seed, (unsigned)rc,
                          Combo_CriticalRenewableKindName((uint8_t)GAME_MM, leftoverKind), (unsigned)itemId);
            }
            SPU_CHECK(mmLeftoversChecked == mmLeftover, "%s: inspected %d MM leftover song checks, want %d\n", seed,
                      mmLeftoversChecked, mmLeftover);
            SPU_CHECK(bombchuOnRenewing > 0, "%s: 10 Bombchus has %d copies on a renewing MM host, want >= 1\n", seed,
                      bombchuOnRenewing);
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

    // The Song of Time reveal (RH_OOT_HINT, Sheik in the Temple of Time) exists only
    // under RSK_OOT_HINT, which is off by default.
    CVarSetInteger("gRandoSettings.OoTHint", 1);
    const char* seed = "RSBSSONGHINTS1";
    const SpuWorld w = SpuGenerate(seed, -1, -1, -1, true);
    SPU_CHECK(w.generated, "%s: the paired world did not finish\n", seed);
    if (w.generated) {
        // The forced placements held: each song is a crossing on its check.
        const SharedItem* onSongOfTime = Combo_Crossings_Lookup(GAME_OOT, rcSongOfTime);
        const SharedItem* onSheikForest = Combo_Crossings_Lookup(GAME_OOT, rcSheikForest);
        const SharedItem* onHealing = Combo_Crossings_Lookup(GAME_MM, mmHealingCheck);
        SPU_CHECK(
            onSongOfTime != nullptr && onSongOfTime->originGame == (uint8_t)GAME_MM && onSongOfTime->id == mmSonata.id,
            "%s: OoT's Song of Time check %u does not host MM's Sonata of Awakening\n", seed, (unsigned)rcSongOfTime);
        SPU_CHECK(onSheikForest != nullptr && onSheikForest->originGame == (uint8_t)GAME_MM &&
                      onSheikForest->id == mmOath.id,
                  "%s: OoT's Sacred Forest Meadow song check %u does not host MM's Oath to Order\n", seed,
                  (unsigned)rcSheikForest);
        SPU_CHECK(onHealing != nullptr && onHealing->originGame == (uint8_t)GAME_OOT && onHealing->id == ootLullaby.id,
                  "%s: MM's Song of Healing check %u does not host OoT's Zelda's Lullaby\n", seed,
                  (unsigned)mmHealingCheck);

        // Arm the loaded-file hinting gate for the completed world
        const int wasSuspended = OoT_Combo_SetHintingSuspendedWorld(1);

        // (a) OoT crossing host hints: gossip stones and loaded hints (including Song of Time)
        int probes = 0;
        int stones = 0;
        const int hintFailures = Rando_TestCheckLoadedCrossingHostHints(&probes, &stones);
        SPU_CHECK(hintFailures == 0 && probes > 0 && stones > 0,
                  "OoT crossing host hints failed: %d failures, %d probes, %d loaded hints\n", hintFailures, probes,
                  stones);

        // (a2) The Song of Time reveal names the crossed Sonata of Awakening, with its
        // article and " (MM)", and never the cover the check physically holds.
        char revealBuf[1024];
        char revealExpected[256];
        int revealNamesSongCheck = 0;
        int revealCoverNamed = 0;
        const int revealEnabled =
            Rando_TestOoTHintRevealText(revealBuf, (int)sizeof(revealBuf), revealExpected, (int)sizeof(revealExpected),
                                        &revealNamesSongCheck, &revealCoverNamed);
        printf("[TEST] song-pool: %s: Song of Time reveal (enabled %d): \"%s\", expected \"%s\"\n", seed, revealEnabled,
               revealBuf, revealExpected);
        SPU_CHECK(revealEnabled == 1 && revealNamesSongCheck == 1,
                  "%s: the Song of Time reveal is not enabled (%d) or does not hint the Song of Time check (%d)\n",
                  seed, revealEnabled, revealNamesSongCheck);
        SPU_CHECK(revealExpected[0] != '\0' && std::strstr(revealExpected, "Sonata of Awakening") != nullptr &&
                      std::strstr(revealBuf, revealExpected) != nullptr && revealCoverNamed == 0,
                  "%s: the Song of Time reveal reads \"%s\", not \"%s\" (cover named %d)\n", seed, revealBuf,
                  revealExpected, revealCoverNamed);

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
