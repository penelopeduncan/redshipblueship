/**
 * @file save_layout_pins.h
 * @brief Save format 6's LAYOUT PINS: the measured sizeof/offsetof values of every
 *        saved structure the .redsave stores raw (src/common/save.h).
 *
 * Tier-1 is a raw ComboContext, Tier-2 a raw OoT SaveContext and Tier-3 a raw MM
 * SaveContext. A field that moves inside one of them reinterprets every file
 * already written, so each value a reader depends on is pinned here, in ONE
 * place, and asserted where the struct is visible:
 *   - ComboContext's in context.h (C and C++);
 *   - OoT's in games/oot/soh/GameExports_SingleExe.cpp;
 *   - MM's in games/mm/2s2h/GameExports_SingleExe.cpp.
 *
 * HOW A PIN IS SET. The values cannot be worked out by hand with certainty
 * (padding, and three compilers: MSVC, GCC, Apple Clang), so every pin starts at 0,
 * which means "NOT YET MEASURED: THE ASSERT IS OFF". The ROM-free redship row
 * SaveLayoutPins (`redship --test save-layout-pins`) prints every measured value,
 * one line each, as
 *     [LAYOUT] <name>=<decimal>
 * and, for a pin still at 0, also
 *     [LAYOUT] UNPINNED <name>
 * The row passes while pins are 0, so a tree with unmeasured pins builds and runs
 * green. The values are read from the row's output on all three CI legs (Windows,
 * Linux, macOS); when they agree they are written here, and from then on a
 * nonzero pin that differs from the compiled value FAILS THE BUILD through its
 * static_assert (RSBS_LAYOUT_PIN_ASSERT). If the legs disagree, the layout is not
 * identical across compilers, which is itself a defect to fix before the pin is
 * set (every saved field is fixed-width, with no bitfield and no new bool).
 *
 * WHEN A PIN FIRES. The message names the rule (context.h's growth rule): a new
 * field is carved from the front of a spare block (OoT
 * ShipRandomizerSaveContextData::rsbsSpare, MM RandoSaveInfo::rsbsSpare) or
 * appended at the very end of its struct (ComboContext; OoT's `ship`, the last
 * member of SaveContext), RSBS_SAVE_LAYOUT_REVISION (save.h) rises by one, and the
 * pins that legitimately moved (a sizeof after an append) are re-measured with the
 * row. Any other movement of a pinned field is a save format generation
 * (RSBS_SAVE_VERSION and RSBS_SAVE_VERSION_MIN rise together), never an edit of
 * the pin alone.
 *
 * Plain macros, no game include: game.h's rule (no game headers in src/common)
 * holds here too.
 */

#ifndef RSBS_COMMON_SAVE_LAYOUT_PINS_H
#define RSBS_COMMON_SAVE_LAYOUT_PINS_H

#include <stdio.h>

#ifdef __cplusplus
#define RSBS_LAYOUT_STATIC_ASSERT_(cond, msg) static_assert(cond, msg)
#else
#define RSBS_LAYOUT_STATIC_ASSERT_(cond, msg) _Static_assert(cond, msg)
#endif

#define RSBS_LAYOUT_PIN_RULE                                                                                         \
    "the raw .redsave tier layout changed: carve rsbsSpare, or append at the end of ComboContext or of OoT's "      \
    "`ship`, bump RSBS_SAVE_LAYOUT_REVISION (save.h) and re-measure with the SaveLayoutPins row; anything else is " \
    "a save format generation (context.h growth rule 6)"

/**
 * The one assert form. With the pin at 0 the condition is always true (the assert
 * is off: not yet measured); with a nonzero pin it is a real static_assert that
 * the compiled value equals the pin.
 */
#define RSBS_LAYOUT_PIN_ASSERT(pin, measured) \
    RSBS_LAYOUT_STATIC_ASSERT_((pin) == 0 || (unsigned long long)(pin) == (unsigned long long)(measured), \
                               "layout pin " #pin ": " RSBS_LAYOUT_PIN_RULE)

/**
 * The SaveLayoutPins row's report of one pin: the measured line, the UNPINNED line
 * for a pin at 0, and `mismatches` incremented for a nonzero pin that differs (the
 * static_assert above makes that unreachable in a build that compiled; the runtime
 * compare is what proves the assert form itself was not disabled).
 */
#define RSBS_LAYOUT_PIN_REPORT(pin, measured, mismatches)                                               \
    do {                                                                                                 \
        const unsigned long long rsbsPinMeasured_ = (unsigned long long)(measured);                      \
        printf("[LAYOUT] %s=%llu\n", #pin, rsbsPinMeasured_);                                            \
        if ((pin) == 0) {                                                                                \
            printf("[LAYOUT] UNPINNED %s\n", #pin);                                                      \
        } else if ((unsigned long long)(pin) != rsbsPinMeasured_) {                                      \
            printf("[LAYOUT] FAIL %s: pinned %llu, compiled %llu\n", #pin, (unsigned long long)(pin),    \
                   rsbsPinMeasured_);                                                                    \
            (mismatches)++;                                                                              \
        }                                                                                                \
    } while (0)

// ---- Tier-1: ComboContext (context.h) --------------------------------------
// Offsets <= 1007 are pinned as literals in context.h already (unchanged by format
// 6). These are the members format 6 moved or added: the MM capture (after the
// OoT capture grew to 1024 bytes), the skip journal, the watermark, and the size.
#define RSBS_PIN_COMBO_TRACKER_AVAIL_MM 0          // offsetof(ComboContext, trackerAvailMM)
#define RSBS_PIN_COMBO_SKIP_JOURNAL_COUNT 0        // offsetof(ComboContext, trackerSkipJournalCountOoT)
#define RSBS_PIN_COMBO_SKIP_JOURNAL 0              // offsetof(ComboContext, trackerSkipJournalOoT)
#define RSBS_PIN_COMBO_SAVE_WATERMARK 0            // offsetof(ComboContext, saveWatermark)
#define RSBS_PIN_COMBO_CONTEXT_SIZEOF 0            // sizeof(ComboContext)

// ---- Tier-2: OoT SaveContext (games/oot/include/z64save.h) -------------------
// The fields src/common and the arrival/identity readers take by offset
// (SaveManager.cpp, ForeignMaskOoT.cpp), the start of `ship`, and the size.
#define RSBS_PIN_OOT_SAVE_CONTEXT_SIZEOF 0         // sizeof(SaveContext)
#define RSBS_PIN_OOT_SHIP 0                        // offsetof(SaveContext, ship)
#define RSBS_PIN_OOT_SHIP_QUEST 0                  // offsetof(SaveContext, ship.quest)
#define RSBS_PIN_OOT_SHIP_FILENAME_LANGUAGE 0      // offsetof(SaveContext, ship.filenameLanguage)
#define RSBS_PIN_OOT_SHIP_RANDOMIZER_INF 0         // offsetof(SaveContext, ship.randomizerInf)
#define RSBS_PIN_OOT_SHIP_FOREIGN_MASK 0           // offsetof(SaveContext, ship.foreignMask)

// ---- Tier-3: MM SaveContext (games/mm/include/z64save.h) ---------------------
// The fields read by offset from a raw half (Foreign.cpp's classifier,
// GameExports_SingleExe.cpp's arrival gate, ForeignMaskMM.cpp), the spare block,
// the first SaveContext field after Save (the tripwire for anything inside Save
// growing), and the sizes.
#define RSBS_PIN_MM_SAVE_CONTEXT_SIZEOF 0          // sizeof(SaveContext)
#define RSBS_PIN_MM_RANDO_SAVE_INFO_SIZEOF 0       // sizeof(RandoSaveInfo)
#define RSBS_PIN_MM_SAVE_TYPE 0                    // offsetof(SaveContext, save.shipSaveInfo.saveType)
#define RSBS_PIN_MM_RANDO_FINAL_SEED 0             // offsetof(SaveContext, save.shipSaveInfo.rando.finalSeed)
#define RSBS_PIN_MM_RANDO_OPTIONS 0                // offsetof(SaveContext, save.shipSaveInfo.rando.randoSaveOptions)
#define RSBS_PIN_MM_RANDO_FOREIGN_MASK 0           // offsetof(SaveContext, save.shipSaveInfo.rando.foreignMask)
#define RSBS_PIN_MM_RANDO_SPARE 0                  // offsetof(SaveContext, save.shipSaveInfo.rando.rsbsSpare)
#define RSBS_PIN_MM_EVENT_INF 0                    // offsetof(SaveContext, eventInf)

#endif // RSBS_COMMON_SAVE_LAYOUT_PINS_H
