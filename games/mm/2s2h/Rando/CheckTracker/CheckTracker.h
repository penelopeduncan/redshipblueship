#ifndef RANDO_CHECK_TRACKER_H
#define RANDO_CHECK_TRACKER_H

#include "Rando/Rando.h"
#include <set>
#include <utility>
#include <ship/window/gui/GuiWindow.h>

namespace Rando {

namespace CheckTracker {

void Init();
void OnFileLoad();

/** The name a scene's header carries in the Check Tracker: Ship_GetSceneName, except a scene whose name says a state
 *  or a second place the checks under it are not in ("Southern Swamp (poison)" heads as "Southern Swamp"; 2Ship
 *  cfd1116a4, #1775). The Combo Tracker's MM pane names its areas with the same words (MMSceneAreaName). */
const char* GetCheckTrackerSceneName(SceneId sceneId);

/** The scene the tracker lists region `regionId`'s checks under (initializeSceneChecks' rule, 2Ship f8bf9bb3b): a
 *  region of a scene in scenesToCheckParent (a grotto, a shop or house interior) under the scene of the region it
 *  connects to, any other region under its own scene through Play_GetOriginalSceneId. SCENE_MAX when the tracker
 *  lists the region under no scene (the root, or an interior region with no connection found). The Combo Tracker's
 *  MM pane lists by this same rule (TrackerAdapterSingleExe.cpp, #1706). */
SceneId GetRegionListScene(RandoRegionId regionId);

/** The scene the tracker lists the checks of the interior Link stands in under, while a scene in
 *  scenesToCheckParent is played with save entrance `entrance` (GetScrollTargetScene's parent lookup): the
 *  GetRegionListScene of the entrance's region when that region is in such a scene and has one, else SCENE_MAX (the
 *  tracker then answers the played scene itself, or for a grotto the scene it was entered from). #1706. */
SceneId GetInteriorParentScene(s32 entrance);

/** Whether the tracker lists the checks of scene `sceneId` (an original scene id) under the scene each region of it
 *  opens from (scenesToCheckParent, 2Ship f8bf9bb3b). #1706. */
bool IsListedUnderParentScene(SceneId sceneId);

/** The set of checks the tracker draws as "in logic": reachable from `startEntrance` under the CURRENT save
 *  (CrawlReachableRegions + EvaluateReachableChecks), shuffled and not yet obtained. Gate-free (no CVar, no frame
 *  throttle) so a test reads exactly what the window draws. The crawl zeroes and re-fires RANDO_EVENTS, which lives
 *  inside the SaveContext the departure freeze copies and the .redsave persists, so this brackets those RE_MAX bytes
 *  and leaves the live save byte-identical (Logic.h's contract for a caller that must not perturb it). */
std::set<RandoCheckId> ComputeChecksInLogic(s32 startEntrance);

/** The same set, and in `outInstances` (when not NULL) a per-instance answer (#1498): the (region, check) pairs
 *  whose own row passes, from the same guarded crawl (Rando::Logic::EvaluateReachableCheckInstances), for every
 *  shuffled check that crawl reaches, obtained or not (so the checks it names are the set plus the reached obtained
 *  ones). An enemy drop has one pair per region where its kill is in logic. */
std::set<RandoCheckId> ComputeChecksInLogic(s32 startEntrance,
                                            std::set<std::pair<RandoRegionId, RandoCheckId>>* outInstances);

class CheckTrackerWindow : public Ship::GuiWindow {
  public:
    using GuiWindow::GuiWindow;

    void InitElement() override{};
    void DrawElement() override{};
    void Draw() override;
    void UpdateElement() override{};
};

class SettingsWindow : public Ship::GuiWindow {
  public:
    using GuiWindow::GuiWindow;

    void InitElement() override{};
    void DrawElement() override;
    void UpdateElement() override{};
};

} // namespace CheckTracker

} // namespace Rando

enum { SCENE_LOAD, SCENE_UPDATE };

#endif // RANDO_CHECK_TRACKER_H
