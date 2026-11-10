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
#include "combo_single_bag.h"

extern "C" {
#include "variables.h"
}

extern "C" uint32_t MM_ComboLogic_HostGroupsFor(uint16_t check);
extern "C" const ComboLogicEngine* MM_ComboLogic_TestEngine(void);
extern "C" int MM_ComboLogic_PoolRowConfineMasks(const uint16_t* items, int count, uint32_t* outMasks);
extern "C" int MM_ComboLogic_TestLastPoolConfinement(uint16_t* outItems, uint32_t* outMasks, int cap);
extern "C" void MM_ComboLogic_SetHostPool(const uint16_t* checks, int count);
extern "C" void MM_ComboLogic_ForgetPlacements(void);

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

/** Every option Foreign.cpp folds by NAME, only when non-zero (IsAppendedOption):
 *  listed here independently, so dropping one there is a red count below. */
const RandoOptionId kAppendedOptions[7] = { RO_SHUFFLE_TREE_DROPS,  RO_SHOP_PRICES,          RO_TINGLE_PRICES,
                                            RO_PLACEMENT_BOSS_KEYS, RO_PLACEMENT_SMALL_KEYS, RO_PLACEMENT_STRAY_FAIRIES,
                                            RO_SHUFFLE_SONGS };

struct SavedOptions {
    uint32_t values[4];
    SavedOptions() {
        for (int i = 0; i < 4; ++i) {
            values[i] = RANDO_SAVE_OPTIONS[kPlacementOptions[i]];
        }
    }
    ~SavedOptions() {
        for (int i = 0; i < 4; ++i) {
            RANDO_SAVE_OPTIONS[kPlacementOptions[i]] = values[i];
        }
        MM_ComboLogic_SetHostPool(nullptr, 0);
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
        const int positional = (int)Rando::StaticData::Options.size() - 7;
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

    int crossed = 0;
    for (int i = 0; i < Combo_Logic_PlacementCount(GAME_OOT); ++i) {
        ComboLogicPlacement p;
        if (!Combo_Logic_PlacementAt(GAME_OOT, i, &p) || p.item.originGame != (uint8_t)GAME_MM) {
            continue;
        }
        uint32_t mask = 0u;
        MM_ComboLogic_PoolRowConfineMasks(&p.item.id, 1, &mask);
        crossed += (mask != 0u) ? 1 : 0;
    }
    MPC_CHECK(crossed == 0, "%d confined MM item(s) crossed onto OoT hosts", crossed);
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
