/**
 * @file mm_placement_confinement_test.cpp
 * Locks for MM's key, boss key, stray fairy and song placement settings in a
 * paired world (#1502): the single bag's HOST GROUPS (combo_logic.h ABI 9) as MM's
 * engine answers them, the fold of the four options into the MM seed and the
 * profile identity, and the created world of a confined profile.
 *
 * Lives MM-side because 2Ship's confinement groups (Rando/Logic/
 * PlacementConstraints.cpp), Rando::StaticData and the live save's options need
 * MM's headers, which src/common must never acquire.
 *
 * ============================================================================
 * mm-placement-confinement (CTest MMPlacementConfinement, redship tier)
 * ============================================================================
 *
 *  (a) THE ENCODING AND THE WIRING. For each placement profile, the ROW masks come
 *      from MM_ComboLogic_PoolRowConfineMasks (the function RunPairedSingleBagFill
 *      hands Combo_SingleBag_Run) and the HOST masks from the registered MM
 *      engine's own `hostGroups` vtable entry after MM_ComboLogic_SetHostPool, the
 *      cache the coordinator reads. Over every item and every host of the pool,
 *      "the row is unconfined, or the host's bits meet the row's" must equal 2Ship's
 *      IsItemAllowedAtCheck. This does NOT re-prove 2Ship's grouping (both sides
 *      are built from its functions); it catches a wrong bit encoding (the shift,
 *      -1 read as a group, overlapping bits), a vtable slot left NULL or pointing
 *      elsewhere, a cache that drops or mislabels hosts, and a row mask computed
 *      from anything but 2Ship's item group.
 *  (b) THE DEFAULTS CONFINE NOTHING. With all four options at Anywhere no item
 *      has a mask, which is what keeps default worlds byte-identical (the bag then
 *      hands the fill no masks at all).
 *  (c) THE SHAPE. Under Own Dungeon each dungeon's key has exactly its dungeon's
 *      one bit and the four dungeons' bits differ; the Clock Town Stray Fairy has
 *      none (2Ship keeps it vanilla there); under Song Locations every
 *      song-location song has the song bit and a song check is in the song group
 *      only, and under Anywhere neither.
 *  (d) THE CACHE IS READ, NOT THE SAVE. After the pool is handed in, changing the
 *      live save's Shuffle Songs does not change a song check's answer, so a draw
 *      never reads a value that is not the creation's.
 *  (e) THE FOLD, pinned (critic of the design, #1502): with every option zero the
 *      seed string is exactly "0;" once per option that is NOT one of the seven
 *      appended ones, so an option dropped from Foreign.cpp's IsAppendedOption
 *      (which would fold its 0 positionally and move every default world) turns
 *      this red; each non-zero value of each of the four adds exactly
 *      "|<name>:<value>" to both the seed string and the identity string, and
 *      nothing else, so a positional fold or a skipped option is red too.
 *  (f) THE PATH-HINT SONG RULE (combo_woth.h Combo_Woth_ItemSkipsPathHint, OoTMM
 *      isLocationFullyShuffled with `songs: klass === 'path'`): an MM
 *      song-location song is no Way of the Hero candidate under Song Locations
 *      and Vanilla (red if the rule is missing or reads only one non-Anywhere
 *      value) and is one under Anywhere (red if songs are always skipped); a
 *      shared song recorded as its OoT half (an OoT host holds the pair's OoT id)
 *      gets the same answer as its MM half under every value (red if the rule asks
 *      the hosted half only); an OoT id no pair names, and an item that is not a
 *      song, are never skipped (red if the rule matches too widely). It does not
 *      cover Combo_Woth_DefaultEligible's call (the frozen pairs it passes).
 *  (g) THE PAIR'S ONE SONGS SETTING (#1542 D2; OoTMM `songs`). (g1) MM's paired
 *      Shuffle Songs is the mapping of OoT's key for each of OoT's four values,
 *      with MM's own key set to a value the mapping does not give (red if MM's
 *      key is read or the table drifts), and OoT's default when its key is unset;
 *      (g2) a capture wins over OoT's key (red if the completion could follow a
 *      key changed after the freeze); (g3) a stamp made under each captured value
 *      still matches once the capture is gone and OoT's key moved (red if a
 *      compare depends on the row), the values' stamps differ, and another
 *      option's change is still refused (red if the candidates accept any
 *      profile); (g4) unfrozen the row shows OoT's key mapped, frozen the value
 *      the world's stamp reproduces (red if the frozen row reads the key).
 *
 * ============================================================================
 * mm-placement-confined-world (CTest MMPlacementConfinedWorld, rando tier)
 * ============================================================================
 *
 *  A real paired generation with Small Keys, Boss Keys and Stray Fairies at Own
 *  Dungeon and Shuffle Songs at Song Locations (the confined golden's profile):
 *  the bag confined rows (the masks the last fill was handed are non-zero), every
 *  shuffled MM check holding an MM-confined item is in that item's group (keys,
 *  boss keys and fairies in their own dungeon, songs on MM song checks), at least
 *  one key and one song were checked, and no coordinator placement on an OoT
 *  host carries a confined MM item. Catches the masks not reaching the fill, the
 *  own pass placing a confined row, and a confined row crossing.
 *
 * Linkage: every entry point is extern "C" and declared in test_runner.cpp.
 */

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include <libultraship/bridge/consolevariablebridge.h>

#include "Rando/Rando.h"
#include "Rando/Types.h"
#include "Rando/Foreign.h"
#include "Rando/StaticData/StaticData.h"
#include "Rando/Logic/Logic.h"

// src/common — outside any extern "C" block; these headers manage their own.
#include "combo_logic.h"
#include "combo_mm_options_view.h" // MM_Rando_PairedSongsResolved / _Shown, the stamp and its compare (#1542)
#include "combo_single_bag.h"
#include "combo_song_pool.h" // #1542: the pair's one songs setting
#include "combo_woth.h"
#include "context.h" // gComboCtx.mmProfileDigest: the frozen-row leg (#1542)

extern "C" {
#include "variables.h"
}

extern "C" uint32_t MM_ComboLogic_HostGroupsFor(uint16_t check);
extern "C" const ComboLogicEngine* MM_ComboLogic_TestEngine(void);
extern "C" int MM_ComboLogic_PoolRowConfineMasks(const uint16_t* items, int count, uint32_t* outMasks);
extern "C" int MM_ComboLogic_TestLastPoolConfinement(uint16_t* outItems, uint32_t* outMasks, int cap);
extern "C" void MM_ComboLogic_SetHostPool(const uint16_t* checks, int count);
extern "C" void MM_ComboLogic_ForgetPlacements(void);
extern "C" void MM_ComboLogic_TestSaveHostPool(void);
extern "C" void MM_ComboLogic_TestRestoreHostPool(void);
// ComboLogicEngineOoT.cpp (#1542): the RsbsSongCheck index of an OoT check, or -1.
extern "C" int OoT_ComboLogic_TestSongCheckIndex(uint16_t rc);

namespace {

int sFailures = 0;

#define MPC_CHECK(cond, ...)                                     \
    do {                                                         \
        if (!(cond)) {                                           \
            printf("[TEST] FAIL (%s:%d): ", __FILE__, __LINE__); \
            printf(__VA_ARGS__);                                 \
            printf("\n");                                        \
            sFailures++;                                         \
        }                                                        \
    } while (0)

/** 2Ship's song group (PlacementConstraints.cpp CONFINEMENT_GROUP_SONGS). */
const uint32_t kSongBit = 1u << (DUNGEON_SCENE_INDEX_STONE_TOWER_TEMPLE_BOSS + 1);

/** The four options this lock is about, in the order the profiles set them. */
const RandoOptionId kPlacementOptions[4] = { RO_PLACEMENT_SMALL_KEYS, RO_PLACEMENT_BOSS_KEYS,
                                             RO_PLACEMENT_STRAY_FAIRIES, RO_SHUFFLE_SONGS };

/** Every option Foreign.cpp folds by NAME, only when neither 0 nor its default (IsAppendedOption):
 *  listed here independently, so dropping one there is a red count below. */
const RandoOptionId kAppendedOptions[] = {
    RO_SHUFFLE_TREE_DROPS,
    RO_SHOP_PRICES,
    RO_TINGLE_PRICES,
    RO_PLACEMENT_BOSS_KEYS,
    RO_PLACEMENT_SMALL_KEYS,
    RO_PLACEMENT_STRAY_FAIRIES,
    RO_SHUFFLE_SONGS,
    RO_SHUFFLE_WONDER_ITEMS,
    RO_SHUFFLE_HIVE_DROPS,
    RO_SHUFFLE_BUTTERFLIES,
    RO_SHUFFLE_SKELETON_KEY,
    RO_SKULLTULA_SHUFFLED,
    RO_STRAY_FAIRIES_MAX,
    RO_SHUFFLE_SONG_DOUBLE_TIME,
    RO_SHUFFLE_SONG_INVERTED_TIME,
    RO_SHUFFLE_SONG_SUN,
    RO_SHUFFLE_SONG_SARIA,
};

/** The four options and MM's host pool (with its cached groups) as the row found
 *  them; the legs replace the pool, so it is put back verbatim, not cleared. */
struct SavedOptions {
    uint32_t values[4];
    SavedOptions() {
        for (int i = 0; i < 4; ++i) {
            values[i] = RANDO_SAVE_OPTIONS[kPlacementOptions[i]];
        }
        MM_ComboLogic_TestSaveHostPool();
    }
    ~SavedOptions() {
        for (int i = 0; i < 4; ++i) {
            RANDO_SAVE_OPTIONS[kPlacementOptions[i]] = values[i];
        }
        MM_ComboLogic_TestRestoreHostPool();
    }
};

void SetPlacement(uint32_t smallKeys, uint32_t bossKeys, uint32_t fairies, uint32_t songs) {
    RANDO_SAVE_OPTIONS[RO_PLACEMENT_SMALL_KEYS] = smallKeys;
    RANDO_SAVE_OPTIONS[RO_PLACEMENT_BOSS_KEYS] = bossKeys;
    RANDO_SAVE_OPTIONS[RO_PLACEMENT_STRAY_FAIRIES] = fairies;
    RANDO_SAVE_OPTIONS[RO_SHUFFLE_SONGS] = songs;
}

/** Every item id with a StaticData row, below RI_MAX. */
std::vector<uint16_t> AllItems() {
    std::vector<uint16_t> out;
    for (const auto& [randoItemId, _] : Rando::StaticData::Items) {
        if (randoItemId != RI_UNKNOWN && randoItemId < RI_MAX) {
            out.push_back((uint16_t)randoItemId);
        }
    }
    return out;
}

/** Hand the engine every check as its pool and read back the hosts it kept (its
 *  `allLocations`, which is the pool minus nothing while the curated list is
 *  empty). */
std::vector<uint16_t> InstallWholePool(const ComboLogicEngine* engine) {
    std::vector<uint16_t> checks;
    for (const auto& [randoCheckId, _] : Rando::StaticData::Checks) {
        if (randoCheckId != RC_UNKNOWN) {
            checks.push_back((uint16_t)randoCheckId);
        }
    }
    MM_ComboLogic_SetHostPool(checks.data(), (int)checks.size());
    const int total = engine->allLocations(engine->self, nullptr, 0);
    std::vector<uint16_t> hosts((size_t)(total > 0 ? total : 0));
    if (total > 0) {
        engine->allLocations(engine->self, hosts.data(), total);
    }
    return hosts;
}

int PopCount(uint32_t v) {
    int n = 0;
    for (; v != 0u; v &= v - 1u) {
        n++;
    }
    return n;
}

/** Leg (a) for the profile currently in the live save. Returns confined rows. */
int CheckDifferential(const char* profile, const ComboLogicEngine* engine) {
    const std::vector<uint16_t> hosts = InstallWholePool(engine);
    const std::vector<uint16_t> items = AllItems();
    std::vector<uint32_t> masks(items.size(), 0u);
    const int confined = MM_ComboLogic_PoolRowConfineMasks(items.data(), (int)items.size(), masks.data());
    MPC_CHECK(!hosts.empty(), "%s: the engine kept no host of the whole pool", profile);
    int mismatches = 0;
    for (size_t i = 0; i < items.size(); ++i) {
        const uint32_t rowMask = masks[i];
        MPC_CHECK(PopCount(rowMask) <= 1, "%s: item %u has mask %08X, more than one group", profile, (unsigned)items[i],
                  (unsigned)rowMask);
        if (rowMask == 0u) {
            continue; // unconfined: 2Ship allows it everywhere, and so does the bag
        }
        for (const uint16_t host : hosts) {
            const uint32_t hostMask = engine->hostGroups(engine->self, host);
            const bool bag = (hostMask & rowMask) != 0u;
            const bool twoShip = Rando::Logic::IsItemAllowedAtCheck((RandoItemId)items[i], (RandoCheckId)host);
            if (bag != twoShip && mismatches++ < 5) {
                printf("[TEST]   %s: item %u at check %u: bag %d (host %08X, row %08X), 2Ship %d\n", profile,
                       (unsigned)items[i], (unsigned)host, bag ? 1 : 0, (unsigned)hostMask, (unsigned)rowMask,
                       twoShip ? 1 : 0);
            }
        }
    }
    printf("[TEST] (a) %s: %d confined item ids over %zu hosts, %d disagreement(s) with IsItemAllowedAtCheck\n",
           profile, confined, hosts.size(), mismatches);
    MPC_CHECK(mismatches == 0, "%s: the bag's groups disagree with 2Ship's IsItemAllowedAtCheck", profile);
    return confined;
}

uint32_t MaskOf(RandoItemId ri) {
    const uint16_t id = (uint16_t)ri;
    uint32_t mask = 0u;
    MM_ComboLogic_PoolRowConfineMasks(&id, 1, &mask);
    return mask;
}

/** A song check of the pool: the first RCTYPE_SONG check. */
uint16_t FirstSongCheck() {
    for (const auto& [randoCheckId, check] : Rando::StaticData::Checks) {
        if (randoCheckId != RC_UNKNOWN && check.randoCheckType == RCTYPE_SONG) {
            return (uint16_t)randoCheckId;
        }
    }
    return 0;
}

std::string RepeatZero(int n) {
    std::string s;
    for (int i = 0; i < n; ++i) {
        s += "0;";
    }
    return s;
}

} // namespace

extern "C" int MM_PlacementConfinement_RunHeadless(void) {
    printf("[TEST] mm-placement-confinement: MM's Own Dungeon and Song Locations reach the single bag as host groups "
           "that agree with 2Ship's rule, the defaults confine nothing, and the four options fold by name only when "
           "set (#1502)\n");
    sFailures = 0;
    // MM's OWN vtable, read directly: a stub row earlier in `--test all` may have
    // unregistered it from the coordinator. When it IS registered, it must be this one.
    const ComboLogicEngine* engine = MM_ComboLogic_TestEngine();
    const ComboLogicEngine* registered = Combo_Logic_GetEngine(GAME_MM);
    MPC_CHECK(registered == nullptr || registered == engine, "the coordinator's MM engine is not MM's own vtable");
    MPC_CHECK(engine != nullptr && engine->abiVersion == RSBS_COMBO_LOGIC_ENGINE_ABI && engine->hostGroups != nullptr &&
                  engine->allLocations != nullptr,
              "MM's engine does not carry ABI %u and a hostGroups entry", (unsigned)RSBS_COMBO_LOGIC_ENGINE_ABI);
    if (engine == nullptr || engine->hostGroups == nullptr || engine->allLocations == nullptr) {
        return 1;
    }
    const SavedOptions restore;

    // ---- (a) + (b): every profile, against 2Ship's own rule ------------------
    SetPlacement(RO_DUNGEON_ITEM_ANYWHERE, RO_DUNGEON_ITEM_ANYWHERE, RO_DUNGEON_ITEM_ANYWHERE,
                 RO_SONG_SHUFFLE_ANYWHERE);
    const int confinedDefault = CheckDifferential("defaults", engine);
    MPC_CHECK(confinedDefault == 0, "(b) the defaults confine %d item ids; default worlds would move", confinedDefault);

    SetPlacement(RO_DUNGEON_ITEM_OWN_DUNGEON, RO_DUNGEON_ITEM_OWN_DUNGEON, RO_DUNGEON_ITEM_OWN_DUNGEON,
                 RO_SONG_SHUFFLE_SONG_LOCATIONS);
    const int confinedAll = CheckDifferential("own dungeon + song locations", engine);
    MPC_CHECK(confinedAll > 0, "(a) the confined profile confines nothing");

    SetPlacement(RO_DUNGEON_ITEM_OWN_DUNGEON, RO_DUNGEON_ITEM_ANYWHERE, RO_DUNGEON_ITEM_VANILLA,
                 RO_SONG_SHUFFLE_ANYWHERE);
    (void)CheckDifferential("small keys own dungeon only", engine);
    MPC_CHECK(MaskOf(RI_WOODFALL_BOSS_KEY) == 0u && MaskOf(RI_WOODFALL_STRAY_FAIRY) == 0u,
              "(c) only the option set to Own Dungeon may confine its class");

    // ---- (c) the shape -------------------------------------------------------
    SetPlacement(RO_DUNGEON_ITEM_OWN_DUNGEON, RO_DUNGEON_ITEM_OWN_DUNGEON, RO_DUNGEON_ITEM_OWN_DUNGEON,
                 RO_SONG_SHUFFLE_SONG_LOCATIONS);
    const uint32_t wf = MaskOf(RI_WOODFALL_SMALL_KEY);
    const uint32_t sh = MaskOf(RI_SNOWHEAD_SMALL_KEY);
    const uint32_t gb = MaskOf(RI_GREAT_BAY_SMALL_KEY);
    const uint32_t st = MaskOf(RI_STONE_TOWER_SMALL_KEY);
    printf("[TEST] (c) key masks WF %08X SH %08X GB %08X ST %08X, WF boss key %08X fairy %08X, Clock Town fairy %08X\n",
           (unsigned)wf, (unsigned)sh, (unsigned)gb, (unsigned)st, (unsigned)MaskOf(RI_WOODFALL_BOSS_KEY),
           (unsigned)MaskOf(RI_WOODFALL_STRAY_FAIRY), (unsigned)MaskOf(RI_CLOCK_TOWN_STRAY_FAIRY));
    MPC_CHECK(wf == (1u << DUNGEON_SCENE_INDEX_WOODFALL_TEMPLE) && sh == (1u << DUNGEON_SCENE_INDEX_SNOWHEAD_TEMPLE) &&
                  gb == (1u << DUNGEON_SCENE_INDEX_GREAT_BAY_TEMPLE) &&
                  st == (1u << DUNGEON_SCENE_INDEX_STONE_TOWER_TEMPLE),
              "(c) each dungeon's small key must carry exactly its dungeon's bit");
    MPC_CHECK(MaskOf(RI_WOODFALL_BOSS_KEY) == wf && MaskOf(RI_WOODFALL_STRAY_FAIRY) == wf,
              "(c) a dungeon's boss key and fairies share its keys' group");
    MPC_CHECK(MaskOf(RI_CLOCK_TOWN_STRAY_FAIRY) == 0u,
              "(c) the Clock Town Stray Fairy has no dungeon: 2Ship keeps it vanilla under Own Dungeon");
    int songs = 0;
    int songsWrong = 0;
    for (const uint16_t id : AllItems()) {
        if (Rando::Logic::IsSongLocationItem((RandoItemId)id)) {
            songs++;
            songsWrong += (MaskOf((RandoItemId)id) != kSongBit) ? 1 : 0;
        }
    }
    const uint16_t songCheck = FirstSongCheck();
    InstallWholePool(engine);
    const uint32_t songCheckGroups = engine->hostGroups(engine->self, songCheck);
    printf("[TEST] (c) Song Locations: %d song-location songs, %d without the song bit; song check %u groups %08X\n",
           songs, songsWrong, (unsigned)songCheck, (unsigned)songCheckGroups);
    MPC_CHECK(songs > 0 && songsWrong == 0, "(c) every song-location song must carry the song bit");
    MPC_CHECK(songCheck != 0 && songCheckGroups == kSongBit, "(c) a song check must be in the song group only");

    // ---- (d) the cache, not the save -----------------------------------------
    RANDO_SAVE_OPTIONS[RO_SHUFFLE_SONGS] = RO_SONG_SHUFFLE_ANYWHERE;
    const uint32_t afterEdit = MM_ComboLogic_HostGroupsFor(songCheck);
    MPC_CHECK(afterEdit == kSongBit,
              "(d) the engine re-read the live save (song check now %08X): a draw must read "
              "the groups cached when the pool was handed in",
              (unsigned)afterEdit);
    InstallWholePool(engine);
    MPC_CHECK(MM_ComboLogic_HostGroupsFor(songCheck) == 0u && MaskOf(RI_SONG_SONATA) == 0u,
              "(c) under Anywhere a song check is in no group and no song is confined");
    MM_ComboLogic_SetHostPool(nullptr, 0);

    // ---- (e) the fold, pinned ------------------------------------------------
    {
        std::vector<uint32_t> zeros(RO_MAX, 0u);
        const std::string base = Rando::Foreign::TestFoldOptionsString(zeros.data());
        const std::string baseIdentity = Rando::Foreign::TestFoldIdentityString(zeros.data());
        const int appendedCount = (int)(sizeof(kAppendedOptions) / sizeof(kAppendedOptions[0]));
        const int positional = (int)Rando::StaticData::Options.size() - appendedCount;
        printf("[TEST] (e) %zu options, %d positional; zero seed string is %zu chars\n",
               Rando::StaticData::Options.size(), positional, base.size());
        MPC_CHECK(base == RepeatZero(positional),
                  "(e) with every option zero the seed string is not exactly %d positional zeros: an appended option "
                  "is folded by position (or a new one was appended without this list)",
                  positional);
        MPC_CHECK(baseIdentity.compare(0, base.size(), base) == 0,
                  "(e) the identity string must begin with the seed string's option fold");
        for (const RandoOptionId appended : kAppendedOptions) {
            MPC_CHECK(Rando::StaticData::Options.find(appended) != Rando::StaticData::Options.end(),
                      "(e) appended option %d has no StaticData row", (int)appended);
        }
        // #1403: a half persisted before an appended option existed holds 0 in its
        // slot, one created after holds the row default (30 and 15 for Shuffled Gold
        // Skulltulas and Stray Fairies in Pool). Both must fold to the same strings,
        // or every older paired half fails pair membership on load.
        {
            std::vector<uint32_t> defaults = zeros;
            for (const RandoOptionId appended : kAppendedOptions) {
                defaults[appended] = Rando::StaticData::Options[appended].defaultValue;
            }
            MPC_CHECK(Rando::Foreign::TestFoldOptionsString(defaults.data()) == base &&
                          Rando::Foreign::TestFoldIdentityString(defaults.data()) == baseIdentity,
                      "(e) appended options at their row defaults must fold exactly as a pre-#1403 half's zeros do");
            for (const RandoOptionId option : { RO_SKULLTULA_SHUFFLED, RO_STRAY_FAIRIES_MAX }) {
                std::vector<uint32_t> values = defaults;
                values[option] = 1;
                const std::string term = std::string("|") + Rando::StaticData::Options[option].name + ":1";
                MPC_CHECK(Rando::Foreign::TestFoldOptionsString(values.data()) == base + term,
                          "(e) %s=1 must add exactly '%s' to the seed string", Rando::StaticData::Options[option].name,
                          term.c_str());
            }
        }
        for (const RandoOptionId option : kPlacementOptions) {
            const auto it = Rando::StaticData::Options.find(option);
            if (it == Rando::StaticData::Options.end()) {
                continue;
            }
            for (uint32_t value = 1; value <= 2; ++value) {
                std::vector<uint32_t> values(RO_MAX, 0u);
                values[option] = value;
                const std::string term = std::string("|") + it->second.name + ":" + std::to_string(value);
                const std::string seed = Rando::Foreign::TestFoldOptionsString(values.data());
                const std::string identity = Rando::Foreign::TestFoldIdentityString(values.data());
                MPC_CHECK(seed == base + term, "(e) %s=%u must add exactly '%s' to the seed string", it->second.name,
                          (unsigned)value, term.c_str());
                MPC_CHECK(identity == base + term + baseIdentity.substr(base.size()),
                          "(e) %s=%u must add exactly '%s' to the identity string", it->second.name, (unsigned)value,
                          term.c_str());
            }
        }
    }

    // ---- (f) the path-hint song rule, on the item ----------------------------
    {
        uint16_t song = 0;
        for (const uint16_t id : AllItems()) {
            if (Rando::Logic::IsSongLocationItem((RandoItemId)id)) {
                song = id;
                break;
            }
        }
        MPC_CHECK(song != 0, "(f) no song-location song in StaticData");
        // The OoT ids are the test's own: only their pairing is under test.
        const ComboLogicSharedPair pairs[2] = { { 0x0F01, song }, { 0x0F02, (uint16_t)RI_WOODFALL_SMALL_KEY } };
        SharedItem mmSong;
        memset(&mmSong, 0, sizeof(mmSong));
        mmSong.originGame = (uint8_t)GAME_MM;
        mmSong.id = song;
        SharedItem ootHalf = mmSong;
        ootHalf.originGame = (uint8_t)GAME_OOT;
        ootHalf.id = 0x0F01;
        SharedItem ootUnpaired = ootHalf;
        ootUnpaired.id = 0x0F03;
        SharedItem ootKeyHalf = ootHalf;
        ootKeyHalf.id = 0x0F02;
        SharedItem mmKey = mmSong;
        mmKey.id = (uint16_t)RI_WOODFALL_SMALL_KEY;
        const uint32_t values[3] = { RO_SONG_SHUFFLE_SONG_LOCATIONS, RO_SONG_SHUFFLE_VANILLA,
                                     RO_SONG_SHUFFLE_ANYWHERE };
        for (const uint32_t value : values) {
            RANDO_SAVE_OPTIONS[RO_SHUFFLE_SONGS] = value;
            const bool want = value != RO_SONG_SHUFFLE_ANYWHERE;
            const bool mmAnswer = Combo_Woth_ItemSkipsPathHint(mmSong, pairs, 2);
            const bool ootAnswer = Combo_Woth_ItemSkipsPathHint(ootHalf, pairs, 2);
            printf("[TEST] (f) Shuffle Songs %u: MM song %u skipped %d, its OoT half skipped %d\n", (unsigned)value,
                   (unsigned)song, mmAnswer ? 1 : 0, ootAnswer ? 1 : 0);
            MPC_CHECK(mmAnswer == want, "(f) Shuffle Songs %u: the MM song's path-hint answer is %d, want %d",
                      (unsigned)value, mmAnswer ? 1 : 0, want ? 1 : 0);
            MPC_CHECK(ootAnswer == mmAnswer,
                      "(f) Shuffle Songs %u: a shared song on an OoT host answers %d, on an MM host %d: the rule "
                      "must read the item, not the hosted half",
                      (unsigned)value, ootAnswer ? 1 : 0, mmAnswer ? 1 : 0);
            MPC_CHECK(!Combo_Woth_ItemSkipsPathHint(ootUnpaired, pairs, 2) &&
                          !Combo_Woth_ItemSkipsPathHint(ootHalf, nullptr, 0),
                      "(f) Shuffle Songs %u: an OoT item no pair names was skipped", (unsigned)value);
            MPC_CHECK(!Combo_Woth_ItemSkipsPathHint(mmKey, pairs, 2) &&
                          !Combo_Woth_ItemSkipsPathHint(ootKeyHalf, pairs, 2),
                      "(f) Shuffle Songs %u: a non-song item was skipped by the song rule", (unsigned)value);
        }
    }

    // ---- (g) #1542: the pair's one songs setting ------------------------------
    {
        // The two keys, the capture and the frozen digest as the row found them.
        const char* const ootKey = RSBS_OOT_SHUFFLE_SONGS_CVAR;
        const char* const mmKey = Rando::StaticData::Options[RO_SHUFFLE_SONGS].cvar;
        const char* const cowsKey = Rando::StaticData::Options[RO_SHUFFLE_COWS].cvar;
        struct SongsRestore {
            const char* oot;
            const char* mm;
            const char* cows;
            bool ootSet;
            bool mmSet;
            bool cowsSet;
            int32_t ootValue;
            int32_t mmValue;
            int32_t cowsValue;
            int16_t capture;
            uint32_t digest;
            ~SongsRestore() {
                Put(oot, ootSet, ootValue);
                Put(mm, mmSet, mmValue);
                Put(cows, cowsSet, cowsValue);
                Combo_PairSongsRestore(capture);
                gComboCtx.mmProfileDigest = digest;
            }
            static void Put(const char* key, bool set, int32_t value) {
                if (set) {
                    CVarSetInteger(key, value);
                } else {
                    CVarClear(key);
                }
            }
        } songsRestore{ ootKey,
                        mmKey,
                        cowsKey,
                        Combo_CVarIsExplicitInt(ootKey),
                        Combo_CVarIsExplicitInt(mmKey),
                        Combo_CVarIsExplicitInt(cowsKey),
                        CVarGetInteger(ootKey, 0),
                        CVarGetInteger(mmKey, 0),
                        CVarGetInteger(cowsKey, 0),
                        Combo_PairSongsSnapshot(),
                        gComboCtx.mmProfileDigest };
        gComboCtx.mmProfileDigest = 0; // unfrozen: the next creation's resolution
        Combo_PairSongsClear();

        // (g1) THE MAPPING, spelled here independently of combo_song_pool.cpp: OoT
        // Off, Song Locations, Dungeon Rewards, Anywhere -> MM Vanilla, Song
        // Locations, Song Locations, Anywhere. MM's own key is set to a value
        // the mapping does NOT give, so a resolution that reads it is red.
        static const uint32_t kWant[4] = { RO_SONG_SHUFFLE_VANILLA, RO_SONG_SHUFFLE_SONG_LOCATIONS,
                                           RO_SONG_SHUFFLE_SONG_LOCATIONS, RO_SONG_SHUFFLE_ANYWHERE };
        for (uint32_t oot = 0; oot < 4; ++oot) {
            const uint32_t conflicting = (kWant[oot] + 1u) % 3u;
            CVarSetInteger(ootKey, (int32_t)oot);
            CVarSetInteger(mmKey, (int32_t)conflicting);
            const int resolved = MM_Rando_PairedSongsResolved();
            printf("[TEST] (g1) OoT Shuffle Songs %u, MM key %u: MM resolves %d (want %u)\n", (unsigned)oot,
                   (unsigned)conflicting, resolved, (unsigned)kWant[oot]);
            MPC_CHECK(resolved == (int)kWant[oot] && Combo_PairSongsMmValue(oot) == kWant[oot],
                      "(g1) OoT Shuffle Songs %u: MM's paired Shuffle Songs is %d, want %u (MM's own key must not be "
                      "read, and the mapping must be OoTMM's one setting)",
                      (unsigned)oot, resolved, (unsigned)kWant[oot]);
        }
        // An unset OoT key is OoT's shipped default, Song Locations: a default
        // paired world confines MM's songs to MM's song checks.
        CVarClear(ootKey);
        MPC_CHECK(MM_Rando_PairedSongsResolved() == (int)RO_SONG_SHUFFLE_SONG_LOCATIONS,
                  "(g1) with OoT's key unset MM resolves %d, not Song Locations (OoT's default)",
                  MM_Rando_PairedSongsResolved());

        // (g2) THE CAPTURE WINS over OoT's key: between the freeze and the
        // completion MM's half follows OoT's frozen value, never a key changed
        // since (red if the resolution skips the capture: the F2 hazard).
        CVarSetInteger(ootKey, (int32_t)RSBS_OOT_SONGS_SONG_LOCATIONS);
        Combo_PairSongsCapture(RSBS_OOT_SONGS_ANYWHERE);
        MPC_CHECK(MM_Rando_PairedSongsResolved() == (int)RO_SONG_SHUFFLE_ANYWHERE,
                  "(g2) with Anywhere captured and OoT's key at Song Locations MM resolves %d, not Anywhere",
                  MM_Rando_PairedSongsResolved());
        Combo_PairSongsCapture(RSBS_OOT_SONGS_OFF);
        MPC_CHECK(MM_Rando_PairedSongsResolved() == (int)RO_SONG_SHUFFLE_VANILLA,
                  "(g2) with Off captured MM resolves %d, not Vanilla", MM_Rando_PairedSongsResolved());

        // (g3) THE COMPARES DO NOT DEPEND ON THE ROW. A stamp made with each OoT
        // value captured still matches once the capture is gone and OoT's key
        // says something else (red if the arrival, the load classification or
        // the restore's after-check compares one MM Shuffle Songs value only);
        // the stamps of different MM values differ (the row is in the identity);
        // and a stamp is still refused for another option's change (red if the
        // candidates accept any profile).
        uint32_t stamps[4] = {};
        for (uint32_t oot = 0; oot < 4; ++oot) {
            Combo_PairSongsCapture(oot);
            stamps[oot] = MM_Rando_ComputeProfileStamp();
            Combo_PairSongsClear();
            const uint32_t moved = oot == RSBS_OOT_SONGS_ANYWHERE ? RSBS_OOT_SONGS_OFF : RSBS_OOT_SONGS_ANYWHERE;
            CVarSetInteger(ootKey, (int32_t)moved);
            MPC_CHECK(MM_Rando_ProfileStampMatches(stamps[oot]) == 1,
                      "(g3) the stamp %08X made with OoT Shuffle Songs %u captured does not match once OoT's key "
                      "is %u: the MM compare depends on the songs row",
                      (unsigned)stamps[oot], (unsigned)oot, (unsigned)moved);
        }
        MPC_CHECK(stamps[RSBS_OOT_SONGS_OFF] != stamps[RSBS_OOT_SONGS_SONG_LOCATIONS] &&
                      stamps[RSBS_OOT_SONGS_SONG_LOCATIONS] != stamps[RSBS_OOT_SONGS_ANYWHERE] &&
                      stamps[RSBS_OOT_SONGS_OFF] != stamps[RSBS_OOT_SONGS_ANYWHERE],
                  "(g3) MM's three Shuffle Songs values gave stamps %08X %08X %08X: the row must be in the identity",
                  (unsigned)stamps[RSBS_OOT_SONGS_OFF], (unsigned)stamps[RSBS_OOT_SONGS_SONG_LOCATIONS],
                  (unsigned)stamps[RSBS_OOT_SONGS_ANYWHERE]);
        MPC_CHECK(stamps[RSBS_OOT_SONGS_SONG_LOCATIONS] == stamps[RSBS_OOT_SONGS_DUNGEON_REWARDS],
                  "(g3) OoT Song Locations and Dungeon Rewards both give MM Song Locations, so one stamp");
        const int32_t cowsNow = CVarGetInteger(cowsKey, Rando::StaticData::Options[RO_SHUFFLE_COWS].defaultValue);
        CVarSetInteger(cowsKey, cowsNow != 0 ? 0 : 1);
        MPC_CHECK(MM_Rando_ProfileStampMatches(stamps[RSBS_OOT_SONGS_SONG_LOCATIONS]) == 0,
                  "(g3) a stamp matched after Shuffle Cows changed: the songs candidates accept another profile");
        SongsRestore::Put(cowsKey, songsRestore.cowsSet, songsRestore.cowsValue);

        // (g4) THE ROW SHOWS THE WORLD'S VALUE. Unfrozen it shows the next
        // creation's (OoT's key mapped); frozen, the value the world's stamp
        // reproduces, whatever OoT's key says now (red if the frozen row reads
        // the key).
        CVarSetInteger(ootKey, (int32_t)RSBS_OOT_SONGS_OFF);
        MPC_CHECK(MM_Rando_PairedSongsShown() == (int)RO_SONG_SHUFFLE_VANILLA,
                  "(g4) unfrozen with OoT Off the row shows %d, not Vanilla", MM_Rando_PairedSongsShown());
        gComboCtx.mmProfileDigest = stamps[RSBS_OOT_SONGS_ANYWHERE];
        CVarSetInteger(ootKey, (int32_t)RSBS_OOT_SONGS_SONG_LOCATIONS);
        MPC_CHECK(MM_Rando_PairedSongsShown() == (int)RO_SONG_SHUFFLE_ANYWHERE,
                  "(g4) a world frozen with MM Anywhere shows %d once OoT's key is Song Locations",
                  MM_Rando_PairedSongsShown());
        gComboCtx.mmProfileDigest = 0;

        // (g5) THE FROZEN ROW'S TWO OTHER SOURCES (#1542 review). (g5a) A digest
        // no candidate reproduces now (stamped with Shuffle Cows flipped) shows
        // the fallback, and once the key the world was made with is back (as the
        // load restore puts it back) the next draw finds the world's value: red if
        // a miss is kept past a change of the inputs the search reads. (g5b) With
        // MM running this pair's randomizer half, the row shows the half's own
        // frozen Shuffle Songs although no candidate reproduces the digest: red if
        // the resident half's option is not read before the fallback, or is read
        // for a half another pair's seed made.
        {
            const int32_t cowsBase = CVarGetInteger(cowsKey, Rando::StaticData::Options[RO_SHUFFLE_COWS].defaultValue);
            CVarSetInteger(cowsKey, cowsBase != 0 ? 0 : 1);
            Combo_PairSongsCapture(RSBS_OOT_SONGS_ANYWHERE);
            const uint32_t flippedStamp = MM_Rando_ComputeProfileStamp();
            Combo_PairSongsClear();
            SongsRestore::Put(cowsKey, songsRestore.cowsSet, songsRestore.cowsValue);
            CVarSetInteger(ootKey, (int32_t)RSBS_OOT_SONGS_SONG_LOCATIONS);
            gComboCtx.mmProfileDigest = flippedStamp;
            const int missShown = MM_Rando_PairedSongsShown();
            const int missAgain = MM_Rando_PairedSongsShown();
            CVarSetInteger(cowsKey, cowsBase != 0 ? 0 : 1);
            const int refreshed = MM_Rando_PairedSongsShown();
            SongsRestore::Put(cowsKey, songsRestore.cowsSet, songsRestore.cowsValue);
            printf(
                "[TEST] (g5a) unmatched digest %08X shows %d then %d; with the world's Shuffle Cows back it shows %d "
                "(want Anywhere %d)\n",
                (unsigned)flippedStamp, missShown, missAgain, refreshed, (int)RO_SONG_SHUFFLE_ANYWHERE);
            MPC_CHECK(missShown == (int)RO_SONG_SHUFFLE_SONG_LOCATIONS && missAgain == missShown,
                      "(g5a) an unmatched frozen digest shows %d then %d, not the fallback (Song Locations) twice",
                      missShown, missAgain);
            MPC_CHECK(refreshed == (int)RO_SONG_SHUFFLE_ANYWHERE,
                      "(g5a) with the world's keys back the frozen row shows %d, not the world's Anywhere: a miss was "
                      "kept past a change of the search's inputs",
                      refreshed);

            // (g5b) A placeholder digest no candidate reproduces, and MM's live
            // save made a randomizer half of this pair with Shuffle Songs Vanilla.
            const GameId gameBefore = Context_GetCurrentGame();
            const SaveType typeBefore = gSaveContext.save.shipSaveInfo.saveType;
            const uint32_t finalBefore = gSaveContext.save.shipSaveInfo.rando.finalSeed;
            const uint32_t songsBefore = RANDO_SAVE_OPTIONS[RO_SHUFFLE_SONGS];
            const uint32_t masterBefore = gComboCtx.sharedRandoSeed;
            const uint32_t attemptBefore = gComboCtx.mmPairedAttempt;
            gComboCtx.mmProfileDigest = 0x0BADF00Du;
            gComboCtx.sharedRandoSeed = 0x1542u;
            gComboCtx.mmPairedAttempt = 1u; // the recorded rung: attempt 0
            RANDO_SAVE_OPTIONS[RO_SHUFFLE_SONGS] = RO_SONG_SHUFFLE_VANILLA;
            gSaveContext.save.shipSaveInfo.saveType = SAVETYPE_RANDO;
            gSaveContext.save.shipSaveInfo.rando.finalSeed =
                Rando::Foreign::MixPairedFinalSeedFromOptions(gComboCtx.sharedRandoSeed, RANDO_SAVE_OPTIONS, 0u);
            Context_SetCurrentGame(GAME_OOT);
            const int ootRunning = MM_Rando_PairedSongsShown();
            Context_SetCurrentGame(GAME_MM);
            const int resident = MM_Rando_PairedSongsShown();
            gSaveContext.save.shipSaveInfo.rando.finalSeed ^= 1u;
            const int otherPair = MM_Rando_PairedSongsShown();
            printf("[TEST] (g5b) MM half of this pair (Vanilla) resident: shows %d (OoT running %d, another pair's "
                   "half %d)\n",
                   resident, ootRunning, otherPair);
            MPC_CHECK(resident == (int)RO_SONG_SHUFFLE_VANILLA,
                      "(g5b) with this pair's MM half resident the frozen row shows %d, not the half's Vanilla",
                      resident);
            MPC_CHECK(ootRunning == (int)RO_SONG_SHUFFLE_SONG_LOCATIONS &&
                          otherPair == (int)RO_SONG_SHUFFLE_SONG_LOCATIONS,
                      "(g5b) the row read MM's save while OoT runs (%d) or a half another pair made (%d)", ootRunning,
                      otherPair);
            Context_SetCurrentGame(gameBefore);
            gSaveContext.save.shipSaveInfo.saveType = typeBefore;
            gSaveContext.save.shipSaveInfo.rando.finalSeed = finalBefore;
            RANDO_SAVE_OPTIONS[RO_SHUFFLE_SONGS] = songsBefore;
            gComboCtx.sharedRandoSeed = masterBefore;
            gComboCtx.mmPairedAttempt = attemptBefore;
            gComboCtx.mmProfileDigest = 0;
        }
        printf("[TEST] (g) the paired songs row follows OoT's (capture first), the compares try every value, and "
               "the frozen row shows the world's (stamps Off %08X, Song Locations %08X, Anywhere %08X)\n",
               (unsigned)stamps[RSBS_OOT_SONGS_OFF], (unsigned)stamps[RSBS_OOT_SONGS_SONG_LOCATIONS],
               (unsigned)stamps[RSBS_OOT_SONGS_ANYWHERE]);
    }

    printf("[TEST] mm-placement-confinement: %s\n", sFailures == 0 ? "PASS" : "FAIL");
    return sFailures == 0 ? 0 : 1;
}

/**
 * The rando row's world check, called by test_runner.cpp after a real paired
 * generation of the confined profile (MM's live save holds the created world).
 * @return 0 when every confined placement is in its group and none crossed.
 */
extern "C" int MM_PlacementConfinedWorld_Check(void) {
    sFailures = 0;
    const int total = MM_ComboLogic_TestLastPoolConfinement(nullptr, nullptr, 0);
    std::vector<uint16_t> items((size_t)(total > 0 ? total : 1), 0);
    std::vector<uint32_t> masks((size_t)(total > 0 ? total : 1), 0u);
    MM_ComboLogic_TestLastPoolConfinement(items.data(), masks.data(), total);
    int confinedRows = 0;
    for (int i = 0; i < total; ++i) {
        confinedRows += (masks[(size_t)i] != 0u) ? 1 : 0;
    }
    const ComboSingleBagReport* report = Combo_SingleBag_LastReport();
    printf("[TEST] mm-placement-confined-world: %d of %d MM pool rows confined, %d bag rows confined, fill %s\n",
           confinedRows, total, report->confinedRows, Combo_Logic_StatusName(report->status));
    MPC_CHECK(confinedRows > 0, "the last paired fill was handed no confined row: the options never reached the bag");
    MPC_CHECK(report->confinedRows > 0, "the bag carried no confined row into the fill");

    int checked = 0;
    int keys = 0;
    int songs = 0;
    int wrong = 0;
    for (int rc = 0; rc < (int)RC_MAX; ++rc) {
        const RandoSaveCheck& check = RANDO_SAVE_CHECKS[rc];
        if (!check.shuffled || check.skipped) {
            continue;
        }
        const int group = Rando::Logic::RandoItemIdToConfinementGroup(check.randoItemId);
        if (group < 0) {
            continue;
        }
        checked++;
        const RandoItemType type = Rando::StaticData::Items[check.randoItemId].randoItemType;
        keys += (type == RITYPE_SMALL_KEY || type == RITYPE_BOSS_KEY) ? 1 : 0;
        songs += Rando::Logic::IsSongLocationItem(check.randoItemId) ? 1 : 0;
        if (Rando::Logic::CheckIdToConfinementGroup((RandoCheckId)rc) != group) {
            if (wrong++ < 5) {
                printf("[TEST]   item %u (group %d) on check %d (group %d)\n", (unsigned)check.randoItemId, group, rc,
                       Rando::Logic::CheckIdToConfinementGroup((RandoCheckId)rc));
            }
        }
    }
    printf("[TEST] mm-placement-confined-world: %d confined placements checked (%d keys, %d songs), %d outside their "
           "group\n",
           checked, keys, songs, wrong);
    MPC_CHECK(keys > 0 && songs > 0, "the world holds no confined key or no confined song: nothing was checked");
    MPC_CHECK(wrong == 0, "%d confined MM item(s) were placed outside their group", wrong);

    // A LOCALLY confined MM item never crosses. Under the shared song pool
    // (#1542) an MM song carries the combo-wide song group instead and may sit on
    // an OoT song check, but on no other OoT check.
    int crossed = 0;
    int songsCrossed = 0;
    int songsOffSongChecks = 0;
    for (int i = 0; i < Combo_Logic_PlacementCount(GAME_OOT); ++i) {
        ComboLogicPlacement p;
        if (!Combo_Logic_PlacementAt(GAME_OOT, i, &p) || p.item.originGame != (uint8_t)GAME_MM) {
            continue;
        }
        uint32_t mask = 0u;
        MM_ComboLogic_PoolRowConfineMasks(&p.item.id, 1, &mask);
        if ((mask & RSBS_COMBO_GROUP_WIDE_MASK) != 0u) {
            songsCrossed++;
            songsOffSongChecks += (OoT_ComboLogic_TestSongCheckIndex(p.hostCheck) < 0) ? 1 : 0;
        } else {
            crossed += (mask != 0u) ? 1 : 0;
        }
    }
    printf("[TEST] mm-placement-confined-world: %d MM song(s) on OoT checks, %d of them off a song check\n",
           songsCrossed, songsOffSongChecks);
    MPC_CHECK(crossed == 0, "%d locally confined MM item(s) crossed onto OoT hosts", crossed);
    MPC_CHECK(songsOffSongChecks == 0, "%d MM song(s) crossed onto an OoT check that is no song check",
              songsOffSongChecks);
    printf("[TEST] mm-placement-confined-world: %s\n", sFailures == 0 ? "PASS" : "FAIL");
    return sFailures == 0 ? 0 : 1;
}

/** Every MM check, shuffled or not, holding MM item `id`: what the player finds,
 *  vanilla checks included (#1502's Shuffle Songs Vanilla shared-song lock). */
extern "C" int MM_PlacementConfinement_WorldCopies(uint16_t id) {
    int copies = 0;
    for (int rc = 1; rc < (int)RC_MAX; ++rc) {
        const RandoSaveCheck& check = RANDO_SAVE_CHECKS[rc];
        if (check.skipped || (uint16_t)check.randoItemId != id) {
            continue;
        }
        copies++;
    }
    return copies;
}

/** The live MM save's frozen Shuffle Songs (RANDO_SAVE_OPTIONS), what a generated
 *  half was made with (#1542: the pair's one songs setting). */
extern "C" int MM_PlacementConfinement_FrozenSongs(void) {
    return (int)RANDO_SAVE_OPTIONS[RO_SHUFFLE_SONGS];
}
