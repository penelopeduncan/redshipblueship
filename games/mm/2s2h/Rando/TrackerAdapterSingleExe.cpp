/**
 * TrackerAdapterSingleExe.cpp — MM's combo-tracker adapter: the offset
 * descriptor over the frozen MM shadow blob, plus the check-name resolver
 * (#458; ADR 0002), and the skip toggle's write into MM's live save (#458 U5)
 * and, for the far pane while OoT is played, into the frozen MM half (#1294),
 * which are made here because common code only reads MM's save (ADR 0008).
 *
 * This is the ONE translation unit where the combo tracker's MM half may see
 * MM's layout: the byte offsets of ShipSaveInfo's rando check table inside
 * SaveContext, the SAVETYPE_RANDO value, and Rando::StaticData's names. All
 * of it crosses into src/common as a flat ComboMMTrackerDesc — the
 * RsbsGameMetaDesc pattern (src/common/save.h) — so common code never
 * includes z64save.h and never hardcodes an MM offset.
 *
 * THE SHADOW BLOB WHILE MM IS NOT PLAYED, THE LIVE SAVE WHILE IT IS. While
 * OoT runs there is no live MM gSaveContext to read (unified storage holds
 * OoT's bytes), but the frozen shadow Context_GetMMSaveContext() hands out is
 * a raw SaveContext image written at freeze/save time. Reading it at these
 * offsets is exactly as valid as the freeze itself, and the view reports the
 * result stale. While MM is played the
 * shadow is useless: MM's arrival consumes it into gSaveContext and zeroes it,
 * and nothing refills it before MM's first save or the departure freeze
 * (#799). So the descriptor also carries MMTrackerLiveSave, which hands the
 * view &gSaveContext — the same layout, the same offsets — only while MM's
 * play state is loaded (ADR 0008 rule 5's amendment), and the view reports
 * that read live.
 *
 * THE TRIPWIRES. The descriptor is built from offsetof/sizeof, so it cannot
 * drift from the struct — what CAN break silently is the geometry: the table
 * outgrowing the MM_SAVE_CONTEXT_SIZE blob capacity (a truncated read), or a
 * flag member leaving its row. The static_asserts below turn both into build
 * errors in the TU that owns the layout knowledge.
 *
 * Lives in 2s2h/Rando/, glob-collected into `2ship_rando` (WHOLE_ARCHIVE), and
 * additionally referenced by name from Combo_TrackerWindow_Init — so unlike
 * the #516 dead-registrar class there is no elision mode where the tracker
 * silently shows an empty MM panel.
 */
#ifdef RSBS_SINGLE_EXECUTABLE

#include <cstddef>
#include <cstdio>
#include <cstring>
#include <string>

#include <algorithm>
#include <deque>
#include <set>
#include <unordered_map>
#include <utility>
#include <vector>

#include "Rando/Rando.h"                          // SaveContext (via variables.h), RC_MAX, SAVETYPE_RANDO, StaticData
#include "2s2h/Rando/CheckTracker/CheckTracker.h" // ComputeChecksInLogic: the in-logic set (#1129)
#include "2s2h/Rando/Foreign.h"                   // ForeignNameForCheck: a crossing host's item (#796)
#include "2s2h/Rando/Logic/Logic.h"               // Regions: the native tracker's grotto/enemy-drop grouping
#include "2s2h/ShipUtils.h"                       // Ship_GetSceneName

extern "C" {
s16 Play_GetOriginalSceneId(s16 sceneId);
}

// CheckTracker.cpp (#1161): the logic MM's check tracker shows on a row's hover, by
// check; global, not in its header.
extern std::unordered_map<RandoCheckId, std::string> accessLogicFuncs;

// src/common. Included OUTSIDE any extern "C" block: the header manages its
// own linkage and pulls context.h, whose <type_traits> include must not be
// wrapped in C linkage (matching Foreign.cpp).
#include "combo_tracker_view.h"
#include "combo_hint_view.h"         // MM_HintAdapter_Register: the Hint Tracker's MM adapter (#1475)
#include "combo_mm_locations_view.h" // Randomizer > MM Locations' check table (#1101 lane R3)
#include "combo_mm_options_view.h"   // MM_Rando_PairedLogicShown: the resolved MM Logic (#1367)
#include "foreign_items.h"           // Combo_GetForeignPlacementForCheck: a crossed item's id, for its tag (#1653)
#include "game.h"                    // MM_SAVE_CONTEXT_SIZE
#include "warp_song_items.h"         // Combo_SharedFamily2DisplayName: a shared warp song's name (#1169 XW14)

// The reader in combo_tracker_view.c walks the row's flag bytes as u8 and the
// header fields as u32; these pin the assumptions to the real types.
static_assert(sizeof(bool) == 1, "RandoSaveCheck's flags are read as single bytes through the descriptor");
static_assert(sizeof(SaveType) == 4, "ShipSaveInfo.saveType is read as a u32 through the descriptor");
static_assert(sizeof(((RandoSaveInfo*)0)->finalSeed) == 4, "finalSeed is read as a u32 through the descriptor");
static_assert(sizeof(((ShipSaveInfo*)0)->fileCreatedAt) == 8, "fileCreatedAt is read as a u64 through the descriptor");

// The whole check table must sit inside the shadow blob capacity, or the
// reader would count truncated rows. GameExports_SingleExe.cpp already
// asserts sizeof(SaveContext) <= MM_SAVE_CONTEXT_SIZE; this narrows it to the
// exact bytes the tracker reads, so an upstream ShipSaveInfo growth that
// pushes the table past the capacity names the actual consumer that breaks.
static_assert(offsetof(SaveContext, save.shipSaveInfo.rando.randoSaveChecks) +
                      sizeof(((RandoSaveInfo*)0)->randoSaveChecks) <=
                  (size_t)MM_SAVE_CONTEXT_SIZE,
              "MM's rando check table no longer fits the cross-game shadow blob; the combo tracker (and the "
              "freeze machinery) would truncate it");

// The departure capture's record holds every MM check id (#1129), and since
// save format 6 it is exactly the saved check capacity (a capture bit per saved
// row), so the two can only move together, at a save format generation.
static_assert((unsigned)RC_MAX <= RSBS_TRACKER_AVAIL_MM_CAP,
              "MM's RC_MAX outgrew the tracker availability capture (context.h): a save format generation");
static_assert(RSBS_TRACKER_AVAIL_MM_CAP == (unsigned)RSBS_MM_SAVE_CHECK_CAP,
              "the MM tracker capture (context.h) and MM's saved check capacity (z64save.h) are one capacity");

// Every flag the descriptor names must live inside one row's stride, and adding
// RandoSaveCheck.seen at byte 9 (the 1-byte alignment gap before u16 price at
// byte 10) must not grow RandoSaveCheck past 12 bytes.
static_assert(sizeof(RandoSaveCheck) == 12 && offsetof(RandoSaveCheck, seen) == 9 &&
                  offsetof(RandoSaveCheck, price) == 10,
              "RandoSaveCheck.seen must occupy the alignment byte before price without changing row size");
static_assert(offsetof(RandoSaveCheck, shuffled) < sizeof(RandoSaveCheck) &&
                  offsetof(RandoSaveCheck, obtained) < sizeof(RandoSaveCheck) &&
                  offsetof(RandoSaveCheck, skipped) < sizeof(RandoSaveCheck) &&
                  offsetof(RandoSaveCheck, seen) < sizeof(RandoSaveCheck) &&
                  offsetof(RandoSaveCheck, price) + sizeof(u16) <= sizeof(RandoSaveCheck),
              "RandoSaveCheck flag and price offsets must stay within the row stride");

namespace {

/**
 * Display name for an MM check id, from the same CheckNames array MM's own
 * check tracker labels rows with. Empty (RC_MAX empty strings) until
 * PopulateCheckNames runs — MM_TrackerAdapter_Register below calls it, so the
 * names exist the moment the adapter does, MM booted or not (#489 cause 2 was
 * exactly this array staying empty in single-exe). Returns NULL rather than
 * "" so the renderer's "no name -> show the id" fallback triggers.
 */
const char* MMTrackerCheckName(uint16_t checkId) {
    if (checkId >= (uint16_t)RC_MAX) {
        return nullptr;
    }
    const std::string& name = Rando::StaticData::CheckNames[checkId];
    return name.empty() ? nullptr : name.c_str();
}

// ---- #458 U4: areas and placed items --------------------------------------

// MM's scene order (scene_table.h's betterMapSelectIndex): the order MM's own
// check tracker sorts its scene headers in (CheckTracker.cpp's betterSceneIndex).
#define DEFINE_SCENE(_name, enumValue, _textId, _drawConfig, _restrictionFlags, _persistentCycleFlags, \
                     _entranceSceneId, betterMapSelectIndex, _humanName)                               \
    { enumValue, betterMapSelectIndex },
#define DEFINE_SCENE_UNSET(_enumValue)
const std::unordered_map<s32, s32> kMMSceneOrder = {
#include "tables/scene_table.h"
};
#undef DEFINE_SCENE
#undef DEFINE_SCENE_UNSET

/**
 * The scene MMCheckScenes lists region `regionId`'s grotto checks, interior checks
 * and enemy drops under: MM's own tracker's rule, read from it
 * (Rando::CheckTracker::GetRegionListScene, #1706). A region of a scene in its
 * scenesToCheckParent table (a grotto, or a shop or house interior since 2Ship
 * f8bf9bb3b) goes under the scene of the region it connects to (Lone Peak Shrine
 * connects to itself), any other region under its own scene through
 * Play_GetOriginalSceneId. SCENE_MAX for a region it lists nowhere (the root, an
 * interior with no connection).
 */
s16 MMRegionListScene(RandoRegionId regionId) {
    return (s16)Rando::CheckTracker::GetRegionListScene(regionId);
}

/** Whether MM's tracker lists region `region`'s checks under the scene it opens from (a grotto or an interior). */
bool MMRegionListedUnderParent(const Rando::Logic::RandoRegion& region) {
    return Rando::CheckTracker::IsListedUnderParentScene((SceneId)Play_GetOriginalSceneId(region.sceneId));
}

bool MMIsEnemyDrop(RandoCheckId checkId) {
    const auto check = Rando::StaticData::Checks.find(checkId);
    return check != Rando::StaticData::Checks.end() && check->second.randoCheckType == RCTYPE_ENEMY_DROP;
}

// #1127, #1711: every scene a check is listed under, by check id, built with MMCheckScenes: each region's scene
// (MMRegionListScene) once in region order; empty only for a check no listed region holds (the root's starting items),
// which has one place, its MMCheckScenes scene.
std::vector<std::vector<s16>> sCheckPlaces;

// #1706: the scene Randomizer > MM Locations lists each check under, built with MMCheckScenes by its rule before
// #1706 (a grotto's checks under the scene it opens from, an enemy drop under its first region's own scene, any other
// check under its own scene): MM's own Check Filter tab lists its checks with no areas at all, so parity asks
// nothing of that page, and its shop and house interiors keep their own headers.
std::vector<s16> sLocationScenes;

/**
 * The scene each check is listed under, by the rule MM's check tracker grouped
 * its rows with (CheckTracker.cpp, initializeSceneChecks): the check's own scene
 * through Play_GetOriginalSceneId, except a check of a scene MM's tracker lists
 * under its parent (scenesToCheckParent: a grotto, and since 2Ship f8bf9bb3b a shop
 * or house interior, #1706), which goes under the scene the first region (in
 * region order) of such a scene that lists it opens from (MMRegionListScene), and
 * an enemy drop, which goes under the first region that lists it. MM's tracker
 * lists every check of those scenes in such regions only, so the pane's one place
 * is the native one. A check neither rule places (an interior with no
 * connection) keeps its own scene rather than vanishing, as MM's tracker now
 * lists it too (its local divergence from f8bf9bb3b, #1498); no such region lacks
 * a connection in today's graph.
 *
 * #1127, #1711: MM's tracker lists a check under EVERY scene a region that holds
 * it is listed under (initializeSceneChecks), so this also fills sCheckPlaces:
 * each such region's scene (MMRegionListScene), once, in region order. sScenes
 * stays the check's own listing scene, which the Hint Tracker reads; sCheckPlaces
 * is where the pane lists it, and the two differ only where MM's tracker lists a
 * check away from its own scene (such as the Moon's Tear or the Oath to Order).
 *
 * Built from Rando::Logic::Regions, which registration fills during bring-up, so
 * the table is rebuilt whenever the region count changes (the #659 entrance
 * cache's rule) instead of freezing a half-registered graph.
 */
const std::vector<s16>& MMCheckScenes(void) {
    static std::vector<s16> sScenes;
    static size_t sBuiltAt = (size_t)-1;
    if (sBuiltAt == Rando::Logic::Regions.size() && !sScenes.empty()) {
        return sScenes;
    }
    sScenes.assign((size_t)RC_MAX, (s16)SCENE_MAX);
    sCheckPlaces.assign((size_t)RC_MAX, std::vector<s16>());
    sLocationScenes.assign((size_t)RC_MAX, (s16)SCENE_MAX);
    for (const auto& [_, check] : Rando::StaticData::Checks) {
        const s16 own = Play_GetOriginalSceneId(check.sceneId);
        if (check.randoCheckType == RCTYPE_ENEMY_DROP) {
            continue;
        }
        if (own != (s16)SCENE_KAKUSIANA) {
            sLocationScenes[check.randoCheckId] = own;
        }
        if (!Rando::CheckTracker::IsListedUnderParentScene((SceneId)own)) {
            sScenes[check.randoCheckId] = own;
        }
    }
    for (const auto& [regionId, region] : Rando::Logic::Regions) {
        const s16 scene = MMRegionListScene(regionId);
        const bool grotto = Play_GetOriginalSceneId(region.sceneId) == (s16)SCENE_KAKUSIANA;
        // MM Locations' area (#1706 leaves it as it was): a grotto's parent, any other region's own scene.
        const s16 locationScene = grotto ? scene : Play_GetOriginalSceneId(region.sceneId);
        const bool underParent = MMRegionListedUnderParent(region);
        for (const auto& [checkId, _] : region.checks) {
            const bool drop = MMIsEnemyDrop(checkId);
            if (locationScene != (s16)SCENE_MAX && (grotto || drop) && sLocationScenes[checkId] == (s16)SCENE_MAX) {
                sLocationScenes[checkId] = locationScene;
            }
            if (scene == (s16)SCENE_MAX) {
                continue;
            }
            if ((underParent || drop) && sScenes[checkId] == (s16)SCENE_MAX) {
                sScenes[checkId] = scene;
            }
            std::vector<s16>& places = sCheckPlaces[checkId];
            if (std::find(places.begin(), places.end(), scene) == places.end()) {
                places.push_back(scene);
            }
        }
    }
    for (const auto& [_, check] : Rando::StaticData::Checks) {
        if (sScenes[check.randoCheckId] == (s16)SCENE_MAX) {
            sScenes[check.randoCheckId] = Play_GetOriginalSceneId(check.sceneId);
        }
        if (sLocationScenes[check.randoCheckId] == (s16)SCENE_MAX) {
            sLocationScenes[check.randoCheckId] = Play_GetOriginalSceneId(check.sceneId);
        }
    }
    sBuiltAt = Rando::Logic::Regions.size();
    return sScenes;
}

/**
 * A scene's area key: the order MM's tracker sorts its scene headers in (scene
 * order in the high byte), keeping two scenes of one order apart (the scene id
 * in the low byte; MM has fewer than 256 scenes).
 */
uint16_t MMSceneAreaKey(s16 scene) {
    const auto order = kMMSceneOrder.find(scene);
    const uint16_t rank = (order != kMMSceneOrder.end()) ? (uint16_t)(order->second & 0xFF) : 0xFF;
    return (uint16_t)((rank << 8) | ((uint16_t)scene & 0xFF));
}

/**
 * The area scene `scene`'s rows are listed under: its name, as MM's tracker heads
 * the scene (Rando::CheckTracker::GetCheckTrackerSceneName, 2Ship #1775), keyed by
 * MMSceneAreaKey. NULL for no scene.
 */
const char* MMSceneAreaName(s16 scene, uint16_t* outKey) {
    if (scene < 0 || scene >= (s16)SCENE_MAX) {
        return nullptr; // not a check MM's static data knows
    }
    if (outKey != nullptr) {
        *outKey = MMSceneAreaKey(scene);
    }
    return Rando::CheckTracker::GetCheckTrackerSceneName((SceneId)scene);
}

/** The area a check is listed under by MMCheckScenes' static rule: its row's area, which the Hint Tracker reads. */
const char* MMTrackerStaticAreaName(uint16_t checkId, uint16_t* outKey) {
    if (checkId >= (uint16_t)RC_MAX) {
        return nullptr;
    }
    return MMSceneAreaName(MMCheckScenes()[checkId], outKey);
}

/** The area Randomizer > MM Locations' table lists a check under (sLocationScenes, #1706). */
const char* MMLocationsAreaName(uint16_t checkId, uint16_t* outKey) {
    if (checkId >= (uint16_t)RC_MAX) {
        return nullptr;
    }
    (void)MMCheckScenes(); // builds sLocationScenes with it
    return MMSceneAreaName(sLocationScenes[checkId], outKey);
}

// #1498: from the last in-logic computation (MMRefreshInLogic), the (drop, list scene) copies of an unobtained enemy
// drop in logic, the places where its kill is (#1127: each place the combo pane lists the drop in is greyed from
// these, as MM's tracker greys each scene's copy), and the tooltip naming every such scene. Computed with the
// availability, never per frame, and only answered for the save it was computed on (sDropAreasSeed /
// sDropAreasCreatedAt) while Enable Available Checks is on.
std::set<std::pair<uint16_t, s16>> sDropCopiesInLogic;
std::unordered_map<uint16_t, std::string> sDropLogicText;
bool sDropAreasValid = false;
uint32_t sDropAreasSeed = 0;
uint64_t sDropAreasCreatedAt = 0;

bool MMAvailableChecksOn(void);

/** Whether the drop areas were computed on the MM save the combo pane reads now. */
bool MMDropAreasApply(void) {
    if (!sDropAreasValid || !MMAvailableChecksOn()) {
        return false;
    }
    const auto* shown = static_cast<const SaveContext*>(Combo_TrackerMMSave(nullptr));
    return shown != nullptr && shown->save.shipSaveInfo.rando.finalSeed == sDropAreasSeed &&
           shown->save.shipSaveInfo.fileCreatedAt == sDropAreasCreatedAt;
}

/**
 * The descriptor's areaName: the check's own listing scene (MMTrackerStaticAreaName),
 * which the Hint Tracker reads to map a scene key to an MM hint area. Where the
 * Check Tracker pane lists the check is MMTrackerAreaPlaceAt (#1127, #1711); the
 * two differ only where MM's tracker lists a check away from its own scene.
 */
const char* MMTrackerAreaName(uint16_t checkId, uint16_t* outKey) {
    return MMTrackerStaticAreaName(checkId, outKey);
}

/** The descriptor's areaPlaceCount (#1127, #1711): how many scenes MMCheckScenes lists check `checkId` under. */
int MMTrackerAreaPlaceCount(uint16_t checkId) {
    if (checkId >= (uint16_t)RC_MAX) {
        return 0;
    }
    (void)MMCheckScenes(); // builds sCheckPlaces with it
    return sCheckPlaces[checkId].empty() ? 1 : (int)sCheckPlaces[checkId].size();
}

/**
 * The descriptor's areaPlaceAt (#1127, #1711): the `place`th scene MM's tracker
 * lists check `checkId` under, named and keyed as MMSceneAreaName does (which
 * differs from MMTrackerAreaName where MM's tracker lists the check away from its
 * own scene), with in `*outAvailable` for an enemy drop whether its kill there is
 * in logic, from the last availability computation on the save shown
 * (MMDropAreasApply), or -1 for any other check or without one (the row's
 * per-check answer stands, as CheckCopyInLogic does). NULL out of range.
 */
const char* MMTrackerAreaPlaceAt(uint16_t checkId, int place, uint16_t* outKey, int* outAvailable) {
    if (outAvailable != nullptr) {
        *outAvailable = -1;
    }
    if (place < 0 || place >= MMTrackerAreaPlaceCount(checkId)) {
        return nullptr;
    }
    const std::vector<s16>& places = sCheckPlaces[checkId];
    if (places.empty()) {
        return MMTrackerStaticAreaName(checkId, outKey);
    }
    const s16 scene = places[(size_t)place];
    if (outAvailable != nullptr && MMIsEnemyDrop((RandoCheckId)checkId) && MMDropAreasApply()) {
        *outAvailable = sDropCopiesInLogic.count({ checkId, scene }) != 0 ? 1 : 0;
    }
    return MMSceneAreaName(scene, outKey);
}

/**
 * The item an obtained check's row names, from the SaveContext image `save`:
 * MM's own check tracker's rule (CheckTracker.cpp, ObtainedItemTrackerName,
 * #796) without its " (OoT)" suffix, which the window adds from `*outGame`. A
 * check that hosts an OoT item holds RI_JUNK in MM's table, so the crossed item
 * comes first; otherwise the item MM's table stores in this save. #1499: on a
 * seen, uncollected row over MM's LIVE save, what the player will receive there
 * (Rando::UncollectedNamedItem, the MM tracker's rule): a placed item already
 * owned for good reads as the junk it becomes. A frozen image is named as stored
 * (ownership is MM's live state).
 */
const char* MMTrackerPlacedItemName(const void* save, uint16_t checkId, uint8_t* outGame, uint16_t* outId) {
    if (save == nullptr || checkId == (uint16_t)RC_UNKNOWN || checkId >= (uint16_t)RC_MAX) {
        return nullptr;
    }
    if (const char* foreignName = Rando::Foreign::ForeignNameForCheck((RandoCheckId)checkId)) {
        if (outGame != nullptr) {
            *outGame = (uint8_t)GAME_OOT;
        }
        if (outId != nullptr) {
            // #1653: the crossed item's own id, which picks its tag.
            const SharedItem* crossed = Combo_GetForeignPlacementForCheck(checkId);
            *outId = crossed != nullptr ? crossed->id : (uint16_t)0;
        }
        return foreignName;
    }
    const RandoSaveCheck& row = static_cast<const SaveContext*>(save)->save.shipSaveInfo.rando.randoSaveChecks[checkId];
    const RandoItemId named = (save == static_cast<const void*>(&gSaveContext) && !row.obtained)
                                  ? Rando::UncollectedNamedItem(row.randoItemId, (RandoCheckId)checkId)
                                  : row.randoItemId;
    const auto item = Rando::StaticData::Items.find(named);
    if (item == Rando::StaticData::Items.end() || item->second.name == nullptr) {
        return nullptr;
    }
    if (outGame != nullptr) {
        *outGame = (uint8_t)GAME_MM;
    }
    if (outId != nullptr) {
        *outId = (uint16_t)named;
    }
    // #1169 XW14: a shared warp song is named without its game (OoTMM's isItemAmbiguous);
    // #1560: so is an added OoT mask under its shared rule.
    if (const char* shared = Combo_SharedFamily2DisplayName((uint8_t)GAME_MM, (uint16_t)named)) {
        return shared;
    }
    if (const char* mask = Combo_SharedFamilyDisplayName((uint8_t)GAME_MM, (uint16_t)named)) {
        return mask;
    }
    return item->second.name;
}

/**
 * Whether check `checkId` carries a purchase price in RandoSaveCheck.price to
 * print once identified (seen >= RCSEEN_IDENTIFIED): MM's shop slots
 * (RCTYPE_SHOP) and Tingle's map offers (RCTYPE_TINGLE_SHOP).
 */
bool MMTrackerCanBePurchased(uint16_t checkId) {
    if (checkId == (uint16_t)RC_UNKNOWN || checkId >= (uint16_t)RC_MAX) {
        return false;
    }
    const auto check = Rando::StaticData::Checks.find((RandoCheckId)checkId);
    if (check == Rando::StaticData::Checks.end()) {
        return false;
    }
    return check->second.randoCheckType == RCTYPE_SHOP || check->second.randoCheckType == RCTYPE_TINGLE_SHOP;
}

// The reachability lock's stand-ins (MM_TrackerAdapter_TestLiveSeam and
// MM_TrackerAdapter_TestInLogicSeam below, #1129). sLiveTest: -1 is production;
// 0 or 1 answers "does MM play a randomizer file now" in place of MM's play state.
// sInLogicAuthored: the computation is replaced by the authored set
// sInLogicAuthoredIds. sInLogicCalls counts the computations made.
int sLiveTest = -1;
bool sInLogicAuthored = false;
std::vector<uint16_t> sInLogicAuthoredIds;
int sInLogicCalls = 0;

/**
 * MM's live save for the view's LIVE read, or NULL (#799). Only while MM's play
 * state is loaded: MM_gPlayState is set late in MM_Play_Init, after the
 * arrival consumes the frozen half into gSaveContext (z_play.c), and nulled
 * by MM_Play_Destroy and MM_Graph_ResetRunFrameContext (graph.c). That excludes the
 * title screen, which authors an unmarked bootstrap save (Sram_InitNewSave),
 * and file select, which reads each slot through the live buffer while it
 * scans them — neither is the player's world.
 */
const void* MMTrackerLiveSave(void) {
    if (sLiveTest >= 0) {
        return sLiveTest == 1 ? static_cast<const void*>(&gSaveContext) : nullptr;
    }
    return (MM_gPlayState != nullptr) ? static_cast<const void*>(&gSaveContext) : nullptr;
}

// MM's "newf" sentinel, mirroring SaveManager.cpp's RsbsRegisterMMMetaOnce.
const char kMMNewf[6] = { 'Z', 'E', 'L', 'D', 'A', '3' };

// ---- #458 U5: the skip toggle ------------------------------------------------

/**
 * The skip write against the SaveContext image `save`: the byte MM's own check
 * tracker flips on a row click (CheckTracker.cpp), which MM's next save persists.
 * Only in a marked rando save, and only on a check SoH's skip button is drawn on
 * (part of the seed, not found), the one rule the combo window keeps for both
 * games. MM's native tracker also flips a found check's flag, with no visible
 * effect there. False, with nothing written, otherwise.
 */
bool MMTrackerSetSkippedIn(SaveContext* save, uint16_t checkId, bool skipped) {
    if (save == nullptr || checkId >= (uint16_t)RC_MAX ||
        std::memcmp(save->save.saveInfo.playerData.newf, kMMNewf, sizeof(kMMNewf)) != 0 ||
        save->save.shipSaveInfo.saveType != SAVETYPE_RANDO) {
        return false;
    }
    RandoSaveCheck& row = save->save.shipSaveInfo.rando.randoSaveChecks[checkId];
    if (!row.shuffled || row.obtained) {
        return false;
    }
    row.skipped = skipped;
    return true;
}

/**
 * The descriptor's setLiveSkipped (#458 U5): the write lands in MM's live save
 * under the conditions the view's LIVE read has (MM is the active game and its
 * play state is loaded, MMTrackerLiveSave's rule), never in the frozen shadow.
 * It is made here, in MM's TU, because common code only reads MM's save
 * (ADR 0008 rule 5).
 */
bool MMTrackerSetLiveSkipped(uint16_t checkId, bool skipped) {
    if (Context_GetCurrentGame() != GAME_MM || MM_gPlayState == nullptr) {
        return false;
    }
    return MMTrackerSetSkippedIn(&gSaveContext, checkId, skipped);
}

/**
 * The descriptor's setShadowSkipped (#1294): the far pane's write, the same byte
 * in the FROZEN MM half while Ocarina of Time plays the paired file. That half is
 * the one the next commit stores as the .redsave's Tier-3 and MM's arrival
 * consumes into gSaveContext (Combo_ConsumeFrozenState), and nothing else
 * rewrites it while OoT runs, so the skip is MM's own from here on, as if MM's
 * tracker had taken it before the departure. Context_GetFrozenMMSaveContextForWrite
 * answers only while the half is armed; MMTrackerSetSkippedIn refuses an unmarked
 * or non-rando half and a check its button is not drawn on.
 */
bool MMTrackerSetShadowSkipped(uint16_t checkId, bool skipped) {
    if (Context_GetCurrentGame() != GAME_OOT || !Combo_ForeignPairingActive()) {
        return false;
    }
    static_assert(sizeof(SaveContext) <= MM_SAVE_CONTEXT_SIZE, "the frozen MM half holds a whole SaveContext");
    return MMTrackerSetSkippedIn(static_cast<SaveContext*>(Context_GetFrozenMMSaveContextForWrite()), checkId, skipped);
}

} // namespace

// ---- #1129: reachability ----------------------------------------------------

namespace {

/**
 * Whether MM plays a randomizer file now: MM is the active game, its play state
 * is loaded (MMTrackerLiveSave's rule) and the live save is a marked rando save,
 * or the lock's stand-in for all three.
 */
bool MMPlaysRandoFile(void) {
    if (Context_GetCurrentGame() != GAME_MM) {
        return false;
    }
    if (sLiveTest >= 0) {
        return sLiveTest == 1;
    }
    return MM_gPlayState != nullptr &&
           std::memcmp(gSaveContext.save.saveInfo.playerData.newf, kMMNewf, sizeof(kMMNewf)) == 0 &&
           gSaveContext.save.shipSaveInfo.saveType == SAVETYPE_RANDO;
}

/** SoH's Enable Available Checks: one set of tracker settings for both panes (ruling D3). */
bool MMAvailableChecksOn(void) {
    return CVarGetInteger("gTrackers.CheckTracker.EnableAvailableChecks", 0) != 0;
}

// The in-logic set the LIVE panel shows, one byte per check, and when it was
// computed: refreshed at most every 20 frames of one game state, the cadence of
// MM's own tracker (CheckTracker.cpp, RefreshChecksInLogic), so the crawl runs
// once per refresh rather than once per row. A new game state (a file load, an
// arrival) recomputes at once.
std::vector<uint8_t> sInLogic;
bool sInLogicValid = false;
const void* sInLogicState = nullptr;
u32 sInLogicFrame = 0;

void MMRefreshInLogic(bool force) {
    const void* state = MM_gGameState;
    const u32 frame = (MM_gGameState != nullptr) ? MM_gGameState->frames : 0;
    if (!force && sInLogicValid && state == sInLogicState && (state == nullptr || frame - sInLogicFrame < 20)) {
        return;
    }
    sInLogicCalls++;
    sInLogic.assign((size_t)RC_MAX, 0);
    sDropCopiesInLogic.clear();
    sDropLogicText.clear();
    sDropAreasValid = false;
    if (sInLogicAuthored) {
        for (uint16_t id : sInLogicAuthoredIds) {
            if (id < (uint16_t)RC_MAX) {
                sInLogic[id] = 1;
            }
        }
    } else {
        // Gate-free and bracketed (#995): the live save is left byte-identical.
        std::set<std::pair<RandoRegionId, RandoCheckId>> instances;
        for (RandoCheckId id : Rando::CheckTracker::ComputeChecksInLogic(gSaveContext.save.entrance, &instances)) {
            sInLogic[id] = 1;
        }
        // #1498: an unobtained drop is in logic in each place where one of its instances is (the instances also
        // name obtained drops, which are in logic nowhere, as MM's tracker greys them); #1127 lists the drop in
        // every place and greys each from these. The tooltip names every such area once.
        std::unordered_map<uint16_t, std::vector<s16>> dropScenes;
        for (const auto& [regionId, checkId] : instances) {
            const auto region = Rando::Logic::Regions.find(regionId);
            if (!MMIsEnemyDrop(checkId) || region == Rando::Logic::Regions.end() || sInLogic[checkId] == 0) {
                continue;
            }
            const s16 scene = MMRegionListScene(regionId);
            if (scene < 0 || scene >= (s16)SCENE_MAX) {
                continue;
            }
            sDropCopiesInLogic.insert({ (uint16_t)checkId, scene });
            std::vector<s16>& scenes = dropScenes[(uint16_t)checkId];
            if (std::find(scenes.begin(), scenes.end(), scene) == scenes.end()) {
                scenes.push_back(scene);
            }
        }
        for (auto& [checkId, scenes] : dropScenes) {
            std::sort(scenes.begin(), scenes.end(), [](s16 a, s16 b) { return MMSceneAreaKey(a) < MMSceneAreaKey(b); });
            std::string text = "In logic at: ";
            for (size_t i = 0; i < scenes.size(); i++) {
                text += (i == 0 ? "" : ", ");
                text += Ship_GetSceneName(scenes[i]);
            }
            sDropLogicText[checkId] = text;
        }
        sDropAreasValid = true;
        sDropAreasSeed = gSaveContext.save.shipSaveInfo.rando.finalSeed;
        sDropAreasCreatedAt = gSaveContext.save.shipSaveInfo.fileCreatedAt;
    }
    sInLogicValid = true;
    sInLogicState = state;
    sInLogicFrame = frame;
}

/**
 * The descriptor's liveAvailable (combo_tracker_view.h): MM's own tracker's
 * in-logic set (shuffled, not obtained, reachable from the save's entrance) for
 * the LIVE panel, while Enable Available Checks is on and MM plays a randomizer
 * file; -1 otherwise, with nothing computed.
 */
int MMTrackerLiveAvailable(uint16_t checkId) {
    if (checkId >= (uint16_t)RC_MAX || !MMAvailableChecksOn() || !MMPlaysRandoFile()) {
        return -1;
    }
    MMRefreshInLogic(false);
    return sInLogic[checkId] != 0 ? 1 : 0;
}

/**
 * The descriptor's logicText (#1161): the tooltip MM's own check tracker draws on
 * a check's row (CheckTracker.cpp, DrawCheckTracker), from the table it draws it
 * from, accessLogicFuncs, which CheckTracker::Init fills from Rando::Logic::Regions
 * (each region's CHECK macro, stringified) at MM's rando init; that TU is linked in
 * the single exe (redship.map: 2ship_rando_ui:CheckTracker.cpp.obj, pulled in by
 * Rando::Init's CheckTracker::Init call). Its rule: none for an empty string, and
 * none for an enemy drop (RC_ENEMY_DROP_ALIEN .. RC_ENEMY_DROP_WOLFOS, plus
 * RC_ENEMY_DROP_GOMESS, appended at the end of the enum), whose conditions
 * differ per instance. NULL before MM's rando init has filled it. #1498: an enemy
 * drop answers MM's tracker's replacement instead, the areas where its kill is in
 * logic, from the last availability computation on the save shown (MMRefreshInLogic);
 * NULL when it has none.
 */
const char* MMTrackerCheckLogic(uint16_t checkId) {
    if (checkId >= (uint16_t)RC_MAX) {
        return nullptr;
    }
    if ((checkId >= (uint16_t)RC_ENEMY_DROP_ALIEN && checkId <= (uint16_t)RC_ENEMY_DROP_WOLFOS) ||
        checkId == (uint16_t)RC_ENEMY_DROP_GOMESS) {
        const auto text = sDropLogicText.find(checkId);
        if (text == sDropLogicText.end() || !MMDropAreasApply()) {
            return nullptr;
        }
        return text->second.c_str();
    }
    auto it = accessLogicFuncs.find((RandoCheckId)checkId);
    if (it == accessLogicFuncs.end() || it->second.empty()) {
        return nullptr;
    }
    return it->second.c_str();
}

// ---- #1131: the current area ---------------------------------------------

// The navigation lock's stand-in (MM_TrackerAdapter_TestSceneSeam below): -1 is
// production (MM's play state); 0 answers "no MM play state"; 1 answers "MM is
// played in sSceneTestId, room sSceneTestRoom" (-1: a room that is no grotto's).
int sSceneTest = -1;
int sSceneTestId = 0;
int sSceneTestRoom = -1;
// The last scene the op saw that was not a grotto, or -1: the scene a grotto
// was entered from.
s16 sAreaLastScene = -1;

// Lone Peak Shrine (#1189): the grotto scene's room ObjGrass.cpp keys the shrine's
// grass on (grassObjMap, { SCENE_KAKUSIANA, 5 } -> RC_LONE_PEAK_SHRINE_GRASS_01),
// and one of its checks. MM's tracker connects RR_LONE_PEAK_SHRINE to itself
// (initializeSceneChecks), so the shrine's checks are listed under the grotto
// scene, not under the scene it opens from as every other grotto's are.
constexpr s8 kLonePeakShrineRoom = 5;
constexpr RandoCheckId kLonePeakShrineCheck = RC_LONE_PEAK_SHRINE_LENS_CHEST;

/**
 * The descriptor's currentArea: the area of the scene MM is played in, the scene
 * MM's own check tracker scrolls to on OnSceneInit and filters by under Only
 * Show Current Scene (CheckTracker.cpp: Play_GetOriginalSceneId of the play
 * state's scene), keyed as the rows are (MMSceneAreaKey). A grotto
 * (SCENE_KAKUSIANA) answers the scene it was entered from, the last other scene
 * the op saw (the window asks every frame, open or not), because MM's tracker
 * lists a grotto's checks under the scene its grotto opens from (MMCheckScenes);
 * none if it saw none. A shop or house interior (#1706, 2Ship f8bf9bb3b) answers
 * the scene MM's tracker lists its checks under, the scene the region of the
 * save's entrance opens from (Rando::CheckTracker::GetInteriorParentScene), or
 * itself when that region names none, as MM's tracker answers there.
 * Lone Peak Shrine is the exception (#1189): it answers the
 * scene MMCheckScenes lists its checks under, so Only Show Current Area keeps
 * them and the scroll lands on them, and it leaves the scene a grotto was
 * entered from as it was. False unless MM plays a randomizer file (#1189, ruling
 * D11, as OoT's op answers only with one loaded). With no MM play state (MM's
 * file select, or the Day Telop between two play states of one file at dawn,
 * which respawns the player where the day ended, grotto included) it keeps the
 * scene a grotto was entered from. A play state on any other save (MM's title
 * demo, on an unmarked bootstrap save, or a vanilla file) forgets it, so the
 * next file's first grotto cannot answer this file's scene.
 */
bool MMTrackerCurrentArea(uint16_t* outKey) {
    const bool playState = (sSceneTest >= 0) ? (sSceneTest == 1) : (MM_gPlayState != nullptr);
    if (!playState) {
        return false;
    }
    if (!MMPlaysRandoFile()) {
        sAreaLastScene = -1;
        return false;
    }
    s16 scene = -1;
    int room = -1;
    if (sSceneTest >= 0) {
        scene = (s16)sSceneTestId;
        room = sSceneTestRoom;
    } else {
        scene = Play_GetOriginalSceneId(MM_gPlayState->sceneId);
        room = MM_gPlayState->roomCtx.curRoom.num;
    }
    if (scene == (s16)SCENE_KAKUSIANA && room == (int)kLonePeakShrineRoom) {
        scene = MMCheckScenes()[kLonePeakShrineCheck];
    } else if (scene == (s16)SCENE_KAKUSIANA) {
        scene = sAreaLastScene;
    } else {
        sAreaLastScene = scene;
        // #1706: inside a shop or house interior, the scene MM's tracker lists the interior's checks under, from the
        // region of the save's entrance (CheckTracker.cpp, GetScrollTargetScene); the interior itself when that
        // region names none, as there.
        if (Rando::CheckTracker::IsListedUnderParentScene((SceneId)scene)) {
            const s16 parent = (s16)Rando::CheckTracker::GetInteriorParentScene(gSaveContext.save.entrance);
            if (parent >= 0 && parent < (s16)SCENE_MAX) {
                scene = parent;
            }
        }
    }
    if (scene < 0 || scene >= (s16)SCENE_MAX) {
        return false;
    }
    *outKey = MMSceneAreaKey(scene);
    return true;
}

} // namespace

/**
 * The departure capture (#1129; combo_tracker_view.h). MM_Game_Suspend calls it
 * after the harvest, while gSaveContext is still MM's. No randomizer file played
 * (the title screen, file select): the last capture stands. Otherwise the in-logic
 * set computed afresh on the departing save, or no capture with Enable Available
 * Checks off.
 */
extern "C" void MM_TrackerAdapter_CaptureAvailability(void) {
    if (!MMPlaysRandoFile()) {
        return;
    }
    if (!MMAvailableChecksOn()) {
        Combo_TrackerFreezeAvailability((uint8_t)GAME_MM, nullptr, 0);
        return;
    }
    MMRefreshInLogic(true);
    std::vector<uint8_t> bits(((size_t)RC_MAX + 7) / 8, 0);
    for (size_t i = 0; i < (size_t)RC_MAX; i++) {
        if (sInLogic[i] != 0) {
            bits[i >> 3] |= (uint8_t)(1u << (i & 7));
        }
    }
    Combo_TrackerFreezeAvailability((uint8_t)GAME_MM, bits.data(), (uint32_t)RC_MAX);
}

extern "C" void MM_TrackerAdapter_Register(void) {
    // Pure overwrite over StaticData::Checks, idempotent, needs no archive —
    // the #457/#489 re-homing precedent. Called here (not from a file-scope
    // registrar) so it cannot race the static init of the maps it reads.
    Rando::StaticData::PopulateCheckNames();

    ComboMMTrackerDesc desc = {};
    desc.newfOffset = (uint32_t)offsetof(SaveContext, save.saveInfo.playerData.newf);
    desc.newfLen = 6;
    // MM's "newf" sentinel: an all-zero shadow (MM never entered) fails this
    // compare and reads as "no data" instead of as a vanilla save with zero
    // progress.
    std::memcpy(desc.newf, kMMNewf, sizeof(kMMNewf));
    desc.saveTypeOffset = (uint32_t)offsetof(SaveContext, save.shipSaveInfo.saveType);
    desc.saveTypeRando = (uint32_t)SAVETYPE_RANDO;
    desc.finalSeedOffset = (uint32_t)offsetof(SaveContext, save.shipSaveInfo.rando.finalSeed);
    desc.checkTableOffset = (uint32_t)offsetof(SaveContext, save.shipSaveInfo.rando.randoSaveChecks);
    desc.checkStride = (uint32_t)sizeof(RandoSaveCheck);
    desc.checkCount = (uint32_t)RC_MAX;
    desc.shuffledOffset = (uint32_t)offsetof(RandoSaveCheck, shuffled);
    desc.obtainedOffset = (uint32_t)offsetof(RandoSaveCheck, obtained);
    desc.skippedOffset = (uint32_t)offsetof(RandoSaveCheck, skipped);
    desc.checkName = MMTrackerCheckName;
    desc.areaName = MMTrackerAreaName;
    desc.placedItemName = MMTrackerPlacedItemName;
    desc.liveSave = MMTrackerLiveSave;
    desc.setLiveSkipped = MMTrackerSetLiveSkipped;
    desc.liveAvailable = MMTrackerLiveAvailable;
    desc.logicText = MMTrackerCheckLogic;
    desc.currentArea = MMTrackerCurrentArea;
    desc.setShadowSkipped = MMTrackerSetShadowSkipped;
    desc.seenOffset = (uint32_t)offsetof(RandoSaveCheck, seen);
    desc.priceOffset = (uint32_t)offsetof(RandoSaveCheck, price);
    desc.canBePurchased = MMTrackerCanBePurchased;
    desc.areaPlaceCount = MMTrackerAreaPlaceCount;
    desc.areaPlaceAt = MMTrackerAreaPlaceAt;

    Combo_Tracker_RegisterMM(&desc);

    // #1475: the combined Hint Tracker's MM adapter (HintSourcesSingleExe.cpp)
    // reads through this descriptor's save pick, so it comes up with it, on every
    // path that brings this one up (Combo_TrackerWindow_Init, which main.cpp calls).
    MM_HintAdapter_Register();
}

/**
 * TEST BRIDGE (redship --test combo-tracker-view, #458 U4): store the MM item
 * whose display name is `name` at check `checkId` of the SaveContext image
 * `save`, as a generated world would, so the ROM-free lock can author a placed
 * item without seeing RandoItemId. Returns 1 when written, 0 for a bad argument
 * or a name no MM item carries.
 */
extern "C" int MM_TrackerAdapter_TestSetStoredItem(void* save, uint16_t checkId, const char* name) {
    if (save == nullptr || name == nullptr || checkId >= (uint16_t)RC_MAX) {
        return 0;
    }
    for (const auto& [randoItemId, item] : Rando::StaticData::Items) {
        if (item.name != nullptr && std::strcmp(item.name, name) == 0) {
            static_cast<SaveContext*>(save)->save.shipSaveInfo.rando.randoSaveChecks[checkId].randoItemId = randoItemId;
            return 1;
        }
    }
    return 0;
}

/**
 * TEST BRIDGE (redship --test combo-tracker-view, #458 U5): the skip toggle's
 * write against the SaveContext image `save`, the same code the descriptor's
 * setLiveSkipped runs on &gSaveContext. The ROM-free lock has no MM play state,
 * so its test descriptor points setLiveSkipped here with its authored live
 * buffer (as it points liveSave at that buffer). Returns 1 when the flag now
 * holds `skipped`, 0 when refused with nothing written.
 */
extern "C" int MM_TrackerAdapter_TestSetSkippedIn(void* save, uint16_t checkId, int skipped) {
    return MMTrackerSetSkippedIn(static_cast<SaveContext*>(save), checkId, skipped != 0) ? 1 : 0;
}

/**
 * TEST BRIDGE (redship --test mm-tingle-map-display, #1499): the name the
 * Combo Tracker's MM pane gives check `checkId`'s item over the SaveContext image
 * `save` (the descriptor's placedItemName), or NULL.
 */
extern "C" const char* MM_TrackerAdapter_TestPlacedItemName(const void* save, uint16_t checkId) {
    return MMTrackerPlacedItemName(save, checkId, nullptr, nullptr);
}

/**
 * TEST BRIDGE (#1129): @p live -1 restores production; 0 or 1 answers "does MM
 * play a randomizer file now" for the reachability op, the departure capture and
 * (#1189) the current-area op, in place of MM's play state, which no ROM-free or rando tier has (1 also hands
 * the view &gSaveContext as the live save, as the play state would).
 */
extern "C" void MM_TrackerAdapter_TestLiveSeam(int live) {
    sLiveTest = live < 0 ? -1 : (live != 0 ? 1 : 0);
}

/**
 * TEST BRIDGE (#1129): with @p authored nonzero, MM's in-logic computation is
 * replaced by the @p count ids at @p ids (the ROM-free tier has no region graph
 * to crawl); with 0 the real Rando::CheckTracker::ComputeChecksInLogic runs.
 * Every call drops the cached set, so the next read computes again. @return the
 * computations made since the previous call, which resets the count.
 */
extern "C" int MM_TrackerAdapter_TestInLogicSeam(int authored, const uint16_t* ids, int count) {
    sInLogicAuthored = authored != 0;
    sInLogicValid = false;
    // #1498: the drop copies in logic and tooltips came from the previous computation's save; drop them with it.
    sDropCopiesInLogic.clear();
    sDropLogicText.clear();
    sDropAreasValid = false;
    sInLogicAuthoredIds.clear();
    if (ids != nullptr && count > 0) {
        sInLogicAuthoredIds.assign(ids, ids + count);
    }
    const int calls = sInLogicCalls;
    sInLogicCalls = 0;
    return calls;
}

// ---- #1131: the navigation lock's stand-ins -------------------------------

/**
 * TEST BRIDGE (#1131): @p mode -1 restores production; 0 stands in for "no MM
 * play state" and 1 for "MM is played in scene @p sceneId", in a room that is no
 * grotto's (the ROM-free tier has no play state), for the current-area op, which
 * also asks MMPlaysRandoFile (MM_TrackerAdapter_TestLiveSeam stands in for it).
 * Mode -1 also forgets the scene a grotto was entered from (a fresh session);
 * mode 0 keeps it, as the op does with no play state (#1189: the Day Telop).
 */
extern "C" void MM_TrackerAdapter_TestSceneSeam(int mode, int sceneId) {
    sSceneTest = mode < 0 ? -1 : (mode != 0 ? 1 : 0);
    sSceneTestId = sceneId;
    sSceneTestRoom = -1;
    if (sSceneTest < 0) {
        sAreaLastScene = -1;
    }
}

/**
 * TEST BRIDGE (#1189): mode 1 of MM_TrackerAdapter_TestSceneSeam in room @p room
 * of scene @p sceneId, for the grotto scene's rooms (Lone Peak Shrine's is
 * MM_TrackerAdapter_TestLonePeakShrine's).
 */
extern "C" void MM_TrackerAdapter_TestSceneRoomSeam(int sceneId, int room) {
    MM_TrackerAdapter_TestSceneSeam(1, sceneId);
    sSceneTestRoom = room;
}

/**
 * TEST BRIDGE (#1189): Lone Peak Shrine's room in the grotto scene, in @p outRoom
 * (which may be NULL), and @return one of its checks.
 */
extern "C" int MM_TrackerAdapter_TestLonePeakShrine(int* outRoom) {
    if (outRoom != nullptr) {
        *outRoom = (int)kLonePeakShrineRoom;
    }
    return (int)kLonePeakShrineCheck;
}

/** TEST BRIDGE (#1131): check @p checkId's own scene, through Play_GetOriginalSceneId; -1 for none. */
extern "C" int MM_TrackerAdapter_TestCheckScene(uint16_t checkId) {
    const auto check = Rando::StaticData::Checks.find((RandoCheckId)checkId);
    if (check == Rando::StaticData::Checks.end()) {
        return -1;
    }
    return Play_GetOriginalSceneId(check->second.sceneId);
}

/** TEST BRIDGE (#1131): MM's grotto scene, whose checks MM's tracker lists under the scene each opens from. */
extern "C" int MM_TrackerAdapter_TestGrottoScene(void) {
    return (int)SCENE_KAKUSIANA;
}

/** TEST BRIDGE (2Ship #1775): the area name the combo pane gives scene @p sceneId's rows (MMSceneAreaName). */
extern "C" const char* MM_TrackerAdapter_TestSceneAreaName(int sceneId) {
    return MMSceneAreaName((s16)sceneId, nullptr);
}

/** TEST BRIDGE (#1498): the area key of scene @p sceneId's rows (MMSceneAreaKey). */
extern "C" uint16_t MM_TrackerAdapter_TestSceneAreaKey(int sceneId) {
    return MMSceneAreaKey((s16)sceneId);
}

/**
 * TEST BRIDGE (#1498): in @p outKey, the area key the combo pane files region
 * @p regionId's enemy drops under (MMRegionListScene). @return 1 when written, 0
 * for an unknown region or a grotto with no connection.
 */
extern "C" int MM_TrackerAdapter_TestRegionAreaKey(int regionId, uint16_t* outKey) {
    const auto region = Rando::Logic::Regions.find((RandoRegionId)regionId);
    if (outKey == nullptr || region == Rando::Logic::Regions.end()) {
        return 0;
    }
    const s16 scene = MMRegionListScene((RandoRegionId)regionId);
    if (scene < 0 || scene >= (s16)SCENE_MAX) {
        return 0;
    }
    *outKey = MMSceneAreaKey(scene);
    return 1;
}

/** TEST BRIDGE (#1498): the static rule's area key for check @p checkId (MMCheckScenes); 0xFFFF for none. */
extern "C" uint16_t MM_TrackerAdapter_TestStaticAreaKey(uint16_t checkId) {
    uint16_t key = 0xFFFF;
    (void)MMTrackerStaticAreaName(checkId, &key);
    return key;
}

/**
 * TEST BRIDGE (#1127): the area keys of the places the combo pane lists check
 * @p checkId in (the descriptor's areaPlaceCount / areaPlaceAt), up to @p cap of
 * them in @p outKeys (may be NULL), and in @p outAvailable (may be NULL) each
 * place's own answer (1 / 0 / -1). @return how many places it has.
 */
extern "C" int MM_TrackerAdapter_TestAreaPlaces(uint16_t checkId, uint16_t* outKeys, int* outAvailable, int cap) {
    const int count = MMTrackerAreaPlaceCount(checkId);
    for (int i = 0; i < count && i < cap; i++) {
        uint16_t key = 0xFFFF;
        int available = -1;
        (void)MMTrackerAreaPlaceAt(checkId, i, &key, &available);
        if (outKeys != nullptr) {
            outKeys[i] = key;
        }
        if (outAvailable != nullptr) {
            outAvailable[i] = available;
        }
    }
    return count;
}

/**
 * TEST BRIDGE (#1706, the ROM-free ComboTrackerWindow leg): with @p install
 * nonzero, give the region graph MM's West Clock Town and Bomb Shop regions, cut
 * to one check each and the shop's door both ways (Regions/Central.cpp's
 * RR_BOMB_SHOP exit and West Clock Town's exit into it), when the graph has no
 * region of its own (the ROM-free tier registers none; a graph that has them
 * already is used as it is), and stand the live save's entrance in the Bomb Shop
 * (@p install 2: West Clock Town's entrance from the shop, whose region is the
 * town's). @p install 0 takes out what an install put in and gives the entrance
 * back. In the out-parameters (each may be NULL): the shop's check, West Clock
 * Town's, and the two scenes. @return 1 when both regions are in the graph after
 * an install; 1 after an uninstall.
 */
extern "C" int MM_TrackerAdapter_TestInteriorGraph(int install, uint16_t* outInteriorCheck, uint16_t* outParentCheck,
                                                   int* outInteriorScene, int* outParentScene) {
    static bool sInstalled = false;
    static bool sEntranceHeld = false;
    static s32 sHeldEntrance = 0;
    const RandoCheckId shopCheck = RC_BOMB_SHOP_ITEM_01;
    const RandoCheckId townCheck = RC_CLOCK_TOWN_WEST_BANK_ADULTS_WALLET;
    if (outInteriorCheck != nullptr) {
        *outInteriorCheck = (uint16_t)shopCheck;
    }
    if (outParentCheck != nullptr) {
        *outParentCheck = (uint16_t)townCheck;
    }
    if (outInteriorScene != nullptr) {
        *outInteriorScene = (int)SCENE_BOMYA;
    }
    if (outParentScene != nullptr) {
        *outParentScene = (int)SCENE_ICHIBA;
    }
    if (install == 0) {
        if (sInstalled) {
            Rando::Logic::Regions.erase(RR_BOMB_SHOP);
            Rando::Logic::Regions.erase(RR_CLOCK_TOWN_WEST);
            Rando::Logic::InvalidateEntranceRegionCache();
            sInstalled = false;
        }
        if (sEntranceHeld) {
            gSaveContext.save.entrance = sHeldEntrance;
            sEntranceHeld = false;
        }
        return 1;
    }
    if (!sEntranceHeld) {
        sHeldEntrance = gSaveContext.save.entrance;
        sEntranceHeld = true;
    }
    if (Rando::Logic::Regions.empty()) {
        Rando::Logic::RandoRegion shop;
        shop.name = "Bomb Shop";
        shop.sceneId = SCENE_BOMYA;
        shop.checks[shopCheck] = { [] { return true; }, "true" };
        shop.exits[ENTRANCE(WEST_CLOCK_TOWN, 6)] = { ENTRANCE(BOMB_SHOP, 0), [] { return true; }, "true" };
        Rando::Logic::RandoRegion town;
        town.name = "West Clock Town";
        town.sceneId = SCENE_ICHIBA;
        town.checks[townCheck] = { [] { return true; }, "true" };
        town.exits[ENTRANCE(BOMB_SHOP, 0)] = { ENTRANCE(WEST_CLOCK_TOWN, 6), [] { return true; }, "true" };
        Rando::Logic::Regions[RR_BOMB_SHOP] = shop;
        Rando::Logic::Regions[RR_CLOCK_TOWN_WEST] = town;
        Rando::Logic::InvalidateEntranceRegionCache();
        sInstalled = true;
    }
    // 2: the town's side of the door, an entrance whose region (West Clock Town) is no interior's.
    gSaveContext.save.entrance = install == 2 ? ENTRANCE(WEST_CLOCK_TOWN, 6) : ENTRANCE(BOMB_SHOP, 0);
    return Rando::Logic::Regions.count(RR_BOMB_SHOP) != 0 && Rando::Logic::Regions.count(RR_CLOCK_TOWN_WEST) != 0 ? 1
                                                                                                                  : 0;
}

/**
 * TEST BRIDGE (#1127, the UI snapshot's "drop-places" state): an enemy drop the
 * snapshot lists in two places, and its name as the combo pane prints it in
 * @p name (may be NULL; "" when the names are not populated yet).
 */
extern "C" uint16_t MM_TrackerAdapter_TestSnapshotDrop(char* name, int cap) {
    const uint16_t drop = (uint16_t)RC_ENEMY_DROP_LIKE_LIKE;
    if (name != nullptr && cap > 0) {
        const char* text = MMTrackerCheckName(drop);
        snprintf(name, (size_t)cap, "%s", text != nullptr ? text : "");
    }
    return drop;
}

// ---- #1101 lane R3: Randomizer > MM Locations' check table ------------------

namespace {

/**
 * True while MM's pool generation ignores gRando.ExcludedChecks: under Vanilla
 * logic GeneratePools.cpp places every check's vanilla item and never reads the
 * list, which is why MM's own Check Filter tab refused to draw there. #1367: the
 * RESOLVED value, not the RO_LOGIC CVar: a paired world's MM Logic follows the
 * pair's [Both Games] Logic (Glitchless or No Logic, never Vanilla), so a
 * Vanilla left in the key no longer disables a list the next world reads. Only a
 * loaded world created with Vanilla before #1367 reads true here, and its page
 * is frozen anyway.
 */
bool MMLocationsVanillaLogic(void) {
    return MM_Rando_PairedLogicShown() == RO_LOGIC_VANILLA;
}

} // namespace

/**
 * Publish Randomizer > MM Locations' check table to src/common
 * (combo_mm_locations_view.h). One row per check MM's pool can hold: every
 * static check but RC_UNKNOWN and the two Stone Tower pots GeneratePools.cpp
 * never adds (excluding a check the pool never holds would be a control that
 * changes nothing). Each row carries the name and an area (MMTrackerCheckName,
 * MMLocationsAreaName above: MM's own check tracker's names and scene order; an
 * enemy drop keeps its first region here, the page having one row per check,
 * #1127; a shop or house interior keeps its own scene, which the check tracker no
 * longer lists it under, #1706), sorted by area, then id, the order the page draws.
 *
 * The rows are built once and own copies of their names: CheckNames[id] is
 * reassigned (a move from a temporary, which frees each longer name's old
 * buffer) by every PopulateCheckNames, and MM's tracker bring-up and the combo
 * tracker adapter both call it after this table exists, so a cached c_str()
 * would dangle. The areas follow sLocationScenes' rule instead: a grotto's or an
 * enemy drop's area comes from the region graph, so the areas are re-resolved
 * and the rows re-sorted in place whenever the region count changes (the page
 * calls this each frame it draws). In place: the row count and the vector's
 * storage never change, so the registry's pointer stays valid. Idempotent.
 */
extern "C" void MM_RandoLocationsUi_Register(void) {
    static std::vector<ComboMMLocationDesc> sTable;
    static std::deque<std::string> sNames; // a deque never moves an element it holds
    static size_t sAreasAt = (size_t)-1;
    if (sTable.empty()) {
        Rando::StaticData::PopulateCheckNames();
        // Reserved first: the registry keeps a raw pointer into the vector.
        sTable.reserve(Rando::StaticData::Checks.size());
        for (const auto& [randoCheckId, check] : Rando::StaticData::Checks) {
            if (randoCheckId == RC_UNKNOWN || check.sceneId == SCENE_LAST_BS) {
                continue;
            }
            const char* name = MMTrackerCheckName((uint16_t)randoCheckId);
            sNames.emplace_back(name != nullptr ? name : (check.name != nullptr ? check.name : ""));
            ComboMMLocationDesc desc = {};
            desc.id = (uint16_t)randoCheckId;
            desc.name = sNames.back().c_str();
            sTable.push_back(desc);
        }
    }
    if (sAreasAt != Rando::Logic::Regions.size()) {
        for (ComboMMLocationDesc& desc : sTable) {
            uint16_t areaKey = 0xFFFF;
            const char* areaName = MMLocationsAreaName(desc.id, &areaKey);
            desc.areaName = areaName != nullptr ? areaName : "";
            desc.areaKey = areaKey;
        }
        std::sort(sTable.begin(), sTable.end(), [](const ComboMMLocationDesc& a, const ComboMMLocationDesc& b) {
            return a.areaKey != b.areaKey ? a.areaKey < b.areaKey : a.id < b.id;
        });
        sAreasAt = Rando::Logic::Regions.size();
    }
    Combo_RegisterMMLocationTable(sTable.data(), (int)sTable.size(), MMLocationsVanillaLogic);
}

#endif // RSBS_SINGLE_EXECUTABLE
