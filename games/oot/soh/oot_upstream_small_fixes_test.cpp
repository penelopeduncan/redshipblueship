/**
 * @file oot_upstream_small_fixes_test.cpp
 * ROM-free, display-free locks for the small Ship of Harkinian fixes ported for #1742, #1747, #1748 and #1750. Each
 * row is one `extern "C" int OoT_<Row>_RunHeadless(void)` (0 on pass), dispatched from src/common/test_runner.cpp and
 * registered in CMake/SingleExecutable.cmake (label "redship"). Every row drives the pure decision helper the fix put
 * in the game code, so the rows need no play state, no OTRGlobals and no ROM.
 */

#ifdef RSBS_SINGLE_EXECUTABLE

// At file scope, not inside an extern "C" block: z64.h includes <memory> under C++.
#include <z64.h>

#include "context.h"
#include "soh/ShipInit.hpp"
#include "soh/cvar_prefixes.h"
#include "soh/Enhancements/cosmetics/cosmeticsTypes.h"
#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/Enhancements/game-interactor/GameInteractor_Hooks.h"
#include "src/overlays/actors/ovl_Fishing/z_fishing.h"

#include <cstdio>
#include <cstring>
#include <string>

extern "C" {
#include "macros.h"
#include "variables.h"
}

#define USF_ASSERT(cond, msg)                                             \
    do {                                                                  \
        if (!(cond)) {                                                    \
            printf("[TEST] FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__); \
            return 1;                                                     \
        }                                                                 \
    } while (0)

namespace {

bool UsfIsSite(const ShipInitEntry& entry, const char* fileName) {
    const std::string file = entry.registeredAt.file_name();
    const std::string want = fileName;
    return file.size() >= want.size() && file.compare(file.size() - want.size(), want.size(), want) == 0 &&
           (file.size() == want.size() || file[file.size() - want.size() - 1] == '/' ||
            file[file.size() - want.size() - 1] == '\\');
}

// How many registrars @p fileName put in ShipInit's map under @p key.
int UsfCountRegistrars(const char* key, const char* fileName) {
    int found = 0;
    for (const auto& entry : ShipInit::GetAll()[key]) {
        if (UsfIsSite(entry, fileName)) {
            found++;
        }
    }
    return found;
}

// Re-runs @p fileName's registrars under @p key, the way a change of that key does.
void UsfRunRegistrars(const char* key, const char* fileName) {
    for (const auto& entry : ShipInit::GetAll()[key]) {
        if (UsfIsSite(entry, fileName)) {
            entry.initFunc();
        }
    }
}

// Restores an integer CVar the row changed: cleared, never stored, when it read 0 before (gate-config
// contamination); the same rule as OoTDarkLinkFixes.
void UsfRestoreInt(const char* key, int value) {
    if (value != 0) {
        CVarSetInteger(key, value);
    } else {
        CVarClear(key);
    }
}

} // namespace

// ---- OoTSwimVoidRespawnRoom (SoH c93b6188c, #1748) ----

// hook_handlers.cpp, beside swimSpecialRespawnInfo.
u8 Randomizer_SwimVoidRespawnRoom(s16 sceneNum, s32 entranceIndex, const EntranceEntry* setupEntranceList,
                                  u8 currentRoom);

extern "C" int OoT_SwimVoidRespawnRoom_RunHeadless(void) {
    // ENTR_ZORAS_RIVER_3 is a swimSpecialRespawnInfo key (Hyrule Field to Zora's River, in the water).
    const u8 spawn = (u8)gEntranceTable[ENTR_ZORAS_RIVER_3].spawn;
    USF_ASSERT(gEntranceTable[ENTR_ZORAS_RIVER_3].scene == SCENE_ZORAS_RIVER,
               "ENTR_ZORAS_RIVER_3 is not in Zora's River: the row's premise is gone");
    USF_ASSERT(spawn < 8, "ENTR_ZORAS_RIVER_3's spawn does not fit the row's fake entrance list");
    EntranceEntry list[8];
    for (int i = 0; i < 8; i++) {
        list[i].spawn = (u8)i;
        list[i].room = 1;
    }
    list[spawn].room = 0;

    // (a) the entrance's own scene: the respawn takes the entrance's room, not the room Link swam into.
    const u8 sameScene = Randomizer_SwimVoidRespawnRoom(SCENE_ZORAS_RIVER, ENTR_ZORAS_RIVER_3, list, 1);
    printf("[TEST] (a) Zora's River, swam into room 1: respawn room %d\n", sameScene);
    USF_ASSERT(sameScene == 0, "(a): the swim-void respawn kept the current room instead of the entrance's room");

    // (b) another scene: the list belongs to that scene, so the current room stands.
    const u8 otherScene = Randomizer_SwimVoidRespawnRoom(SCENE_HYRULE_FIELD, ENTR_ZORAS_RIVER_3, list, 1);
    USF_ASSERT(otherScene == 1, "(b): a room index was read from another scene's entrance list");

    // (c) no entrance list, or an index out of the table: the current room stands.
    USF_ASSERT(Randomizer_SwimVoidRespawnRoom(SCENE_ZORAS_RIVER, ENTR_ZORAS_RIVER_3, nullptr, 1) == 1,
               "(c): a null entrance list did not keep the current room");
    USF_ASSERT(Randomizer_SwimVoidRespawnRoom(SCENE_ZORAS_RIVER, ENTR_MAX, list, 1) == 1,
               "(c): an out-of-range entrance index did not keep the current room");

    printf("[TEST] oot-swim-void-respawn-room: PASS\n");
    return 0;
}

// ---- OoTItemUnequipBow (SoH 16ee01404, #1747) ----

// games/oot/soh/Enhancements/ItemUnequip.cpp.
bool OoT_ItemUnequip_ShouldUnequip(u16 cursorItem, u16 cursorSlot, u8 equippedItem, u8 equippedSlot);

extern "C" int OoT_ItemUnequipBow_RunHeadless(void) {
    const u16 slot = SLOT_BOW;
    // (a) the fix: the bow under the cursor unequips the bow-with-a-magic-arrow on the pressed button.
    USF_ASSERT(OoT_ItemUnequip_ShouldUnequip(ITEM_BOW, slot, ITEM_BOW_ARROW_FIRE, slot),
               "(a): the bow does not unequip the fire-arrow bow");
    USF_ASSERT(OoT_ItemUnequip_ShouldUnequip(ITEM_BOW, slot, ITEM_BOW_ARROW_ICE, slot),
               "(a): the bow does not unequip the ice-arrow bow");
    USF_ASSERT(OoT_ItemUnequip_ShouldUnequip(ITEM_BOW, slot, ITEM_BOW_ARROW_LIGHT, slot),
               "(a): the bow does not unequip the light-arrow bow");
    // (b) unchanged: the same item, an arrow over its own bow, and a different item.
    USF_ASSERT(OoT_ItemUnequip_ShouldUnequip(ITEM_BOW, slot, ITEM_BOW, slot), "(b): the bow does not unequip itself");
    USF_ASSERT(OoT_ItemUnequip_ShouldUnequip(ITEM_ARROW_FIRE, SLOT_ARROW_FIRE, ITEM_BOW_ARROW_FIRE, slot),
               "(b): fire arrows no longer unequip the fire-arrow bow");
    USF_ASSERT(!OoT_ItemUnequip_ShouldUnequip(ITEM_BOW, slot, ITEM_SLINGSHOT, SLOT_SLINGSHOT),
               "(b): the bow unequips the slingshot");
    USF_ASSERT(!OoT_ItemUnequip_ShouldUnequip(ITEM_ARROW_FIRE, SLOT_ARROW_FIRE, ITEM_BOW_ARROW_ICE, slot),
               "(b): fire arrows unequip the ice-arrow bow");
    // (c) bottles still need the same slot.
    USF_ASSERT(!OoT_ItemUnequip_ShouldUnequip(ITEM_BOTTLE, SLOT_BOTTLE_1, ITEM_BOTTLE, SLOT_BOTTLE_2),
               "(c): a bottle unequips another slot's bottle");

    printf("[TEST] oot-item-unequip-bow: PASS\n");
    return 0;
}

// ---- OoTFishingPrizeThreshold (SoH 172b373fb, #1747) ----

namespace {

const char* const kUsfCustomizeFishingKey = CVAR_ENHANCEMENT("CustomizeFishing");
const char* const kUsfMinWeightAdultKey = CVAR_ENHANCEMENT("MinimumFishWeightAdult");

bool UsfFishingPrize(f32 fishWeight, bool vanilla) {
    VBFishingData fishData = {};
    fishData.fishWeight = fishWeight;
    return GameInteractor_Should(VB_SHOULD_GIVE_VANILLA_FISHING_PRIZE, vanilla, &fishData);
}

int UsfFishingPrizeLegs(void) {
    USF_ASSERT(UsfCountRegistrars(kUsfCustomizeFishingKey, "Fishing.cpp") == 1,
               "Fishing.cpp's prize registrar does not re-run on the CustomizeFishing CVar");
    USF_ASSERT(UsfCountRegistrars("IS_RANDO", "Fishing.cpp") == 2,
               "Fishing.cpp's prize registrar does not re-run when a file loads (IS_RANDO)");

    // A vanilla adult file; the adult minimum set to 7 lbs, a score of sqrt(6.5 / 0.0036), about 42.5.
    gSaveContext.ship.quest.id = QUEST_NORMAL;
    gSaveContext.linkAge = LINK_AGE_ADULT;
    gSaveContext.highScores[HS_FISHING] &= ~HS_FISH_PRIZE_ADULT;
    CVarSetInteger(kUsfMinWeightAdultKey, 7);
    CVarSetInteger(kUsfCustomizeFishingKey, 1);
    UsfRunRegistrars(kUsfCustomizeFishingKey, "Fishing.cpp");

    // (a) a fish over the customised minimum wins, although the vanilla 13 lb threshold said no.
    const bool won = UsfFishingPrize(45.0f, false);
    printf("[TEST] (a) Customize Fishing, adult minimum 7 lbs, fish length 45: prize %d\n", won);
    USF_ASSERT(won, "(a): the customised minimum weight does not give the vanilla prize");
    // (b) a fish under it does not, whatever vanilla said.
    USF_ASSERT(!UsfFishingPrize(40.0f, true), "(b): a fish under the customised minimum won the prize");
    // (c) the prize already won: no second Golden Scale.
    gSaveContext.highScores[HS_FISHING] |= HS_FISH_PRIZE_ADULT;
    USF_ASSERT(!UsfFishingPrize(45.0f, true), "(c): the Golden Scale was given twice");
    gSaveContext.highScores[HS_FISHING] &= ~HS_FISH_PRIZE_ADULT;

    // (d) Customize Fishing off: the hook is gone and the vanilla result passes through.
    CVarClear(kUsfCustomizeFishingKey);
    UsfRunRegistrars(kUsfCustomizeFishingKey, "Fishing.cpp");
    USF_ASSERT(!UsfFishingPrize(45.0f, false) && UsfFishingPrize(40.0f, true),
               "(d): the prize hook stayed armed with Customize Fishing off");

    // (e) a randomizer file: the hook stays off (the randomizer gives its own prize).
    CVarSetInteger(kUsfCustomizeFishingKey, 1);
    gSaveContext.ship.quest.id = QUEST_RANDOMIZER;
    UsfRunRegistrars("IS_RANDO", "Fishing.cpp");
    USF_ASSERT(!UsfFishingPrize(45.0f, false), "(e): the vanilla prize hook armed in a randomizer file");
    return 0;
}

} // namespace

extern "C" int OoT_FishingPrizeThreshold_RunHeadless(void) {
    if (GameInteractor::Instance == nullptr) {
        GameInteractor::Instance = new GameInteractor();
    }
    const GameId prevGame = Context_GetCurrentGame();
    // The GameInteractor dispatch is gated off only under GAME_MM.
    Context_SetCurrentGame(GAME_OOT);
    const int savedCustomize = CVarGetInteger(kUsfCustomizeFishingKey, 0);
    const int savedMinAdult = CVarGetInteger(kUsfMinWeightAdultKey, 0);
    const u8 savedQuest = gSaveContext.ship.quest.id;
    const s32 savedAge = gSaveContext.linkAge;
    const s32 savedFishing = gSaveContext.highScores[HS_FISHING];

    const int rc = UsfFishingPrizeLegs();

    gSaveContext.ship.quest.id = savedQuest;
    gSaveContext.linkAge = savedAge;
    gSaveContext.highScores[HS_FISHING] = savedFishing;
    UsfRestoreInt(kUsfCustomizeFishingKey, savedCustomize);
    UsfRestoreInt(kUsfMinWeightAdultKey, savedMinAdult);
    UsfRunRegistrars("IS_RANDO", "Fishing.cpp");
    Context_SetCurrentGame(prevGame);

    if (rc == 0) {
        printf("[TEST] oot-fishing-prize-threshold: PASS\n");
    }
    return rc;
}

// ---- OoTSaveEditorSwordRange (SoH 490e13616, #1746) ----

// games/oot/soh/Enhancements/debugger/debugSaveEditor.cpp.
bool OoT_SaveEditor_IsSwordItem(u32 id);

extern "C" int OoT_SaveEditorSwordRange_RunHeadless(void) {
    // (a) the four swords the B-button selector offers are swords (the old range matched none of them).
    USF_ASSERT(OoT_SaveEditor_IsSwordItem(ITEM_SWORD_KOKIRI), "(a): the Kokiri Sword is not a sword");
    USF_ASSERT(OoT_SaveEditor_IsSwordItem(ITEM_SWORD_MASTER), "(a): the Master Sword is not a sword");
    USF_ASSERT(OoT_SaveEditor_IsSwordItem(ITEM_SWORD_BGS), "(a): Biggoron's Sword is not a sword");
    USF_ASSERT(OoT_SaveEditor_IsSwordItem(ITEM_SWORD_BROKEN), "(a): the broken Giant's Knife is not a sword");
    // (b) neighbours that are not.
    USF_ASSERT(!OoT_SaveEditor_IsSwordItem(ITEM_SHIELD_DEKU), "(b): the Deku Shield is a sword");
    USF_ASSERT(!OoT_SaveEditor_IsSwordItem(ITEM_SWORD_KNIFE), "(b): the trade-quest knife is a sword");
    USF_ASSERT(!OoT_SaveEditor_IsSwordItem(ITEM_PRESCRIPTION), "(b): the Prescription is a sword");
    USF_ASSERT(!OoT_SaveEditor_IsSwordItem(ITEM_NONE), "(b): ITEM_NONE is a sword");

    printf("[TEST] oot-save-editor-sword-range: PASS\n");
    return 0;
}

// ---- OoTTitleCardCvars (SoH 8b3d91506, #1750) ----

extern "C" void OoT_TitleCard_ResolvePos(TitleCardContext* titleCtx, s16* posX, s16* posY); // z_actor.c
void OoT_CosmeticsEditor_ApplyMarginAll(bool SetActivated);                                 // CosmeticsEditor.cpp

namespace {

// CosmeticsEditor.cpp's MarginCvarList: every element the All margins buttons write.
const char* const kUsfMarginElements[] = {
    CVAR_COSMETIC("HUD.Hearts"),        CVAR_COSMETIC("HUD.HeartsCount"),    CVAR_COSMETIC("HUD.MagicBar"),
    CVAR_COSMETIC("HUD.VisualSoA"),     CVAR_COSMETIC("HUD.BButton"),        CVAR_COSMETIC("HUD.AButton"),
    CVAR_COSMETIC("HUD.StartButton"),   CVAR_COSMETIC("HUD.CUpButton"),      CVAR_COSMETIC("HUD.CDownButton"),
    CVAR_COSMETIC("HUD.CLeftButton"),   CVAR_COSMETIC("HUD.CRightButton"),   CVAR_COSMETIC("HUD.Dpad"),
    CVAR_COSMETIC("HUD.Minimap"),       CVAR_COSMETIC("HUD.SmallKey"),       CVAR_COSMETIC("HUD.Rupees"),
    CVAR_COSMETIC("HUD.Carrots"),       CVAR_COSMETIC("HUD.Timers"),         CVAR_COSMETIC("HUD.ArcheryScore"),
    CVAR_COSMETIC("HUD.TitleCard.Map"), CVAR_COSMETIC("HUD.TitleCard.Boss"), CVAR_COSMETIC("HUD.IGT"),
};
constexpr int kUsfMarginElementCount = sizeof(kUsfMarginElements) / sizeof(kUsfMarginElements[0]);
const char* const kUsfTitleCardSuffixes[] = { ".PosType", ".PosX", ".PosY", ".UseMargins" };

std::string UsfKey(const char* element, const char* suffix) {
    return std::string(element).append(suffix);
}

int UsfGet(const char* element, const char* suffix) {
    return CVarGetInteger(UsfKey(element, suffix).c_str(), 0);
}

void UsfSet(const char* element, const char* suffix, int value) {
    CVarSetInteger(UsfKey(element, suffix).c_str(), value);
}

int UsfTitleCardLegs(void) {
    const char* const map = CVAR_COSMETIC("HUD.TitleCard.Map");
    const char* const boss = CVAR_COSMETIC("HUD.TitleCard.Boss");
    for (int i = 0; i < kUsfMarginElementCount; i++) {
        CVarClear(UsfKey(kUsfMarginElements[i], ".PosType").c_str());
        CVarClear(UsfKey(kUsfMarginElements[i], ".UseMargins").c_str());
    }

    TitleCardContext titleCtx = {};
    titleCtx.x = 160;
    titleCtx.y = 40;
    s16 x = 0;
    s16 y = 0;

    // (a) original location: the card's own position.
    OoT_TitleCard_ResolvePos(&titleCtx, &x, &y);
    USF_ASSERT(x == 160 && y == 40, "(a): the original-location title card moved");

    // (b) the map card, not anchored, at (77, 33) in the editor's HUD.TitleCard.Map names.
    UsfSet(map, ".PosType", ANCHOR_NONE);
    UsfSet(map, ".PosX", 77);
    UsfSet(map, ".PosY", 33);
    OoT_TitleCard_ResolvePos(&titleCtx, &x, &y);
    printf("[TEST] (b) map card, HUD.TitleCard.Map not anchored at (77, 33): drawn at (%d, %d)\n", x, y);
    USF_ASSERT(x == 77 && y == 33, "(b): the map title card ignores the HUD.TitleCard.Map position settings");

    // (c) the boss card reads the boss names, not the map ones.
    titleCtx.isBossCard = 1;
    OoT_TitleCard_ResolvePos(&titleCtx, &x, &y);
    USF_ASSERT(x == 160 && y == 40, "(c): the boss title card read the map card's settings");
    UsfSet(boss, ".PosType", ANCHOR_NONE);
    UsfSet(boss, ".PosX", 12);
    UsfSet(boss, ".PosY", 21);
    OoT_TitleCard_ResolvePos(&titleCtx, &x, &y);
    USF_ASSERT(x == 12 && y == 21, "(c): the boss title card ignores the HUD.TitleCard.Boss position settings");

    // (d) hidden.
    titleCtx.isBossCard = 0;
    UsfSet(map, ".PosType", HIDDEN);
    OoT_TitleCard_ResolvePos(&titleCtx, &x, &y);
    USF_ASSERT(x == -9999 && y == 33, "(d): the hidden map title card is not moved off screen");

    // (e) All margins on, every element at its original location, Hearts hidden.
    for (int i = 0; i < kUsfMarginElementCount; i++) {
        CVarClear(UsfKey(kUsfMarginElements[i], ".PosType").c_str());
    }
    UsfSet(CVAR_COSMETIC("HUD.Hearts"), ".PosType", HIDDEN);
    OoT_CosmeticsEditor_ApplyMarginAll(true);
    printf("[TEST] (e) All margins on: map card %d, boss card %d, rupees %d, hidden hearts %d\n",
           UsfGet(map, ".UseMargins"), UsfGet(boss, ".UseMargins"), UsfGet(CVAR_COSMETIC("HUD.Rupees"), ".UseMargins"),
           UsfGet(CVAR_COSMETIC("HUD.Hearts"), ".UseMargins"));
    USF_ASSERT(UsfGet(map, ".UseMargins") == 0,
               "(e): All margins on gave margins to the map title card at its unanchored original location");
    USF_ASSERT(UsfGet(boss, ".UseMargins") == 0,
               "(e): All margins on gave margins to the boss title card at its unanchored original location");
    USF_ASSERT(UsfGet(CVAR_COSMETIC("HUD.Rupees"), ".UseMargins") == 1,
               "(e): All margins on did not give margins to the rupee counter");
    USF_ASSERT(UsfGet(CVAR_COSMETIC("HUD.Hearts"), ".UseMargins") == 0,
               "(e): All margins on gave margins to a hidden element");
    // (f) the map card anchored left: margins apply.
    UsfSet(map, ".PosType", ANCHOR_LEFT);
    OoT_CosmeticsEditor_ApplyMarginAll(true);
    USF_ASSERT(UsfGet(map, ".UseMargins") == 1, "(f): All margins on skipped the anchored map title card");
    // (g) All margins off.
    OoT_CosmeticsEditor_ApplyMarginAll(false);
    USF_ASSERT(UsfGet(map, ".UseMargins") == 0 && UsfGet(CVAR_COSMETIC("HUD.Rupees"), ".UseMargins") == 0,
               "(g): All margins off left a margin on");
    return 0;
}

} // namespace

extern "C" int OoT_TitleCardCvars_RunHeadless(void) {
    // Every CVar the row writes: the four title-card settings of both cards, and PosType / UseMargins of every
    // margin element (the title cards' are among them).
    std::string keys[kUsfMarginElementCount * 2 + 4];
    int count = 0;
    for (int i = 0; i < kUsfMarginElementCount; i++) {
        keys[count++] = UsfKey(kUsfMarginElements[i], ".PosType");
        keys[count++] = UsfKey(kUsfMarginElements[i], ".UseMargins");
    }
    const char* const cards[] = { CVAR_COSMETIC("HUD.TitleCard.Map"), CVAR_COSMETIC("HUD.TitleCard.Boss") };
    for (int c = 0; c < 2; c++) {
        for (int s = 1; s <= 2; s++) { // PosX and PosY; PosType and UseMargins are in the element list
            keys[count++] = UsfKey(cards[c], kUsfTitleCardSuffixes[s]);
        }
    }
    int saved[kUsfMarginElementCount * 2 + 4];
    for (int i = 0; i < count; i++) {
        saved[i] = CVarGetInteger(keys[i].c_str(), 0);
    }

    const int rc = UsfTitleCardLegs();

    for (int i = 0; i < count; i++) {
        UsfRestoreInt(keys[i].c_str(), saved[i]);
    }
    if (rc == 0) {
        printf("[TEST] oot-title-card-cvars: PASS\n");
    }
    return rc;
}

#endif // RSBS_SINGLE_EXECUTABLE
