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

#include <libultraship/bridge.h>
#include "context.h"
#include "soh/ShipInit.hpp"
#include "soh/cvar_prefixes.h"
#include "soh/Enhancements/cosmetics/cosmeticsTypes.h"
#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/Enhancements/game-interactor/GameInteractor_Hooks.h"
#include "src/overlays/actors/ovl_Fishing/z_fishing.h"

#include <cstdint>
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

// ---- OoTBowBodyCosmetic (SoH 11fd0e0e7, #1750) ----

void OoT_CosmeticsEditor_ApplySideEffectsById(const char* id); // CosmeticsEditor.cpp

namespace {

// One CVar's state, to put back exactly: cleared when it did not exist, never stored.
struct UsfSavedCvar {
    std::string key;
    bool existed;
    bool isColor;
    Color_RGBA8 color;
    int value;
};

UsfSavedCvar UsfSaveCvar(const std::string& key, bool isColor) {
    UsfSavedCvar saved = {};
    saved.key = key;
    saved.existed = CVarGet(key.c_str()) != nullptr;
    saved.isColor = isColor;
    if (saved.existed) {
        if (isColor) {
            saved.color = CVarGetColor(key.c_str(), Color_RGBA8{ 0, 0, 0, 0 });
        } else {
            saved.value = CVarGetInteger(key.c_str(), 0);
        }
    }
    return saved;
}

void UsfRestoreCvar(const UsfSavedCvar& saved) {
    if (!saved.existed) {
        CVarClear(saved.key.c_str());
    } else if (saved.isColor) {
        CVarSetColor(saved.key.c_str(), saved.color);
    } else {
        CVarSetInteger(saved.key.c_str(), saved.value);
    }
}

bool UsfColorIs(const char* key, u8 r, u8 g, u8 b) {
    const Color_RGBA8 c = CVarGetColor(key, Color_RGBA8{ 0, 0, 0, 0 });
    return c.r == r && c.g == g && c.b == b;
}

void UsfPrintColor(const char* label, const char* key) {
    const Color_RGBA8 c = CVarGetColor(key, Color_RGBA8{ 0, 0, 0, 0 });
    printf("[TEST] %s = (%d, %d, %d)\n", label, c.r, c.g, c.b);
}

int UsfBowBodyLegs(void) {
    const char* const bodyValue = CVAR_COSMETIC("Equipment.BowBody.Value");
    const char* const tipsValue = CVAR_COSMETIC("Equipment.BowTips.Value");
    const char* const handleValue = CVAR_COSMETIC("Equipment.BowHandle.Value");
    CVarClear(CVAR_COSMETIC("AdvancedMode"));
    // The colour picker writes only the option's .Value (UIWidgets' colour edit), so do the same.
    CVarSetColor(bodyValue, Color_RGBA8{ 40, 80, 120, 255 });
    OoT_CosmeticsEditor_ApplySideEffectsById("Equipment.BowBody");
    UsfPrintColor("(a) Bow Body", bodyValue);
    UsfPrintColor("(b) Bow Tips", tipsValue);
    UsfPrintColor("(b) Bow Handle", handleValue);
    // (a) the picked colour stays as picked (the removed line multiplied it by four, saturating it).
    USF_ASSERT(UsfColorIs(bodyValue, 40, 80, 120), "(a): the Bow Body colour is not the colour the player picked");
    // (b) the derived parts follow the picked colour, not a cached copy of the default (140, 90, 10).
    USF_ASSERT(UsfColorIs(tipsValue, 20, 40, 60), "(b): Bow Tips is not half the picked Bow Body colour");
    USF_ASSERT(UsfColorIs(handleValue, 40, 80, 120), "(b): Bow Handle is not the picked Bow Body colour");
    // (c) a copy over 255 saturates, and also reads the picked colour (Level 1 Secondary copies at x2).
    CVarSetColor(CVAR_COSMETIC("SpinAttack.Level1Secondary.Value"), Color_RGBA8{ 200, 100, 10, 255 });
    OoT_CosmeticsEditor_ApplySideEffectsById("SpinAttack.Level1Secondary");
    USF_ASSERT(UsfColorIs(CVAR_COSMETIC("SpinAttack.Level1Primary.Value"), 255, 200, 20),
               "(c): a doubled colour did not saturate at 255, or read a cached colour");
    return 0;
}

} // namespace

extern "C" int OoT_BowBodyCosmetic_RunHeadless(void) {
    const char* const options[] = { "Equipment.BowBody", "Equipment.BowTips", "Equipment.BowHandle",
                                    "SpinAttack.Level1Secondary", "SpinAttack.Level1Primary" };
    const std::string prefix = CVAR_COSMETIC("");
    UsfSavedCvar saved[5 * 3 + 1];
    int count = 0;
    for (const char* option : options) {
        saved[count++] = UsfSaveCvar(prefix + option + ".Value", true);
        saved[count++] = UsfSaveCvar(prefix + option + ".Changed", false);
        saved[count++] = UsfSaveCvar(prefix + option + ".Rainbow", false);
    }
    saved[count++] = UsfSaveCvar(CVAR_COSMETIC("AdvancedMode"), false);

    const int rc = UsfBowBodyLegs();

    for (int i = 0; i < count; i++) {
        UsfRestoreCvar(saved[i]);
    }
    if (rc == 0) {
        printf("[TEST] oot-bow-body-cosmetic: PASS\n");
    }
    return rc;
}

// ---- OoTLakeHyliaMinimap (SoH 847e3cd32, the z_map_exp.c hunk, #1747) ----

extern "C" s32 OoT_Map_LakeHyliaShowsLoweredMap(void); // z_map_exp.c

namespace {

void UsfSetEventChkInf(s32 flag, bool on) {
    if (on) {
        gSaveContext.eventChkInf[flag >> 4] |= (u16)(1 << (flag & 0xF));
    } else {
        gSaveContext.eventChkInf[flag >> 4] &= (u16) ~(1 << (flag & 0xF));
    }
}

int UsfLakeHyliaLegs(void) {
    const u32 waterMedallion = 1u << QUEST_MEDALLION_WATER;
    gSaveContext.linkAge = LINK_AGE_ADULT;
    gSaveContext.inventory.questItems &= ~waterMedallion;

    // A randomizer file: the map follows the raised-water flag, not the blue warp.
    gSaveContext.ship.quest.id = QUEST_RANDOMIZER;
    // (a) blue warp taken, the floor switch lowered the water: the lowered map (the old key gave the raised one).
    UsfSetEventChkInf(EVENTCHKINF_USED_WATER_TEMPLE_BLUE_WARP, true);
    UsfSetEventChkInf(EVENTCHKINF_RAISED_LAKE_HYLIA_WATER, false);
    USF_ASSERT(OoT_Map_LakeHyliaShowsLoweredMap(), "(a): the minimap shows raised water after the water was lowered");
    // (b) the water raised without the blue warp flag: the raised map (the old key gave the lowered one).
    UsfSetEventChkInf(EVENTCHKINF_USED_WATER_TEMPLE_BLUE_WARP, false);
    UsfSetEventChkInf(EVENTCHKINF_RAISED_LAKE_HYLIA_WATER, true);
    USF_ASSERT(!OoT_Map_LakeHyliaShowsLoweredMap(), "(b): the minimap shows lowered water while the water is raised");

    // A vanilla file: unchanged, the Water Medallion decides.
    gSaveContext.ship.quest.id = QUEST_NORMAL;
    UsfSetEventChkInf(EVENTCHKINF_RAISED_LAKE_HYLIA_WATER, false);
    USF_ASSERT(OoT_Map_LakeHyliaShowsLoweredMap(),
               "(c): vanilla adult without the Water Medallion lost the lowered map");
    gSaveContext.inventory.questItems |= waterMedallion;
    USF_ASSERT(!OoT_Map_LakeHyliaShowsLoweredMap(),
               "(c): vanilla adult with the Water Medallion shows the lowered map");

    // (d) child: always the raised map.
    gSaveContext.linkAge = LINK_AGE_CHILD;
    gSaveContext.ship.quest.id = QUEST_RANDOMIZER;
    USF_ASSERT(!OoT_Map_LakeHyliaShowsLoweredMap(), "(d): child Link sees the lowered map");
    return 0;
}

} // namespace

extern "C" int OoT_LakeHyliaMinimap_RunHeadless(void) {
    const u8 savedQuest = gSaveContext.ship.quest.id;
    const s32 savedAge = gSaveContext.linkAge;
    const u32 savedQuestItems = gSaveContext.inventory.questItems;
    u16 savedEventChkInf[14];
    static_assert(sizeof(savedEventChkInf) == sizeof(gSaveContext.eventChkInf), "eventChkInf changed size");
    memcpy(savedEventChkInf, gSaveContext.eventChkInf, sizeof(savedEventChkInf));

    const int rc = UsfLakeHyliaLegs();

    gSaveContext.ship.quest.id = savedQuest;
    gSaveContext.linkAge = savedAge;
    gSaveContext.inventory.questItems = savedQuestItems;
    memcpy(gSaveContext.eventChkInf, savedEventChkInf, sizeof(savedEventChkInf));
    if (rc == 0) {
        printf("[TEST] oot-lake-hylia-minimap: PASS\n");
    }
    return rc;
}

// ---- OoTHoverFishing (SoH a089fa60b, #1747) ----

namespace {

const char* const kUsfHoverFishingKey = CVAR_ENHANCEMENT("HoverFishing");

int UsfHoverFishingLegs(void) {
    USF_ASSERT(UsfCountRegistrars(kUsfHoverFishingKey, "Fishing.cpp") == 1,
               "Fishing.cpp's Hover Fishing registrar does not re-run on the HoverFishing CVar");

    // (a) option off: both behaviours are vanilla (every cast check runs, Link stops).
    CVarClear(kUsfHoverFishingKey);
    UsfRunRegistrars(kUsfHoverFishingKey, "Fishing.cpp");
    Vec3f rodCheckPos = { 0.0f, 50.0f, 0.0f };
    USF_ASSERT(GameInteractor_Should(VB_NOT_CAST_FISHING, true, &rodCheckPos),
               "(a): a refused cast was allowed with Hover Fishing off");
    USF_ASSERT(!GameInteractor_Should(VB_NOT_CAST_FISHING, false, &rodCheckPos),
               "(a): an allowed cast was refused with Hover Fishing off");
    USF_ASSERT(GameInteractor_Should(VB_FISHING_ZERO_XZ, true),
               "(a): the cast kept Link's speed with Hover Fishing off");

    // (b) option on: the cast keeps Link's momentum, as on NTSC 1.0. The cast check itself is not driven here: it
    // reads OoT_gPlayState->colCtx, which a ROM-free process does not have.
    CVarSetInteger(kUsfHoverFishingKey, 1);
    UsfRunRegistrars(kUsfHoverFishingKey, "Fishing.cpp");
    USF_ASSERT(!GameInteractor_Should(VB_FISHING_ZERO_XZ, true), "(b): the Hover Fishing cast still stops Link");

    // (c) option off again: the hooks are gone.
    CVarClear(kUsfHoverFishingKey);
    UsfRunRegistrars(kUsfHoverFishingKey, "Fishing.cpp");
    USF_ASSERT(GameInteractor_Should(VB_FISHING_ZERO_XZ, true), "(c): a Hover Fishing hook stayed registered");
    return 0;
}

} // namespace

extern "C" int OoT_HoverFishing_RunHeadless(void) {
    if (GameInteractor::Instance == nullptr) {
        GameInteractor::Instance = new GameInteractor();
    }
    const GameId prevGame = Context_GetCurrentGame();
    // The GameInteractor dispatch is gated off only under GAME_MM.
    Context_SetCurrentGame(GAME_OOT);
    const int saved = CVarGetInteger(kUsfHoverFishingKey, 0);

    const int rc = UsfHoverFishingLegs();

    UsfRestoreInt(kUsfHoverFishingKey, saved);
    UsfRunRegistrars(kUsfHoverFishingKey, "Fishing.cpp");
    Context_SetCurrentGame(prevGame);
    if (rc == 0) {
        printf("[TEST] oot-hover-fishing: PASS\n");
    }
    return rc;
}

// ---- OoTGuMtxIdent (SoH c642e8c4d, the gu_pc.c hunks, #1732) ----

namespace {

int UsfGuMtxIdentLegs(void) {
    // (a) guMtxIdent writes a fixed-point identity matrix (without GBI_FLOATS, Mtx is MtxS with int[4][4] words;
    // main's guMtxIdentF(m->m) wrote 0x3F800000 float bits into m.m[0][0]).
    Mtx m;
    memset(&m, 0xCD, sizeof(m));
    guMtxIdent(&m);
    static const uint32_t kWantIdent[4][4] = {
        { 0x00010000u, 0x00000000u, 0x00000001u, 0x00000000u },
        { 0x00000000u, 0x00010000u, 0x00000000u, 0x00000001u },
        { 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u },
        { 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u },
    };
    for (int r = 0; r < 4; r++) {
        for (int c = 0; c < 4; c++) {
            USF_ASSERT(static_cast<uint32_t>(m.m[r][c]) == kWantIdent[r][c],
                       "(a): guMtxIdent did not produce the fixed-point identity matrix");
        }
    }

    // (b) guMtxL2F of that matrix gives exactly 1.0f on the diagonal and 0.0f everywhere else.
    float out[4][4];
    memset(out, 0xCD, sizeof(out));
    guMtxL2F(out, &m);
    for (int r = 0; r < 4; r++) {
        for (int c = 0; c < 4; c++) {
            const float want = (r == c) ? 1.0f : 0.0f;
            USF_ASSERT(out[r][c] == want, "(b): guMtxL2F of guMtxIdent matrix was not the float identity");
        }
    }

    // (c) Lock on guMtxF2L / guMtxL2F conversion (the cast hunk's loop): negative and fractional values round-trip
    // with exact S15.16 bit layout.
    float in[4][4];
    memset(in, 0, sizeof(in));
    in[0][0] = -1.5f;
    in[0][1] = 0.25f;
    memset(&m, 0xCD, sizeof(m));
    guMtxF2L(in, &m);
    USF_ASSERT(static_cast<uint32_t>(m.m[0][0]) == 0xFFFE0000u,
               "(c): guMtxF2L integer word m.m[0][0] mismatch for -1.5f / 0.25f");
    USF_ASSERT(static_cast<uint32_t>(m.m[2][0]) == 0x80004000u,
               "(c): guMtxF2L fractional word m.m[2][0] mismatch for -1.5f / 0.25f");
    for (int r = 0; r < 4; r++) {
        for (int c = 0; c < 4; c++) {
            if ((r == 0 || r == 2) && c == 0) {
                continue;
            }
            USF_ASSERT(static_cast<uint32_t>(m.m[r][c]) == 0u,
                       "(c): guMtxF2L left non-zero words outside the populated pair");
        }
    }
    memset(out, 0xCD, sizeof(out));
    guMtxL2F(out, &m);
    for (int r = 0; r < 4; r++) {
        for (int c = 0; c < 4; c++) {
            float want = 0.0f;
            if (r == 0 && c == 0) {
                want = -1.5f;
            } else if (r == 0 && c == 1) {
                want = 0.25f;
            }
            USF_ASSERT(out[r][c] == want, "(c): guMtxL2F did not round-trip -1.5f / 0.25f");
        }
    }
    return 0;
}

} // namespace

extern "C" int OoT_GuMtxIdent_RunHeadless(void) {
    const int rc = UsfGuMtxIdentLegs();
    if (rc == 0) {
        printf("[TEST] oot-gu-mtx-ident: PASS\n");
    }
    return rc;
}

#endif // RSBS_SINGLE_EXECUTABLE
