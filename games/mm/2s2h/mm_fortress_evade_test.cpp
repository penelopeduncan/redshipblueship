/**
 * @file mm_fortress_evade_test.cpp
 * Pirates' Fortress guard evasion logic (#1703, OoTMM can_evade_gerudo):
 * evaluates MM's real region lambdas for all nine changed sites (three checks,
 * three non-fighter connections, three fighter connections) across legs (a)-(g),
 * plus two controls (Deku Nut on the tower edge vs. crate 02; internal edge
 * 3 Guard Room -> Purple Guard Room).
 */
#ifdef RSBS_SINGLE_EXECUTABLE

#include "global.h"

#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>

#include "Rando/Rando.h"
#include "Rando/Types.h"
#include "Rando/Logic/Logic.h"
#include "Rando/OoTMasksMM.h"

#include "context.h"       // src/common: ComboContext_Init
#include "foreign_items.h" // src/common: the rule bits, Combo_FreezeComboSettingsEx

extern "C" {
#include "variables.h"
}

extern "C" void MM_Rando_InitCore(void);

namespace {

int sFeFailures = 0;

void FeFail(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    fprintf(stderr, "[MM-FORTRESS-EVADE-GERUDO] FAIL: ");
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");
    va_end(args);
    sFeFailures++;
}

void FeFreezeRules(uint32_t flags) {
    ComboContext_Init();
    ComboSettingsRecord rec;
    Combo_ComboSettingsDefaults(&rec);
    ComboItemRulesRecord rules;
    std::memset(&rules, 0, sizeof(rules));
    rules.crossItemFlags = flags;
    Combo_FreezeComboSettingsEx(&rec, &rules);
}

void FeResetSave() {
    std::memset(&gSaveContext, 0, sizeof(gSaveContext));
    std::memset(gSaveContext.save.saveInfo.inventory.items, ITEM_NONE,
                sizeof(gSaveContext.save.saveInfo.inventory.items));
    gSaveContext.save.playerForm = PLAYER_FORM_HUMAN;
}

bool FeEdge(RandoRegionId from, RandoRegionId to, bool* found) {
    auto& region = Rando::Logic::Regions[from];
    const auto it = region.connections.find(to);
    if (it == region.connections.end()) {
        *found = false;
        return false;
    }
    *found = true;
    return it->second.first();
}

void FeExpectEdge(const char* leg, RandoRegionId from, RandoRegionId to, bool expected, const char* why) {
    bool found = false;
    const bool got = FeEdge(from, to, &found);
    if (!found) {
        FeFail("%s: the edge is not in the region graph, so this leg probes nothing", leg);
        return;
    }
    if (got != expected) {
        FeFail("%s: the edge is %s: %s", leg, got ? "open" : "closed", why);
        return;
    }
    printf("[TEST]   ok: %s: %s\n", leg, expected ? "open" : "closed");
}

bool FeCheck(RandoRegionId regionId, RandoCheckId checkId, bool* found) {
    auto& region = Rando::Logic::Regions[regionId];
    const auto it = region.checks.find(checkId);
    if (it == region.checks.end()) {
        *found = false;
        return false;
    }
    *found = true;
    return it->second.first();
}

void FeExpectCheck(const char* leg, RandoRegionId regionId, RandoCheckId checkId, bool expected, const char* why) {
    bool found = false;
    const bool got = FeCheck(regionId, checkId, &found);
    if (!found) {
        FeFail("%s: the check is not in the region, so this leg probes nothing", leg);
        return;
    }
    if (got != expected) {
        FeFail("%s: the check is %s: %s", leg, got ? "open" : "closed", why);
        return;
    }
    printf("[TEST]   ok: %s: %s\n", leg, expected ? "open" : "closed");
}

struct FeSite {
    const char* name;
    bool isCheck;
    RandoRegionId from;
    RandoRegionId to;
    RandoCheckId check;
    bool needsSword;
};

const FeSite kFeSites[] = {
    { "crate-02", true, RR_PIRATES_FORTRESS_PLAZA_TOWER, RR_MAX, RC_PIRATE_FORTRESS_PLAZA_LARGE_CRATE_02, false },
    { "crate-03", true, RR_PIRATES_FORTRESS_PLAZA_TOWER, RR_MAX, RC_PIRATE_FORTRESS_PLAZA_LARGE_CRATE_03, false },
    { "silver-rupee-chest", true, RR_PIRATES_FORTRESS_INSIDE_3_GUARD_ROOM, RR_MAX,
      RC_PIRATE_FORTRESS_INTERIOR_SILVER_RUPEE_CHEST, false },
    { "plaza-left-upper->plaza", false, RR_PIRATES_FORTRESS_PLAZA_LEFT_UPPER, RR_PIRATES_FORTRESS_PLAZA, RC_UNKNOWN,
      false },
    { "chest-egg->orange-guard", false, RR_PIRATES_FORTRESS_INSIDE_CHEST_EGG_ROOM,
      RR_PIRATES_FORTRESS_INSIDE_ORANGE_GUARD, RC_UNKNOWN, false },
    { "right-clam->maze-guard", false, RR_PIRATES_FORTRESS_RIGHT_CLAM_EGG_ROOM, RR_PIRATES_FORTRESS_INSIDE_MAZE_GUARD,
      RC_UNKNOWN, false },
    { "purple-guard->left-clam-egg", false, RR_PIRATES_FORTRESS_INSIDE_PURPLE_GUARD,
      RR_PIRATES_FORTRESS_LEFT_CLAM_EGG_ROOM, RC_UNKNOWN, true },
    { "orange-guard->chest-egg-room", false, RR_PIRATES_FORTRESS_INSIDE_ORANGE_GUARD,
      RR_PIRATES_FORTRESS_INSIDE_CHEST_EGG_ROOM, RC_UNKNOWN, true },
    { "green-guard->right-clam-egg", false, RR_PIRATES_FORTRESS_INSIDE_GREEN_GUARD,
      RR_PIRATES_FORTRESS_RIGHT_CLAM_EGG_ROOM, RC_UNKNOWN, true },
};

void FePrepareBase(bool needsSword) {
    if (needsSword) {
        SET_EQUIP_VALUE(EQUIP_TYPE_SWORD, EQUIP_VALUE_SWORD_KOKIRI);
    }
}

void FeExpectSite(const FeSite& site, const char* leg, bool expected, const char* why) {
    char label[128];
    std::snprintf(label, sizeof(label), "%s (%s)", site.name, leg);
    if (site.isCheck) {
        FeExpectCheck(label, site.from, site.check, expected, why);
        return;
    }
    FeExpectEdge(label, site.from, site.to, expected, why);
}

void FeRunSite(const FeSite& site, uint32_t allRules) {
    // (a) Base loadout alone with RSBS_XITEM_GERUDO_MASK_MM frozen on: closed.
    FeResetSave();
    FeFreezeRules(allRules);
    FePrepareBase(site.needsSword);
    FeExpectSite(site, "a: base alone", false, "opened without guard evasion");

    // (b) Base + Bow: open.
    FeResetSave();
    FeFreezeRules(allRules);
    FePrepareBase(site.needsSword);
    INV_CONTENT(ITEM_BOW) = ITEM_BOW;
    FeExpectSite(site, "b: base + Bow", true, "CAN_EVADE_GERUDO counts Bow");

    // (c) Base + Hookshot: open.
    FeResetSave();
    FeFreezeRules(allRules);
    FePrepareBase(site.needsSword);
    INV_CONTENT(ITEM_HOOKSHOT) = ITEM_HOOKSHOT;
    FeExpectSite(site, "c: base + Hookshot", true, "CAN_EVADE_GERUDO counts Hookshot");

    // (d) Base + Stone Mask: open.
    FeResetSave();
    FeFreezeRules(allRules);
    FePrepareBase(site.needsSword);
    INV_CONTENT(ITEM_MASK_STONE) = ITEM_MASK_STONE;
    FeExpectSite(site, "d: base + Stone Mask", true, "CAN_EVADE_GERUDO counts Stone Mask");

    // (e) Base + Gerudo Mask with RSBS_XITEM_GERUDO_MASK_MM frozen on: open.
    FeResetSave();
    FeFreezeRules(allRules);
    FePrepareBase(site.needsSword);
    MM_OoTMask_Give(RI_MASK_GERUDO);
    FeExpectSite(site, "e: base + Gerudo Mask, rule on", true, "CAN_EVADE_GERUDO counts Gerudo Mask under its rule");

    // (f) Base + Gerudo Mask with RSBS_XITEM_GERUDO_MASK_MM frozen off: closed.
    FeResetSave();
    FeFreezeRules(allRules & ~RSBS_XITEM_GERUDO_MASK_MM);
    FePrepareBase(site.needsSword);
    MM_OoTMask_Give(RI_MASK_GERUDO);
    FeExpectSite(site, "f: base + Gerudo Mask, rule off", false, "Gerudo Mask opened site with its rule off");

    // (g) Zora Mask alone, no sword on any site: open.
    FeResetSave();
    FeFreezeRules(allRules);
    INV_CONTENT(ITEM_MASK_ZORA) = ITEM_MASK_ZORA;
    FeExpectSite(site, "g: Zora Mask alone, no sword", true, "CAN_EVADE_GERUDO counts Zora Mask");
}

} // namespace

extern "C" int MM_FortressEvade_RunHeadless(void) {
    printf("[TEST] mm-fortress-evade-gerudo: Pirates' Fortress guard evasion across nine sites (#1703)\n");
    sFeFailures = 0;
    MM_Rando_InitCore();
    if (gRegEditor == NULL) {
        static RegEditor sProbeRegEditor = {};
        gRegEditor = &sProbeRegEditor;
    }
    auto saved = std::make_unique<SaveContext>();
    std::memcpy(saved.get(), &gSaveContext, sizeof(SaveContext));
    const uint64_t savedTime = Rando::Logic::gCurrentRegionTime;
    Rando::Logic::gCurrentRegionTime = Rando::Logic::TIME_ALL_SLICES;
    const uint32_t kAll = RSBS_XITEM_GERUDO_MASK_MM | RSBS_XITEM_SKULL_MASK_MM | RSBS_XITEM_SPOOKY_MASK_MM;

    for (const FeSite& site : kFeSites) {
        FeRunSite(site, kAll);
    }

    // Control 1: Deku Nut opens the plaza-to-tower edge, not crate 02.
    FeResetSave();
    FeFreezeRules(kAll);
    INV_CONTENT(ITEM_DEKU_NUT) = ITEM_DEKU_NUT;
    FeExpectEdge("control-1 tower with Deku Nut", RR_PIRATES_FORTRESS_PLAZA, RR_PIRATES_FORTRESS_PLAZA_TOWER, true,
                 "the tower edge still takes a Deku Nut");
    FeExpectCheck("control-1 crate-02 with Deku Nut", RR_PIRATES_FORTRESS_PLAZA_TOWER,
                  RC_PIRATE_FORTRESS_PLAZA_LARGE_CRATE_02, false, "crate-02 must not accept a Deku Nut");

    // Control 2: internal edge inside one OoTMM region stays open empty-handed.
    FeResetSave();
    FeFreezeRules(kAll);
    FeExpectEdge("control-2 internal 3-guard->purple-guard empty-handed", RR_PIRATES_FORTRESS_INSIDE_3_GUARD_ROOM,
                 RR_PIRATES_FORTRESS_INSIDE_PURPLE_GUARD, true, "an edge inside one OoTMM region was tightened");

    ComboContext_Init();
    Rando::Logic::gCurrentRegionTime = savedTime;
    std::memcpy(&gSaveContext, saved.get(), sizeof(SaveContext));
    printf("[TEST] %s: mm-fortress-evade-gerudo (%d failure(s))\n", sFeFailures == 0 ? "PASS" : "FAIL", sFeFailures);
    return sFeFailures;
}

#endif // RSBS_SINGLE_EXECUTABLE
