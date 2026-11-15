#include "Logic.h"
#include "Rando/StaticData/StaticData.h"
#include <algorithm>
#include <array>
#include <utility>

namespace Rando {

namespace Logic {

// if we keep adding more groups we should probably make this an enum or somthing
static constexpr int CONFINEMENT_GROUP_SONGS = DUNGEON_SCENE_INDEX_STONE_TOWER_TEMPLE_BOSS + 1;

int SceneIdToDungeon(SceneId sceneId) {
    switch (sceneId) {
        case SCENE_MITURIN:
        case SCENE_MITURIN_BS:
            return DUNGEON_SCENE_INDEX_WOODFALL_TEMPLE;
        case SCENE_HAKUGIN:
        case SCENE_HAKUGIN_BS:
            return DUNGEON_SCENE_INDEX_SNOWHEAD_TEMPLE;
        case SCENE_SEA:
        case SCENE_SEA_BS:
            return DUNGEON_SCENE_INDEX_GREAT_BAY_TEMPLE;
        case SCENE_INISIE_N:
        case SCENE_INISIE_R:
        case SCENE_INISIE_BS:
            return DUNGEON_SCENE_INDEX_STONE_TOWER_TEMPLE;
        default:
            return -1;
    }
}

int DungeonItemToDungeon(RandoItemId itemId) {
    int dungeon = -1;
    switch (itemId) {
        case RI_WOODFALL_SMALL_KEY:
        case RI_WOODFALL_BOSS_KEY:
        case RI_WOODFALL_STRAY_FAIRY:
        case RI_REMAINS_ODOLWA:
            dungeon = DUNGEON_SCENE_INDEX_WOODFALL_TEMPLE;
            break;
        case RI_SNOWHEAD_SMALL_KEY:
        case RI_SNOWHEAD_BOSS_KEY:
        case RI_SNOWHEAD_STRAY_FAIRY:
        case RI_REMAINS_GOHT:
            dungeon = DUNGEON_SCENE_INDEX_SNOWHEAD_TEMPLE;
            break;
        case RI_GREAT_BAY_SMALL_KEY:
        case RI_GREAT_BAY_BOSS_KEY:
        case RI_GREAT_BAY_STRAY_FAIRY:
        case RI_REMAINS_GYORG:
            dungeon = DUNGEON_SCENE_INDEX_GREAT_BAY_TEMPLE;
            break;
        case RI_STONE_TOWER_SMALL_KEY:
        case RI_STONE_TOWER_BOSS_KEY:
        case RI_STONE_TOWER_STRAY_FAIRY:
        case RI_REMAINS_TWINMOLD:
            dungeon = DUNGEON_SCENE_INDEX_STONE_TOWER_TEMPLE;
            break;
        default:
            return -1;
    }
    return dungeon;
}

static constexpr int kDungeonCount = DUNGEON_SCENE_INDEX_STONE_TOWER_TEMPLE + 1;

static bool IsFreestandingStrayFairyCheck(RandoCheckId checkId) {
    if (checkId == RC_UNKNOWN) {
        return false;
    }
    auto it = Rando::StaticData::Checks.find(checkId);
    return it != Rando::StaticData::Checks.end() && it->second.randoCheckType == RCTYPE_STRAY_FAIRY;
}

static const std::array<std::pair<int, int>, kDungeonCount>& StrayFairyCountsByDungeon() {
    static const std::array<std::pair<int, int>, kDungeonCount> counts = [] {
        std::array<std::pair<int, int>, kDungeonCount> result{};
        for (const auto& [checkId, staticCheck] : Rando::StaticData::Checks) {
            if (checkId == RC_UNKNOWN) {
                continue;
            }
            if (Rando::StaticData::Items[staticCheck.randoItemId].randoItemType != RITYPE_STRAY_FAIRY) {
                continue;
            }
            int dungeon = SceneIdToDungeon(staticCheck.sceneId);
            if (dungeon < 0 || dungeon >= kDungeonCount) {
                continue;
            }
            if (staticCheck.randoCheckType == RCTYPE_STRAY_FAIRY) {
                result[dungeon].second++;
            } else {
                result[dungeon].first++;
            }
        }
        return result;
    }();
    return counts;
}

void GetStrayFairyCheckCounts(int dungeonIndex, int* outChestCount, int* outOtherCount) {
    int chest = 0;
    int other = 0;
    if (dungeonIndex >= 0 && dungeonIndex < kDungeonCount) {
        const auto& pair = StrayFairyCountsByDungeon()[dungeonIndex];
        chest = pair.first;
        other = pair.second;
    }
    if (outChestCount != nullptr) {
        *outChestCount = chest;
    }
    if (outOtherCount != nullptr) {
        *outOtherCount = other;
    }
}

int OwnDungeonStrayFairyQuota(int dungeonIndex) {
    int chestCount = 0;
    int otherCount = 0;
    GetStrayFairyCheckCounts(dungeonIndex, &chestCount, &otherCount);
    int quota = 0;
    if (RANDO_SAVE_OPTIONS[RO_PLACEMENT_STRAY_FAIRIES] == RO_DUNGEON_ITEM_OWN_DUNGEON) {
        quota += chestCount;
    }
    if (RANDO_SAVE_OPTIONS[RO_PLACEMENT_STRAY_FAIRIES_OTHER] == RO_DUNGEON_ITEM_OWN_DUNGEON) {
        quota += otherCount;
    }
    return quota;
}

RandoOptionId DungeonItemPlacementOption(RandoCheckId checkId, RandoItemId itemId) {
    if (DungeonItemToDungeon(itemId) < 0 && itemId != RI_CLOCK_TOWN_STRAY_FAIRY) {
        return RO_MAX;
    }
    switch (Rando::StaticData::Items[itemId].randoItemType) {
        case RITYPE_SMALL_KEY:
            return RO_PLACEMENT_SMALL_KEYS;
        case RITYPE_BOSS_KEY:
            return RO_PLACEMENT_BOSS_KEYS;
        case RITYPE_STRAY_FAIRY:
            if (itemId == RI_CLOCK_TOWN_STRAY_FAIRY || IsFreestandingStrayFairyCheck(checkId)) {
                return RO_PLACEMENT_STRAY_FAIRIES_OTHER;
            }
            return RO_PLACEMENT_STRAY_FAIRIES;
        default:
            return RO_MAX;
    }
}

bool StaysAtVanillaCheck(RandoCheckId checkId, RandoItemId itemId, const RandoSaveInfo& saveInfo) {
    RandoOptionId placementOption = DungeonItemPlacementOption(checkId, itemId);
    if (placementOption == RO_MAX) {
        return false;
    }
    switch (saveInfo.randoSaveOptions[placementOption]) {
        case RO_DUNGEON_ITEM_VANILLA:
            return true;
        case RO_DUNGEON_ITEM_OWN_DUNGEON:
            return DungeonItemToDungeon(itemId) < 0;
        default:
            return false;
    }
}

int RandoItemIdToDungeon(RandoItemId itemId) {
    switch (itemId) {
        case RI_REMAINS_ODOLWA:
        case RI_REMAINS_GOHT:
        case RI_REMAINS_GYORG:
        case RI_REMAINS_TWINMOLD:
            return RANDO_SAVE_OPTIONS[RO_SHUFFLE_BOSS_REMAINS] == RO_REMAINS_SHUFFLE_OWN_DUNGEON
                       ? DungeonItemToDungeon(itemId)
                       : -1;
        default:
            break;
    }

    int dungeon = DungeonItemToDungeon(itemId);
    if (dungeon < 0) {
        return -1;
    }
    if (Rando::StaticData::Items[itemId].randoItemType == RITYPE_STRAY_FAIRY) {
        return OwnDungeonStrayFairyQuota(dungeon) > 0 ? dungeon : -1;
    }

    RandoOptionId placementOption = DungeonItemPlacementOption(RC_UNKNOWN, itemId);
    if (placementOption == RO_MAX || RANDO_SAVE_OPTIONS[placementOption] != RO_DUNGEON_ITEM_OWN_DUNGEON) {
        return -1;
    }
    return dungeon;
}

bool IsSongLocationItem(RandoItemId itemId) {
    static const std::array<bool, RI_MAX> songLocationItems = [] {
        std::array<bool, RI_MAX> items{};
        for (auto& [randoCheckId, randoStaticCheck] : Rando::StaticData::Checks) {
            if (randoStaticCheck.randoCheckType == RCTYPE_SONG) {
                items[randoStaticCheck.randoItemId] = true;
            }
        }
        return items;
    }();
    return itemId < RI_MAX && songLocationItems[itemId];
}

int CheckIdToConfinementGroup(RandoCheckId checkId) {
    auto& randoStaticCheck = Rando::StaticData::Checks[checkId];
    if (RANDO_SAVE_OPTIONS[RO_SHUFFLE_SONGS] == RO_SONG_SHUFFLE_SONG_LOCATIONS &&
        randoStaticCheck.randoCheckType == RCTYPE_SONG) {
        return CONFINEMENT_GROUP_SONGS;
    }
    return SceneIdToDungeon(randoStaticCheck.sceneId);
}

int RandoItemIdToConfinementGroup(RandoItemId itemId) {
    if (RANDO_SAVE_OPTIONS[RO_SHUFFLE_SONGS] == RO_SONG_SHUFFLE_SONG_LOCATIONS && IsSongLocationItem(itemId)) {
        return CONFINEMENT_GROUP_SONGS;
    }
    return RandoItemIdToDungeon(itemId);
}

static const std::array<std::vector<RandoCheckId>, kDungeonCount>& DungeonChecksByIndex() {
    static const std::array<std::vector<RandoCheckId>, kDungeonCount> checks = [] {
        std::array<std::vector<RandoCheckId>, kDungeonCount> result;
        for (const auto& [checkId, staticCheck] : Rando::StaticData::Checks) {
            if (checkId == RC_UNKNOWN) {
                continue;
            }
            int dungeon = SceneIdToDungeon(staticCheck.sceneId);
            if (dungeon >= 0 && dungeon < kDungeonCount) {
                result[dungeon].push_back(checkId);
            }
        }
        return result;
    }();
    return checks;
}

static void GetRemainingDungeonFairyRequirement(int dungeonIndex, RandoItemId fairyItem,
                                                const std::vector<RandoItemId>& itemPool, RandoCheckId excludedCheckId,
                                                int* outInPool, int* outStillMustStay) {
    int inPool = 0;
    for (RandoItemId itemId : itemPool) {
        if (itemId == fairyItem) {
            inPool++;
        }
    }

    const int quota = OwnDungeonStrayFairyQuota(dungeonIndex);
    int placedInDungeon = 0;
    if (quota > 0 && dungeonIndex >= 0 && dungeonIndex < kDungeonCount) {
        for (RandoCheckId checkId : DungeonChecksByIndex()[dungeonIndex]) {
            if (checkId == excludedCheckId) {
                continue;
            }
            const RandoSaveCheck& saveCheck = RANDO_SAVE_CHECKS[checkId];
            if (!saveCheck.shuffled || saveCheck.skipped) {
                continue;
            }
            if (saveCheck.randoItemId == fairyItem &&
                !StaysAtVanillaCheck(checkId, Rando::StaticData::Checks.at(checkId).randoItemId,
                                     gSaveContext.save.shipSaveInfo.rando)) {
                placedInDungeon++;
            }
        }
    }

    const int remainingQuota = (std::max)(0, quota - placedInDungeon);
    *outInPool = inPool;
    *outStillMustStay = (std::min)(inPool, remainingQuota);
}

bool IsItemAllowedAtCheck(RandoItemId itemId, RandoCheckId checkId) {
    int confinedGroup = RandoItemIdToConfinementGroup(itemId);
    return confinedGroup < 0 || confinedGroup == CheckIdToConfinementGroup(checkId);
}

bool CanPlaceItemAtCheck(RandoItemId itemId, RandoCheckId checkId, const std::vector<RandoItemId>& itemPool) {
    int confinedGroup = RandoItemIdToConfinementGroup(itemId);
    if (confinedGroup < 0 || confinedGroup == CheckIdToConfinementGroup(checkId)) {
        return true;
    }
    if (confinedGroup < kDungeonCount && Rando::StaticData::Items[itemId].randoItemType == RITYPE_STRAY_FAIRY) {
        int inPool = 0;
        int stillMustStay = 0;
        GetRemainingDungeonFairyRequirement(confinedGroup, itemId, itemPool, checkId, &inPool, &stillMustStay);
        return inPool > stillMustStay;
    }
    return false;
}

size_t SelectItemForCheck(const std::vector<RandoItemId>& itemPool, const std::vector<RandoCheckId>& checkPool,
                          RandoCheckId checkId) {
    int group = CheckIdToConfinementGroup(checkId);
    if (group >= 0) {
        int remainingFairyStillMustStay = 0;
        RandoItemId groupFairyItem = RI_UNKNOWN;
        if (group < kDungeonCount) {
            for (RandoItemId itemId : itemPool) {
                if (Rando::StaticData::Items[itemId].randoItemType == RITYPE_STRAY_FAIRY &&
                    DungeonItemToDungeon(itemId) == group && RandoItemIdToConfinementGroup(itemId) == group) {
                    groupFairyItem = itemId;
                    break;
                }
            }
            if (groupFairyItem != RI_UNKNOWN) {
                int inPool = 0;
                GetRemainingDungeonFairyRequirement(group, groupFairyItem, itemPool, checkId, &inPool,
                                                    &remainingFairyStillMustStay);
            }
        }

        int confinedItems = remainingFairyStillMustStay;
        for (RandoItemId itemId : itemPool) {
            if (itemId == groupFairyItem) {
                continue;
            }
            if (RandoItemIdToConfinementGroup(itemId) == group) {
                confinedItems++;
            }
        }
        if (confinedItems > 0) {
            int groupChecks = 1; // this check was already removed from checkPool
            for (RandoCheckId other : checkPool) {
                if (CheckIdToConfinementGroup(other) == group) {
                    groupChecks++;
                }
            }
            if (confinedItems >= groupChecks) {
                for (size_t i = itemPool.size(); i-- > 0;) {
                    if (itemPool[i] == groupFairyItem) {
                        if (remainingFairyStillMustStay > 0) {
                            return i;
                        }
                        continue;
                    }
                    if (RandoItemIdToConfinementGroup(itemPool[i]) == group) {
                        return i;
                    }
                }
            }
        }
    }

    for (size_t i = itemPool.size(); i-- > 0;) {
        if (CanPlaceItemAtCheck(itemPool[i], checkId, itemPool)) {
            return i;
        }
    }
    return itemPool.size();
}

void PreplaceConfinedItems(std::vector<RandoCheckId>& checkPool, std::vector<RandoItemId>& itemPool) {
    std::map<int, std::vector<RandoCheckId>> groupChecks;
    for (RandoCheckId checkId : checkPool) {
        int group = CheckIdToConfinementGroup(checkId);
        if (group >= 0) {
            groupChecks[group].push_back(checkId);
        }
    }

    std::array<int, kDungeonCount> remainingFairyQuota{};
    for (int d = 0; d < kDungeonCount; d++) {
        remainingFairyQuota[d] = OwnDungeonStrayFairyQuota(d);
    }

    std::set<RandoCheckId> placedChecks;
    std::set<size_t> placedItems;
    for (size_t i = 0; i < itemPool.size(); i++) {
        int group = RandoItemIdToConfinementGroup(itemPool[i]);
        if (group < 0) {
            continue; // not confined to a dungeon
        }
        if (group < kDungeonCount && Rando::StaticData::Items[itemPool[i]].randoItemType == RITYPE_STRAY_FAIRY) {
            if (remainingFairyQuota[group] <= 0) {
                continue; // quota met: leave excess copies for the general pool
            }
            remainingFairyQuota[group]--;
        }
        auto& checks = groupChecks[group];
        if (checks.empty()) {
            continue; // dungeon out of room: leave it for the general pool
        }
        size_t pick = Ship_Random(0, checks.size());
        RandoCheckId checkId = checks[pick];
        checks.erase(checks.begin() + pick);

        RANDO_SAVE_CHECKS[checkId].shuffled = true;
        RANDO_SAVE_CHECKS[checkId].randoItemId = itemPool[i];
        placedChecks.insert(checkId);
        placedItems.insert(i);
    }

    // Remove the placed items and checks from the pools.
    std::vector<RandoItemId> remainingItems;
    for (size_t i = 0; i < itemPool.size(); i++) {
        if (placedItems.find(i) == placedItems.end()) {
            remainingItems.push_back(itemPool[i]);
        }
    }
    itemPool = std::move(remainingItems);

    std::vector<RandoCheckId> remainingChecks;
    for (RandoCheckId checkId : checkPool) {
        if (placedChecks.find(checkId) == placedChecks.end()) {
            remainingChecks.push_back(checkId);
        }
    }
    checkPool = std::move(remainingChecks);
}

} // namespace Logic

} // namespace Rando
