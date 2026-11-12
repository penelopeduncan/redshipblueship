#include "Logic.h"
#include "Rando/MiscBehavior/ClockShuffle.h"
#include <libultraship/bridge/consolevariablebridge.h>
#include <algorithm>
#include <sstream>

extern "C" {
#include "variables.h"
#include "ShipUtils.h"
}

#ifdef RSBS_SINGLE_EXECUTABLE
#include "Rando/Foreign.h" // #1369: ShuffleSwimForWorld / ShuffleOcarinaButtonsForWorld
#include "ShopPrices.h"    // #1427: the shop and Tingle price modes
#include "Rando/OoTMasksMM.h" // #980: the OoT masks' frozen rules
#include "combo_single_bag.h" // #1502: the shared pairs a vanilla song check is released for
#include "foreign_items.h"    // #1502: Combo_ForeignPairingActive
#include "Rando/WarpSongsMM.h" // #1169 XW5: the warp songs' frozen rules
#include "combo_song_pool.h"   // #1542: the shared song pool's MM half
// RedShipBlueShip (#645 increment 3, lane K9): where RO_PLENTIFUL_ITEMS' copies
// sit in the last `itemPool` this function returned — [from, to), both 0 when
// plentiful added none — and that pool's final size, so a consumer can tell it is
// marking the pool this record describes. RECORDED around the step itself, never
// re-derived; since 2Ship bfcb60476 (#1398) the step has no room test and draws
// no random number: it appends one copy of every major, mask and key and of
// every lesser item except maps, compasses and Tingle maps. Read by
// MM_ComboLogic_MarkPoolRows (ComboLogicEngineSingleExe.cpp).
size_t gRsbsComboPlentifulFrom = 0;
size_t gRsbsComboPlentifulTo = 0;
size_t gRsbsComboPoolSize = 0;

// RedShipBlueShip #1499: the six Tingle maps. Owning one turns every other copy
// into junk (ConvertItem: its WEEKEVENTREG_TINGLE_MAP_BOUGHT_* flag is set), and
// the twelve Tingle checks carry each map twice (StaticData/Checks.cpp).
static bool RsbsIsTingleMap(RandoItemId randoItemId) {
    switch (randoItemId) {
        case RI_TINGLE_MAP_CLOCK_TOWN:
        case RI_TINGLE_MAP_GREAT_BAY:
        case RI_TINGLE_MAP_ROMANI_RANCH:
        case RI_TINGLE_MAP_SNOWHEAD:
        case RI_TINGLE_MAP_STONE_TOWER:
        case RI_TINGLE_MAP_WOODFALL:
            return true;
        default:
            return false;
    }
}

// RedShipBlueShip #1502 (the MM mirror of OoT's #1085 RsbsReleaseVanillaSharedSongs,
// fill.cpp): under Shuffle Songs Vanilla, a song whose shared rule is frozen on in
// a paired world is the single bag's ONE shared row, so MM's copy must not also
// stay on its vanilla check. The check stays an ordinary host and the song a pool
// row, which the composer pairs (and trims when OoT's half is the row).
static bool RsbsReleasesVanillaSharedSong(RandoItemId randoItemId) {
    if (!Combo_ForeignPairingActive()) {
        return false;
    }
    ComboLogicSharedPair pairs[RSBS_XITEM_BIT_COUNT];
    const int n = Combo_SingleBag_SharedPairs(pairs, (int)RSBS_XITEM_BIT_COUNT);
    for (int i = 0; i < n; ++i) {
        if (pairs[i].mmId == (uint16_t)randoItemId) {
            return true;
        }
    }
    return false;
}

// RedShipBlueShip #1542: the shared song pool for THIS save's options (the one
// MM_ComboLogic_SongPoolUnionLive reads at the paired creation, where `saveInfo` is
// the live save OnFileCreate passes): OoT's union and MM's own songs value at
// Song Locations.
static bool RsbsSongPoolUnion(const RandoSaveInfo& saveInfo) {
    return OoT_ComboLogic_SongPoolUnion() &&
           saveInfo.randoSaveOptions[RO_SHUFFLE_SONGS] == RO_SONG_SHUFFLE_SONG_LOCATIONS;
}

// RedShipBlueShip #1499: one copy of each Tingle map in the pool, and none of a
// map the file starts with (Starting Maps and Compasses grants all six). Every
// other copy becomes RI_JUNK in place, so the pool keeps its size and order.
static void RsbsKeepOneOfEachTingleMap(const std::vector<RandoItemId>& startingItems,
                                       std::vector<RandoItemId>& itemPool) {
    std::vector<RandoItemId> kept;
    for (RandoItemId& item : itemPool) {
        if (!RsbsIsTingleMap(item)) {
            continue;
        }
        const bool started = std::find(startingItems.begin(), startingItems.end(), item) != startingItems.end();
        if (started || std::find(kept.begin(), kept.end(), item) != kept.end()) {
            item = RI_JUNK;
        } else {
            kept.push_back(item);
        }
    }
}
#endif

namespace Rando {

namespace Logic {

void GeneratePools(RandoSaveInfo& saveInfo, std::vector<RandoCheckId>& checkPool, std::vector<RandoItemId>& itemPool) {
    std::vector<RandoItemId> startingItems = Rando::GetStartingItemsFromSave(saveInfo);
    std::vector<RandoItemId> computedStartingItems = Rando::GetComputedStartingItems(saveInfo);
    startingItems.insert(startingItems.end(), computedStartingItems.begin(), computedStartingItems.end());

    std::vector<RandoCheckId> excludedChecks;
    std::string excludedChecksList = CVarGetString("gRando.ExcludedChecks", "");
    std::string word;
    std::istringstream stream(excludedChecksList);
    while (std::getline(stream, word, ',')) {
        excludedChecks.push_back((RandoCheckId)std::stoi(word));
    }

    std::set<RandoCheckId> vanillaSkulltulas;
    if (saveInfo.randoSaveOptions[RO_SHUFFLE_GOLD_SKULLTULAS] == RO_GENERIC_YES) {
        std::map<SceneId, std::vector<RandoCheckId>> skulltulasByScene;
        for (auto& [randoCheckId, randoStaticCheck] : Rando::StaticData::Checks) {
            if (randoStaticCheck.randoCheckType == RCTYPE_SKULL_TOKEN) {
                skulltulasByScene[randoStaticCheck.sceneId].push_back(randoCheckId);
            }
        }

        size_t shuffledCount = saveInfo.randoSaveOptions[RO_SKULLTULA_SHUFFLED];
        for (auto& [sceneId, sceneSkulltulas] : skulltulasByScene) {
            if (shuffledCount >= sceneSkulltulas.size()) {
                continue;
            }

            for (size_t i = 0; i < sceneSkulltulas.size(); i++) {
                std::swap(sceneSkulltulas[i], sceneSkulltulas[Ship_Random(0, sceneSkulltulas.size())]);
            }
            vanillaSkulltulas.insert(sceneSkulltulas.begin() + shuffledCount, sceneSkulltulas.end());
        }
    }

    // First loop through all regions and add checks/items to the pool
    for (auto& [randoRegionId, randoRegion] : Rando::Logic::Regions) {
        for (auto& [randoCheckId, _] : randoRegion.checks) {
            auto& randoStaticCheck = Rando::StaticData::Checks[randoCheckId];

            // Initialize the check with it's vanilla item
            if (randoStaticCheck.randoCheckId != RC_UNKNOWN) {
                saveInfo.randoSaveChecks[randoCheckId].randoItemId = randoStaticCheck.randoItemId;
            }

            // Skip checks that are already in the pool
            if (std::find(checkPool.begin(), checkPool.end(), randoCheckId) != checkPool.end()) {
                continue;
            }

            // TODO: We may never shuffle these 2 pots, leaving this decision for later
            if (randoStaticCheck.sceneId == SCENE_LAST_BS) {
                continue;
            }

            // dungeon items
            if (StaysAtVanillaCheck(randoStaticCheck.randoItemId, saveInfo)) {
                saveInfo.randoSaveChecks[randoCheckId].shuffled = true;
                continue;
            }

            if (randoStaticCheck.randoCheckType == RCTYPE_SKULL_TOKEN) {
                if (saveInfo.randoSaveOptions[RO_SHUFFLE_GOLD_SKULLTULAS] == RO_GENERIC_NO) {
                    continue;
                }

                if (vanillaSkulltulas.contains(randoCheckId)) {
                    saveInfo.randoSaveChecks[randoCheckId].shuffled = true;
                    continue;
                }
            }

            if (randoStaticCheck.randoCheckType == RCTYPE_SONG &&
                saveInfo.randoSaveOptions[RO_SHUFFLE_SONGS] == RO_SONG_SHUFFLE_VANILLA) {
#ifdef RSBS_SINGLE_EXECUTABLE
                // #1502: a frozen-on shared song leaves its vanilla check to the bag.
                if (!RsbsReleasesVanillaSharedSong(randoStaticCheck.randoItemId)) {
                    continue;
                }
#else
                continue;
#endif
            }

            if (randoStaticCheck.randoCheckType == RCTYPE_OWL &&
                saveInfo.randoSaveOptions[RO_SHUFFLE_OWL_STATUES] == RO_GENERIC_NO) {
                continue;
            }

            if (randoStaticCheck.randoCheckType == RCTYPE_POT &&
                saveInfo.randoSaveOptions[RO_SHUFFLE_POT_DROPS] == RO_GENERIC_NO) {
                continue;
            }

            if (randoStaticCheck.randoCheckType == RCTYPE_CRATE &&
                saveInfo.randoSaveOptions[RO_SHUFFLE_CRATE_DROPS] == RO_GENERIC_NO) {
                continue;
            }

            if (randoStaticCheck.randoCheckType == RCTYPE_BARREL &&
                saveInfo.randoSaveOptions[RO_SHUFFLE_BARREL_DROPS] == RO_GENERIC_NO) {
                continue;
            }

            if (randoStaticCheck.randoCheckType == RCTYPE_GRASS &&
                saveInfo.randoSaveOptions[RO_SHUFFLE_GRASS_DROPS] == RO_GENERIC_NO) {
                continue;
            }

            if (randoStaticCheck.randoCheckType == RCTYPE_BEEHIVE &&
                saveInfo.randoSaveOptions[RO_SHUFFLE_HIVE_DROPS] == RO_GENERIC_NO) {
                continue;
            }

            if (randoStaticCheck.randoCheckType == RCTYPE_TREE &&
                saveInfo.randoSaveOptions[RO_SHUFFLE_TREE_DROPS] == RO_GENERIC_NO) {
                continue;
            }

            if (randoStaticCheck.randoCheckType == RCTYPE_BUTTERFLY &&
                saveInfo.randoSaveOptions[RO_SHUFFLE_BUTTERFLIES] == RO_GENERIC_NO) {
                continue;
            }

            if (randoStaticCheck.randoCheckType == RCTYPE_FREESTANDING &&
                saveInfo.randoSaveOptions[RO_SHUFFLE_FREESTANDING_ITEMS] == RO_GENERIC_NO) {
                continue;
            }

            if (randoStaticCheck.randoCheckType == RCTYPE_SNOWBALL &&
                saveInfo.randoSaveOptions[RO_SHUFFLE_SNOWBALL_DROPS] == RO_GENERIC_NO) {
                continue;
            }

            if (randoStaticCheck.randoCheckType == RCTYPE_FROG &&
                saveInfo.randoSaveOptions[RO_SHUFFLE_FROGS] == RO_GENERIC_NO) {
                continue;
            }

            if (randoStaticCheck.randoCheckType == RCTYPE_REMAINS &&
                saveInfo.randoSaveOptions[RO_SHUFFLE_BOSS_REMAINS] == RO_GENERIC_NO) {
                continue;
            }

            if (randoStaticCheck.randoCheckType == RCTYPE_COW &&
                saveInfo.randoSaveOptions[RO_SHUFFLE_COWS] == RO_GENERIC_NO) {
                continue;
            }

            if (randoStaticCheck.randoCheckType == RCTYPE_ENEMY_DROP &&
                saveInfo.randoSaveOptions[RO_SHUFFLE_ENEMY_DROPS] == RO_GENERIC_NO) {
                continue;
            }

            if (randoStaticCheck.randoCheckType == RCTYPE_WONDER_ITEM &&
                saveInfo.randoSaveOptions[RO_SHUFFLE_WONDER_ITEMS] == RO_GENERIC_NO) {
                continue;
            }

            if (randoStaticCheck.randoCheckType == RCTYPE_TINGLE_SHOP &&
                saveInfo.randoSaveOptions[RO_SHUFFLE_TINGLE_SHOPS] == RO_GENERIC_NO) {
                continue;
            } else {
#ifdef RSBS_SINGLE_EXECUTABLE
                // #1427: priced after this loop by Rando::ShopPrices::Assign. This
                // branch gave EVERY check reaching it a junk price, one draw each.
#else
                int price = Ship_Random(0, 200);
                saveInfo.randoSaveChecks[randoCheckId].price = price;
#endif
            }

            if (randoStaticCheck.randoCheckType == RCTYPE_SHOP) {
                // We always want shuffle RC_CURIOSITY_SHOP_SPECIAL_ITEM &
                // RC_BOMB_SHOP_ITEM_04_OR_CURIOSITY_SHOP_ITEM
                if (saveInfo.randoSaveOptions[RO_SHUFFLE_SHOPS] == RO_GENERIC_NO &&
                    randoCheckId != RC_CURIOSITY_SHOP_SPECIAL_ITEM &&
                    randoCheckId != RC_BOMB_SHOP_ITEM_04_OR_CURIOSITY_SHOP_ITEM) {
                    continue;
                } else {
#ifdef RSBS_SINGLE_EXECUTABLE
                    // #1427: the shop price modes, priced after this loop.
#else
                    // We may come up with a better solution for this in the future, but for now we choose a
                    // random price ahead of time, logic will account for whatever price we choose
                    int price = Ship_Random(0, 200);
                    saveInfo.randoSaveChecks[randoCheckId].price = price;
#endif
                }
            }

            // Excluded checks are always left out of the check pool, but what happens to them depends on what their
            // vanilla item is worth. A check whose vanilla item is junk (grass, pots, snowballs...) has nothing worth
            // preserving, so it stays unshuffled and behaves exactly like it does in vanilla, and its item never enters
            // the item pool. Every other check keeps its item in the pool and is marked as skipped with junk in its
            // place. That leaves an inbalance in the pools that will get sorted automatically if there is enough space.
            if (saveInfo.randoSaveOptions[RO_LOGIC] != RO_LOGIC_VANILLA) {
                auto it = std::find(excludedChecks.begin(), excludedChecks.end(), randoCheckId);
                if (it != excludedChecks.end()) {
                    if (saveInfo.randoSaveOptions[RO_SHUFFLE_SONGS] == RO_SONG_SHUFFLE_SONG_LOCATIONS &&
                        randoStaticCheck.randoCheckType == RCTYPE_SONG) {
                        continue;
                    }

                    if (Rando::StaticData::Items[randoStaticCheck.randoItemId].randoItemType != RITYPE_JUNK) {
                        itemPool.push_back(randoStaticCheck.randoItemId);

                        saveInfo.randoSaveChecks[randoCheckId].shuffled = true;
                        saveInfo.randoSaveChecks[randoCheckId].randoItemId = RI_JUNK;
                        saveInfo.randoSaveChecks[randoCheckId].skipped = true;
                    }
                    continue;
                }
            }

            checkPool.emplace_back(randoCheckId);
            itemPool.push_back(randoStaticCheck.randoItemId);
        }
    }

#ifdef RSBS_SINGLE_EXECUTABLE
    // #1427: OoTMM's MM shop and Tingle price modes, one pass in a fixed order
    // from the seeded stream, before the fill reads any price (CAN_AFFORD).
    Rando::ShopPrices::Assign(saveInfo, checkPool);
#endif

    // Add sword and shield to the pool because they don't have a vanilla location, if you are starting with
    // them they will be removed from the pool in the next step
    itemPool.push_back(RI_PROGRESSIVE_SWORD);
    itemPool.push_back(RI_SHIELD_HERO);

    // Add other items that don't have a vanilla location like Sun's Song or Song of Double Time

    // Boss Souls
    if (saveInfo.randoSaveOptions[RO_SHUFFLE_BOSS_SOULS] == RO_GENERIC_YES) {
        for (int i = RI_SOUL_BOSS_GOHT; i <= RI_SOUL_BOSS_TWINMOLD; i++) {
            if (i == RI_SOUL_BOSS_MAJORA && saveInfo.randoSaveOptions[RO_SHUFFLE_TRIFORCE_PIECES] == RO_GENERIC_YES) {
                continue;
            }
            itemPool.push_back((RandoItemId)i);
        }
    }

    // Enemy Souls
    if (saveInfo.randoSaveOptions[RO_SHUFFLE_ENEMY_SOULS] == RO_GENERIC_YES) {
        for (int i = RI_SOUL_ENEMY_ALIEN; i <= RI_SOUL_ENEMY_WOLFOS; i++) {
            itemPool.push_back((RandoItemId)i);
        }
    }

    // Shuffle Time
    if (saveInfo.randoSaveOptions[RO_CLOCK_SHUFFLE] == RO_GENERIC_YES) {
        auto clockShuffleMode = saveInfo.randoSaveOptions[RO_CLOCK_SHUFFLE_PROGRESSIVE];

        if (clockShuffleMode == RO_CLOCK_SHUFFLE_RANDOM) {
            itemPool.push_back(RI_TIME_DAY_1);
            itemPool.push_back(RI_TIME_NIGHT_1);
            itemPool.push_back(RI_TIME_DAY_2);
            itemPool.push_back(RI_TIME_NIGHT_2);
            itemPool.push_back(RI_TIME_DAY_3);
            itemPool.push_back(RI_TIME_NIGHT_3);
        } else {
            for (int i = 0; i < ClockItems::HALF_COUNT; ++i) {
                itemPool.push_back(RI_TIME_PROGRESSIVE);
            }
        }
    }

    // Abilities
#ifdef RSBS_SINGLE_EXECUTABLE
    // RedShipBlueShip #1369: swim and the buttons also enter the pool under the
    // pair's one shuffle from the frozen combo record (StartingItems.cpp leaves
    // them out of the start under the same test).
    const bool rsbsShuffleSwim = Rando::Foreign::ShuffleSwimForWorld(saveInfo.randoSaveOptions);
    const bool rsbsShuffleButtons = Rando::Foreign::ShuffleOcarinaButtonsForWorld(saveInfo.randoSaveOptions);
#else
    const bool rsbsShuffleSwim = saveInfo.randoSaveOptions[RO_SHUFFLE_SWIM] == RO_GENERIC_YES;
    const bool rsbsShuffleButtons = saveInfo.randoSaveOptions[RO_SHUFFLE_OCARINA_BUTTONS] == RO_GENERIC_YES;
#endif
    if (rsbsShuffleSwim) {
        itemPool.push_back(RI_ABILITY_SWIM);
    }

    // Ocarina Buttons
    if (rsbsShuffleButtons) {
        for (int i = RI_OCARINA_BUTTON_A; i <= RI_OCARINA_BUTTON_C_UP; i++) {
            itemPool.push_back((RandoItemId)i);
        }
    }

    // Shuffle the Skeleton Key into the Pool
    if (saveInfo.randoSaveOptions[RO_SHUFFLE_SKELETON_KEY] == RO_GENERIC_YES) {
        itemPool.push_back(RI_SKELETON_KEY);
    }

#ifdef RSBS_SINGLE_EXECUTABLE
    // RedShipBlueShip #980 (OoTMM gerudoMaskMm, skullMaskMm, spookyMaskMm): each
    // Ocarina of Time mask its frozen Cross-Game Rules row adds to Majora's Mask.
    // Off by default, so a world without the rules builds the pool it always did.
    for (RandoItemId mask : { RI_MASK_GERUDO, RI_MASK_SKULL, RI_MASK_SPOOKY }) {
        if (MM_OoTMask_RuleOn(mask)) {
            itemPool.push_back(mask);
        }
    }
    // RedShipBlueShip #1169 XW5 (OoTMM songMinuetMm .. songPreludeMm): each Ocarina
    // of Time warp song its frozen rule adds to Majora's Mask, one copy, none under
    // its shared rule (lane XW10's family, held back). Hidden until XW12 and off by
    // default, so every world a player can make builds the pool it always did. A
    // starting copy leaves the pool with the other starting items below.
    for (RandoItemId song : { RI_SONG_MINUET, RI_SONG_BOLERO, RI_SONG_SERENADE, RI_SONG_REQUIEM, RI_SONG_NOCTURNE,
                              RI_SONG_PRELUDE }) {
        for (int copy = 0; copy < MM_WarpSongMM_PooledCopies(song); copy++) {
            itemPool.push_back(song);
        }
    }
#endif

    // Songs
    if (saveInfo.randoSaveOptions[RO_SHUFFLE_SONG_SUN] == RO_GENERIC_YES) {
        itemPool.push_back(RI_SONG_SUN);
    }
    if (saveInfo.randoSaveOptions[RO_SHUFFLE_SONG_DOUBLE_TIME] == RO_GENERIC_YES) {
        itemPool.push_back(RI_SONG_DOUBLE_TIME);
    }
    if (saveInfo.randoSaveOptions[RO_SHUFFLE_SONG_INVERTED_TIME] == RO_GENERIC_YES) {
        itemPool.push_back(RI_SONG_INVERTED_TIME);
    }
    if (saveInfo.randoSaveOptions[RO_SHUFFLE_SONG_SARIA] == RO_GENERIC_YES) {
        itemPool.push_back(RI_SONG_SARIA);
    }

    // Shuffle Triforce Pieces into the Pool
    if (saveInfo.randoSaveOptions[RO_SHUFFLE_TRIFORCE_PIECES] == RO_GENERIC_YES) {
        int piecesToShuffle = saveInfo.randoSaveOptions[RO_TRIFORCE_PIECES_MAX];
        while (piecesToShuffle) {
            itemPool.push_back(RI_TRIFORCE_PIECE);
            piecesToShuffle--;
        }
    }

    // Remove extra stray fairies from the pool.
    std::map<RandoItemId, int> removeAbleItemsInPool = {
        { RI_STONE_TOWER_STRAY_FAIRY, 0 },
        { RI_GREAT_BAY_STRAY_FAIRY, 0 },
        { RI_SNOWHEAD_STRAY_FAIRY, 0 },
        { RI_WOODFALL_STRAY_FAIRY, 0 },
    };
    for (RandoItemId itemId : itemPool) {
        if (removeAbleItemsInPool.find(itemId) != removeAbleItemsInPool.end()) {
            removeAbleItemsInPool[itemId]++;
        }
    }
    for (auto& [itemId, count] : removeAbleItemsInPool) {
        int max = 0;
        switch (itemId) {
            case RI_STONE_TOWER_STRAY_FAIRY:
            case RI_GREAT_BAY_STRAY_FAIRY:
            case RI_SNOWHEAD_STRAY_FAIRY:
            case RI_WOODFALL_STRAY_FAIRY:
                max = saveInfo.randoSaveOptions[RO_STRAY_FAIRIES_MAX];
                break;
            default:
                break;
        }

        while (count > max) {
            auto it = std::find(itemPool.begin(), itemPool.end(), itemId);
            if (it != itemPool.end()) {
                itemPool.erase(it);
                count--;
            } else {
                break;
            }
        }
    }

    // Remove starting items from the pool (but only one per entry in startingItems)
    for (RandoItemId startingItem : startingItems) {
        auto it = std::find(itemPool.begin(), itemPool.end(), startingItem);
        if (it != itemPool.end()) {
            itemPool.erase(it);
        }
    }

#ifdef RSBS_SINGLE_EXECUTABLE
    // #1499: after the starting items left the pool, before Plentiful reads it.
    RsbsKeepOneOfEachTingleMap(startingItems, itemPool);
    gRsbsComboPlentifulFrom = itemPool.size();
#endif
    // Plentiful
    if (saveInfo.randoSaveOptions[RO_PLENTIFUL_ITEMS] == RO_GENERIC_YES) {
        std::vector<RandoItemId> plentifulItems;
        for (size_t i = 0; i < itemPool.size(); i++) {
            // The user can specify exactly how many pieces they want to shuffle, so skip those
            if (itemPool[i] == RI_TRIFORCE_PIECE) {
                continue;
            }
            if (itemPool[i] == RI_SKELETON_KEY) {
                continue;
            }
            if (saveInfo.randoSaveOptions[RO_SHUFFLE_SONGS] == RO_SONG_SHUFFLE_SONG_LOCATIONS &&
                IsSongLocationItem(itemPool[i])) {
                continue;
            }
#ifdef RSBS_SINGLE_EXECUTABLE
            // #1542: under the shared song pool OoTMM adds no song copy (transform.ts
            // gives songs a Plentiful copy only under `anywhere`), the XW5 warp
            // songs included.
            if (RsbsSongPoolUnion(saveInfo) && MM_ComboLogic_IsPairSongItem((uint16_t)itemPool[i])) {
                continue;
            }
#endif

            switch (Rando::StaticData::Items[itemPool[i]].randoItemType) {
                case RITYPE_BOSS_KEY:
                case RITYPE_SMALL_KEY:
                case RITYPE_MASK:
                case RITYPE_MAJOR:
                    plentifulItems.push_back(itemPool[i]);
                    break;
                case RITYPE_LESSER:
                    // 2Ship bfcb60476 (#1398): every lesser item gets a copy except the maps and
                    // compasses. That keeps #1499's rule too: Plentiful never adds a second Tingle
                    // map, which would be junk once one is owned.
                    if (Rando::StaticData::Items[itemPool[i]].itemId != ITEM_TINGLE_MAP &&
                        Rando::StaticData::Items[itemPool[i]].itemId != ITEM_DUNGEON_MAP &&
                        Rando::StaticData::Items[itemPool[i]].itemId != ITEM_COMPASS) {
                        plentifulItems.push_back(itemPool[i]);
                    }
                    break;
                case RITYPE_HEALTH:
                case RITYPE_JUNK:
                default:
                    break;
            }
        }

        for (RandoItemId plentifulItem : plentifulItems) {
            itemPool.push_back(plentifulItem);
        }
    }

#ifdef RSBS_SINGLE_EXECUTABLE
    gRsbsComboPlentifulTo = itemPool.size();
#endif
    // Traps
    if (saveInfo.randoSaveOptions[RO_SHUFFLE_TRAPS] == RO_GENERIC_YES) {
        int trapsToShuffle = saveInfo.randoSaveOptions[RO_TRAP_AMOUNT];
        while (trapsToShuffle) {
            itemPool.push_back(RI_TRAP);
            trapsToShuffle--;
        }
    }

#ifdef RSBS_SINGLE_EXECUTABLE
    // #1542 (rulings D1, D5): under the shared song pool the song checks' surplus
    // is a fact of BOTH games' song checks, so no MM song check is junked or
    // skipped here and no random number is drawn: OoTMM keeps 'MM Initial Song of
    // Healing' among its song locations (world/builder.ts loadChecks) and junks
    // only the song locations still empty after the songs are placed (solve.ts
    // fillJunk), which MM's own pass does after the single bag.
    const bool rsbsSongUnion = RsbsSongPoolUnion(saveInfo);
#else
    const bool rsbsSongUnion = false;
#endif
    if (!rsbsSongUnion && saveInfo.randoSaveOptions[RO_SHUFFLE_SONGS] == RO_SONG_SHUFFLE_SONG_LOCATIONS) {
        size_t songItems = std::count_if(itemPool.begin(), itemPool.end(), IsSongLocationItem);

        std::vector<RandoCheckId> songChecks;
        for (RandoCheckId checkId : checkPool) {
            if (Rando::StaticData::Checks[checkId].randoCheckType == RCTYPE_SONG) {
                songChecks.push_back(checkId);
            }
        }

        for (size_t i = 0; i < songChecks.size(); i++) {
            std::swap(songChecks[i], songChecks[Ship_Random(0, songChecks.size())]);
        }

        // we want to junk the song of healing check first, people normally start with SoT so we don't want
        // them getting two free songs at the start of every seed.
        auto healing = std::find(songChecks.begin(), songChecks.end(), RC_STARTING_ITEM_SONG_OF_HEALING);
        if (healing != songChecks.end()) {
            std::swap(songChecks[0], *healing);
        }

        size_t surplusLocations = songChecks.size() - songItems;
        for (size_t i = 0; i < surplusLocations; i++) {
            auto& randoSaveCheck = saveInfo.randoSaveChecks[songChecks[i]];
            randoSaveCheck.shuffled = true;
            randoSaveCheck.randoItemId = RI_JUNK;
            randoSaveCheck.skipped = true;
            checkPool.erase(std::find(checkPool.begin(), checkPool.end(), songChecks[i]));
        }
    }
#ifdef RSBS_SINGLE_EXECUTABLE
    gRsbsComboPoolSize = itemPool.size();
#endif
}

} // namespace Logic

} // namespace Rando
