/**
 * @file test_oot_host_mm_masks.c
 * @brief Locks for Ocarina of Time drawing Majora's Mask's copies of OoT's
 *        Gerudo, Skull and Spooky Masks as OoT's own models (#1452 item 3,
 *        #1568), rather than the mystery stand-in.
 *
 * Linkage note: #included into test_runner.cpp at FILE SCOPE (compiled as C++),
 * next to tests/test_foreign_model.c; every symbol it drives is C-linkage, and
 * file-local names use the Ohmm prefix so they cannot collide with a neighbour.
 */

#include "../context.h"
#include "../foreign_items.h"
#include "../foreign_masks.h"
#include "../foreign_model.h"
#include "../game.h"
#include "../shared_resources.h"
#include "../test_runner.h"
#include "test_named_items.h"

#include <cstdio>
#include <cstring>

#ifdef __cplusplus
extern "C" {
#endif
void MM_OoTMasks_RegisterRuntime(void);
int OoT_ComboModel(uint16_t id, ComboModel* out);
int OoT_ComboModel_DrawRowModel(int drawId, ComboModel* out);
void OoT_ForeignModel_TestSetMountOverride(int value);
int OoT_ForeignModel_TestHostModelFor(uint8_t originGame, uint16_t id, ComboModel* out);
int OoT_ForeignModel_TestForeignMaskDrawId(uint8_t foreignMaskId);
#ifdef __cplusplus
}
#endif

static int sOhmmFailures;

#define OHMM_CHECK(cond, ...)                         \
    do {                                              \
        if (!(cond)) {                                \
            printf("[TEST] FAIL: ");                  \
            printf(__VA_ARGS__);                      \
            printf(" (%s:%d)\n", __FILE__, __LINE__); \
            sOhmmFailures++;                          \
        }                                             \
    } while (0)

namespace {

bool OhmmColorEqual(const ComboModelColor& a, const ComboModelColor& b) {
    return a.set == b.set && a.primLodFrac == b.primLodFrac && std::memcmp(a.prim, b.prim, 3) == 0 &&
           std::memcmp(a.env, b.env, 3) == 0;
}

bool OhmmScrollEqual(const ComboModelScroll& a, const ComboModelScroll& b) {
    return a.segment == b.segment && a.layer == b.layer && a.twoTiles == b.twoTiles && a.x1 == b.x1 && a.y1 == b.y1 &&
           a.x1PerFrame == b.x1PerFrame && a.y1PerFrame == b.y1PerFrame && a.w1 == b.w1 && a.h1 == b.h1 &&
           a.x2 == b.x2 && a.y2 == b.y2 && a.x2PerFrame == b.x2PerFrame && a.y2PerFrame == b.y2PerFrame &&
           a.w2 == b.w2 && a.h2 == b.h2;
}

bool OhmmModelEqual(const ComboModel& a, const ComboModel& b) {
    if (a.partCount != b.partCount || a.opaSetupDl != b.opaSetupDl || a.xluSetupDl != b.xluSetupDl ||
        a.opaSetupList != b.opaSetupList || a.xluSetupList != b.xluSetupList ||
        !OhmmColorEqual(a.opaColor, b.opaColor) || !OhmmColorEqual(a.xluColor, b.xluColor) ||
        a.grayscale != b.grayscale || std::memcmp(a.grayscaleRgb, b.grayscaleRgb, 3) != 0 || a.scale != b.scale) {
        return false;
    }
    for (int i = 0; i < 3; i++) {
        if (a.rotation[i] != b.rotation[i] || a.billboardOffset[i] != b.billboardOffset[i]) {
            return false;
        }
    }
    for (int i = 0; i < a.partCount; i++) {
        if (a.parts[i].dl != b.parts[i].dl || a.parts[i].layer != b.parts[i].layer ||
            a.parts[i].billboard != b.parts[i].billboard || a.parts[i].joint != b.parts[i].joint) {
            return false;
        }
    }
    for (int i = 0; i < COMBO_MODEL_MAX_SEGMENT_COLORS; i++) {
        const ComboModelSegmentColor& x = a.segmentColors[i];
        const ComboModelSegmentColor& y = b.segmentColors[i];
        if (x.segment != y.segment || x.layer != y.layer || x.primLodFrac != y.primLodFrac ||
            std::memcmp(x.prim, y.prim, 4) != 0 || std::memcmp(x.env, y.env, 4) != 0) {
            return false;
        }
    }
    if (a.jointCount != b.jointCount || a.jointSegment != b.jointSegment) {
        return false;
    }
    for (int i = 0; i < a.jointCount && i < COMBO_MODEL_MAX_JOINTS; i++) {
        if (a.joints[i].parent != b.joints[i].parent ||
            std::memcmp(a.joints[i].pos, b.joints[i].pos, sizeof(a.joints[i].pos)) != 0 ||
            std::memcmp(a.joints[i].rot, b.joints[i].rot, sizeof(a.joints[i].rot)) != 0) {
            return false;
        }
    }
    for (int i = 0; i < COMBO_MODEL_MAX_SCROLLS; i++) {
        if (!OhmmScrollEqual(a.scrolls[i], b.scrolls[i])) {
            return false;
        }
    }
    return true;
}

bool OhmmModelZeroed(const ComboModel& m) {
    ComboModel zero;
    Combo_ModelInit(&zero);
    return OhmmModelEqual(m, zero);
}

} // namespace

TestResult Test_OoTHostMMMasksModel(void) {
    sOhmmFailures = 0;
    printf("\n=== Test: OoT Host MM Masks Model (#1452 item 3, #1568) ===\n");

    // Other rows clear the shared-family registry, so re-register MM's members
    // before resolving MM's copies of OoT's three masks.
    MM_OoTMasks_RegisterRuntime();

    ComboModel maskModels[3];
    for (int i = 0; i < 3; i++) {
        Combo_ModelInit(&maskModels[i]);
    }

    for (uint8_t mask = COMBO_FOREIGN_MASK_OOT_GERUDO; mask <= COMBO_FOREIGN_MASK_OOT_SPOOKY; mask++) {
        const int idx = (int)(mask - COMBO_FOREIGN_MASK_OOT_GERUDO);
        const ComboForeignMaskInfo* info = Combo_ForeignMask_Info(mask);
        OHMM_CHECK(info != nullptr && info->name != nullptr, "mask info present for %u", (unsigned)mask);
        if (info == nullptr || info->name == nullptr) {
            continue;
        }

        uint16_t mmId = 0;
        const bool hasMmMember = Combo_SharedFamilyMemberId(GAME_MM, info->sharedRuleBit, &mmId);
        OHMM_CHECK(hasMmMember && mmId != 0, "MM shared family member registered for %s", info->name);
        if (!hasMmMember || mmId == 0) {
            continue;
        }

        const int drawId = OoT_ForeignModel_TestForeignMaskDrawId(mask);
        OHMM_CHECK(drawId >= 0, "OoT draw id resolved for %s", info->name);
        ComboModel wantRow;
        Combo_ModelInit(&wantRow);
        const int gotRow = drawId >= 0 ? OoT_ComboModel_DrawRowModel(drawId, &wantRow) : 0;
        OHMM_CHECK(gotRow == 1, "OoT_ComboModel_DrawRowModel succeeded for %s (drawId=%d)", info->name, drawId);

        // Also compare against OoT's own item source independently of the host's
        // draw-id bridge so a wrong map in both places still fails.
        SharedItem ootItem;
        const bool namedOoT = TestNamedItem((uint8_t)GAME_OOT, info->name, &ootItem);
        OHMM_CHECK(namedOoT, "OoT item found by name for %s", info->name);
        ComboModel ootOwn;
        Combo_ModelInit(&ootOwn);
        const int gotOwn = namedOoT ? OoT_ComboModel(ootItem.id, &ootOwn) : 0;
        OHMM_CHECK(gotOwn == 1, "OoT_ComboModel succeeded for %s", info->name);
        if (gotRow == 1 && gotOwn == 1) {
            OHMM_CHECK(OhmmModelEqual(wantRow, ootOwn), "OoT draw row matches OoT_ComboModel for %s", info->name);
        }

        OoT_ForeignModel_TestSetMountOverride(1);
        ComboModel got;
        const int answered = OoT_ForeignModel_TestHostModelFor((uint8_t)GAME_MM, mmId, &got);
        OHMM_CHECK(answered == 1, "HostModelForItem answered true for MM's %s (id=%u) when mounted", info->name,
                   (unsigned)mmId);
        if (answered == 1 && gotRow == 1) {
            OHMM_CHECK(OhmmModelEqual(got, wantRow), "MM's %s model matches OoT's draw row part-for-part", info->name);
        }
        if (answered == 1 && gotOwn == 1) {
            OHMM_CHECK(OhmmModelEqual(got, ootOwn), "MM's %s model matches OoT's own mask model", info->name);
        }
        maskModels[idx] = got;

        // An OoT-origin item carrying MM's registered id is not MM's copy: the
        // originGame guard must refuse it even with the archive mounted.
        ComboModel ootOrigin;
        const int answeredOoTOrigin = OoT_ForeignModel_TestHostModelFor((uint8_t)GAME_OOT, mmId, &ootOrigin);
        OHMM_CHECK(answeredOoTOrigin == 0 && OhmmModelZeroed(ootOrigin),
                   "OoT-origin item with MM's %s id (%u) is rejected when mounted", info->name, (unsigned)mmId);

        OoT_ForeignModel_TestSetMountOverride(0);
        ComboModel unmounted;
        const int answeredUnmounted = OoT_ForeignModel_TestHostModelFor((uint8_t)GAME_MM, mmId, &unmounted);
        OHMM_CHECK(answeredUnmounted == 0 && OhmmModelZeroed(unmounted),
                   "MM's %s answers false and zeroed when unmounted", info->name);
    }

    OHMM_CHECK(!OhmmModelEqual(maskModels[0], maskModels[1]) && !OhmmModelEqual(maskModels[0], maskModels[2]) &&
                   !OhmmModelEqual(maskModels[1], maskModels[2]),
               "all three MM-hosted OoT masks produce distinct models");

    // Control MM item with a real descriptor: answers identically to
    // Combo_GetForeignItemModel when mounted, false when unmounted.
    SharedItem dekuMask;
    const bool namedDeku = TestNamedItem((uint8_t)GAME_MM, "Deku Mask", &dekuMask);
    OHMM_CHECK(namedDeku, "MM control item Deku Mask found");
    if (namedDeku) {
        ComboModelAnswer dekuWant;
        const uint8_t dekuKind = Combo_GetForeignItemModel((uint8_t)GAME_OOT, dekuMask, &dekuWant);
        OHMM_CHECK(dekuKind == COMBO_MODEL_ANSWER_DESCRIPTOR, "Deku Mask classifies as DESCRIPTOR");

        OoT_ForeignModel_TestSetMountOverride(1);
        ComboModel dekuGot;
        const int dekuOk = OoT_ForeignModel_TestHostModelFor((uint8_t)GAME_MM, dekuMask.id, &dekuGot);
        OHMM_CHECK(dekuOk == 1 && OhmmModelEqual(dekuGot, dekuWant.model),
                   "MM control item Deku Mask still answers its descriptor when mounted");

        OoT_ForeignModel_TestSetMountOverride(0);
        ComboModel dekuUnmounted;
        const int dekuOff = OoT_ForeignModel_TestHostModelFor((uint8_t)GAME_MM, dekuMask.id, &dekuUnmounted);
        OHMM_CHECK(dekuOff == 0 && OhmmModelZeroed(dekuUnmounted),
                   "MM control item Deku Mask answers false when unmounted");
    }

    // Native OoT item passed to the MM-host query still answers false.
    SharedItem ootSpooky;
    const bool namedOoTSpooky = TestNamedItem((uint8_t)GAME_OOT, "Spooky Mask", &ootSpooky);
    OHMM_CHECK(namedOoTSpooky, "OoT control item Spooky Mask found");
    if (namedOoTSpooky) {
        OoT_ForeignModel_TestSetMountOverride(1);
        ComboModel ootRejected;
        const int rejected = OoT_ForeignModel_TestHostModelFor((uint8_t)GAME_OOT, ootSpooky.id, &ootRejected);
        OHMM_CHECK(rejected == 0 && OhmmModelZeroed(ootRejected), "OoT-origin item is rejected by HostModelForItem");
    }

    OoT_ForeignModel_TestSetMountOverride(-1);
    printf("[TEST] oot-host-mm-masks-model: %s\n", sOhmmFailures == 0 ? "PASS" : "FAIL");
    return sOhmmFailures == 0 ? TEST_PASS : TEST_FAIL;
}

#undef OHMM_CHECK
