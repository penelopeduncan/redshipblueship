#ifndef RANDO_H
#define RANDO_H

#include "StaticData/StaticData.h"
#include "Types.h"
#include "variables.h"

#define IS_RANDO (gSaveContext.save.shipSaveInfo.saveType == SAVETYPE_RANDO)
#define RANDO_SAVE_CHECKS gSaveContext.save.shipSaveInfo.rando.randoSaveChecks
#define RANDO_SAVE_OPTIONS gSaveContext.save.shipSaveInfo.rando.randoSaveOptions
#define RANDO_EVENTS gSaveContext.save.shipSaveInfo.rando.randoEvents

namespace Rando {

void Init();
void DrawItem(RandoItemId randoItemId, Actor* actor = nullptr);
void GiveItem(RandoItemId randoItemId);
void RemoveItem(RandoItemId randoItemId);
RandoItemId CurrentJunkItem();
bool IsItemObtainable(RandoItemId randoItemId, RandoCheckId randoCheckId = RC_UNKNOWN);
RandoItemId ConvertItem(RandoItemId randoItemId, RandoCheckId randoCheckId = RC_UNKNOWN);
#ifdef RSBS_SINGLE_EXECUTABLE
// #1499 (ConvertItem.cpp): CurrentJunkItem for a sale the player paid for (never
// RI_NONE); whether a check is such a sale; and the item an uncollected check's
// tracker row or hint names (RI_JUNK for an item already owned for good).
RandoItemId CurrentPaidJunkItem();
bool IsPaidCheck(RandoCheckId randoCheckId);
RandoItemId UncollectedNamedItem(RandoItemId randoItemId, RandoCheckId randoCheckId);
#endif
RandoCheckId FindItemPlacement(RandoItemId randoItemId);
void RegisterMenu();

std::vector<RandoItemId> GetComputedStartingItems(RandoSaveInfo& randoSaveInfo);
void GrantStartingItems();
std::vector<RandoItemId> GetStartingItemsFromSpoiler(nlohmann::json& spoiler);
void SetStartingItemsInSpoiler(nlohmann::json& spoiler, std::vector<RandoItemId>& startingItems);
std::vector<RandoItemId> GetStartingItemsFromSave(RandoSaveInfo& randoSaveInfo);
void SetStartingItemsInSave(RandoSaveInfo& randoSaveInfo, std::vector<RandoItemId>& startingItems);
std::vector<RandoItemId> GetStartingItemsFromConfig();
void SetStartingItemsInConfig(std::vector<RandoItemId>& startingItems);

} // namespace Rando

#endif
