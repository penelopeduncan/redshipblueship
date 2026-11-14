/**
 * @file mm_oot_masks_pool_test.cpp
 * MM-side helper for `oot-masks-mm-pool` (#1569, refs #1452, #980).
 *
 * Names MM's RandoItemIds (`RI_MASK_GERUDO`, `RI_MASK_SKULL`, `RI_MASK_SPOOKY`)
 * and snapshots/restores `gSaveContext` around the production pool bridge;
 * every pool decision is production code's (`GeneratePools` via
 * `MM_ComboLogic_TestPairedPool`).
 */
#ifdef RSBS_SINGLE_EXECUTABLE

#include "global.h"

#include <cstdint>
#include <cstring>

#include "Rando/Types.h"

extern "C" {

/** MM's RandoItemId for the three OoT-in-MM masks (0=Gerudo, 1=Skull, 2=Spooky). */
uint16_t MM_OoTMasksPoolTest_Id(int which) {
    switch (which) {
        case 0:
            return (uint16_t)RI_MASK_GERUDO;
        case 1:
            return (uint16_t)RI_MASK_SKULL;
        case 2:
            return (uint16_t)RI_MASK_SPOOKY;
        default:
            return (uint16_t)RI_UNKNOWN;
    }
}

void* MM_OoTMasksPoolTest_SaveSnapshot(void) {
    SaveContext* copy = new SaveContext;
    std::memcpy(copy, &gSaveContext, sizeof(SaveContext));
    return copy;
}

void MM_OoTMasksPoolTest_RestoreSnapshot(void* snapshot) {
    if (snapshot == nullptr) {
        return;
    }
    SaveContext* copy = static_cast<SaveContext*>(snapshot);
    std::memcpy(&gSaveContext, copy, sizeof(SaveContext));
    delete copy;
}

} // extern "C"

#endif // RSBS_SINGLE_EXECUTABLE
