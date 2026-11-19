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

#include <cstdio>
#include <cstring>

extern "C" {
#include "variables.h"
}

#define USF_ASSERT(cond, msg)                                             \
    do {                                                                  \
        if (!(cond)) {                                                    \
            printf("[TEST] FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__); \
            return 1;                                                     \
        }                                                                 \
    } while (0)

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

#endif // RSBS_SINGLE_EXECUTABLE
