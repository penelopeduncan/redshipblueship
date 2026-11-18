/**
 * @file test_blast_mask_oot.c
 * @brief Locks for the Blast Mask in Ocarina of Time (#979, epic #975 P4; OoTMM
 *        blastMaskOot, sharedMaskBlast and blastMaskCooldown).
 *
 * Four rows, split by what each one needs:
 *
 *   blast-mask-oot-rule (CTest BlastMaskOoTRule, redship tier) -- THE RULES. The
 *   three tier-4 keys (gCombo.Rando.BlastMaskOoT, SharedMaskBlast and
 *   BlastMaskCooldown), labelled with OoTMM's names, off and Default by
 *   default, in their pinned value spaces; each reaches its own part of the
 *   resolved item rules and nothing else; the shared one is dropped while the
 *   extension is off (OoTMM's `cond`), so the resolver never makes a record the
 *   validity rule refuses; the cooldown resolves from its key into the record's
 *   byte; and a session that walked away from a file holding all three restores
 *   them by name at load (frozen wins at load, #781). Then OoTMM's cooldown table
 *   (frames per value, Default being Majora's Mask's own 310), MM's No Blast Mask
 *   Cooldown composing over it, and one blast per cooldown.
 *
 *   blast-mask-oot-items (CTest BlastMaskOoTItems, redship tier) -- THE ITEM.
 *   The custom ItemID after Roc's Feather and the tables that must cover it (the
 *   lock #977 left to P4): never SLOT()-indexed (past OoT_gItemSlots' 56
 *   entries), inside gItemAgeReqs and sItemActions (PLAYER_IA_NONE), with its own
 *   curated icon in OoT_gItemIcons' custom section and a child age row. Then
 *   ownership gated by the frozen rule (Combo_ForeignMask_Owned over OoT's
 *   registered source), the child trade cycle visiting it only then, the shared
 *   member's give (the flag and an empty trade slot), THE SHARED PAIR (both games
 *   register their Blast Mask at RSBS_XITEM_SHARED_BLAST and the bag composes
 *   one row from them only under the shared rule), and OoT's REAL logic
 *   crediting the mask as a child's explosive only while the rule is frozen on.
 *
 *   blast-mask-oot-spawn (CTest BlastMaskOoTSpawn, redship tier) -- THE SPAWN
 *   DECISION (#1615). Pure ROM-free lock over Combo_BlastMask_StepOotSpawn:
 *   world.pos float coordinates and truncated int16_t explosionCollider sphere
 *   centers (MM_EnBom_Init parity), one blast per cooldown across moving world
 *   positions, idle countdown, and null-pointer guards.
 *
 *   blast-mask-oot-shrub-guardhouse (CTest BlastMaskOoTShrubGuardHouse, redship
 *   tier) -- THE LIVE COLLISION & MARKET GUARD HOUSE LOCK (#1615). Headless
 *   PlayState bring-up driving BlastMaskBeforePlayerUpdate and, from the test
 *   player's update, OoT_BlastMask_OnItemButtons (OoT_Player_UpdateItems'
 *   call), ACTOR_EN_BOM, ACTOR_EN_KUSA, and ACTOR_OBJ_TSUBO without calling
 *   OoT_EnBom_Draw: cuts ground shrubs beside child Link at non-origin X/Y/Z
 *   while leaving an origin control shrub untouched, and in
 *   SCENE_MARKET_GUARD_HOUSE spawns and centers the blast at Link's world.pos
 *   rather than (0, 0, 0) or focus.pos. The press is the mask's only on a frame
 *   that reaches the item buttons (#1718, OoTMM's wrapper), so its water legs
 *   (#1701, #1714) blast swimming at the surface, sinking after falling in, in
 *   every dive and on the Iron Boots, and refuse it while surfacing for breath,
 *   hit in water, arriving swimming or dying in water, and with the hookshot's
 *   hook out (fired) or in flight; its land legs refuse it in a sword swing, a jump
 *   attack, a roll, knocked down, shocked and frozen. Its source half checks
 *   that the call sits in OoT_Player_UpdateItems alone, and that each action a
 *   leg samples reaches the item buttons in z_player.c exactly when the leg says.
 *
 * Every key is spelled from the identity prefix rather than as a literal, so
 * the tier-4 classification scan sees only the manifest's spelling.
 *
 * Linkage note: #included into test_runner.cpp at FILE SCOPE (compiled as C++),
 * like its neighbours; every symbol under test is C-linkage.
 */

#include "../combo_logic.h"
#include "../combo_settings_view.h"
#include "../combo_single_bag.h"
#include "../context.h"
#include "../curated_archives.h"
#include "../cvar_shared_keys.h"
#include "../foreign_items.h"
#include "../foreign_masks.h"
#include "../shared_resources.h"
#include "../test_runner.h"
#include "test_named_items.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <initializer_list>
#include <iterator>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include <libultraship/bridge/consolevariablebridge.h>

#ifdef __cplusplus
extern "C" {
#endif
// games/oot/soh/oot_blast_mask_test.cpp
uint8_t OoT_BlastMaskTest_ItemId(void);
uint8_t OoT_BlastMaskTest_RocsFeatherId(void);
int OoT_BlastMaskTest_ItemSlotsCount(void);
uint32_t OoT_BlastMaskTest_ItemIconsCount(void);
const char* OoT_BlastMaskTest_Icon(uint8_t item);
int OoT_BlastMaskTest_AgeReq(uint8_t item);
int OoT_BlastMaskTest_ChildAge(void);
void OoT_BlastMaskTest_ApplyAgeReqs(int adultMayWear);
int OoT_BlastMaskTest_AgeReqNone(void);
int OoT_BlastMaskTest_SlotAgeReq(void);
void OoT_BlastMaskTest_SetSlotAgeReq(int req);
int OoT_BlastMaskTest_PoolId(void);
int OoT_BlastMaskTest_IsAdvancement(void);
void OoT_BlastMaskTest_SetOwned(int held);
int OoT_BlastMaskTest_Owned(void);
int OoT_BlastMaskTest_TradeSlot(void);
void OoT_BlastMaskTest_SetTradeSlot(uint8_t item);
int OoT_BlastMaskTest_Logic(int held, int child, int which);
int OoT_BlastMaskTest_ShrubAndGuardHouse(void);
int OoT_BlastMaskTest_ActionCount(void);
const char* OoT_BlastMaskTest_ActionFunc(int i);
int OoT_BlastMaskTest_ActionReaches(int i);
// games/oot/soh/Enhancements/randomizer/BlastMaskOoT.cpp
void OoT_BlastMask_RegisterRuntime(void);
// games/oot/soh/oot_foreign_mask_test.cpp
void OoT_ForeignMaskTest_ArmLive(void);
void OoT_TradeCycleTest_Set(uint8_t item, uint16_t ownedBits);
uint8_t OoT_TradeCycleTest_Next(void);
uint8_t OoT_TradeCycleTest_FirstItem(void);
int OoT_TradeCycleTest_ChildTradeCount(void);
// z_kaleido_scope_PAL.c and z_player.c (#983, #984): the two tables' entry counts
// and sItemActions' raw entry.
extern const uint32_t gItemAgeReqsCount;
extern const uint32_t gItemActionsCount;
int32_t OoT_Player_ItemActionTableEntry(uint32_t item);
#ifdef __cplusplus
}
#endif

static int sBmFailures;

#define BM_CHECK(cond, ...)                           \
    do {                                              \
        if (!(cond)) {                                \
            printf("[TEST] FAIL: ");                  \
            printf(__VA_ARGS__);                      \
            printf(" (%s:%d)\n", __FILE__, __LINE__); \
            sBmFailures++;                            \
        }                                             \
    } while (0)

namespace {

// The rule bits, spelled as #976 reserved them (literals, so the lock states the
// .redsave format rather than reading it back from the header under test).
const uint32_t kBmBlastOoT = 0x00000020u;
const uint32_t kBmSharedBlast = 0x00000800u;
const uint32_t kBmSettingsHash = 0x09790979u;
const uint32_t kBmProfileDigest = 0x0BADB1A5u;
const char* const kBmIconPath = "__OTR__icon_item_static_yar/gItemIconBlastMaskTex";

/** A paired world frozen with the shipped combo record and @p flags /
 *  @p cooldown as its item rules (test_cross_item_rules.c's XirFreezeWorld). */
void BmFreezeWorld(uint32_t flags, uint8_t cooldown) {
    ComboContext_Init();
    gComboCtx.sourceIsRando = true;
    gComboCtx.sharedRandoSeed = 0x0979B1A5u;
    gComboCtx.sharedRandoSettingsHash = kBmSettingsHash;
    gComboCtx.mmProfileDigest = kBmProfileDigest;
    ComboSettingsRecord rec;
    Combo_ComboSettingsDefaults(&rec);
    ComboItemRulesRecord rules;
    memset(&rules, 0, sizeof(rules));
    rules.crossItemFlags = flags;
    rules.blastMaskCooldown = cooldown;
    Combo_FreezeComboSettingsEx(&rec, &rules);
}

void BmClearKeys() {
    Combo_ComboSettingClear(COMBO_SETTING_BLAST_MASK_OOT);
    Combo_ComboSettingClear(COMBO_SETTING_SHARED_MASK_BLAST);
    Combo_ComboSettingClear(COMBO_SETTING_BLAST_MASK_COOLDOWN);
}

/** True when the embedded manifest carries `otrPath` ("__OTR__<path>") in the
 *  mm->oot half, by its path or a directory entry. */
bool BmCuratedIntoOoT(const char* otrPath) {
    static const char kOtr[] = "__OTR__";
    if (otrPath == nullptr || strncmp(otrPath, kOtr, sizeof(kOtr) - 1) != 0) {
        return false;
    }
    const std::string path(otrPath + sizeof(kOtr) - 1);
    std::istringstream in{ std::string(rsbs::curated::EmbeddedManifest()) };
    std::string line;
    static const std::string kDir = "mm->oot ";
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (line.compare(0, kDir.size(), kDir) != 0) {
            continue;
        }
        const std::string entry = line.substr(kDir.size());
        const bool dir = !entry.empty() && entry.back() == '/';
        if (dir ? path.compare(0, entry.size(), entry) == 0 : path == entry) {
            return true;
        }
    }
    return false;
}

} // namespace

// ============================================================================
// blast-mask-oot-rule
// ============================================================================

TestResult BlastMaskOoTRule_Run(void) {
    printf("[TEST] blast-mask-oot-rule: the Blast Mask (OoT), Shared Blast Mask and Blast Mask Cooldown keys author "
           "the item rules, the shared one only with its extension, and a file's values come back at load (#979)\n");
    sBmFailures = 0;
    BM_CHECK(Combo_ComboSettingStoreAvailable(), "no CVar store: every key leg below would pass on the defaults");
    ComboContext_Init();
    BmClearKeys();

    // ---- (1) The keys, their labels, defaults and value spaces -------------------
    {
        const std::string prefix = RSBS::kComboIdentityKeyPrefix;
        BM_CHECK(Combo_ComboSettingKey(COMBO_SETTING_BLAST_MASK_OOT) == prefix + "BlastMaskOoT" &&
                     Combo_ComboSettingKey(COMBO_SETTING_SHARED_MASK_BLAST) == prefix + "SharedMaskBlast" &&
                     Combo_ComboSettingKey(COMBO_SETTING_BLAST_MASK_COOLDOWN) == prefix + "BlastMaskCooldown",
                 "the keys are %s, %s and %s", Combo_ComboSettingKey(COMBO_SETTING_BLAST_MASK_OOT),
                 Combo_ComboSettingKey(COMBO_SETTING_SHARED_MASK_BLAST),
                 Combo_ComboSettingKey(COMBO_SETTING_BLAST_MASK_COOLDOWN));
        BM_CHECK(strcmp(Combo_ComboSettingLabel(COMBO_SETTING_BLAST_MASK_OOT), "Blast Mask (OoT)") == 0 &&
                     strcmp(Combo_ComboSettingLabel(COMBO_SETTING_SHARED_MASK_BLAST), "Shared Blast Mask") == 0 &&
                     strcmp(Combo_ComboSettingLabel(COMBO_SETTING_BLAST_MASK_COOLDOWN), "Blast Mask Cooldown") == 0,
                 "the rows are not labelled with OoTMM's names");
        BM_CHECK(Combo_ComboSettingDefault(COMBO_SETTING_BLAST_MASK_OOT) == 0 &&
                     Combo_ComboSettingDefault(COMBO_SETTING_SHARED_MASK_BLAST) == 0 &&
                     Combo_ComboSettingDefault(COMBO_SETTING_BLAST_MASK_COOLDOWN) == 0,
                 "every Blast Mask rule must ship off and Default (OoTMM's defaults; no world moves)");
        for (const ComboSettingId id : { COMBO_SETTING_BLAST_MASK_OOT, COMBO_SETTING_SHARED_MASK_BLAST }) {
            BM_CHECK(Combo_ComboSettingValueValid(id, 0) && Combo_ComboSettingValueValid(id, 1) &&
                         !Combo_ComboSettingValueValid(id, 2) && !Combo_ComboSettingValueValid(id, -1),
                     "'%s' is not exactly 0 or 1", Combo_ComboSettingKey(id));
        }
        for (int32_t v = -1; v <= 7; v++) {
            const bool want = v >= 0 && v <= 5;
            BM_CHECK(Combo_ComboSettingValueValid(COMBO_SETTING_BLAST_MASK_COOLDOWN, v) == want,
                     "cooldown %d judged %s", (int)v, want ? "INVALID" : "valid");
        }
    }

    // ---- (2) The resolver ---------------------------------------------------------
    {
        ComboItemRulesRecord r;
        Combo_ResolveComboItemRules(&r);
        BM_CHECK((r.crossItemFlags & (kBmBlastOoT | kBmSharedBlast)) == 0u && r.blastMaskCooldown == 0u,
                 "unset keys resolved flags %08X, cooldown %u", (unsigned)r.crossItemFlags,
                 (unsigned)r.blastMaskCooldown);

        BM_CHECK(Combo_ComboSettingSet(COMBO_SETTING_BLAST_MASK_OOT, 1) == 1, "the writer refused Blast Mask (OoT)");
        Combo_ResolveComboItemRules(&r);
        BM_CHECK((r.crossItemFlags & (kBmBlastOoT | kBmSharedBlast)) == kBmBlastOoT,
                 "Blast Mask (OoT) alone resolved %08X", (unsigned)r.crossItemFlags);

        BmClearKeys();
        BM_CHECK(Combo_ComboSettingSet(COMBO_SETTING_SHARED_MASK_BLAST, 1) == 1,
                 "the writer refused Shared Blast Mask");
        Combo_ResolveComboItemRules(&r);
        BM_CHECK((r.crossItemFlags & (kBmBlastOoT | kBmSharedBlast)) == 0u && Combo_ComboItemRulesValid(&r),
                 "the Shared Blast Mask without its extension resolved %08X; OoTMM's cond makes it off, and a "
                 "record with it alone is not a world (Combo_ComboItemRulesValid)",
                 (unsigned)r.crossItemFlags);

        BM_CHECK(Combo_ComboSettingSet(COMBO_SETTING_BLAST_MASK_OOT, 1) == 1, "the writer refused Blast Mask (OoT)");
        BM_CHECK(Combo_ComboSettingSet(COMBO_SETTING_BLAST_MASK_COOLDOWN, (int32_t)RSBS_BLAST_COOLDOWN_SHORT) == 1,
                 "the writer refused a cooldown in its space");
        BM_CHECK(Combo_ComboSettingSet(COMBO_SETTING_BLAST_MASK_COOLDOWN, 6) == 0,
                 "the writer took a cooldown outside its pinned space");
        Combo_ResolveComboItemRules(&r);
        BM_CHECK((r.crossItemFlags & (kBmBlastOoT | kBmSharedBlast)) == (kBmBlastOoT | kBmSharedBlast) &&
                     r.blastMaskCooldown == (uint8_t)RSBS_BLAST_COOLDOWN_SHORT && Combo_ComboItemRulesValid(&r),
                 "all three on resolved flags %08X cooldown %u", (unsigned)r.crossItemFlags,
                 (unsigned)r.blastMaskCooldown);
        BM_CHECK((Combo_ComboItemRuleKeyedBits() & (kBmBlastOoT | kBmSharedBlast)) == (kBmBlastOoT | kBmSharedBlast),
                 "the two rule bits are not keyed (%08X)", (unsigned)Combo_ComboItemRuleKeyedBits());
        BM_CHECK((Combo_ComboSettingsRestorableMask() & RSBS_COMBO_DIVERGE_BLAST_MASK_COOLDOWN) != 0u,
                 "the cooldown's divergence is not restorable through its key");
        BmClearKeys();
    }

    // ---- (3) Frozen wins at load ----------------------------------------------------
    {
        BmFreezeWorld(kBmBlastOoT | kBmSharedBlast, (uint8_t)RSBS_BLAST_COOLDOWN_LONG);
        const uint32_t bits = Combo_ComboSettingsDivergence();
        BM_CHECK(bits == (RSBS_COMBO_DIVERGE_CROSS_ITEM_FLAGS | RSBS_COMBO_DIVERGE_BLAST_MASK_COOLDOWN),
                 "a session with every key unset diverges by %08X from a file holding all three", (unsigned)bits);
        BM_CHECK(Combo_ComboSettingsCanRestoreLive(&gComboCtx.comboSettings, &gComboCtx.comboItemRules, bits) == 1,
                 "the load cannot restore the file's Blast Mask rules");
        char names[160];
        ComboSettingsKeyUndo undo;
        const int ok = Combo_ComboSettingsRestoreLive(&gComboCtx.comboSettings, &gComboCtx.comboItemRules, bits, names,
                                                      sizeof(names), &undo);
        printf("[TEST] (3) restored: %s\n", names);
        BM_CHECK(ok == 1 && strcmp(names, "Blast Mask (OoT), Shared Blast Mask, Blast Mask Cooldown") == 0,
                 "the restore wrote '%s', not the three rows in page order", names);
        BM_CHECK(Combo_ComboSettingsDivergence() == 0u, "after the restore the pair still diverges by %08X",
                 (unsigned)Combo_ComboSettingsDivergence());
        Combo_ComboSettingsRestoreUndo(&undo);
        BM_CHECK(!Combo_ComboSettingIsExplicit(COMBO_SETTING_BLAST_MASK_OOT) &&
                     !Combo_ComboSettingIsExplicit(COMBO_SETTING_BLAST_MASK_COOLDOWN),
                 "the undo did not put the keys back to unset");
        ComboContext_Init();
        BmClearKeys();
    }

    // ---- (4) OoTMM's cooldown table and one blast per cooldown ----------------------
    {
        static const uint16_t kFrames[] = { 310u, 1u, 32u, 128u, 512u, 1024u };
        for (uint8_t v = 0; v <= (uint8_t)RSBS_BLAST_COOLDOWN_MAX; v++) {
            BM_CHECK(Combo_BlastMaskCooldownFrames(v) == kFrames[v], "cooldown %u is %u frames, OoTMM's is %u",
                     (unsigned)v, (unsigned)Combo_BlastMaskCooldownFrames(v), (unsigned)kFrames[v]);
            BM_CHECK(Combo_BlastMaskCooldownFor(v, 1) == 0u && Combo_BlastMaskCooldownFor(v, 0) == kFrames[v],
                     "MM's No Blast Mask Cooldown does not compose over cooldown %u", (unsigned)v);
        }
        BM_CHECK(Combo_BlastMaskCooldownFrames(6u) == 310u && Combo_BlastMaskCooldownFrames(255u) == 310u,
                 "a cooldown outside the pinned space must read as Default");
        struct Run {
            uint16_t arm;
            int frames;
            int want;
        };
        static const Run kRuns[] = { { 310u, 620, 2 }, { 310u, 621, 3 }, { 1u, 10, 10 }, { 0u, 10, 10 } };
        for (const Run& run : kRuns) {
            uint16_t left = 0;
            int fired = 0;
            for (int f = 0; f < run.frames; f++) {
                fired += Combo_BlastMask_Step(&left, 1, run.arm);
            }
            BM_CHECK(fired == run.want, "B pressed on each of %d frames with %u armed fired %d blast(s), not %d",
                     run.frames, (unsigned)run.arm, fired, run.want);
        }
        uint16_t left = 5;
        BM_CHECK(Combo_BlastMask_Step(&left, 0, 310u) == 0 && left == 4,
                 "an idle frame must count the cooldown down and fire nothing");
        BM_CHECK(Combo_BlastMask_Step(nullptr, 1, 310u) == 0, "a null cooldown must fire nothing");
    }

    printf("[TEST] blast-mask-oot-rule: %s\n", sBmFailures == 0 ? "PASS" : "FAIL");
    return sBmFailures == 0 ? TEST_PASS : TEST_FAIL;
}

// ============================================================================
// blast-mask-oot-items
// ============================================================================

TestResult Test_BlastMaskOoTItems(void) {
    printf("[TEST] blast-mask-oot-items: the Blast Mask's custom ItemID stays inside OoT's tables and out of SLOT(), "
           "is owned and cycled only under the frozen rule, pairs into one bag row under the shared rule, and OoT's "
           "logic credits it only then (#979)\n");
    sBmFailures = 0;
    BM_CHECK(OoT_ComboLogic_TestEnsureItemTableTransient() == 0, "OoT's item table is not available");
    const uint8_t blast = OoT_BlastMaskTest_ItemId();
    const uint8_t rocs = OoT_BlastMaskTest_RocsFeatherId();

    // ---- (1) THE TABLE-BOUNDS LOCK for the custom ItemIDs ----------------------------
    {
        BM_CHECK(blast == (uint8_t)(rocs + 1u), "the Blast Mask is ItemID 0x%02X, not the one after Roc's (0x%02X)",
                 (unsigned)blast, (unsigned)rocs);
        OoT_BlastMask_RegisterRuntime();
        for (unsigned id = rocs; id <= blast; id++) {
            BM_CHECK((int)id >= OoT_BlastMaskTest_ItemSlotsCount(),
                     "custom ItemID 0x%02X is inside OoT_gItemSlots (%d entries): SLOT() would name a real slot", id,
                     OoT_BlastMaskTest_ItemSlotsCount());
            BM_CHECK(id < gItemAgeReqsCount, "custom ItemID 0x%02X is past gItemAgeReqs (%u)", id,
                     (unsigned)gItemAgeReqsCount);
            BM_CHECK(id < gItemActionsCount && OoT_Player_ItemActionTableEntry(id) == 0,
                     "custom ItemID 0x%02X is past sItemActions (%u) or not PLAYER_IA_NONE", id,
                     (unsigned)gItemActionsCount);
            const char* icon = OoT_BlastMaskTest_Icon((uint8_t)id);
            BM_CHECK(id < OoT_BlastMaskTest_ItemIconsCount() && icon != nullptr && icon[0] != '\0',
                     "custom ItemID 0x%02X has no OoT_gItemIcons row (%u entries)", id,
                     (unsigned)OoT_BlastMaskTest_ItemIconsCount());
        }
        const char* icon = OoT_BlastMaskTest_Icon(blast);
        BM_CHECK(icon != nullptr && strcmp(icon, kBmIconPath) == 0 && BmCuratedIntoOoT(icon),
                 "the Blast Mask's icon is '%s', not MM's own icon curated in OoT's half", icon ? icon : "(null)");
        BM_CHECK(BmCuratedIntoOoT("__OTR__item_name_static/gItemNameBlastMaskENGTex") &&
                     BmCuratedIntoOoT("__OTR__objects/object_mask_bakuretu/object_mask_bakuretu_DL_000440"),
                 "the Blast Mask's pause name or its glow pass is not curated in OoT's half");
        BM_CHECK(OoT_BlastMaskTest_AgeReq(blast) == OoT_BlastMaskTest_ChildAge(),
                 "the Blast Mask's age row is %d, not the child's %d", OoT_BlastMaskTest_AgeReq(blast),
                 OoT_BlastMaskTest_ChildAge());
        // #1647: OoT_ForeignMask_ApplyAgeReqs (z_play.c, the pause screen) frees the row and the trade
        // slot holding the mask for an adult while allowed, puts the row back to the child's otherwise,
        // and never frees the slot for another item nor restricts it.
        {
            const int child = OoT_BlastMaskTest_ChildAge();
            const int none = OoT_BlastMaskTest_AgeReqNone();
            const int savedSlotReq = OoT_BlastMaskTest_SlotAgeReq();
            const int savedTrade = OoT_BlastMaskTest_TradeSlot();
            OoT_BlastMaskTest_SetTradeSlot(blast);
            OoT_BlastMaskTest_SetSlotAgeReq(child);
            OoT_BlastMaskTest_ApplyAgeReqs(1);
            BM_CHECK(OoT_BlastMaskTest_AgeReq(blast) == none && OoT_BlastMaskTest_SlotAgeReq() == none,
                     "allowed: the Blast Mask's age row is %d and the trade slot's %d, not any age's %d",
                     OoT_BlastMaskTest_AgeReq(blast), OoT_BlastMaskTest_SlotAgeReq(), none);
            OoT_BlastMaskTest_ApplyAgeReqs(0);
            BM_CHECK(OoT_BlastMaskTest_AgeReq(blast) == child && OoT_BlastMaskTest_SlotAgeReq() == none,
                     "not allowed: the Blast Mask's age row is %d (not the child's) or the slot was restricted (%d)",
                     OoT_BlastMaskTest_AgeReq(blast), OoT_BlastMaskTest_SlotAgeReq());
            OoT_BlastMaskTest_SetTradeSlot(0x24); // ITEM_MASK_KEATON: the native code's own
            OoT_BlastMaskTest_SetSlotAgeReq(child);
            OoT_BlastMaskTest_ApplyAgeReqs(1);
            BM_CHECK(OoT_BlastMaskTest_SlotAgeReq() == child,
                     "allowed with the Keaton Mask in the slot: the cross-game rows freed the slot (%d)",
                     OoT_BlastMaskTest_SlotAgeReq());
            OoT_BlastMask_RegisterRuntime();
            BM_CHECK(OoT_BlastMaskTest_AgeReq(blast) == child, "RegisterRuntime left the Blast Mask's row at %d",
                     OoT_BlastMaskTest_AgeReq(blast));
            OoT_BlastMaskTest_ApplyAgeReqs(0); // the Stone Mask's row back to the child's too
            OoT_BlastMaskTest_SetTradeSlot((uint8_t)savedTrade);
            OoT_BlastMaskTest_SetSlotAgeReq(savedSlotReq);
        }
        BM_CHECK(OoT_BlastMaskTest_IsAdvancement() == 1, "RG_BLAST_MASK is not an advancement item");
    }

    // ---- (2) Ownership, gated by the frozen rule --------------------------------------
    {
        OoT_ForeignMaskTest_ArmLive();
        OoT_BlastMask_RegisterRuntime();
        OoT_BlastMaskTest_SetOwned(1);
        ComboContext_Init();
        BM_CHECK(Combo_ForeignMask_Owned(GAME_OOT, COMBO_FOREIGN_MASK_MM_BLAST) == 0,
                 "with nothing frozen OoT owns the Blast Mask");
        BmFreezeWorld(0u, 0u);
        BM_CHECK(Combo_ForeignMask_Owned(GAME_OOT, COMBO_FOREIGN_MASK_MM_BLAST) == 0,
                 "a world frozen with the rule off owns the Blast Mask");
        BmFreezeWorld(kBmBlastOoT, 0u);
        BM_CHECK(Combo_ForeignMask_Owned(GAME_OOT, COMBO_FOREIGN_MASK_MM_BLAST) == 1,
                 "a world frozen with the rule on does not own the Blast Mask its flag says it holds");
        OoT_BlastMaskTest_SetOwned(0);
        BM_CHECK(Combo_ForeignMask_Owned(GAME_OOT, COMBO_FOREIGN_MASK_MM_BLAST) == 0,
                 "the rule alone owns the Blast Mask without its flag");
    }

    // ---- (3) The child trade cycle visits it only while owned under the rule ----------
    {
        const uint8_t first = OoT_TradeCycleTest_FirstItem();
        const int count = OoT_TradeCycleTest_ChildTradeCount();
        const uint8_t truth = (uint8_t)(first + count - 1);
        const uint16_t truthOnly = (uint16_t)(1u << (count - 1));
        OoT_BlastMaskTest_SetOwned(1);
        BmFreezeWorld(kBmBlastOoT, 0u);
        OoT_TradeCycleTest_Set(truth, truthOnly);
        BM_CHECK(OoT_TradeCycleTest_Next() == blast,
                 "after the Mask of Truth the cycle gave 0x%02X, not the Blast Mask",
                 (unsigned)OoT_TradeCycleTest_Next());
        BmFreezeWorld(0u, 0u);
        OoT_TradeCycleTest_Set(truth, truthOnly);
        BM_CHECK(OoT_TradeCycleTest_Next() == truth, "with the rule off the cycle still visited 0x%02X",
                 (unsigned)OoT_TradeCycleTest_Next());
        OoT_BlastMaskTest_SetOwned(0);
    }

    // ---- (4) The shared member's give: the flag and an empty child trade slot --------
    {
        ComboSharedFamilyMember oot;
        memset(&oot, 0, sizeof(oot));
        const bool registered = Combo_SharedFamilyMemberGet(GAME_OOT, kBmSharedBlast, &oot);
        BM_CHECK(registered && (int)oot.nativeId == OoT_BlastMaskTest_PoolId(),
                 "OoT's Blast Mask is not the Shared Blast Mask family's OoT member");
        if (registered && oot.give != nullptr && oot.held != nullptr) {
            OoT_ForeignMaskTest_ArmLive();
            BM_CHECK(!oot.held(oot.nativeId), "a fresh save holds the Blast Mask");
            oot.give(oot.nativeId);
            BM_CHECK(oot.held(oot.nativeId) && OoT_BlastMaskTest_Owned() == 1 &&
                         OoT_BlastMaskTest_TradeSlot() == (int)blast,
                     "the give left owned %d, trade slot 0x%02X", OoT_BlastMaskTest_Owned(),
                     (unsigned)OoT_BlastMaskTest_TradeSlot());
            OoT_ForeignMaskTest_ArmLive();
            OoT_BlastMaskTest_SetTradeSlot(OoT_TradeCycleTest_FirstItem());
            oot.give(oot.nativeId);
            BM_CHECK(OoT_BlastMaskTest_TradeSlot() == (int)OoT_TradeCycleTest_FirstItem(),
                     "the give replaced the child trade slot's own item");
        }
    }

    // ---- (5) THE SHARED PAIR: one bag row, only under the shared rule -----------------
    {
        SharedItem mmBlast;
        memset(&mmBlast, 0, sizeof(mmBlast));
        BM_CHECK(TestNamedItem((uint8_t)GAME_MM, "Blast Mask", &mmBlast), "MM has no item named \"Blast Mask\"");
        uint16_t mmId = 0;
        BM_CHECK(Combo_SharedFamilyMemberId(GAME_MM, kBmSharedBlast, &mmId) && mmId == mmBlast.id,
                 "MM's Blast Mask (%u) is not the Shared Blast Mask family's MM member (%u)", (unsigned)mmBlast.id,
                 (unsigned)mmId);
        ComboLogicSharedPair pairs[RSBS_XITEM_BIT_COUNT];
        BmFreezeWorld(kBmBlastOoT, 0u);
        int n = Combo_SingleBag_SharedPairs(pairs, (int)RSBS_XITEM_BIT_COUNT);
        BM_CHECK(n == 0, "the Blast Mask (OoT) rule alone built %d shared pair(s)", n);
        BmFreezeWorld(kBmBlastOoT | kBmSharedBlast, 0u);
        n = Combo_SingleBag_SharedPairs(pairs, (int)RSBS_XITEM_BIT_COUNT);
        printf("[TEST] (5) shared rule on: %d pair(s)%s", n, n > 0 ? ":" : "");
        for (int i = 0; i < n; i++) {
            printf(" (%u, %u)", (unsigned)pairs[i].ootId, (unsigned)pairs[i].mmId);
        }
        printf(" (expected 1: (%d, %u))\n", OoT_BlastMaskTest_PoolId(), (unsigned)mmBlast.id);
        BM_CHECK(n == 1 && (int)pairs[0].ootId == OoT_BlastMaskTest_PoolId() && pairs[0].mmId == mmBlast.id,
                 "the Shared Blast Mask did not compose exactly one row from OoT's and MM's Blast Masks");
        BM_CHECK((Combo_SharedFamiliesArmed() & kBmSharedBlast) == kBmSharedBlast,
                 "the shared rule did not arm its family");
    }

    // ---- (6) OoT's logic credits it as a child's explosive only under the rule --------
    {
        static const char* const kWhich[] = { "BlastOrSmash", "CanCutShrubs", "CanHitSwitch(close)",
                                              "CanKillEnemy(Beamos)", "CanUse" };
        BmFreezeWorld(0u, 0u);
        for (int w = 0; w < 5; w++) {
            const int got = OoT_BlastMaskTest_Logic(1, 1, w);
            BM_CHECK(got == 0, "rule off: %s answered %d for a child holding the Blast Mask", kWhich[w], got);
        }
        BmFreezeWorld(kBmBlastOoT, 0u);
        for (int w = 0; w < 5; w++) {
            const int child = OoT_BlastMaskTest_Logic(1, 1, w);
            const int adult = OoT_BlastMaskTest_Logic(1, 0, w);
            const int none = OoT_BlastMaskTest_Logic(0, 1, w);
            BM_CHECK(child == 1 && adult == 0 && none == 0,
                     "rule on: %s answered child %d, adult %d, child without it %d (expected 1, 0, 0)", kWhich[w],
                     child, adult, none);
        }
    }

    ComboContext_Init();
    OoT_ForeignMaskTest_ArmLive();
    printf("[TEST] blast-mask-oot-items: %s\n", sBmFailures == 0 ? "PASS" : "FAIL");
    return sBmFailures == 0 ? TEST_PASS : TEST_FAIL;
}

// ============================================================================
// blast-mask-oot-spawn
// ============================================================================

TestResult Test_BlastMaskOoTSpawn(void) {
    printf("[TEST] blast-mask-oot-spawn: Combo_BlastMask_StepOotSpawn anchors bomb spawn and explosion sphere center "
           "at Link's world.pos once per cooldown (#1615)\n");
    sBmFailures = 0;

    // ---- (1) Coordinate propagation and int16_t sphere-center truncation -------------
    {
        struct Case {
            float wx;
            float wy;
            float wz;
            int16_t cx;
            int16_t cy;
            int16_t cz;
        };
        static const Case kCases[] = {
            { 240.75f, 140.5f, -180.75f, 240, 140, -180 },
            { -100.5f, -45.25f, 50.875f, -100, -45, 50 },
            { 0.0f, 0.0f, 0.0f, 0, 0, 0 },
            { 3276.9f, -120.9f, -2048.4f, 3276, -120, -2048 },
        };
        for (const Case& c : kCases) {
            uint16_t left = 0;
            ComboBlastMaskSpawn out = { -999.0f, -999.0f, -999.0f, -999, -999, -999 };
            const int fired = Combo_BlastMask_StepOotSpawn(&left, 1, 310u, c.wx, c.wy, c.wz, &out);
            BM_CHECK(fired == 1 && left == 310u, "ready press did not fire and arm 310 (fired=%d left=%u)", fired,
                     (unsigned)left);
            BM_CHECK(out.x == c.wx && out.y == c.wy && out.z == c.wz, "spawn floats (%f, %f, %f) != (%f, %f, %f)",
                     (double)out.x, (double)out.y, (double)out.z, (double)c.wx, (double)c.wy, (double)c.wz);
            BM_CHECK(out.centerX == c.cx && out.centerY == c.cy && out.centerZ == c.cz,
                     "center (%d, %d, %d) != (%d, %d, %d)", (int)out.centerX, (int)out.centerY, (int)out.centerZ,
                     (int)c.cx, (int)c.cy, (int)c.cz);
        }
    }

    // ---- (2) One blast per cooldown across moving world positions -------------------
    {
        const uint16_t defArm = Combo_BlastMaskCooldownFrames((uint8_t)RSBS_BLAST_COOLDOWN_DEFAULT);
        uint16_t left = 0;
        int firedCount = 0;
        int fireFrames[4] = { -1, -1, -1, -1 };
        ComboBlastMaskSpawn out = { -1.0f, -1.0f, -1.0f, -1, -1, -1 };
        for (int f = 0; f < 621; f++) {
            const float wx = 10.0f + (float)f;
            const float wy = -20.0f - (float)f;
            const float wz = 30.0f + (float)(2 * f);
            ComboBlastMaskSpawn sentinel = { 777.0f, 888.0f, 999.0f, 777, 888, 999 };
            if (Combo_BlastMask_StepOotSpawn(&left, 1, defArm, wx, wy, wz, &sentinel) != 0) {
                if (firedCount < 4) {
                    fireFrames[firedCount] = f;
                }
                firedCount++;
                out = sentinel;
                BM_CHECK(out.x == wx && out.y == wy && out.z == wz && out.centerX == (int16_t)wx &&
                             out.centerY == (int16_t)wy && out.centerZ == (int16_t)wz,
                         "frame %d did not stamp current world.pos into spawn", f);
            } else {
                BM_CHECK(sentinel.x == 777.0f && sentinel.centerX == 777 && sentinel.centerZ == 999,
                         "non-firing cooldown frame %d overwrote spawn struct", f);
            }
        }
        BM_CHECK(firedCount == 3 && fireFrames[0] == 0 && fireFrames[1] == 310 && fireFrames[2] == 620,
                 "621 held-B frames with Default 310 fired %d blasts at (%d, %d, %d)", firedCount, fireFrames[0],
                 fireFrames[1], fireFrames[2]);
        BM_CHECK(out.centerX == 630 && out.centerY == -640 && out.centerZ == 1270,
                 "final blast center (%d, %d, %d) != (630, -640, 1270)", (int)out.centerX, (int)out.centerY,
                 (int)out.centerZ);

        // Instant (1 frame) and NoBlastMaskCooldown (0 frames) fire on every pressed frame.
        const uint16_t instantArm = Combo_BlastMaskCooldownFrames((uint8_t)RSBS_BLAST_COOLDOWN_INSTANT);
        const uint16_t zeroArm = Combo_BlastMaskCooldownFor((uint8_t)RSBS_BLAST_COOLDOWN_VERY_LONG, 1);
        for (const uint16_t arm : { instantArm, zeroArm }) {
            uint16_t cd = 0;
            int blasts = 0;
            for (int f = 0; f < 8; f++) {
                ComboBlastMaskSpawn stepOut = { 0.0f, 0.0f, 0.0f, 0, 0, 0 };
                blasts += Combo_BlastMask_StepOotSpawn(&cd, 1, arm, (float)(f * 16), 12.0f, -8.0f, &stepOut);
                BM_CHECK(stepOut.centerX == (int16_t)(f * 16) && stepOut.centerY == 12 && stepOut.centerZ == -8,
                         "arm=%u frame %d center mismatch", (unsigned)arm, f);
            }
            BM_CHECK(blasts == 8, "arm=%u over 8 pressed frames fired %d blasts, expected 8", (unsigned)arm, blasts);
        }
    }

    // ---- (3) Idle countdown and null-pointer guards ---------------------------------
    {
        uint16_t left = 4;
        ComboBlastMaskSpawn untouched = { 42.0f, 43.0f, 44.0f, 42, 43, 44 };
        BM_CHECK(Combo_BlastMask_StepOotSpawn(&left, 0, 310u, 1.0f, 2.0f, 3.0f, &untouched) == 0 && left == 3u &&
                     untouched.x == 42.0f && untouched.centerX == 42,
                 "idle frame must decrement cooldown without writing spawn");
        BM_CHECK(Combo_BlastMask_StepOotSpawn(nullptr, 1, 310u, 1.0f, 2.0f, 3.0f, &untouched) == 0 &&
                     untouched.x == 42.0f && untouched.centerX == 42,
                 "null cooldownLeft must return 0 without writing spawn");
        uint16_t cd0 = 0;
        uint16_t cd4 = 4;
        BM_CHECK(Combo_BlastMask_StepOotSpawn(&cd0, 1, 310u, 1.0f, 2.0f, 3.0f, nullptr) == 0 && cd0 == 0u,
                 "null out pointer must return 0 without arming cooldownLeft");
        BM_CHECK(Combo_BlastMask_StepOotSpawn(&cd4, 1, 310u, 1.0f, 2.0f, 3.0f, nullptr) == 0 && cd4 == 4u,
                 "null out pointer must return 0 without decrementing cooldownLeft");
    }

    printf("[TEST] blast-mask-oot-spawn: %s\n", sBmFailures == 0 ? "PASS" : "FAIL");
    return sBmFailures == 0 ? TEST_PASS : TEST_FAIL;
}

// ============================================================================
// blast-mask-oot-shrub-guardhouse
// ============================================================================

namespace {

/** A checkout file (RSBS_SOURCE_DIR), or "" when it cannot be read. */
std::string BmReadSource(const char* rel) {
#ifdef RSBS_SOURCE_DIR
    std::ifstream in(std::string(RSBS_SOURCE_DIR) + "/" + rel, std::ios::binary);
    if (!in.good()) {
        return std::string();
    }
    std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    text.erase(std::remove(text.begin(), text.end(), '\r'), text.end());
    return text;
#else
    (void)rel;
    return std::string();
#endif
}

/** `text` without its comments (a '/' pair inside a string is never followed by
 *  the scan's needles, so strings are not tracked). */
std::string BmStripComments(const std::string& text) {
    std::string out;
    out.reserve(text.size());
    for (size_t i = 0; i < text.size(); i++) {
        if (text[i] == '/' && i + 1 < text.size() && text[i + 1] == '/') {
            while (i < text.size() && text[i] != '\n') {
                i++;
            }
            out.push_back('\n');
        } else if (text[i] == '/' && i + 1 < text.size() && text[i + 1] == '*') {
            const size_t end = text.find("*/", i + 2);
            i = (end == std::string::npos) ? text.size() : end + 1;
            out.push_back(' ');
        } else {
            out.push_back(text[i]);
        }
    }
    return out;
}

/** From `open` (a '{' or '('), the index just past its match, or npos. */
size_t BmMatchClose(const std::string& text, size_t open) {
    const char o = text[open];
    const char c = (o == '{') ? '}' : ')';
    int depth = 0;
    for (size_t q = open; q < text.size(); q++) {
        if (text[q] == o) {
            depth++;
        } else if (text[q] == c && --depth == 0) {
            return q + 1;
        }
    }
    return std::string::npos;
}

/** The body ("{...}") of the function `name` defined at column 0 of `src`, or "".
 *  `name` is a bare name, or the definition's head from column 0 ("void Name"). */
std::string BmBodyOf(const std::string& src, const std::string& name) {
    const std::string needle = name + "(";
    const bool head = name.find(' ') != std::string::npos;
    size_t at = 0;
    while ((at = src.find(needle, at)) != std::string::npos) {
        const size_t lineStart =
            at == 0 || src.rfind('\n', at - 1) == std::string::npos ? 0 : src.rfind('\n', at - 1) + 1;
        const std::string prefix = src.substr(lineStart, at - lineStart);
        const bool topLevel = head ? prefix.empty()
                                   : !prefix.empty() && prefix[0] != ' ' && prefix[0] != '\t' &&
                                         prefix.find_first_of("(;=#{}") == std::string::npos &&
                                         (prefix.back() == ' ' || prefix.back() == '*');
        if (topLevel) {
            size_t p = BmMatchClose(src, at + name.size());
            while (p != std::string::npos && p < src.size() && (src[p] == ' ' || src[p] == '\n' || src[p] == '\t')) {
                p++;
            }
            if (p != std::string::npos && p < src.size() && src[p] == '{') {
                const size_t end = BmMatchClose(src, p);
                if (end != std::string::npos) {
                    return src.substr(p, end - p);
                }
            }
        }
        at += needle.size();
    }
    return std::string();
}

/** Does `body` reach the item buttons (#1718)? True when it, or a function of
 *  `src` it calls (followed through their bodies), calls OoT_Player_UpdateUpperBody
 *  (the only caller of OoT_Player_UpdateItems), OoT_Player_UpdateItems itself,
 *  OoT_Player_TryActionInterrupt (which runs the idle list with the upper body),
 *  or OoT_Player_TryActionHandlerList with a last argument other than false.
 *  `via` names the first one found. */
bool BmReachesItems(const std::string& src, const std::string& body, std::set<std::string>& seen, std::string& via) {
    const std::string code = BmStripComments(body);
    static const char* const kKeywords[] = { "if", "while", "for", "switch", "return", "sizeof" };
    for (size_t i = 0; i < code.size(); i++) {
        if (!(isalpha((unsigned char)code[i]) || code[i] == '_') ||
            (i > 0 && (isalnum((unsigned char)code[i - 1]) || code[i - 1] == '_'))) {
            continue;
        }
        size_t j = i;
        while (j < code.size() && (isalnum((unsigned char)code[j]) || code[j] == '_')) {
            j++;
        }
        const std::string name = code.substr(i, j - i);
        size_t k = j;
        while (k < code.size() && (code[k] == ' ' || code[k] == '\n' || code[k] == '\t')) {
            k++;
        }
        const bool member = i > 0 && (code[i - 1] == '.' || code[i - 1] == '>');
        i = j - 1;
        if (k >= code.size() || code[k] != '(' || member) {
            continue;
        }
        bool keyword = false;
        for (const char* kw : kKeywords) {
            keyword = keyword || name == kw;
        }
        if (keyword) {
            continue;
        }
        if (name == "OoT_Player_UpdateUpperBody" || name == "OoT_Player_UpdateItems" ||
            name == "OoT_Player_TryActionInterrupt") {
            via = name;
            return true;
        }
        if (name == "OoT_Player_TryActionHandlerList") {
            const size_t close = BmMatchClose(code, k);
            const std::string args = code.substr(k + 1, close == std::string::npos ? 0 : close - k - 2);
            std::string last = args.substr(args.rfind(',') == std::string::npos ? 0 : args.rfind(',') + 1);
            last.erase(std::remove_if(last.begin(), last.end(), [](char ch) { return isspace((unsigned char)ch); }),
                       last.end());
            if (last != "false" && last != "0") {
                via = name + " (upper body: " + last + ")";
                return true;
            }
            continue;
        }
        if (!seen.insert(name).second) {
            continue;
        }
        const std::string callee = BmBodyOf(src, name);
        if (!callee.empty() && BmReachesItems(src, callee, seen, via)) {
            via = name + " -> " + via;
            return true;
        }
    }
    return false;
}

bool BmReachesItems(const std::string& src, const std::string& body, std::string& via) {
    std::set<std::string> seen;
    return BmReachesItems(src, body, seen, via);
}

size_t BmCount(const std::string& text, const std::string& needle) {
    size_t n = 0;
    for (size_t at = text.find(needle); at != std::string::npos; at = text.find(needle, at + needle.size())) {
        n++;
    }
    return n;
}

bool BmInOrder(const std::string& text, std::initializer_list<const char*> needles) {
    size_t at = 0;
    for (const char* needle : needles) {
        at = text.find(needle, at);
        if (at == std::string::npos) {
            return false;
        }
        at += strlen(needle);
    }
    return true;
}

/** The source half of the row (#1718): the press is taken at the item buttons
 *  and nowhere else, and kBm1615Actions' `reaches` column (which decides, for
 *  each leg, whether its frame reaches the item buttons) is z_player.c's. */
void BmCheckItemButtonSource() {
    const std::string player = BmReadSource("games/oot/src/overlays/actors/ovl_player_actor/z_player.c");
    const std::string blast = BmReadSource("games/oot/soh/Enhancements/randomizer/BlastMaskOoT.cpp");
    if (player.empty() || blast.empty()) {
        BM_CHECK(false, "SL0 z_player.c or BlastMaskOoT.cpp could not be read (RSBS_SOURCE_DIR)");
        return;
    }

    // SL1: one call, inside OoT_Player_UpdateItems' gate, just before the vanilla
    // OoT_Player_ProcessItemButtons call it shares a block with.
    BM_CHECK(BmCount(player, "OoT_BlastMask_OnItemButtons(") == 1,
             "SL1 z_player.c does not call OoT_BlastMask_OnItemButtons exactly once");
    BM_CHECK(BmCount(player, "OoT_Player_ProcessItemButtons(this, play);") == 1,
             "SL1 z_player.c does not call OoT_Player_ProcessItemButtons exactly once");
    const std::string items = BmBodyOf(player, "void OoT_Player_UpdateItems");
    BM_CHECK(BmInOrder(items, { "(gSaveContext.timerState != TIMER_STATE_STOP)) {", "#ifdef RSBS_SINGLE_EXECUTABLE",
                                "OoT_BlastMask_OnItemButtons(play, this, CHECK_BTN_ALL(sControlInput->press.button, "
                                "BTN_B))",
                                "sControlInput->press.button &= ~BTN_B;", "#endif",
                                "OoT_Player_ProcessItemButtons(this, play);" }),
             "SL1 the Blast Mask's call is not inside OoT_Player_UpdateItems' gate before the item buttons");
    const size_t call = items.find("OoT_BlastMask_OnItemButtons(");
    const size_t vanilla = items.find("OoT_Player_ProcessItemButtons(this, play);");
    if (call != std::string::npos && vanilla != std::string::npos && call < vanilla) {
        const std::string between = items.substr(call, vanilla - call);
        BM_CHECK(BmCount(between, "{") == BmCount(between, "}"),
                 "SL1 the Blast Mask's call and the item buttons are not in the same block");
    }
    // SL2: OoT_Player_UpdateItems' one caller is OoT_Player_UpdateUpperBody.
    BM_CHECK(BmCount(player, "OoT_Player_UpdateItems(this, play);") == 1 &&
                 BmBodyOf(player, "s32 OoT_Player_UpdateUpperBody").find("OoT_Player_UpdateItems(this, play);") !=
                     std::string::npos,
             "SL2 OoT_Player_UpdateItems is not called from OoT_Player_UpdateUpperBody alone");

    // SL3: the frame hook takes no press and fires nothing; the entry point does both.
    const std::string hook = BmBodyOf(blast, "void BlastMaskBeforePlayerUpdate");
    BM_CHECK(!hook.empty() && hook.find("BTN_B") == std::string::npos &&
                 hook.find("SpawnBlast(") == std::string::npos &&
                 hook.find("PlayerFreeToBlast(") == std::string::npos &&
                 hook.find("Combo_BlastMask_Step(&sCooldownLeft, 0, 0);") != std::string::npos,
             "SL3 BlastMaskBeforePlayerUpdate reads B, blasts or decides, or no longer counts the cooldown down");
    const std::string entry = BmBodyOf(blast, "extern \"C\" int OoT_BlastMask_OnItemButtons");
    BM_CHECK(entry.find("PlayerFreeToBlast(play, player)") != std::string::npos &&
                 entry.find("SpawnBlast(play, spawn);") != std::string::npos &&
                 BmCount(blast, "PlayerFreeToBlast(play, player)") == 1 &&
                 BmCount(blast, "SpawnBlast(play, spawn);") == 1,
             "SL3 the blast is not decided and spawned in OoT_BlastMask_OnItemButtons alone");

    // SL4: each sampled action reaches the item buttons exactly when the legs say.
    const int count = OoT_BlastMaskTest_ActionCount();
    BM_CHECK(count > 0, "SL4 the action table is empty");
    for (int i = 0; i < count; i++) {
        const char* func = OoT_BlastMaskTest_ActionFunc(i);
        const std::string body = BmBodyOf(player, std::string("void ") + func);
        if (body.empty()) {
            BM_CHECK(false, "SL4 %s is not defined in z_player.c", func);
            continue;
        }
        std::string via;
        std::string scanned = body;
        if (std::string(func) == "Player_Action_Roll") {
            // The roll reaches the item buttons only in the recovery after a bonk
            // (its action interrupt); the legs sample the roll itself.
            BM_CHECK(BmReachesItems(player, body, via), "SL4 the roll no longer reaches the item buttons after a bonk");
            const size_t bonk = body.find("if (this->av2.bonked) {");
            const size_t open = bonk == std::string::npos ? bonk : body.find('{', bonk);
            const size_t close = open == std::string::npos ? open : BmMatchClose(body, open);
            BM_CHECK(close != std::string::npos, "SL4 the roll's bonk branch was not found");
            if (close != std::string::npos) {
                scanned = body.substr(0, open) + body.substr(close);
            }
            via.clear();
        }
        const int reaches = BmReachesItems(player, scanned, via) ? 1 : 0;
        BM_CHECK(reaches == OoT_BlastMaskTest_ActionReaches(i), "SL4 %s %s the item buttons (%s), the legs say %s",
                 func, reaches ? "reaches" : "never reaches", reaches ? via.c_str() : "-",
                 OoT_BlastMaskTest_ActionReaches(i) ? "it does" : "it never does");
    }
}

} // namespace

TestResult BlastMaskOoTShrubGuardHouse_Run(void) {
    printf("[TEST] blast-mask-oot-shrub-guardhouse: child Link's Blast Mask cuts ground shrubs and follows world.pos "
           "in the Market guard house without OoT_EnBom_Draw (#1615), adult Link's only while Masks Equippable "
           "as Adult holds (#1647), and only on a frame on which the item buttons are processed, on land as in "
           "water (#1701, #1714, #1718)\n");
    sBmFailures = 0;
    const int rc = OoT_BlastMaskTest_ShrubAndGuardHouse();
    BmCheckItemButtonSource();
    const bool pass = rc == 0 && sBmFailures == 0;
    printf("[TEST] blast-mask-oot-shrub-guardhouse: %s\n", pass ? "PASS" : "FAIL");
    return pass ? TEST_PASS : TEST_FAIL;
}
