/**
 * @file game.h
 * @brief Common game definitions for single-executable architecture
 *
 * This header defines the Game enum and common types shared between
 * OoT and MM in the unified single-executable build.
 */

#ifndef RSBS_COMMON_GAME_H
#define RSBS_COMMON_GAME_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Game identifiers for the unified executable
 */
typedef enum {
    GAME_NONE = 0,
    GAME_OOT = 1, // Ocarina of Time
    GAME_MM = 2   // Majora's Mask
} GameId;

// SaveContext blob capacities — the SINGLE source of truth for every buffer
// that holds a full runtime SaveContext image (context.cpp shadow copies,
// unified_save.c storage, the .redsave tiers, and the headless tests).
//
// These are CAPACITIES, not exact struct sizes. Each port's runtime
// SaveContext is much larger than the original N64 struct the "// size ="
// comments in z64save.h describe:
//   OoT: N64 struct 0x1428, but SoH appends ShipSaveContextData (SohStats
//        with sceneTimestamps[8191], ...) — sizeof(SaveContext) ~= 0x21C30.
//   MM:  N64 struct 0x48C8, but 2S2H appends ShipSaveInfo (rando check
//        tables, ...) and ShipSaveContext — sizeof(SaveContext) ~= 0xC000.
// Those extension sections grow as the upstream ports evolve, so the exact
// sizeof cannot be hardcoded here (this header must stay free of game
// includes). Instead each capacity carries headroom, and a static_assert in a
// TU that CAN see the real struct (games/oot/soh/GameExports_SingleExe.cpp,
// games/mm/2s2h/GameExports_SingleExe.cpp) verifies
// sizeof(SaveContext) <= *_SAVE_CONTEXT_SIZE so drift fails the build loudly
// instead of silently truncating cross-game save state (issue: freeze/restore
// used to clamp OoT to 0x1428, losing all SoH ship.* state per switch).
//
// Save format 6 (save.h) raised both, unconditionally, because the fixed saved
// capacities it gave the enum-sized arrays (both z64save.h files) do not fit the
// old values: OoT grows by about 1.5 KB (itemTimestamp 320, count 192,
// randomizerInf 8192 flags = 512 words, the 48-byte quest union), on a struct the
// old 0x22000 already held with under 1 KB to spare; MM grows by about 22.4 KB
// (checks 4096 rows of 12 bytes, options 128, tricks 128, randoInf 256 flags,
// events 256, the excluded-check set 512 bytes, the 256-byte spare block), from
// about 0xC000 to about 0x117A0. The exact sizes are pinned in
// save_layout_pins.h (RSBS_PIN_*_SAVE_CONTEXT_SIZEOF) once measured. Raising a
// capacity is prefix-compatible (context.h's growth rule 7).
#define OOT_SAVE_CONTEXT_SIZE 0x23000 // 140KB capacity (SoH runtime struct ~0x22200 at save format 6)
#define MM_SAVE_CONTEXT_SIZE 0x14000  // 80KB capacity (2S2H runtime struct ~0x117A0 at save format 6)

/**
 * Convert game ID string to enum
 * @param id "oot" or "mm"
 * @return GameId enum value, GAME_NONE if invalid
 */
GameId Game_FromString(const char* id);

/**
 * Convert game enum to ID string
 * @param game Game enum value
 * @return "oot", "mm", or NULL if invalid
 */
const char* Game_ToString(GameId game);

/**
 * Get the other game (for switching)
 */
GameId Game_GetOther(GameId game);

#ifdef __cplusplus
}
#endif

#endif // RSBS_COMMON_GAME_H
