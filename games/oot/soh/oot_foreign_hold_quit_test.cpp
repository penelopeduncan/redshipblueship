/**
 * @file oot_foreign_hold_quit_test.cpp
 * ROM-free, display-free lock for #1635: an Ocarina of Time host of a Majora's
 * Mask item that is held (the full outbox, #1222) or queued (the RC-queue drain
 * has not run yet) when the player quits survives the quit. CTest label
 * "redship", row ForeignHoldSurvivesQuitOoT in CMake/SingleExecutable.cmake,
 * dispatch "foreign-hold-survives-quit-oot" in src/common/test_runner.cpp.
 *
 * WHAT WAS BROKEN. The host's own taken flag is set when the player collects it
 * and reaches the save at the next commit, but the item lived only in the drain's
 * RAM queue or hold list, which OnLoadGame emptied. After a reload the flag said
 * collected and nothing ever gave the item.
 *
 * THE FIX UNDER TEST. The durable pending set (gComboCtx.crossingPendingOoT,
 * Tier-1): set at both flag-hook push sites (hook_handlers.cpp) and at the hold
 * (OoT_Rando_Foreign_HoldPickup), cleared in the call that writes the outbox row
 * (OoT_Rando_Foreign_RecordPickup), carried by every commit, never promoted to
 * found by the load reconcile (CheckStateLoadSingleExe.cpp), and restored by
 * OnLoadGame's queue reset (OoT_Rando_Foreign_RestoreHolds through the bridge
 * OoT_Rando_TestResetQueuesForLoad).
 *
 * THE FILE. Four OoT hosts of four MM items, as crossing rows 0..3:
 *   C  the Kokiri Sword chest (a chest flag; the scene-flag hook queues it),
 *   R  the Kokiri Forest bridge rupee (a RandomizerInf; the flag hook queues it),
 *   H  Mido's top-left chest (held for a full outbox, then handed back to the
 *      drain by a freed row, and the player quits before the drain records it),
 *   U  the Lake Hylia freestanding heart piece (queued, but the committed half
 *      no longer holds its flag: a half that went back).
 * C, R and U are queued and never drained (the queue window). R's tracker status
 * reads found at the reload (the newest save says pending: pending wins).
 *
 * LEGS, every one through a real save and load (two commits on disk, the real
 * OnLoadFile seam, OnLoadGame's queue reset):
 *   1 the reconcile does not promote a pending host to found;
 *   2 without a live pairing the restore queues nothing and moves no bit;
 *   3 with the pairing the restore queues C, R and H in row order, resets R's
 *     found status, and drops U's bit without queueing it;
 *   4 the drain records each once and every bit clears;
 *   5 a second save and load queues nothing and the outbox holds one row per
 *     host.
 * Every leg runs before any verdict, so a red run shows them all.
 */

#include <z64.h>

#include "context.h"
#include "crossing_store.h"
#include "foreign_items.h"
#include "game.h"
#include "save.h"
#include "shared_items.h"
#include "tests/test_named_items.h"

#include "soh/Enhancements/randomizer/randomizerTypes.h"
#include "soh/Enhancements/randomizer/SeedContext.h"
#include "soh/Enhancements/randomizer/static_data.h"
#include "soh/Enhancements/randomizer/item_location.h"
#include "soh/Enhancements/randomizer/logic.h"
#include "soh/Enhancements/game-interactor/GameInteractor.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <memory>
#include <vector>
#include "test_scratch.h" // a scratch directory per process

extern "C" SaveContext gSaveContext;
// The seam (games/oot/soh/SaveManager.cpp).
extern "C" void OoT_Combo_OnLoadFileSeam(int32_t fileNum, uint32_t savGeneration,
                                         const RsbsOoTSavIdentity* savIdentity);
// hook_handlers.cpp: the real hooks a host's flag fires, OnLoadGame's queue
// reset, and the bridge that empties the queue the hooks feed.
void RandomizerOnFlagSetHandler(int16_t flagType, int16_t flag);
void RandomizerOnSceneFlagSetHandler(int16_t sceneNum, int16_t flagType, int16_t flag);
extern "C" int OoT_Rando_TestResetQueuesForLoad(void);
extern "C" int OoT_Rando_TestTakeQueuedChecks(uint16_t* out, int cap);
// games/mm/2s2h/Rando/Foreign.cpp: an MM half that is the given pair's world.
extern "C" int MM_Rando_AuthorPairHalfForTest(void* mmHalf, size_t mmHalfSize, uint32_t masterSeed);

namespace {

inline const char* kSaveDir() {
    return rsbs::test::ScratchDirCStr("rsbs_test_foreign_hold_quit");
}
constexpr int kSlot = 0;
constexpr uint32_t kMasterSeed = 0x1635C0DEu;
constexpr uint32_t kSettingsHash = 0x16350001u;
constexpr uint16_t kFillId = 0x0042; // the OoT-bound rows that fill the outbox

// The hosts (location_list.cpp) and their flags.
constexpr RandomizerCheck kChest = RC_KF_KOKIRI_SWORD_CHEST;  // SCENE_KOKIRI_FOREST chest 0x00
constexpr RandomizerCheck kRupee = RC_KF_BRIDGE_RUPEE;        // RAND_INF_KF_BRIDGE_RUPEE
constexpr RandomizerCheck kHeld = RC_KF_MIDOS_TOP_LEFT_CHEST; // SCENE_MIDOS_HOUSE chest 0x00
constexpr RandomizerCheck kUnset = RC_LH_FREESTANDING_POH;    // SCENE_LAKE_HYLIA collectible 0x1E
constexpr uint32_t kChestBit = 1u << 0x00;
constexpr uint32_t kHeldBit = 1u << 0x00;
constexpr uint32_t kUnsetBit = 1u << 0x1E;
const RandomizerCheck kHosts[4] = { kChest, kRupee, kHeld, kUnset };
const char* const kHostNames[4] = { "C (chest)", "R (bridge rupee)", "H (held chest)", "U (flag unset)" };
const char* const kItemNames[4] = { "Lens of Truth", "Hookshot", "Odolwa's Remains", "Oath to Order" };
SharedItem sItems[4];

// File-static: SoH's runtime SaveContext is over 100 KB.
SaveContext sSavBase;
SaveContext sCommitted;

std::shared_ptr<Rando::Context> sWorld;
int sFailures = 0;

#define FHQ_CHECK(cond, msg)                                                                  \
    do {                                                                                      \
        if (!(cond)) {                                                                        \
            printf("[TEST] FAIL (foreign-hold-survives-quit-oot): %s  [%s]\n", (msg), #cond); \
            sFailures++;                                                                      \
        }                                                                                     \
    } while (0)

Rando::ItemLocation* Loc(RandomizerCheck rc) {
    return sWorld->GetItemLocation(rc);
}

bool AuthorWorld(void) {
    // SetCheckStatus dispatches a GameInteractor hook.
    if (GameInteractor::Instance == nullptr) {
        GameInteractor::Instance = new GameInteractor();
    }
    sWorld = Rando::Context::CreateInstance();
    if (sWorld == nullptr) {
        return false;
    }
    sWorld->SetSeed(0x1635);
    if (Rando::StaticData::GetLocation(kChest)->GetName().empty()) {
        Rando::StaticData::InitLocationTable();
    }
    if (Rando::StaticData::RetrieveItem(RG_BLUE_RUPEE).GetName().GetEnglish().empty()) {
        Rando::StaticData::InitItemTable();
    }
    for (RandomizerCheck rc : kHosts) {
        // A crossing host holds OoT's junk cover (ComboLogicEngineOoT.cpp).
        Loc(rc)->SetPlacedItem(RG_BLUE_RUPEE);
        Loc(rc)->SetCheckStatus(RCSHOW_UNCHECKED);
    }
    return true;
}

void ReleaseWorld(void) {
    if (sWorld != nullptr) {
        sWorld->GetLogic()->SetContext(nullptr);
    }
    sWorld.reset();
}

void BlankRandoHalf(SaveContext* s) {
    memset(s, 0, sizeof(SaveContext));
    s->ship.quest.id = QUEST_RANDOMIZER;
    s->fileNum = kSlot;
}

void SetRandoInf(SaveContext* s, RandomizerInf flag) {
    s->ship.randomizerInf[flag >> 4] |= (uint16_t)(1u << (flag & 0xF));
}

void Pair(void) {
    gComboCtx.sourceIsRando = true;
    gComboCtx.sharedRandoSeed = kMasterSeed;
    gComboCtx.sharedRandoSettingsHash = kSettingsHash;
}

// The world's crossings, rows 0..3 in kHosts order.
bool StoreCrossings(void) {
    ComboCrossing rows[4];
    for (int i = 0; i < 4; i++) {
        rows[i].hostCheck = (uint16_t)kHosts[i];
        rows[i].itemClass = 0;
        rows[i].item = sItems[i];
    }
    Combo_Crossings_Clear();
    return Combo_Crossings_Replace(rows, 4, nullptr, 0) == 4;
}

int RowsHolding(const SharedItem& item) {
    int n = 0;
    for (int i = 0; i < (int)RSBS_SHARED_ITEM_LOGICAL_CAP; i++) {
        const SharedItem* row = Combo_SharedItemSlot(i);
        if (row != nullptr && row->originGame == item.originGame && row->id == item.id) {
            n++;
        }
    }
    return n;
}

void NoAward(const SharedItem*, void*) {
}

// Two commits on disk, as the file select later finds them: the one the .sav's
// last save matches (OoT half `sSavBase`), then the quit snapshot, whose OoT half
// is the live gSaveContext and whose Tier-1 carries the live pending set. Returns
// the .sav's mirrored generation, or 0 on failure. Then the process ends: nothing
// cross-game survives in memory.
uint32_t CommitAndQuit(void) {
    rsbs::SaveManager& mgr = rsbs::SaveManager::Instance();
    mgr.SetSaveDirectory(kSaveDir());
    mgr.DeleteSave(kSlot);
    mgr.ResetSlotSessionState();
    mgr.ArmSlotOnCreate(kSlot);
    mgr.SetActiveSlot(kSlot);

    static std::vector<uint8_t> sMMHalf(MM_SAVE_CONTEXT_SIZE, 0);
    std::fill(sMMHalf.begin(), sMMHalf.end(), (uint8_t)0);
    if (MM_Rando_AuthorPairHalfForTest(sMMHalf.data(), sMMHalf.size(), kMasterSeed) != 1) {
        return 0;
    }
    Context_UpdateShadowCopy(GAME_MM, sMMHalf.data(), sMMHalf.size());

    memcpy(&sCommitted, &gSaveContext, sizeof(SaveContext));
    Context_UpdateShadowCopy(GAME_OOT, &sSavBase, sizeof(SaveContext));
    gComboCtx.sourceGame = GAME_OOT;
    if (RsbsSave_Save(kSlot) != 1) {
        return 0;
    }
    const uint32_t savGeneration = gComboCtx.commitGeneration;
    Context_UpdateShadowCopy(GAME_OOT, &sCommitted, sizeof(SaveContext));
    if (RsbsSave_Save(kSlot) != 1 || gComboCtx.commitGeneration != savGeneration + 1) {
        return 0;
    }
    ComboContext_Init();
    Combo_ClearSharedItemOutbox();
    Combo_Crossings_Clear();
    Context_ClearAllFrozenStates();
    (void)OoT_Rando_TestTakeQueuedChecks(nullptr, 0);
    mgr.ResetSlotSessionState();
    mgr.SetActiveSlot(-1);
    return savGeneration;
}

// LoadFile as OoT's file select runs it: the .sav's base section and tracker
// statuses in memory, then the OnLoadFile seam (which loads the .redsave).
void LoadAsFileSelect(uint32_t savGeneration, const RandomizerCheckStatus statuses[4]) {
    memcpy(&gSaveContext, &sSavBase, sizeof(SaveContext));
    for (int i = 0; i < 4; i++) {
        Loc(kHosts[i])->SetCheckStatus(statuses[i]);
    }
    OoT_Combo_OnLoadFileSeam(kSlot, savGeneration, nullptr);
}

int RunLegs(void) {
    for (int i = 0; i < 4; i++) {
        FHQ_CHECK(TestNamedItem((uint8_t)GAME_MM, kItemNames[i], &sItems[i]), kItemNames[i]);
    }
    Context_InitFrozenStates();
    Context_ClearAllFrozenStates();
    ComboContext_Init();
    Combo_ClearSharedItemOutbox();
    Pair();
    FHQ_CHECK(StoreCrossings(), "setup: the four crossings are stored");
    FHQ_CHECK(Combo_ForeignPairingActive(), "setup: the pairing is live");
    if (sFailures != 0) {
        return 1;
    }
    OoT_Rando_Foreign_ClearHolds();
    (void)OoT_Rando_TestTakeQueuedChecks(nullptr, 0);
    BlankRandoHalf(&sSavBase);
    BlankRandoHalf(&gSaveContext);

    // ======================================================================
    // Session 1: the player collects all four, then quits.
    // ======================================================================
    // H: every outbox row holds an item OoT has not redeemed. The player opens
    // Mido's chest (its flag reaches the save when he leaves the house) and the
    // drain holds it: nothing recorded, nothing given.
    int filled = 0;
    for (uint32_t i = 0; i < RSBS_SHARED_ITEM_LOGICAL_CAP; i++) {
        filled += Combo_RecordSharedItemCrossing(GAME_OOT, kFillId) >= 0 ? 1 : 0;
    }
    gSaveContext.sceneFlags[SCENE_MIDOS_HOUSE].chest |= kHeldBit;
    const int held = OoT_Rando_Foreign_HoldPickup((uint16_t)kHeld);
    const bool heldPending = Combo_CrossingPendingOoT_Test((uint16_t)kHeld);
    // A crossing to Termina frees a row; the held chest goes back to the drain's
    // queue, and the player quits before the drain records it.
    gComboCtx.sharedItemsTagged[0].flags |= RSBS_SHARED_ITEM_REDEEMED;
    uint16_t released[4] = {};
    const int releasedCount = OoT_Rando_Foreign_TakeReleasedHolds(released, 4);
    const bool releasedPending = Combo_CrossingPendingOoT_Test((uint16_t)kHeld);
    printf("[TEST] foreign-hold-survives-quit-oot: H outbox filled %d/%u; hold %d, pending %d; handed back %d "
           "(check %u), still pending %d\n",
           filled, (unsigned)RSBS_SHARED_ITEM_LOGICAL_CAP, held, (int)heldPending, releasedCount, (unsigned)released[0],
           (int)releasedPending);
    FHQ_CHECK(filled == (int)RSBS_SHARED_ITEM_LOGICAL_CAP, "setup: every outbox row is recorded");
    FHQ_CHECK(held == 1 && heldPending, "H: the drain holds Mido's chest and marks it pending");
    FHQ_CHECK(releasedCount == 1 && released[0] == (uint16_t)kHeld && releasedPending,
              "H: a freed row hands the held chest back to the drain, and it stays pending until its record");

    // The queue window: C, R and U are collected and queued (the real hooks),
    // and the drain never runs (Link carries a pot, then the player quits).
    gSaveContext.sceneFlags[SCENE_KOKIRI_FOREST].chest |= kChestBit;
    RandomizerOnSceneFlagSetHandler(SCENE_KOKIRI_FOREST, FLAG_SCENE_TREASURE, 0x00);
    SetRandoInf(&gSaveContext, RAND_INF_KF_BRIDGE_RUPEE);
    RandomizerOnFlagSetHandler(FLAG_RANDOMIZER_INF, RAND_INF_KF_BRIDGE_RUPEE);
    gSaveContext.sceneFlags[SCENE_LAKE_HYLIA].collect |= kUnsetBit;
    RandomizerOnSceneFlagSetHandler(SCENE_LAKE_HYLIA, FLAG_SCENE_COLLECTIBLE, 0x1E);
    const bool pendingC = Combo_CrossingPendingOoT_Test((uint16_t)kChest);
    const bool pendingR = Combo_CrossingPendingOoT_Test((uint16_t)kRupee);
    const bool pendingU = Combo_CrossingPendingOoT_Test((uint16_t)kUnset);
    printf("[TEST] foreign-hold-survives-quit-oot: queue window: pending C %d, R %d, U %d (set count %d)\n",
           (int)pendingC, (int)pendingR, (int)pendingU, Combo_CrossingPendingOoT_Count());
    FHQ_CHECK(pendingC && pendingR && pendingU,
              "queue window: both flag hooks mark a queued host of an MM item pending (scene flag and RandomizerInf)");
    // U's committed half went back: it no longer holds the heart piece's flag.
    gSaveContext.sceneFlags[SCENE_LAKE_HYLIA].collect &= ~kUnsetBit;
    // R's tracker status reads found where the reload's .sav section has it
    // (below): the newest save still says pending.

    uint32_t savGeneration = CommitAndQuit();
    FHQ_CHECK(savGeneration != 0, "setup: the two commits are written");
    if (savGeneration == 0) {
        return 1;
    }

    // ======================================================================
    // Session 2: the file select loads the file.
    // ======================================================================
    const RandomizerCheckStatus atLoad[4] = { RCSHOW_UNCHECKED, RCSHOW_SAVED, RCSHOW_UNCHECKED, RCSHOW_UNCHECKED };
    LoadAsFileSelect(savGeneration, atLoad);
    const bool chestOpen = (gSaveContext.sceneFlags[SCENE_KOKIRI_FOREST].chest & kChestBit) != 0;
    const int pendingLoaded = Combo_CrossingPendingOoT_Count();
    const bool chestFound = Loc(kChest)->HasObtained();
    const bool heldFound = Loc(kHeld)->HasObtained();
    printf("[TEST] foreign-hold-survives-quit-oot: after the seam: chest open %d, pending count %d, C found %d, H "
           "found %d, R found %d, pairing %d\n",
           (int)chestOpen, pendingLoaded, (int)chestFound, (int)heldFound, (int)Loc(kRupee)->HasObtained(),
           (int)Combo_ForeignPairingActive());
    FHQ_CHECK(chestOpen && Combo_ForeignPairingActive(), "setup: the newer commit's half and its pairing loaded");
    FHQ_CHECK(pendingLoaded == 4, "the commit carries the pending set: four hosts pending after the load");
    FHQ_CHECK(!chestFound && !heldFound,
              "leg 1: the reconcile must not promote a pending host to found (its item was never delivered)");

    // ---- leg 2: no live pairing ---------------------------------------------
    gComboCtx.sourceIsRando = false;
    const int unpairedQueued = OoT_Rando_TestResetQueuesForLoad();
    const int unpairedPending = Combo_CrossingPendingOoT_Count();
    const int unpairedTaken = OoT_Rando_TestTakeQueuedChecks(nullptr, 0);
    gComboCtx.sourceIsRando = true;
    printf("[TEST] foreign-hold-survives-quit-oot: leg 2 unpaired: restored %d, queued %d, pending %d\n",
           unpairedQueued, unpairedTaken, unpairedPending);
    FHQ_CHECK(unpairedQueued == 0 && unpairedTaken == 0, "leg 2: without a live pairing the restore queues nothing");
    FHQ_CHECK(unpairedPending == 4, "leg 2: without a live pairing the restore leaves every bit as the file has it");

    // ---- leg 3: the restore -------------------------------------------------
    const int restored = OoT_Rando_TestResetQueuesForLoad();
    uint16_t queued[8] = {};
    const int queuedCount = OoT_Rando_TestTakeQueuedChecks(queued, 8);
    const bool rupeeFound = Loc(kRupee)->HasObtained();
    const bool unsetPending = Combo_CrossingPendingOoT_Test((uint16_t)kUnset);
    printf("[TEST] foreign-hold-survives-quit-oot: leg 3: restored %d, queued %d: %u %u %u %u; R found %d; U pending "
           "%d\n",
           restored, queuedCount, (unsigned)queued[0], (unsigned)queued[1], (unsigned)queued[2], (unsigned)queued[3],
           (int)rupeeFound, (int)unsetPending);
    FHQ_CHECK(restored == 3 && queuedCount == 3, "leg 3: the restore queues exactly the three hosts whose flag is set");
    FHQ_CHECK(queued[0] == (uint16_t)kChest, "leg 3: the chest queued at the quit is queued again after the reload");
    FHQ_CHECK(queued[1] == (uint16_t)kRupee,
              "leg 3: the bridge rupee (a RandomizerInf host) is queued again, though its tracker status read found "
              "(pending wins)");
    FHQ_CHECK(queued[2] == (uint16_t)kHeld, "leg 3: the held chest is queued again after the reload");
    FHQ_CHECK(!rupeeFound, "leg 3: pending wins: the restore resets the found status of a pending host");
    FHQ_CHECK(!unsetPending, "leg 3: a pending host whose flag the loaded save lacks is no longer pending (and is not "
                             "queued: opening it again queues it)");

    // ---- leg 4: the drain records each once ----------------------------------
    // Termina redeems what OoT owed, so the outbox has room again.
    (void)Combo_RedeemSharedItemsForGame(GAME_OOT, NoAward, nullptr);
    int recorded = 0;
    for (int i = 0; i < queuedCount && i < 8; i++) {
        if (OoT_Rando_Foreign_RecordPickup(queued[i]) == 1) {
            recorded++;
            Loc((RandomizerCheck)queued[i])->SetCheckStatus(RCSHOW_COLLECTED); // as the drain marks it
        }
    }
    int rows[4];
    for (int i = 0; i < 4; i++) {
        rows[i] = RowsHolding(sItems[i]);
    }
    const int pendingAfterDrain = Combo_CrossingPendingOoT_Count();
    printf("[TEST] foreign-hold-survives-quit-oot: leg 4: recorded %d; rows C %d R %d H %d U %d; pending %d\n",
           recorded, rows[0], rows[1], rows[2], rows[3], pendingAfterDrain);
    FHQ_CHECK(recorded == 3 && rows[0] == 1 && rows[1] == 1 && rows[2] == 1 && rows[3] == 0,
              "leg 4: each restored host records its MM item once");
    FHQ_CHECK(pendingAfterDrain == 0, "leg 4: the record clears each host's pending bit");

    // ---- leg 5: save and load again ------------------------------------------
    BlankRandoHalf(&sSavBase); // the .sav's base is still the one before any of it
    savGeneration = CommitAndQuit();
    FHQ_CHECK(savGeneration != 0, "leg 5 setup: the two commits are written");
    if (savGeneration == 0) {
        return 1;
    }
    const RandomizerCheckStatus delivered[4] = { RCSHOW_SAVED, RCSHOW_SAVED, RCSHOW_SAVED, RCSHOW_UNCHECKED };
    LoadAsFileSelect(savGeneration, delivered);
    const int pendingSecond = Combo_CrossingPendingOoT_Count();
    const int restoredSecond = OoT_Rando_TestResetQueuesForLoad();
    const int queuedSecond = OoT_Rando_TestTakeQueuedChecks(nullptr, 0);
    for (int i = 0; i < 4; i++) {
        rows[i] = RowsHolding(sItems[i]);
    }
    printf("[TEST] foreign-hold-survives-quit-oot: leg 5: pending %d, restored %d, queued %d; rows C %d R %d H %d U "
           "%d\n",
           pendingSecond, restoredSecond, queuedSecond, rows[0], rows[1], rows[2], rows[3]);
    FHQ_CHECK(pendingSecond == 0 && restoredSecond == 0 && queuedSecond == 0,
              "leg 5: a delivered host is not pending in the next commit, so a second reload queues nothing (a host "
              "queued again would send its item a second time)");
    FHQ_CHECK(rows[0] == 1 && rows[1] == 1 && rows[2] == 1 && rows[3] == 0,
              "leg 5: the reloaded outbox holds exactly one row per delivered host");
    for (int i = 0; i < 4; i++) {
        printf("[TEST] foreign-hold-survives-quit-oot:   %s hosts %s\n", kHostNames[i], kItemNames[i]);
    }
    return 0;
}

} // namespace

extern "C" int OoT_ForeignHoldSurvivesQuit_RunHeadless(void) {
    printf("[TEST] foreign-hold-survives-quit-oot: an OoT host of an MM item held or queued at a quit is given after "
           "the reload, once (#1635)\n");
    sFailures = 0;
    if (!AuthorWorld()) {
        printf("[TEST] FAIL (foreign-hold-survives-quit-oot): could not create a Rando::Context\n");
        return 1;
    }
    const int setup = RunLegs();

    // Leave process state clean for whatever runs next, on pass or fail.
    (void)OoT_Rando_TestTakeQueuedChecks(nullptr, 0);
    OoT_Rando_Foreign_ClearHolds();
    rsbs::SaveManager& mgr = rsbs::SaveManager::Instance();
    mgr.DeleteSave(kSlot);
    mgr.ResetSlotSessionState();
    mgr.SetActiveSlot(-1);
    mgr.SetSaveDirectory("Save");
    ComboContext_Init();
    Combo_ClearSharedItemOutbox();
    Combo_Crossings_Clear();
    Context_ClearAllFrozenStates();
    memset(&gSaveContext, 0, sizeof(SaveContext));
    ReleaseWorld();

    if (setup != 0 || sFailures != 0) {
        printf("[TEST] FAIL (foreign-hold-survives-quit-oot): %d failure(s)\n", sFailures);
        return 1;
    }
    printf("[TEST] PASS: an OoT host held or queued at a quit is given after the reload, once\n");
    return 0;
}
