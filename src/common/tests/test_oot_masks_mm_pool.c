/**
 * @file test_oot_masks_mm_pool.c
 * Two rows for OoT masks in MM (#1569, refs #1452 item 10, #980):
 *  - `oot-masks-mm-pool` (rando tier): drives `GeneratePools` via
 *    `MM_ComboLogic_TestPairedPool` under frozen rules and checks counts for
 *    `RI_MASK_GERUDO`, `RI_MASK_SKULL`, and `RI_MASK_SPOOKY`.
 *  - `oot-masks-mm-owned-source` (redship tier, skipped by `--test all`):
 *    verifies that C++ static initialization (`OoTMasksMMRegistrar`) registers
 *    MM's `OwnedSource` before any `ShipInit`, `MM_Rando_InitCore`, or
 *    `OnSaveLoad` runs.
 *
 * Included at file scope in `src/common/test_runner.cpp` right after
 * `tests/test_oot_masks_mm.c` (compiled as C++).
 */

#include "../context.h"
#include "../foreign_items.h"
#include "../foreign_masks.h"
#include "../test_runner.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

#ifdef __cplusplus
extern "C" {
#endif
// games/mm/2s2h/mm_oot_masks_pool_test.cpp
uint16_t MM_OoTMasksPoolTest_Id(int which);
void* MM_OoTMasksPoolTest_SaveSnapshot(void);
void MM_OoTMasksPoolTest_RestoreSnapshot(void* snapshot);
// games/mm/2s2h/Rando/ComboLogicEngineSingleExe.cpp
int MM_ComboLogic_TestPairedPool(uint16_t* outItems, uint16_t* outFlags, int cap);
// games/mm/2s2h/Rando/ForeignMaskMM.cpp
uint8_t MM_ForeignMask_SlotSelRaw(uint8_t slot);
void MM_ForeignMask_SetSlotSelRaw(uint8_t slot, uint8_t value);
#ifdef __cplusplus
}
#endif

static int sOmmpFailures;

#define OMMP_CHECK(cond, ...)                         \
    do {                                              \
        if (!(cond)) {                                \
            printf("[TEST] FAIL: ");                  \
            printf(__VA_ARGS__);                      \
            printf(" (%s:%d)\n", __FILE__, __LINE__); \
            sOmmpFailures++;                          \
        }                                             \
    } while (0)

namespace {

const uint32_t kOmmpGerudoMM = 0x00000100u;
const uint32_t kOmmpSkullMM = 0x00000200u;
const uint32_t kOmmpSpookyMM = 0x00000400u;
const uint32_t kOmmpSharedGerudo = 0x00004000u;
const uint32_t kOmmpSharedSkull = 0x00008000u;
const uint32_t kOmmpSharedSpooky = 0x00010000u;

void OmmpFreezeWorld(uint32_t flags) {
    ComboContext_Init();
    gComboCtx.sourceIsRando = true;
    gComboCtx.sharedRandoSeed = 0x1569570Eu;
    ComboSettingsRecord rec;
    Combo_ComboSettingsDefaults(&rec);
    ComboItemRulesRecord rules;
    std::memset(&rules, 0, sizeof(rules));
    rules.crossItemFlags = flags;
    Combo_FreezeComboSettingsEx(&rec, &rules);
}

struct OmmpPoolCase {
    const char* label;
    uint32_t rules;
    int want[3];
};

} // namespace

/**
 * Pool assertions for `oot-masks-mm-pool`, invoked from `Test_OoTMasksMMPool`
 * in `test_runner.cpp` after `CreateHarnessStyleContext()`,
 * `InitOTRForMMFirstBoot()`, and `MM_Rando_InitCore()`.
 */
static TestResult OmmpRunPoolChecks(void) {
    printf("[TEST] oot-masks-mm-pool: MM pool counts for RI_MASK_GERUDO/SKULL/SPOOKY under frozen rules (#1569)\n");
    sOmmpFailures = 0;

    void* savedSave = MM_OoTMasksPoolTest_SaveSnapshot();

    const uint16_t unknownId = MM_OoTMasksPoolTest_Id(-1);
    const uint16_t kIds[3] = {
        MM_OoTMasksPoolTest_Id(0),
        MM_OoTMasksPoolTest_Id(1),
        MM_OoTMasksPoolTest_Id(2),
    };
    const char* const kNames[3] = { "RI_MASK_GERUDO", "RI_MASK_SKULL", "RI_MASK_SPOOKY" };
    for (int i = 0; i < 3; i++) {
        OMMP_CHECK(kIds[i] != unknownId, "%s resolved to RI_UNKNOWN", kNames[i]);
    }
    OMMP_CHECK(kIds[0] != kIds[1] && kIds[1] != kIds[2] && kIds[0] != kIds[2],
               "MM OoT-mask ids must be distinct (%u, %u, %u)", (unsigned)kIds[0], (unsigned)kIds[1],
               (unsigned)kIds[2]);

    const uint32_t allMasks = kOmmpGerudoMM | kOmmpSkullMM | kOmmpSpookyMM;
    const uint32_t allBoth = allMasks | kOmmpSharedGerudo | kOmmpSharedSkull | kOmmpSharedSpooky;
    const OmmpPoolCase kCases[] = {
        { "(a) every rule off", 0u, { 0, 0, 0 } },
        { "(b1) Gerudo Mask (MM) rule only", kOmmpGerudoMM, { 1, 0, 0 } },
        { "(b2) Skull Mask (MM) rule only", kOmmpSkullMM, { 0, 1, 0 } },
        { "(b3) Spooky Mask (MM) rule only", kOmmpSpookyMM, { 0, 0, 1 } },
        { "(c) all three MM mask rules on", allMasks, { 1, 1, 1 } },
        { "(d1) Gerudo Mask (MM) + Shared Gerudo", kOmmpGerudoMM | kOmmpSharedGerudo, { 1, 0, 0 } },
        { "(d2) Skull Mask (MM) + Shared Skull", kOmmpSkullMM | kOmmpSharedSkull, { 0, 1, 0 } },
        { "(d3) Spooky Mask (MM) + Shared Spooky", kOmmpSpookyMM | kOmmpSharedSpooky, { 0, 0, 1 } },
        { "(d4) all three MM mask rules + all three shared rules", allBoth, { 1, 1, 1 } },
    };

    constexpr int kCap = 8192;
    std::vector<uint16_t> items((size_t)kCap, 0);
    std::vector<uint16_t> flags((size_t)kCap, 0);

    for (const OmmpPoolCase& c : kCases) {
        OmmpFreezeWorld(c.rules);
        items.assign((size_t)kCap, 0);
        flags.assign((size_t)kCap, 0);
        const int rows = MM_ComboLogic_TestPairedPool(items.data(), flags.data(), kCap);
        OMMP_CHECK(rows > 0 && rows <= kCap, "%s: MM_ComboLogic_TestPairedPool returned %d rows", c.label, rows);
        if (rows > 0 && rows <= kCap) {
            int counts[3] = { 0, 0, 0 };
            for (int i = 0; i < rows; i++) {
                for (int m = 0; m < 3; m++) {
                    if (items[(size_t)i] == kIds[m]) {
                        counts[m]++;
                    }
                }
            }
            for (int m = 0; m < 3; m++) {
                OMMP_CHECK(counts[m] == c.want[m], "%s: %s count=%d, expected %d", c.label, kNames[m], counts[m],
                           c.want[m]);
            }
        }
    }

    MM_OoTMasksPoolTest_RestoreSnapshot(savedSave);
    ComboContext_Init();
    return sOmmpFailures == 0 ? TEST_PASS : TEST_FAIL;
}

/**
 * Verifies in a fresh `--test oot-masks-mm-owned-source` process that C++
 * static initialization (`OoTMasksMMRegistrar`) registers MM's `OwnedSource`
 * before any `ShipInit`, `MM_Rando_InitCore`, or `OnSaveLoad` runs (#1569).
 * Skipped by `--test all` because earlier unit tests (`FmResetAll`) clear
 * `sOwnedSources[GAME_MM]`.
 */
TestResult Test_OoTMasksMMOwnedSource(void) {
    printf("[TEST] oot-masks-mm-owned-source: MM static-init OwnedSource before ShipInit/OnSaveLoad (#1569)\n");
    sOmmpFailures = 0;

    const uint8_t savedRaw = MM_ForeignMask_SlotSelRaw(COMBO_FOREIGN_MASK_SLOT_MM_STONE);
    const uint8_t gerudoBit = Combo_ForeignMask_SlotOwnedBit(COMBO_FOREIGN_MASK_OOT_GERUDO);
    OMMP_CHECK(gerudoBit != 0u, "Combo_ForeignMask_SlotOwnedBit(OOT_GERUDO) returned 0");

    // Rule off: Combo_ForeignMask_Owned stays 0 even with no ShipInit run.
    ComboContext_Init();
    OMMP_CHECK(Combo_ForeignMask_Owned(GAME_MM, COMBO_FOREIGN_MASK_OOT_GERUDO) == 0,
               "Owned() must return 0 when Gerudo Mask (MM) rule is off");

    // Freeze Gerudo Mask (MM) rule WITHOUT calling MM_OoTMasks_RegisterRuntime().
    OmmpFreezeWorld(kOmmpGerudoMM);

    MM_ForeignMask_SetSlotSelRaw(COMBO_FOREIGN_MASK_SLOT_MM_STONE, (uint8_t)(savedRaw | gerudoBit));
    OMMP_CHECK(Combo_ForeignMask_Owned(GAME_MM, COMBO_FOREIGN_MASK_OOT_GERUDO) == 1,
               "static-init OwnedSource must read Gerudo owned bit with frozen rule on");

    MM_ForeignMask_SetSlotSelRaw(COMBO_FOREIGN_MASK_SLOT_MM_STONE, (uint8_t)(savedRaw & (uint8_t)~gerudoBit));
    OMMP_CHECK(Combo_ForeignMask_Owned(GAME_MM, COMBO_FOREIGN_MASK_OOT_GERUDO) == 0,
               "clearing Gerudo owned bit must read 0");

    MM_ForeignMask_SetSlotSelRaw(COMBO_FOREIGN_MASK_SLOT_MM_STONE, savedRaw);
    ComboContext_Init();
    return sOmmpFailures == 0 ? TEST_PASS : TEST_FAIL;
}
