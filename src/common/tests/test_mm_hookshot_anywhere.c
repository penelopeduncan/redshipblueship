/**
 * @file test_mm_hookshot_anywhere.c
 * @brief ROM-free unit lock for src/common/combo_mm_hookshot_anywhere.h (#1574):
 *        Combo_MMHookshotAnywhereSurfaces and Combo_MMHookshotAnywhereLogical
 *        over Off, On, Logical, and out-of-range values with the cheat off and on.
 */

#include "../combo_mm_hookshot_anywhere.h"
#include "../test_runner.h"

#include <stdint.h>
#include <stdio.h>

typedef struct {
    const char* name;
    uint32_t option;
    int cheat;
    bool expectedSurfaces;
    bool expectedLogical;
} MmHookshotAnywhereCase;

TestResult Test_MMHookshotAnywhere(void) {
    static const MmHookshotAnywhereCase kCases[] = {
        { "Off, cheat 0", (uint32_t)COMBO_MM_HOOKSHOT_ANYWHERE_OFF, 0, false, false },
        { "Off, cheat 1", (uint32_t)COMBO_MM_HOOKSHOT_ANYWHERE_OFF, 1, true, false },
        { "Off, cheat -1", (uint32_t)COMBO_MM_HOOKSHOT_ANYWHERE_OFF, -1, true, false },
        { "On, cheat 0", (uint32_t)COMBO_MM_HOOKSHOT_ANYWHERE_ON, 0, true, false },
        { "On, cheat 1", (uint32_t)COMBO_MM_HOOKSHOT_ANYWHERE_ON, 1, true, false },
        { "Logical, cheat 0", (uint32_t)COMBO_MM_HOOKSHOT_ANYWHERE_LOGICAL, 0, true, true },
        { "Logical, cheat 1", (uint32_t)COMBO_MM_HOOKSHOT_ANYWHERE_LOGICAL, 1, true, true },
        { "out-of-range 3, cheat 0", 3u, 0, true, false },
        { "out-of-range 3, cheat 1", 3u, 1, true, false },
        { "UINT32_MAX, cheat 0", UINT32_MAX, 0, true, false },
    };
    const size_t caseCount = sizeof(kCases) / sizeof(kCases[0]);

    printf("[TEST] mm-hookshot-anywhere: pure surface and logical helpers across %zu cases (#1574)\n", caseCount);

    if ((int)COMBO_MM_HOOKSHOT_ANYWHERE_OFF != 0 || (int)COMBO_MM_HOOKSHOT_ANYWHERE_ON != 1 ||
        (int)COMBO_MM_HOOKSHOT_ANYWHERE_LOGICAL != 2) {
        printf("[TEST] FAIL: ComboMMHookshotAnywhereMode ordinals drifted (%d, %d, %d)\n",
               (int)COMBO_MM_HOOKSHOT_ANYWHERE_OFF, (int)COMBO_MM_HOOKSHOT_ANYWHERE_ON,
               (int)COMBO_MM_HOOKSHOT_ANYWHERE_LOGICAL);
        return TEST_FAIL;
    }

    for (size_t i = 0; i < caseCount; i++) {
        const MmHookshotAnywhereCase* c = &kCases[i];
        const bool surfaces = Combo_MMHookshotAnywhereSurfaces(c->option, c->cheat);
        const bool logical = Combo_MMHookshotAnywhereLogical(c->option);
        if (surfaces != c->expectedSurfaces) {
            printf("[TEST] FAIL: case '%s' (option=%u, cheat=%d) surfaces=%d, expected %d\n", c->name,
                   (unsigned)c->option, c->cheat, surfaces ? 1 : 0, c->expectedSurfaces ? 1 : 0);
            return TEST_FAIL;
        }
        if (logical != c->expectedLogical) {
            printf("[TEST] FAIL: case '%s' (option=%u) logical=%d, expected %d\n", c->name, (unsigned)c->option,
                   logical ? 1 : 0, c->expectedLogical ? 1 : 0);
            return TEST_FAIL;
        }
    }

    printf("[TEST] PASS: Combo_MMHookshotAnywhereSurfaces and Combo_MMHookshotAnywhereLogical match all %zu cases\n",
           caseCount);
    return TEST_PASS;
}
