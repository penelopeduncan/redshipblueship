/**
 * @file test_map_compass_colour.c
 * @brief OoTMapCompassColours: every dungeon map and compass draws in its
 *        dungeon's colour (SoH 2f12677b2, #6351).
 *
 * ROM-free, redship tier. SoH's "Map & Compass Colors Match Dungeon" patches the
 * map and compass display lists' own colour commands out and lets the custom
 * draws set the dungeon's colour. Before #6351 only the compasses had a custom
 * draw, so a map never took its colour, and the compass draw had its own
 * twelve-row table whose tenth row was the Gerudo Training Ground's gold, so
 * the Ice Cavern compass drew gold. The row reads OoT's item table and the one
 * colour table both draws now index by drawItemId
 * (games/oot/soh/oot_map_compass_colour_test.cpp):
 *
 *   (1) every map row's custom draw is Randomizer_DrawMap and every compass
 *       row's is Randomizer_DrawCompass;
 *   (2) every row's drawItemId lands on its own dungeon's colour row, inside
 *       the ten-row table;
 *   (3) the Ice Cavern's colour is cyan (0x00FFFF), not the GTG gold
 *       (0xDDD43C), and the ten dungeons' colours are distinct.
 *
 * The other half of #6351 (the colouring re-runs on the "IS_RANDO" ShipInit
 * key instead of its own OnLoadFile hook) patches display lists from the OoT
 * archive, so it needs the ROM and is not covered here.
 *
 * Linkage note: #included into test_runner.cpp at FILE SCOPE (compiled as C++),
 * like its neighbours; every symbol under test is C-linkage. Helpers carry the
 * MapCompass prefix because every included test file shares one translation
 * unit.
 */

#include "../test_runner.h"

#include <cstdint>
#include <cstdio>

extern "C" {
// games/oot/soh/oot_map_compass_colour_test.cpp (SoH #6351)
int OoT_MapCompassTest_DungeonCount(void);
int OoT_MapCompassTest_RowFact(int dungeon, int compass, int fact);
int32_t OoT_MapCompassTest_Color(int dungeon);
// games/oot/soh/Enhancements/randomizer/ComboLogicEngineOoT.cpp
int OoT_ComboLogic_TestEnsureItemTableTransient(void);
}

namespace {

const char* const kMapCompassDungeons[] = {
    "Deku Tree",    "Dodongo's Cavern", "Jabu Jabu's Belly", "Forest Temple",      "Fire Temple",
    "Water Temple", "Spirit Temple",    "Shadow Temple",     "Bottom of the Well", "Ice Cavern",
};
constexpr int kMapCompassDungeonCount = (int)(sizeof(kMapCompassDungeons) / sizeof(kMapCompassDungeons[0]));
constexpr int kMapCompassIceCavern = 9;
constexpr int32_t kMapCompassIceCavernColor = 0x00FFFF;

} // namespace

TestResult Test_OoTMapCompassColours(void) {
    printf("[TEST] oot-map-compass-colours: every dungeon map and compass draws in its dungeon's colour, and the Ice "
           "Cavern compass is cyan (SoH #6351)\n");
    if (OoT_ComboLogic_TestEnsureItemTableTransient() != 0) {
        printf("[TEST] FAIL: OoT's item table is not available\n");
        return TEST_FAIL;
    }
    bool ok = true;
    const int count = OoT_MapCompassTest_DungeonCount();
    if (count != kMapCompassDungeonCount) {
        printf("[TEST] FAIL: OoT has %d dungeon map rows, expected %d (the colour table has ten rows)\n", count,
               kMapCompassDungeonCount);
        return TEST_FAIL;
    }
    for (int d = 0; d < count; d++) {
        for (int compass = 0; compass <= 1; compass++) {
            const char* what = compass ? "compass" : "map";
            const int draw = OoT_MapCompassTest_RowFact(d, compass, 0);
            if (draw != 1) {
                printf("[TEST] FAIL: the %s %s's custom draw is not Randomizer_Draw%s (%d): it never takes its "
                       "dungeon's colour\n",
                       kMapCompassDungeons[d], what, compass ? "Compass" : "Map", draw);
                ok = false;
            }
            const int slot = OoT_MapCompassTest_RowFact(d, compass, 1);
            if (slot != 1) {
                printf("[TEST] FAIL: the %s %s's drawItemId does not index its own colour row (%d)\n",
                       kMapCompassDungeons[d], what, slot);
                ok = false;
            }
        }
    }
    const int32_t ice = OoT_MapCompassTest_Color(kMapCompassIceCavern);
    if (ice != kMapCompassIceCavernColor) {
        printf("[TEST] FAIL: the Ice Cavern map and compass draw in 0x%06X, expected cyan 0x%06X\n", (unsigned)ice,
               (unsigned)kMapCompassIceCavernColor);
        ok = false;
    }
    for (int a = 0; a < count; a++) {
        for (int b = a + 1; b < count; b++) {
            if (OoT_MapCompassTest_Color(a) == OoT_MapCompassTest_Color(b)) {
                printf("[TEST] FAIL: the %s and %s maps and compasses share the colour 0x%06X\n",
                       kMapCompassDungeons[a], kMapCompassDungeons[b], (unsigned)OoT_MapCompassTest_Color(a));
                ok = false;
            }
        }
    }
    if (!ok) {
        return TEST_FAIL;
    }
    printf("[TEST] PASS: oot-map-compass-colours\n");
    return TEST_PASS;
}
