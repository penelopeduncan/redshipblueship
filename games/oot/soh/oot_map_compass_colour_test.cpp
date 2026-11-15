/**
 * @file oot_map_compass_colour_test.cpp
 * OoT's half of the map and compass colour lock (SoH 2f12677b2, #6351; the row is
 * src/common/tests/test_map_compass_colour.c).
 *
 * THE HELPERS the row reads OoT's item table and the dungeon colour table
 * through. They only read; every decision is the production code's
 * (soh/Enhancements/randomizer/item_list.cpp and draw.cpp). They live here
 * because only an OoT TU has RG_*, the item table and Color_RGB8 in scope.
 */

#include <cstdint>

// At file scope, not inside an extern "C" block: z64.h includes <memory> under
// C++ (see oot_shared_kokiri_sword_test.cpp).
#include <z64.h>
#include "soh/Enhancements/randomizer/randomizerTypes.h"
#include "soh/Enhancements/randomizer/static_data.h"
#include "soh/Enhancements/randomizer/draw.h"

// draw.cpp: the dungeon colour both Randomizer_DrawMap and
// Randomizer_DrawCompass index by drawItemId (SoH #6351).
extern Color_RGB8 MapOrCompassColor[10];

/** How many dungeons have a map and a compass row (Deku Tree .. Ice Cavern). */
extern "C" int OoT_MapCompassTest_DungeonCount(void) {
    return (int)(RG_ICE_CAVERN_MAP - RG_DEKU_TREE_MAP + 1);
}

/**
 * One fact of dungeon @p dungeon's map (@p compass 0) or compass (@p compass 1)
 * row, 1 or 0, by @p fact: 0 its custom draw is Randomizer_DrawMap (map) or
 * Randomizer_DrawCompass (compass), 1 its drawItemId minus the first map's or
 * compass's id is @p dungeon, so the draw indexes the colour table inside its
 * ten rows. -1 for an unknown fact, an out-of-range dungeon or a row with no
 * static entry.
 */
extern "C" int OoT_MapCompassTest_RowFact(int dungeon, int compass, int fact) {
    if (dungeon < 0 || dungeon >= OoT_MapCompassTest_DungeonCount()) {
        return -1;
    }
    const RandomizerGet base = compass ? RG_DEKU_TREE_COMPASS : RG_DEKU_TREE_MAP;
    const GetItemEntry* entry = Rando::StaticData::RetrieveItem((RandomizerGet)(base + dungeon)).GetStaticGIEntry();
    if (entry == nullptr) {
        return -1;
    }
    switch (fact) {
        case 0:
            return entry->drawFunc == (compass ? Randomizer_DrawCompass : Randomizer_DrawMap) ? 1 : 0;
        case 1:
            return (int)entry->drawItemId - (int)base == dungeon ? 1 : 0;
        default:
            return -1;
    }
}

/** Dungeon @p dungeon's map and compass colour as 0xRRGGBB, or -1 out of range. */
extern "C" int32_t OoT_MapCompassTest_Color(int dungeon) {
    if (dungeon < 0 || dungeon >= OoT_MapCompassTest_DungeonCount()) {
        return -1;
    }
    const Color_RGB8 c = MapOrCompassColor[dungeon];
    return (int32_t)(((uint32_t)c.r << 16) | ((uint32_t)c.g << 8) | (uint32_t)c.b);
}
