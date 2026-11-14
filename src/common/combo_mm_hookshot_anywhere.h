/**
 * @file combo_mm_hookshot_anywhere.h
 * @brief Pure helpers for Majora's Mask's frozen Hookshot Anywhere randomizer
 *        option (#1574; OoTMM packages/core/src/settings/data.ts:1690-1702,
 *        hookshotAnywhereMm).
 *
 * The per-install cheat (gCheats.HookshotEverything) toggles both games live
 * and logic never credits it. Majora's Mask's RO_HOOKSHOT_ANYWHERE is frozen
 * in the file's randoSaveOptions at world creation: non-Off values turn
 * VB_BE_HOOKSHOT_SURFACE on for that file, and only Logical credits OoTMM's
 * short_hook_anywhere macro in logic.
 *
 * C-compatible and game-header-free; MM static-asserts that these constants
 * match RandoOptionHookshotAnywhere. Locked by the MMHookshotAnywhere CTest.
 */

#ifndef RSBS_COMMON_COMBO_MM_HOOKSHOT_ANYWHERE_H
#define RSBS_COMMON_COMBO_MM_HOOKSHOT_ANYWHERE_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    COMBO_MM_HOOKSHOT_ANYWHERE_OFF = 0,
    COMBO_MM_HOOKSHOT_ANYWHERE_ON = 1,
    COMBO_MM_HOOKSHOT_ANYWHERE_LOGICAL = 2
} ComboMMHookshotAnywhereMode;

/**
 * Whether MM's surface check treats surfaces as hookshotable: true when the
 * per-install cheat is non-zero or the file's frozen option is not Off.
 */
static inline bool Combo_MMHookshotAnywhereSurfaces(uint32_t option, int cheat) {
    return cheat != 0 || option != (uint32_t)COMBO_MM_HOOKSHOT_ANYWHERE_OFF;
}

/**
 * Whether MM's logic may credit OoTMM's short_hook_anywhere: true only when
 * the file's frozen option is Logical (neither Off nor On, nor the cheat).
 */
static inline bool Combo_MMHookshotAnywhereLogical(uint32_t option) {
    return option == (uint32_t)COMBO_MM_HOOKSHOT_ANYWHERE_LOGICAL;
}

#ifdef __cplusplus
}
#endif

#endif // RSBS_COMMON_COMBO_MM_HOOKSHOT_ANYWHERE_H
