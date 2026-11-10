
#include "CheckTracker.h"
#include "2s2h/Rando/Logic/Logic.h"
#include "2s2h/Rando/Logic/ResetTimeGuard.h"
#include "2s2h/ShipUtils.h"
#include "2s2h/BenGui/UIWidgets.hpp"
#include "2s2h/Rando/StaticData/StaticData.h"
#include <algorithm>
#include <cstring>
#include <map>
#include <string>
#include <utility>
#include <vector>
#ifdef RSBS_SINGLE_EXECUTABLE
#include "2s2h/Rando/Foreign.h" // ForeignNameForCheck: the item a crossing host yields (#796)
#endif

// Image Icons
#include "assets/2s2h_assets.h"
#include "interface/parameter_static/parameter_static.h"
#include "assets/archives/icon_item_static/icon_item_static_yar.h"
#include "assets/interface/icon_item_dungeon_static/icon_item_dungeon_static.h"
#include "assets/archives/icon_item_24_static/icon_item_24_static_yar.h"
#include "assets/archives/schedule_dma_static/schedule_dma_static_yar.h"
#include "assets/interface/icon_item_field_static/icon_item_field_static.h"
#include "assets/objects/gameplay_keep/gameplay_keep.h"
#include "assets/overlays/ovl_En_Syateki_Okuta/ovl_En_Syateki_Okuta.h"

extern "C" {
s16 Play_GetOriginalSceneId(s16 sceneId);
}

namespace BenGui {
extern std::shared_ptr<Rando::CheckTracker::CheckTrackerWindow> mRandoCheckTrackerWindow;
}

#ifdef RSBS_SINGLE_EXECUTABLE
// The item an obtained check's row names (#796), from that check's save row.
static std::string ObtainedItemTrackerName(RandoCheckId randoCheckId, const RandoSaveCheck& randoSaveCheck) {
    // A check that hosts an OoT item holds RI_JUNK in MM's table, and the foreign
    // give never rewrites it (CheckQueue.cpp). What the player found is the OoT
    // item the give's textbox named, from the same lookup.
    if (const char* foreignName = Rando::Foreign::ForeignNameForCheck(randoCheckId)) {
        return std::string(foreignName) + " (OoT)";
    }
    return Rando::StaticData::Items[randoSaveCheck.randoItemId].name;
}

// TEST BRIDGE (#796): the name the row for MM check `randoCheckId` prints once
// obtained, read from the MM SaveContext at `mmSave` (the live one, or a frozen
// MM world such as the armed shadow a paired creation leaves). Also reports the
// item the MM table stores there. Returns 1 when written, 0 otherwise.
extern "C" int MM_CheckTracker_TestItemName(const void* mmSave, uint16_t randoCheckId, char* out, int cap,
                                            uint16_t* outStoredItem) {
    if (mmSave == nullptr || out == nullptr || cap <= 0 || randoCheckId == RC_UNKNOWN || randoCheckId >= RC_MAX) {
        return 0;
    }
    const RandoSaveCheck& row =
        static_cast<const SaveContext*>(mmSave)->save.shipSaveInfo.rando.randoSaveChecks[randoCheckId];
    if (outStoredItem != nullptr) {
        *outStoredItem = (uint16_t)row.randoItemId;
    }
    const std::string name = ObtainedItemTrackerName((RandoCheckId)randoCheckId, row);
    if ((int)name.size() >= cap) {
        return 0;
    }
    memcpy(out, name.c_str(), name.size() + 1);
    return 1;
}
#endif

#define WIDGET_COLOR UIWidgets::Colors(CVarGetInteger("gSettings.Menu.Theme", 5))

#define DEFINE_SCENE(_name, enumValue, _textId, _drawConfig, _restrictionFlags, _persistentCycleFlags, \
                     _entranceSceneId, betterMapSelectIndex, _humanName)                               \
    { enumValue, betterMapSelectIndex },
#define DEFINE_SCENE_UNSET(_enumValue)

static std::unordered_map<s32, s32> betterSceneIndex = {
#include "tables/scene_table.h"
};

#undef DEFINE_SCENE
#undef DEFINE_SCENE_UNSET

typedef enum {
    CHECK_MODE_NORMAL,
    CHECK_MODE_COLORED,
    CHECK_MODE_HIDDEN,
} CheckDisplayMode;

static std::unordered_map<int32_t, const char*> sCheckModes = {
    { CHECK_MODE_NORMAL, "Normal" },
    { CHECK_MODE_COLORED, "Colored" },
    { CHECK_MODE_HIDDEN, "Hidden" },
};

#define CVAR_NAME_SHOW_CHECK_TRACKER "gWindows.CheckTracker"
#define CVAR_NAME_OUT_OF_LOGIC_MODE "gRando.CheckTracker.OutOfLogicMode"
#define CVAR_NAME_OUT_OF_LOGIC_COLOR "gRando.CheckTracker.OutOfLogicColor"
#define CVAR_NAME_COLLECTED_MODE "gRando.CheckTracker.CollectedMode"
#define CVAR_NAME_COLLECTED_COLOR "gRando.CheckTracker.CollectedColor"
#define CVAR_NAME_SKIPPED_MODE "gRando.CheckTracker.SkippedMode"
#define CVAR_NAME_SKIPPED_COLOR "gRando.CheckTracker.SkippedColor"
#define CVAR_NAME_SCROLL_TO_SCENE "gRando.CheckTracker.ScrollToCurrentScene"
#define CVAR_NAME_TRACKER_OPACITY "gRando.CheckTracker.Opacity"
#define CVAR_NAME_TRACKER_SCALE "gRando.CheckTracker.Scale"
#define CVAR_NAME_SHOW_CURRENT_SCENE "gRando.CheckTracker.ShowCurrentScene"
#define CVAR_NAME_SHOW_CHECK_TYPE_FILTER "gRando.CheckTracker.ShowCheckTypeFilter"
#define CVAR_NAME_SHOW_SEARCH "gRando.CheckTracker.ShowSearch"
#define CVAR_SHOW_CHECK_TRACKER CVarGetInteger(CVAR_NAME_SHOW_CHECK_TRACKER, 0)
#define CVAR_OUT_OF_LOGIC_MODE CVarGetInteger(CVAR_NAME_OUT_OF_LOGIC_MODE, CHECK_MODE_COLORED)
#define CVAR_OUT_OF_LOGIC_COLOR CVarGetColor(CVAR_NAME_OUT_OF_LOGIC_COLOR ".Value", { 255, 255, 255, 100 })
#define CVAR_COLLECTED_MODE CVarGetInteger(CVAR_NAME_COLLECTED_MODE, CHECK_MODE_HIDDEN)
#define CVAR_COLLECTED_COLOR CVarGetColor(CVAR_NAME_COLLECTED_COLOR ".Value", { 100, 255, 100, 255 })
#define CVAR_SKIPPED_MODE CVarGetInteger(CVAR_NAME_SKIPPED_MODE, CHECK_MODE_HIDDEN)
#define CVAR_SKIPPED_COLOR CVarGetColor(CVAR_NAME_SKIPPED_COLOR ".Value", { 255, 100, 255, 255 })
#define CVAR_SCROLL_TO_SCENE CVarGetInteger(CVAR_NAME_SCROLL_TO_SCENE, 1)
#define CVAR_TRACKER_OPACITY CVarGetFloat(CVAR_NAME_TRACKER_OPACITY, 0.5f)
#define CVAR_TRACKER_SCALE CVarGetFloat(CVAR_NAME_TRACKER_SCALE, 1.0f)
#define CVAR_SHOW_CURRENT_SCENE CVarGetInteger(CVAR_NAME_SHOW_CURRENT_SCENE, 0)
#define CVAR_SHOW_CHECK_TYPE_FILTER CVarGetInteger(CVAR_NAME_SHOW_CHECK_TYPE_FILTER, 0)
#define CVAR_SHOW_SEARCH CVarGetInteger(CVAR_NAME_SHOW_SEARCH, 1)

static bool sExpandedHeadersToggle = true;
static bool sExpandedHeadersState = true;
static s32 sScrollToTargetScene = -1;
static s32 sScrollToTargetEntrance = -1;
static ImGuiTextFilter sCheckTrackerFilter;
float trackerScale = 1.0f;
float searchBoxPadding = 15.0f;
ImVec4 trackerBG = ImVec4{ 0, 0, 0, 0.5f };

std::map<SceneId, std::set<RandoCheckId>> sceneChecks;
std::vector<SceneId> sortedSceneIds;
std::unordered_map<RandoCheckId, std::string> accessLogicFuncs;
static std::unordered_map<RandoRegionId, SceneId> regionParentSceneMap;
std::set<RandoCheckType> checkTypeFiltersAvailable;
std::set<RandoCheckType> checkTypeFilter;

std::vector<const char*> checkTypeIconList = {
    /*RCTYPE_UNKNOWN*/ gItemIconBombersNotebookTex,
    /*RCTYPE_BARREL*/ gBarrelTrackerIcon,
    /*RCTYPE_CHEST*/ gChestTrackerIcon,
    /*RCTYPE_COW*/ gItemIconRomaniMaskTex,
    /*RCTYPE_CRATE*/ gCrateTrackerIcon,
    /*RCTYPE_ENEMY_DROP*/ gDungeonMapSkullTex,
    /*RCTYPE_FREESTANDING*/ gRupeeCounterIconTex,
    /*RCTYPE_FROG*/ gItemIconDonGeroMaskTex,
    /*RCTYPE_GRASS*/ gameplay_keep_Tex_053140,
    /*RCTYPE_HEART*/ gQuestIconPieceOfHeartTex,
    /*RCTYPE_MINIGAME*/ gArcheryScoreIconTex,
    /*RCTYPE_NPC*/ gItemIconBombersNotebookTex,
    /*RCTYPE_OWL*/ gWorldMapOwlFaceTex,
    /*RCTYPE_POT*/ gPotTrackerIcon,
    /*RCTYPE_REMAINS*/ gItemIconBombersNotebookTex,
    /*RCTYPE_SHOP*/ gItemIconAdultsWalletTex,
    /*RCTYPE_SKULL_TOKEN*/ gQuestIconGoldSkulltulaTex,
    /*RCTYPE_SNOWBALL*/ gMagicArrowEquipEffectTex,
    /*RCTYPE_SONG*/ gItemIconSongNoteTex,
    /*RCTYPE_STRAY_FAIRY*/ gStrayFairyGreatBayIconTex,
    /*RCTYPE_TINGLE_SHOP*/ gItemIconAdultsWalletTex,
    /*RCTYPE_TREE*/ gItemIconDekuStickTex,
};

static constexpr ImVec4 tintColor = {};

std::string GetTotalCheckCount() {
    std::string totalChecks;
    uint32_t collected = 0;
    uint32_t totalShuffled = 0;
    for (auto& [_, randoStaticCheck] : Rando::StaticData::Checks) {
        if (RANDO_SAVE_CHECKS[randoStaticCheck.randoCheckId].obtained ||
            RANDO_SAVE_CHECKS[randoStaticCheck.randoCheckId].skipped) {
            collected++;
        }
        if (RANDO_SAVE_CHECKS[randoStaticCheck.randoCheckId].shuffled) {
            totalShuffled++;
        }
    }
    totalChecks = std::to_string(collected);
    totalChecks += "/";
    totalChecks += std::to_string(totalShuffled);
    return totalChecks;
}

void DrawCheckTypeIcon(RandoCheckId randoCheckId) {
    RandoCheckType checkType = Rando::StaticData::Checks[randoCheckId].randoCheckType;
    ImGui::Image(Ship::Context::GetInstance()->GetWindow()->GetGui()->GetTextureByName(checkTypeIconList[checkType]),
                 checkType == RCTYPE_SONG  ? ImVec2(12.0f * trackerScale, 18.0f * trackerScale)
                 : checkType == RCTYPE_OWL ? ImVec2(18.0f * trackerScale, 9.0f * trackerScale)
                                           : ImVec2(18.0f * trackerScale, 18.0f * trackerScale),
                 ImVec2(0, 0), ImVec2(1, 1),
                 checkType == RCTYPE_FREESTANDING ? ImVec4(0.78f, 1, 0.39f, 1) : ImVec4(1, 1, 1, 1), tintColor);
}

// 2Ship f8bf9bb3b (#1561): scenes whose checks are listed under the scene they open from, and the scene the tracker
// scrolls to and filters by while one of them is played.
static std::set<SceneId> scenesToCheckParent = {
    SCENE_KAKUSIANA, SCENE_TAKARAYA,    SCENE_BOWLING,      SCENE_SONCHONOIE, SCENE_SYATEKI_MIZU, SCENE_YADOYA,
    SCENE_MILK_BAR,  SCENE_AYASHIISHOP, SCENE_YOUSEI_IZUMI, SCENE_DOUJOU,     SCENE_8ITEMSHOP,    SCENE_BOMYA,
    SCENE_POSTHOUSE, SCENE_TAKARAKUJI,  SCENE_SYATEKI_MORI, SCENE_MAP_SHOP,   SCENE_WITCH_SHOP,   SCENE_F01C,
    SCENE_F01_B,     SCENE_OMOYA,       SCENE_GORONSHOP,    SCENE_KAJIYA,     SCENE_FISHERMAN,    SCENE_LABO,
    SCENE_BANDROOM,  SCENE_TOUGITES,    SCENE_MUSICHOUSE,
};

// The last scene OnSceneInit saw that is not a grotto, or -1: GetScrollTargetScene's last resort.
static s32 sLastNonGrottoScene = -1;

static SceneId GetScrollTargetScene(s32 rawSceneId) {
    SceneId sceneId = (SceneId)Play_GetOriginalSceneId(rawSceneId);
    if (scenesToCheckParent.contains(sceneId)) {
        RandoRegionId regionId = Rando::Logic::GetRegionIdFromEntrance(gSaveContext.save.entrance);
        if (regionParentSceneMap.contains(regionId)) {
            return regionParentSceneMap[regionId];
        }
        // Local divergence from f8bf9bb3b (#1498): upstream's Logic half gives each grotto that shares
        // ENTRANCE(GROTTOS, 4) / (GROTTOS, 10) its own entrance. That half is not ported (it moves the fill), so here
        // those grottos' entrance names no region. Answer the scene the grotto was entered from instead: the region
        // of the entrance the respawn point was last set from outside a grotto (Play_SetupRespawnPoint skips
        // SCENE_KAKUSIANA; upstream keys on the same field), else the last non-grotto scene played.
        if (sceneId == SCENE_KAKUSIANA) {
            const RandoRegionId fromRegionId =
                Rando::Logic::GetRegionIdFromEntrance(gSaveContext.respawn[RESPAWN_MODE_DOWN].entrance);
            const auto fromRegion = Rando::Logic::Regions.find(fromRegionId);
            if (fromRegionId != RR_MAX && fromRegion != Rando::Logic::Regions.end()) {
                const SceneId fromScene = (SceneId)Play_GetOriginalSceneId(fromRegion->second.sceneId);
                if (fromScene != SCENE_MAX && fromScene != SCENE_KAKUSIANA) {
                    return fromScene;
                }
            }
            if (sLastNonGrottoScene >= 0) {
                return (SceneId)Play_GetOriginalSceneId(sLastNonGrottoScene);
            }
        }
    }
    return sceneId;
}

void initializeSceneChecks() {
    sceneChecks.clear();
    regionParentSceneMap.clear();
    checkTypeFiltersAvailable.clear();
    checkTypeFiltersAvailable.insert(RCTYPE_UNKNOWN); // Always include the unknown type

    // Local divergence from f8bf9bb3b (#1498): upstream lists a check only through a region. The root (RR_MAX at
    // SCENE_MAX: the starting Deku Mask and Song of Healing) and a grotto with no parent found are skipped here, and
    // every shuffled check no listed region holds goes under its own static scene below, as before the port.
    std::set<RandoCheckId> listedChecks;
    for (auto& [regionId, staticRegion] : Rando::Logic::Regions) {
        SceneId sceneId = (SceneId)Play_GetOriginalSceneId(staticRegion.sceneId);
        if (sceneId == SCENE_MAX) {
            continue;
        }

        // For some scenes we want to show them in their parent instead
        if (scenesToCheckParent.contains(sceneId)) {
            RandoRegionId connectedRegionId = RR_MAX;
            if (regionId == RR_LONE_PEAK_SHRINE) {
                connectedRegionId = RR_LONE_PEAK_SHRINE;
            } else {
                for (auto& [entrance, _] : staticRegion.exits) {
                    connectedRegionId = Rando::Logic::GetRegionIdFromEntrance(entrance);
                }
                for (auto& [regionId, _] : staticRegion.connections) {
                    connectedRegionId = regionId;
                }
                if (connectedRegionId == RR_MAX) {
                    continue;
                }
            }
            sceneId = Rando::Logic::Regions[connectedRegionId].sceneId;
            regionParentSceneMap[regionId] = sceneId;
        }
        for (auto& [randoCheckId, _] : staticRegion.checks) {
            RandoSaveCheck& randoSaveCheck = RANDO_SAVE_CHECKS[randoCheckId];
            auto& randoStaticCheck = Rando::StaticData::Checks[randoCheckId];
            if (!randoSaveCheck.shuffled) {
                continue;
            }
            checkTypeFiltersAvailable.insert(randoStaticCheck.randoCheckType);
            sceneChecks[sceneId].insert(randoCheckId);
            listedChecks.insert(randoCheckId);
        }
    }
    for (auto& [randoCheckId, randoStaticCheck] : Rando::StaticData::Checks) {
        if (!RANDO_SAVE_CHECKS[randoCheckId].shuffled || listedChecks.contains(randoCheckId)) {
            continue;
        }
        checkTypeFiltersAvailable.insert(randoStaticCheck.randoCheckType);
        sceneChecks[(SceneId)Play_GetOriginalSceneId(randoStaticCheck.sceneId)].insert(randoCheckId);
    }

    sortedSceneIds.clear();
    for (auto& [sceneId, _] : sceneChecks) {
        sortedSceneIds.push_back(sceneId);
    }
    std::sort(sortedSceneIds.begin(), sortedSceneIds.end(),
              [](SceneId a, SceneId b) { return betterSceneIndex[a] < betterSceneIndex[b]; });
}

bool CheckTrackerIsFiltered(RandoCheckId randoCheckId) {
    bool filterCheck = false;
    if (checkTypeFilter.empty()) {
        return false;
    }
    if (!checkTypeFilter.contains(Rando::StaticData::Checks[randoCheckId].randoCheckType)) {
        filterCheck = true;
    }
    return filterCheck;
}

std::unordered_map<RandoCheckId, bool> checksInLogic;
// #1498: an enemy drop is one check listed under every scene its species lives in, so each scene's copy is in logic
// only when a kill in that scene is: the (list scene, drop) pairs with a satisfiable instance, and for each drop the
// tooltip naming those scenes.
static std::set<std::pair<SceneId, RandoCheckId>> dropCopiesInLogic;
static std::unordered_map<RandoCheckId, std::string> dropLogicAreas;
static u32 lastFrame = 0;

static bool IsEnemyDrop(RandoCheckId randoCheckId) {
    auto it = Rando::StaticData::Checks.find(randoCheckId);
    return it != Rando::StaticData::Checks.end() && it->second.randoCheckType == RCTYPE_ENEMY_DROP;
}

// The scene initializeSceneChecks lists region `regionId`'s checks under.
static SceneId TrackerListScene(RandoRegionId regionId) {
    auto parent = regionParentSceneMap.find(regionId);
    if (parent != regionParentSceneMap.end()) {
        return parent->second;
    }
    auto region = Rando::Logic::Regions.find(regionId);
    if (region == Rando::Logic::Regions.end()) {
        return SCENE_MAX;
    }
    return (SceneId)Play_GetOriginalSceneId(region->second.sceneId);
}

// Is the copy of `randoCheckId` listed under `sceneId` in logic? An enemy drop's copy follows its own instances.
static bool CheckCopyInLogic(SceneId sceneId, RandoCheckId randoCheckId) {
    if (IsEnemyDrop(randoCheckId)) {
        return dropCopiesInLogic.contains({ sceneId, randoCheckId });
    }
    return checksInLogic.contains(randoCheckId);
}

static void RecomputeChecksInLogic() {
    checksInLogic.clear();
    dropCopiesInLogic.clear();
    dropLogicAreas.clear();

    // The regions+events fixpoint lives in Rando::CheckTracker::ComputeChecksInLogic, which also brackets the
    // crawl's RANDO_EVENTS clobber (#995): the crawl zeroes and re-fires them in the live save, so an unbracketed
    // call made the MM save bytes depend on whether this window was open.
    std::set<Rando::Logic::CheckInstance> instances;
    for (RandoCheckId randoCheckId :
         Rando::CheckTracker::ComputeChecksInLogic(gSaveContext.save.entrance, &instances)) {
        checksInLogic.insert({ randoCheckId, true });
    }

    std::map<RandoCheckId, std::vector<SceneId>> dropScenes;
    for (auto& [regionId, randoCheckId] : instances) {
        // The instances also name obtained checks (the combo pane files by them); a copy is in logic only unobtained.
        if (!IsEnemyDrop(randoCheckId) || !checksInLogic.contains(randoCheckId)) {
            continue;
        }
        const SceneId sceneId = TrackerListScene(regionId);
        if (sceneId == SCENE_MAX || !dropCopiesInLogic.insert({ sceneId, randoCheckId }).second) {
            continue;
        }
        dropScenes[randoCheckId].push_back(sceneId);
    }
    for (auto& [randoCheckId, scenes] : dropScenes) {
        std::sort(scenes.begin(), scenes.end(),
                  [](SceneId a, SceneId b) { return betterSceneIndex[a] < betterSceneIndex[b]; });
        std::string text = "In logic at: ";
        for (size_t i = 0; i < scenes.size(); i++) {
            text += (i == 0 ? "" : ", ");
            text += Ship_GetSceneName(scenes[i]);
        }
        dropLogicAreas[randoCheckId] = text;
    }
}

void RefreshChecksInLogic() {
    if (MM_gGameState == NULL || MM_gGameState->frames - lastFrame < 20 ||
        CVAR_OUT_OF_LOGIC_MODE == CHECK_MODE_NORMAL) {
        return;
    }

    lastFrame = MM_gGameState->frames;
    RecomputeChecksInLogic();
}

// 2Ship f8bf9bb3b (#1561): Only Show Current Scene compares the scene a row is listed under, not the check's own
// scene, which for an enemy drop is SCENE_SPOT00 wherever it is listed.
static bool CheckListedUnderCurrentScene(SceneId sceneId, s32 rawCurrentSceneId) {
    return sceneId == GetScrollTargetScene(rawCurrentSceneId);
}

#ifdef RSBS_SINGLE_EXECUTABLE
// TEST BRIDGE (#1498): rebuild the scene lists from the live save, compute the in-logic answer, and report the copy
// of `randoCheckId` listed under `sceneId`: 1 in logic, 0 out of logic, -1 not listed there. `tip` (may be NULL)
// receives the drop tooltip ("" for none). `currentSceneId` >= 0 also reports, in *outShown, whether Only Show
// Current Scene keeps that copy while that scene is played.
extern "C" int MM_CheckTracker_TestCheckCopy(uint16_t sceneId, uint16_t randoCheckId, char* tip, int cap,
                                             int currentSceneId, int* outShown) {
    initializeSceneChecks();
    RecomputeChecksInLogic();
    if (tip != nullptr && cap > 0) {
        tip[0] = '\0';
        auto text = dropLogicAreas.find((RandoCheckId)randoCheckId);
        if (text != dropLogicAreas.end() && (int)text->second.size() < cap) {
            memcpy(tip, text->second.c_str(), text->second.size() + 1);
        }
    }
    if (outShown != nullptr && currentSceneId >= 0) {
        *outShown = CheckListedUnderCurrentScene((SceneId)sceneId, currentSceneId) ? 1 : 0;
    }
    auto listed = sceneChecks.find((SceneId)sceneId);
    if (listed == sceneChecks.end() || !listed->second.contains((RandoCheckId)randoCheckId)) {
        return -1;
    }
    return CheckCopyInLogic((SceneId)sceneId, (RandoCheckId)randoCheckId) ? 1 : 0;
}

// TEST BRIDGE (#1498): rebuild the scene lists from the live save and count the shuffled checks listed under no
// scene the window draws (only under SCENE_MAX, or nowhere); up to `cap` of them go to `out` (may be NULL).
extern "C" int MM_CheckTracker_TestUnlistedShuffled(uint16_t* out, int cap) {
    initializeSceneChecks();
    int count = 0;
    for (auto& [randoCheckId, _] : Rando::StaticData::Checks) {
        // RC_UNKNOWN is the table's placeholder row, not a check: it has no scene and is never drawn.
        if (randoCheckId == RC_UNKNOWN || !RANDO_SAVE_CHECKS[randoCheckId].shuffled) {
            continue;
        }
        bool drawn = false;
        for (auto& [sceneId, checks] : sceneChecks) {
            if (sceneId != SCENE_MAX && checks.contains(randoCheckId)) {
                drawn = true;
                break;
            }
        }
        if (!drawn) {
            if (out != nullptr && count < cap) {
                out[count] = (uint16_t)randoCheckId;
            }
            count++;
        }
    }
    return count;
}

// TEST BRIDGE (#1498): the scene Only Show Current Scene and the scroll target answer while raw scene `rawSceneId`
// is played, from the live save's entrance and respawn point. The scene lists must have been built.
extern "C" int MM_CheckTracker_TestScrollTargetScene(int rawSceneId) {
    return (int)GetScrollTargetScene((s32)rawSceneId);
}

// TEST BRIDGE (#1498): after a row restores the live save, rebuild the scene lists from it and forget the in-logic
// answer, so nothing the row authored stays listed for later code in the process.
extern "C" void MM_CheckTracker_TestReset(void) {
    initializeSceneChecks();
    checksInLogic.clear();
    dropCopiesInLogic.clear();
    dropLogicAreas.clear();
}
#endif

void CheckTrackerDrawNonLogicalList() {
    RefreshChecksInLogic();

    for (auto& sceneId : sortedSceneIds) {
        if (sceneId == SCENE_MAX) {
            continue;
        }

        auto& unfilteredChecks = sceneChecks[sceneId];
        std::vector<RandoCheckId> checks;
        uint32_t obtainedCheckSum = 0;

        for (auto& checkId : unfilteredChecks) {
            if (RANDO_SAVE_CHECKS[checkId].obtained) {
                obtainedCheckSum++;
                if (CVAR_COLLECTED_MODE == CHECK_MODE_HIDDEN) {
                    continue;
                }
            } else if (RANDO_SAVE_CHECKS[checkId].skipped) {
                obtainedCheckSum++;
                if (CVAR_SKIPPED_MODE == CHECK_MODE_HIDDEN) {
                    continue;
                }
            } else if (CVAR_OUT_OF_LOGIC_MODE == CHECK_MODE_HIDDEN && !CheckCopyInLogic(sceneId, checkId)) {
                continue;
            }

            if (!sCheckTrackerFilter.PassFilter(Rando::StaticData::CheckNames[checkId].c_str())) {
                continue;
            }

            if (CVAR_SHOW_CURRENT_SCENE && !CheckListedUnderCurrentScene(sceneId, MM_gPlayState->sceneId)) {
                continue;
            }

            if (CheckTrackerIsFiltered(checkId)) {
                continue;
            }

            checks.push_back(checkId);
        }

        if (checks.size() == 0) {
            continue;
        }

        if (!CVAR_SHOW_CURRENT_SCENE && CVAR_SCROLL_TO_SCENE && sScrollToTargetScene != -1 &&
            sScrollToTargetScene == sceneId) {
            ImGui::SetScrollHereY(0.0f);
            sScrollToTargetScene = -1;
            sScrollToTargetEntrance = -1;
        }

        ImGui::PushID(sceneId);
        ImGui::Separator();
        ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0, 0, 0, 0));
        std::string headerText = Ship_GetSceneName(sceneId);
        headerText += " (" + std::to_string(obtainedCheckSum) + "/" + std::to_string(unfilteredChecks.size()) + ")";

        ImGui::PushStyleColor(ImGuiCol_Text, obtainedCheckSum == unfilteredChecks.size()
                                                 ? UIWidgets::ColorValues.at(UIWidgets::Colors::Green)
                                                 : UIWidgets::ColorValues.at(UIWidgets::Colors::White));

        if (sExpandedHeadersState != sExpandedHeadersToggle) {
            ImGui::SetNextItemOpen(sExpandedHeadersToggle);
        }
        if (ImGui::CollapsingHeader(headerText.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Indent(20.0f);
            if (ImGui::BeginTable("Check Tracker", 2)) {
                ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, (16.0f * trackerScale));
                ImGui::TableSetupColumn("Check");
                ImGui::TableNextColumn();
                for (auto& randoCheckId : checks) {
                    Rando::StaticData::RandoStaticCheck& randoStaticCheck = Rando::StaticData::Checks[randoCheckId];
                    RandoSaveCheck& randoSaveCheck = RANDO_SAVE_CHECKS[randoCheckId];
                    ImVec4 textColor = UIWidgets::ColorValues.at(UIWidgets::Colors::White);
                    if (randoSaveCheck.obtained) {
                        if (CVAR_COLLECTED_MODE == CHECK_MODE_COLORED) {
                            textColor = VecFromRGBA8(CVAR_COLLECTED_COLOR);
                        }
                    } else if (randoSaveCheck.skipped) {
                        if (CVAR_SKIPPED_MODE == CHECK_MODE_COLORED) {
                            textColor = VecFromRGBA8(CVAR_SKIPPED_COLOR);
                        }
                    } else if (!CheckCopyInLogic(sceneId, randoCheckId)) {
                        if (CVAR_OUT_OF_LOGIC_MODE == CHECK_MODE_COLORED) {
                            textColor = VecFromRGBA8(CVAR_OUT_OF_LOGIC_COLOR);
                        }
                    }

                    ImGui::PushStyleColor(ImGuiCol_Text, textColor);
                    ImGui::BeginGroup();
                    float cursorPosY = ImGui::GetCursorPosY();
                    if (Rando::StaticData::Checks[randoCheckId].randoCheckType == RCTYPE_OWL) {
                        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 3.5f);
                    }
                    DrawCheckTypeIcon(randoCheckId);
                    ImGui::TableNextColumn();

                    ImGui::SetCursorPosY(cursorPosY);
                    ImGui::Text("%s", Rando::StaticData::CheckNames[randoCheckId].c_str());
                    if (randoSaveCheck.obtained) {
                        ImGui::SameLine();
#ifdef RSBS_SINGLE_EXECUTABLE
                        ImGui::Text("(%s)", ObtainedItemTrackerName(randoCheckId, randoSaveCheck).c_str());
#else
                        ImGui::Text("(%s)", Rando::StaticData::Items[randoSaveCheck.randoItemId].name);
#endif
                    } else if (randoSaveCheck.seen != RCSEEN_UNCHECKED) {
                        ImGui::SameLine();
#ifdef RSBS_SINGLE_EXECUTABLE
                        const std::string seenName = ObtainedItemTrackerName(randoCheckId, randoSaveCheck);
                        const char* itemName = seenName.c_str();
#else
                        const char* itemName = Rando::StaticData::Items[randoSaveCheck.randoItemId].name;
#endif
                        if (randoSaveCheck.seen >= RCSEEN_IDENTIFIED &&
                            (randoStaticCheck.randoCheckType == RCTYPE_SHOP ||
                             randoStaticCheck.randoCheckType == RCTYPE_TINGLE_SHOP)) {
                            ImGui::Text("(%s - %u)", itemName, (unsigned)randoSaveCheck.price);
                        } else {
                            ImGui::Text("(%s)", itemName);
                        }
                    } else if (randoSaveCheck.skipped) {
                        ImGui::SameLine();
                        ImGui::Text("(Skipped)");
                    }
                    ImGui::SameLine();
                    ImGui::Dummy(ImVec2(ImGui::GetContentRegionAvail().x, 0));
                    ImGui::EndGroup();
                    ImGui::PopStyleColor();
                    std::string accessLogicString = accessLogicFuncs.find(randoCheckId) != accessLogicFuncs.end()
                                                        ? accessLogicFuncs[randoCheckId]
                                                        : "";
                    /*
                     * Enemy drop checks are multiple in number and may have unique conditions per location. This
                     * can result in arbitrary particular instances' conditions being displayed for the general
                     * check. Since the basic requirement of defeating the enemy is self-explanatory and the
                     * minimum, we'll omit the logic tooltip for them in particular.
                     *
                     * #1498: in its place, an enemy drop names the scenes where a kill is in logic.
                     */
                    if ((randoCheckId >= RC_ENEMY_DROP_ALIEN && randoCheckId <= RC_ENEMY_DROP_WOLFOS) ||
                        randoCheckId == RC_ENEMY_DROP_GOMESS) {
                        auto dropAreas = dropLogicAreas.find(randoCheckId);
                        if (dropAreas != dropLogicAreas.end()) {
                            UIWidgets::Tooltip(dropAreas->second.c_str());
                        }
                    } else if (accessLogicString != "") {
                        UIWidgets::Tooltip(accessLogicString.c_str());
                    }
                    ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, ImGui::IsItemHovered()
                                                                          ? IM_COL32(255, 255, 0, 128)
                                                                          : IM_COL32(255, 255, 255, 0));
                    if (ImGui::IsItemClicked()) {
                        randoSaveCheck.skipped = !randoSaveCheck.skipped;
                    }
                    ImGui::TableNextColumn();
                }
                ImGui::EndTable();
            }
            ImGui::Unindent(20.0f);
        }
        ImGui::PopStyleColor(2);
        ImGui::PopID();
    }
}

namespace Rando {

namespace CheckTracker {

std::set<RandoCheckId> ComputeChecksInLogic(s32 startEntrance) {
    return ComputeChecksInLogic(startEntrance, nullptr);
}

std::set<RandoCheckId> ComputeChecksInLogic(s32 startEntrance, std::set<Rando::Logic::CheckInstance>* outInstances) {
    // The regions+events fixpoint that used to live inline in the tracker is the factored
    // Rando::Logic::CrawlReachableRegions (ADR 0010 increment 1.3; #500 work item 2) -- one traversal shared with
    // generation's foreign-host reachability gate, with the join-and-recheck discipline the inline copy lacked (a
    // region re-reached with better time slices is re-explored instead of keeping its first-arrival set).
    //
    // The crawl zeroes RANDO_EVENTS and re-fires them. They are inside the live SaveContext, so snapshot the
    // RE_MAX bytes and put them back (#995; ComputeReachableCheckSet brackets the whole struct for the same reason).
    // gCurrentRegionTime is thread_local and outside the save.
    u8 savedEvents[RE_MAX];
    memcpy(savedEvents, RANDO_EVENTS, sizeof(savedEvents));

    std::set<RandoCheckId> inLogic;
    Rando::Logic::ReachabilityCrawl crawl = Rando::Logic::CrawlReachableRegions(startEntrance);
    std::set<RandoCheckId> reached = Rando::Logic::EvaluateReachableChecks(crawl);
    // ADR 0010 answer O2: the reset-time guard, the same function the combo fill's engine applies to the same crawl
    // (Rando/Logic/ResetTimeGuard.h), so a check the fill does not count on before a reset is not listed in logic.
    (void)Rando::Logic::ApplyResetTimeGuard(crawl, reached);

    // The tracker's own semantics on top of the shared crawl: a check is "in logic" when its access lambda passes
    // AND it is shuffled AND not yet obtained.
    std::set<RandoCheckId> shuffledReached;
    for (RandoCheckId randoCheckId : reached) {
        auto& randoSaveCheck = RANDO_SAVE_CHECKS[randoCheckId];
        if (randoSaveCheck.shuffled) {
            shuffledReached.insert(randoCheckId);
            if (!randoSaveCheck.obtained) {
                inLogic.insert(randoCheckId);
            }
        }
    }
    // #1498: the same answer per instance, over the same guarded crawl, for every shuffled check it reaches,
    // obtained or not, so a collected drop keeps the area it was filed under. Before the events are put back: the
    // rows read them.
    if (outInstances != nullptr) {
        *outInstances = Rando::Logic::EvaluateReachableCheckInstances(crawl, shuffledReached);
    }

    memcpy(RANDO_EVENTS, savedEvents, sizeof(savedEvents));
    return inLogic;
}

void CheckTrackerWindow::Draw() {
    if (!CVAR_SHOW_CHECK_TRACKER) {
        return;
    }

    ImGui::PushStyleColor(ImGuiCol_TitleBgActive, trackerBG);
    ImGui::PushStyleColor(ImGuiCol_TitleBg, trackerBG);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, trackerBG);
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 4.0f);

    ImGui::SetNextWindowSize(ImVec2(485.0f, 500.0f), ImGuiCond_FirstUseEver);

#ifdef RSBS_SINGLE_EXECUTABLE
    // OoT's check tracker owns the "Check Tracker" ImGui id on the shared Gui
    // (#392): sharing it would merge the two trackers' ImGui windows/ini state.
    ImGui::Begin("Check Tracker (MM)", nullptr, ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoFocusOnAppearing);
#else
    ImGui::Begin("Check Tracker", nullptr, ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoFocusOnAppearing);
#endif

    trackerBG.w = ImGui::IsWindowDocked() ? 1.0f : CVAR_TRACKER_OPACITY;
    ImGui::SetWindowFontScale(trackerScale);

    if (!MM_gPlayState || !IS_RANDO) {
        ImGui::SetCursorPosX((ImGui::GetWindowWidth() - ImGui::CalcTextSize("No Rando Save Loaded").x) / 2);
        ImGui::SetCursorPosY(ImGui::GetWindowHeight() / 2 - 10.0f);
        ImGui::TextColored(UIWidgets::ColorValues.at(UIWidgets::Colors::Gray), "No Rando Save Loaded");
        ImGui::End();
        ImGui::PopStyleColor(4);
        ImGui::PopStyleVar(1);
        return;
    }

    if (CVAR_SHOW_SEARCH) {
        bool sameLine = !(ImGui::GetContentRegionAvail().x <= 300.0f * trackerScale);
        auto totalCheckCount = GetTotalCheckCount();

        UIWidgets::PushStyleCombobox();
        sCheckTrackerFilter.Draw("##filter", (ImGui::GetContentRegionAvail().x -
                                              (sameLine ? (ImGui::CalcTextSize("Total: ").x +
                                                           ImGui::CalcTextSize(totalCheckCount.c_str()).x + 15.0f)
                                                        : 0)));
        UIWidgets::PopStyleCombobox();
        if (!sCheckTrackerFilter.IsActive()) {
            ImGui::SameLine(18.0f);
            ImGui::Text("Search");
        }

        if (sameLine) {
            ImGui::SameLine(ImGui::GetContentRegionAvail().x -
                            (ImGui::CalcTextSize("Total: ").x + ImGui::CalcTextSize(totalCheckCount.c_str()).x));
        }
        ImGui::Text("Total: %s", totalCheckCount.c_str());
    }

    if (CVAR_SHOW_CHECK_TYPE_FILTER) {
        auto columns = ImGui::GetContentRegionAvail().x / (24.0f * trackerScale + 15.0f);

        if (ImGui::BeginTable("Type Filter", columns)) {
            for (auto randoCheckType : checkTypeFiltersAvailable) {
                ImGui::TableNextColumn();
                ImVec4 buttonCol = checkTypeFilter.contains(randoCheckType)
                                       ? UIWidgets::ColorValues.at(UIWidgets::Colors::Gray)
                                       : UIWidgets::ColorValues.at(UIWidgets::Colors::NoColor);
                ImGui::PushStyleColor(ImGuiCol_Button, buttonCol);
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.0f, 1.0f, 1.0f, 0.2f));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(1.0f, 1.0f, 1.0f, 0.1f));
                TexturePtr textureId = Ship::Context::GetInstance()->GetWindow()->GetGui()->GetTextureByName(
                    randoCheckType == RCTYPE_UNKNOWN ? (const char*)gShootingGalleryOctorokCrossTex
                                                     : checkTypeIconList[randoCheckType]);
                if (randoCheckType == RCTYPE_OWL) {
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 4.0f);
                } else if (randoCheckType == RCTYPE_SONG) {
                    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 4.0f);
                }
                if (ImGui::ImageButton(
                        std::to_string(randoCheckType).c_str(), textureId,
                        ImVec2((randoCheckType == RCTYPE_SONG ? 18.0f : 24.0f) * trackerScale,
                               (randoCheckType == RCTYPE_OWL ? 12.0f * trackerScale : 24.0f * trackerScale) *
                                   trackerScale),
                        ImVec2(0, 0), ImVec2(1, 1), ImVec4(0, 0, 0, 0),
                        randoCheckType == RCTYPE_UNKNOWN        ? ImVec4(1, 0, 0, 1)
                        : randoCheckType == RCTYPE_FREESTANDING ? ImVec4(0.78f, 1, 0.39f, 1)
                                                                : ImVec4(1, 1, 1, 1))) {
                    if (randoCheckType == RCTYPE_UNKNOWN) {
                        checkTypeFilter.clear();
                    } else {
                        if (checkTypeFilter.contains(randoCheckType)) {
                            checkTypeFilter.erase(randoCheckType);
                        } else {
                            checkTypeFilter.insert(randoCheckType);
                        }
                    }
                }
                ImGui::PopStyleColor(3);
            }
            ImGui::EndTable();
        }
    }

    ImGui::BeginChild("Checks");
    CheckTrackerDrawNonLogicalList();
    sExpandedHeadersState = sExpandedHeadersToggle;
    ImGui::EndChild();

    ImGui::End();

    ImGui::PopStyleColor(4);
    ImGui::PopStyleVar(1);
}

void SettingsWindow::DrawElement() {
    if (CVarGetInteger("gWindows.CheckTracker", 0)) {
        UIWidgets::WindowButton("Disable Check Tracker", "gWindows.CheckTracker", BenGui::mRandoCheckTrackerWindow,
                                { .size = UIWidgets::Sizes::Inline, .color = UIWidgets::Colors::Red });
    } else {
        UIWidgets::WindowButton("Enable Check Tracker", "gWindows.CheckTracker", BenGui::mRandoCheckTrackerWindow,
                                { .size = UIWidgets::Sizes::Inline, .color = UIWidgets::Colors::Green });
    }
    if (ImGui::BeginTable("Settings Table", 2)) {
        ImGui::TableSetupColumn("col1", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("col2", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableNextColumn();
        ImGui::SeparatorText("Check Settings");
        // UIWidgets::CVarCheckbox("Dim Out of Logic Checks", CVAR_NAME_OUT_OF_LOGIC_MODE);
        UIWidgets::CVarCombobox("Out of Logic Checks", CVAR_NAME_OUT_OF_LOGIC_MODE, &sCheckModes,
                                UIWidgets::ComboboxOptions()
                                    .DefaultIndex(1)
                                    .ComponentAlignment(UIWidgets::ComponentAlignment::Right)
                                    .LabelPosition(UIWidgets::LabelPosition::Far));
        if (CVAR_OUT_OF_LOGIC_MODE == 1) {
            ImGui::SameLine(ImGui::GetContentRegionAvail().x - 127.0f);
            UIWidgets::CVarColorPicker("##OutOfLogicColor", CVAR_NAME_OUT_OF_LOGIC_COLOR, { 255, 255, 255, 100 }, true);
        }
        UIWidgets::CVarCombobox("Collected Checks", CVAR_NAME_COLLECTED_MODE, &sCheckModes,
                                UIWidgets::ComboboxOptions()
                                    .DefaultIndex(2)
                                    .ComponentAlignment(UIWidgets::ComponentAlignment::Right)
                                    .LabelPosition(UIWidgets::LabelPosition::Far));
        if (CVAR_COLLECTED_MODE == 1) {
            ImGui::SameLine(ImGui::GetContentRegionAvail().x - 127.0f);
            UIWidgets::CVarColorPicker("##CollectedColor", CVAR_NAME_COLLECTED_COLOR, { 100, 255, 100, 255 }, true);
        }
        UIWidgets::CVarCombobox("Skipped Checks", CVAR_NAME_SKIPPED_MODE, &sCheckModes,
                                UIWidgets::ComboboxOptions()
                                    .DefaultIndex(2)
                                    .ComponentAlignment(UIWidgets::ComponentAlignment::Right)
                                    .LabelPosition(UIWidgets::LabelPosition::Far));
        if (CVAR_SKIPPED_MODE == 1) {
            ImGui::SameLine(ImGui::GetContentRegionAvail().x - 127.0f);
            UIWidgets::CVarColorPicker("##SkippedColor", CVAR_NAME_SKIPPED_COLOR, { 255, 100, 255, 255 }, true);
        }
        UIWidgets::CVarCheckbox("Only Show Current Scene", CVAR_NAME_SHOW_CURRENT_SCENE);
        UIWidgets::CVarCheckbox("Auto Scroll To Current Scene", CVAR_NAME_SCROLL_TO_SCENE,
                                UIWidgets::CheckboxOptions().DefaultValue(true));
        if (UIWidgets::Button("Expand/Collapse All Scenes")) {
            sExpandedHeadersToggle = !sExpandedHeadersToggle;
        }

        ImGui::TableNextColumn();
        ImGui::SeparatorText("Window Settings");
        UIWidgets::CVarCheckbox("Show Search", CVAR_NAME_SHOW_SEARCH, UIWidgets::CheckboxOptions().DefaultValue(true));
        UIWidgets::CVarCheckbox("Show Check Type Filters", CVAR_NAME_SHOW_CHECK_TYPE_FILTER);

        if (UIWidgets::CVarSliderFloat("Opacity", CVAR_NAME_TRACKER_OPACITY,
                                       {
                                           .format = "Opacity: %.1f",
                                           .step = 0.10f,
                                           .min = 0.0f,
                                           .max = 1.0f,
                                           .defaultValue = 0.5f,
                                           .labelPosition = UIWidgets::LabelPosition::None,
                                       })) {
            trackerBG.w = CVAR_TRACKER_OPACITY;
        }
        if (UIWidgets::CVarSliderFloat("Scale", CVAR_NAME_TRACKER_SCALE,
                                       {
                                           .format = "Scale: %.1f",
                                           .step = 0.10f,
                                           .min = 0.7f,
                                           .max = 2.5f,
                                           .defaultValue = 1.0f,
                                           .labelPosition = UIWidgets::LabelPosition::None,
                                       })) {
            trackerScale = CVAR_TRACKER_SCALE;
        }

        ImGui::EndTable();
    }
}

void Init() {
    for (auto& [randoRegionId, randoRegion] : Rando::Logic::Regions) {
        for (auto& [randoCheckId, accessLogicFunc] : randoRegion.checks) {
            accessLogicFuncs[randoCheckId] = accessLogicFunc.second;
        }
    }
    S2H::GameHooks::Register<GameInteractor::OnSceneInit>([](s8 sceneId, s8 spawnNum) {
        if (Play_GetOriginalSceneId(sceneId) != SCENE_KAKUSIANA) {
            sLastNonGrottoScene = sceneId;
        }
        if (CVAR_SCROLL_TO_SCENE) {
            sScrollToTargetScene = GetScrollTargetScene(sceneId);
            sScrollToTargetEntrance = gSaveContext.save.entrance;
        }
    });

    trackerBG = { 0, 0, 0, CVAR_TRACKER_OPACITY };
    trackerScale = CVAR_TRACKER_SCALE;
}

void OnFileLoad() {
    if (!IS_RANDO) {
        return;
    }

    initializeSceneChecks();
}

} // namespace CheckTracker

} // namespace Rando
