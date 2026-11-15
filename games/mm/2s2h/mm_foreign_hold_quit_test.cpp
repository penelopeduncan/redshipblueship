/**
 * @file mm_foreign_hold_quit_test.cpp
 * #1635, the Majora's Mask half: a held MM host of an OoT item survives a
 * commit, a load, a cycle reset and a moon crash.
 *
 *   foreign-hold-survives-quit-mm (redship tier, ROM-free). MM already keeps a
 *   held host: pending is RANDO_SAVE_CHECKS[rc].eligible and delivered is
 *   .obtained, both inside MM's half of every commit (CheckQueue.cpp keeps a held
 *   host eligible, #1222). This row locks that, beside its OoT twin
 *   (games/oot/soh/oot_foreign_hold_quit_test.cpp), so a change that drops either
 *   bit from any of the four paths goes red:
 *     (1) the commit (the owl save's funnel, MM_Combo_CaptureSaveToUnifiedSlot)
 *         carries a held host's `eligible` in MM's half;
 *     (2) a moon crash (the real Interface_StartMoonCrash and
 *         Sram_ResetSaveFromMoonCrash, which restore the last whole commit)
 *         brings it back with that commit's outbox: a give made after the commit
 *         is undone together with its row, so the host is owed again, once;
 *     (3) the Song of Time (the real Sram_SaveEndOfCycle, the rando's
 *         Before/AfterEndOfCycleSave armed through the production load path)
 *         keeps it: the cycle reset clears cycleObtained, never eligible;
 *     (4) a load of the file in a new session arms MM's half with it.
 */

#include "global.h"

#include <cstdio>
#include <cstring>
#include <vector>

#if !defined(RSBS_SINGLE_EXECUTABLE)
// The combo surfaces are single-exe only; test_runner.cpp declares this
// unconditionally, so a standalone 2ship build links.
extern "C" int MM_ForeignHoldSurvivesQuit_Run(void) {
    return 1;
}
#else

#include "2s2h/Rando/Rando.h"
#include "2s2h/Rando/MiscBehavior/MiscBehavior.h"
#include "2s2h/ShipInit.hpp"
#include "context.h"
#include "game.h"
#include "save.h"
#include "shared_items.h"
#include "test_scratch.h" // a scratch directory per process

extern "C" {
void MM_Creation_StampNewFileFields(const u8* playerName);
int MM_Rando_ArmCreatedHalf(int slot);
int Combo_ConsumeFrozenState(const char* gameId, void* saveContext, size_t size);
int MM_Combo_CaptureSaveToUnifiedSlot(void);
}

namespace {

const char* sRow = "foreign-hold-survives-quit-mm";

#define FHM_ASSERT(cond, msg)                                                                     \
    do {                                                                                          \
        if (!(cond)) {                                                                            \
            printf("[TEST] FAIL (%s): %s  [%s] (%s:%d)\n", sRow, msg, #cond, __FILE__, __LINE__); \
            return 1;                                                                             \
        }                                                                                         \
    } while (0)

inline const char* kSaveDir() {
    return rsbs::test::ScratchDirCStr("rsbs_test_saves_foreign_hold_quit_mm");
}

// A chest, held: the player opened it while every outbox row was full.
constexpr RandoCheckId kHost = RC_CLOCK_TOWN_EAST_UPPER_CHEST;
const u8 kName[8] = { 0x0A, 0x25, 0x01, 0x3E, 0x3E, 0x3E, 0x3E, 0x3E };

void SetHost(bool obtained, bool eligible) {
    RANDO_SAVE_CHECKS[kHost].obtained = obtained;
    RANDO_SAVE_CHECKS[kHost].cycleObtained = obtained;
    RANDO_SAVE_CHECKS[kHost].eligible = eligible;
}

void PrintHost(const char* when, const SaveContext& s) {
    const auto& c = s.save.shipSaveInfo.rando.randoSaveChecks[kHost];
    printf("[TEST] %s: %s: obtained %d cycleObtained %d eligible %d\n", sRow, when, (int)c.obtained,
           (int)c.cycleObtained, (int)c.eligible);
}

// The crash as production drives it (mm_creation_new_file_test.cpp's CrashTheMoon).
int CrashTheMoon() {
    static u8 sCrashSaveBuf[SAVE_BUFFER_SIZE];
    static PlayState sCrashPlayState;
    memset(&sCrashPlayState, 0, sizeof(sCrashPlayState));
    memset(sCrashSaveBuf, 0, sizeof(sCrashSaveBuf));
    sCrashPlayState.sramCtx.saveBuf = sCrashSaveBuf;
    PlayState* const savedPlayState = MM_gPlayState;
    MM_gPlayState = &sCrashPlayState;
    Interface_StartMoonCrash(&sCrashPlayState);
    Sram_ResetSaveFromMoonCrash(&sCrashPlayState.sramCtx);
    MM_gPlayState = savedPlayState;
    return 0;
}

// The Song of Time's reset, headless (mm_cycle_keep_items_test.cpp's PlaySongOfTime).
void PlaySongOfTime() {
    static PlayState sStandIn;
    memset(&sStandIn, 0, sizeof(sStandIn));
    PlayState* const previous = MM_gPlayState;
    MM_gPlayState = &sStandIn;
    Sram_SaveEndOfCycle(&sStandIn);
    MM_gPlayState = previous;
}

// What OnSaveLoadHandler arms on every MM load (Rando/Rando.cpp).
void DriveRandoLoad(bool rando) {
    gSaveContext.save.shipSaveInfo.saveType = rando ? SAVETYPE_RANDO : SAVETYPE_VANILLA;
    Rando::MiscBehavior::OnFileLoad();
    ShipInit::Init("IS_RANDO");
}

int RunLegs(rsbs::SaveManager& mgr) {
    // ---- the creation and its commit (as mm-moon-crash-pool-applied) --------
    memset(&gSaveContext, 0, sizeof(SaveContext));
    MM_Sram_InitNewSave();
    MM_Creation_StampNewFileFields(kName);
    gSaveContext.save.shipSaveInfo.saveType = SAVETYPE_RANDO;
    gSaveContext.save.shipSaveInfo.rando.finalSeed = 0x16350002u;
    FHM_ASSERT(MM_Rando_ArmCreatedHalf(0) == 0, "the authored half arms");
    Context_SetCurrentGame(GAME_OOT);
    {
        std::vector<uint8_t> oot((size_t)OOT_SAVE_CONTEXT_SIZE, 0);
        Context_UpdateShadowCopy(GAME_OOT, oot.data(), oot.size());
    }
    mgr.SetSaveDirectory(kSaveDir());
    mgr.DeleteSave(0);
    mgr.ResetSlotSessionState();
    RsbsSave_ArmSlotOnCreate(0);
    FHM_ASSERT(mgr.Save(0), "the creation commits the .redsave");
    RsbsSave_SetActiveSlot(0);

    // ---- the arrival; the held chest; the owl save -------------------------
    FHM_ASSERT(Combo_ConsumeFrozenState("mm", &gSaveContext, sizeof(gSaveContext)) == 1,
               "the arrival consumes the armed half");
    Context_SetCurrentGame(GAME_MM);
    gSaveContext.gameMode = GAMEMODE_NORMAL;
    SetHost(/*obtained=*/false, /*eligible=*/true); // CheckQueue held it (#1222)
    gSaveContext.save.isOwlSave = true;
    FHM_ASSERT(MM_Combo_CaptureSaveToUnifiedSlot() == 1, "(1) the owl save commits the whole file");
    gSaveContext.save.isOwlSave = false;
    {
        const SaveContext* shadow = reinterpret_cast<const SaveContext*>(Context_GetMMSaveContext());
        FHM_ASSERT(shadow != nullptr, "(1) MM's shadow exists after the commit");
        PrintHost("(1) the committed half", *shadow);
        FHM_ASSERT(shadow->save.shipSaveInfo.rando.randoSaveChecks[kHost].eligible &&
                       !shadow->save.shipSaveInfo.rando.randoSaveChecks[kHost].obtained,
                   "(1) the commit carries the held host as eligible and not obtained");
    }

    // ---- (2) a give after the commit, then the moon falls -------------------
    // A row frees and CheckQueue delivers the chest (obtained, no longer
    // eligible) with its outbox row; none of it is committed.
    SetHost(/*obtained=*/true, /*eligible=*/false);
    FHM_ASSERT(Combo_RecordSharedItemCrossing(GAME_OOT, 0x0042) >= 0, "(2) the delivery's row is recorded (premise)");
    if (CrashTheMoon() != 0) {
        return 1;
    }
    PrintHost("(2) after the moon crash", gSaveContext);
    int rowsAfterCrash = 0;
    for (int i = 0; i < (int)RSBS_SHARED_ITEM_LOGICAL_CAP; i++) {
        const SharedItem* row = Combo_SharedItemSlot(i);
        rowsAfterCrash += (row != nullptr && row->originGame == (uint8_t)GAME_OOT && row->id == 0x0042) ? 1 : 0;
    }
    FHM_ASSERT(RANDO_SAVE_CHECKS[kHost].eligible && !RANDO_SAVE_CHECKS[kHost].obtained,
               "(2) the moon crash restores the committed held host: eligible, not obtained");
    FHM_ASSERT(rowsAfterCrash == 0, "(2) the moon crash drops the uncommitted delivery's row with it: owed once");

    // ---- (3) the Song of Time ------------------------------------------------
    DriveRandoLoad(true);
    // A probe bit: Sram_SaveEndOfCycle never touches shipSaveInfo, so only the
    // rando's AfterEndOfCycleSave, visiting this host, clears it. Without this
    // the leg passes with that hook unregistered or undispatched (the COND_HOOK
    // vacuity trap), and an `eligible = false` beside its clear goes unseen.
    RANDO_SAVE_CHECKS[kHost].cycleObtained = true;
    PlaySongOfTime();
    PrintHost("(3) after the Song of Time", gSaveContext);
    FHM_ASSERT(!RANDO_SAVE_CHECKS[kHost].cycleObtained,
               "(3) the reset did not clear the host's cycleObtained: the rando's AfterEndOfCycleSave did not run "
               "(premise)");
    FHM_ASSERT(RANDO_SAVE_CHECKS[kHost].eligible && !RANDO_SAVE_CHECKS[kHost].obtained,
               "(3) the cycle reset keeps a held host eligible (it clears cycleObtained only)");
    gSaveContext.save.isOwlSave = true;
    FHM_ASSERT(MM_Combo_CaptureSaveToUnifiedSlot() == 1, "(3) the new cycle commits");
    gSaveContext.save.isOwlSave = false;
    const uint32_t generation = gComboCtx.commitGeneration;
    DriveRandoLoad(false);

    // ---- (4) a new session loads the file -----------------------------------
    Context_ClearAllFrozenStates();
    ComboContext_Init();
    memset(&gSaveContext, 0, sizeof(SaveContext));
    Context_SetCurrentGame(GAME_OOT);
    mgr.ResetSlotSessionState();
    FHM_ASSERT(mgr.LoadSlot(0, generation) == RSBS_LOAD_OK, "(4) the file loads in a new session");
    {
        const SaveContext* shadow = reinterpret_cast<const SaveContext*>(Context_GetMMSaveContext());
        FHM_ASSERT(shadow != nullptr, "(4) the load arms MM's half");
        PrintHost("(4) the loaded half", *shadow);
        FHM_ASSERT(shadow->save.shipSaveInfo.rando.randoSaveChecks[kHost].eligible &&
                       !shadow->save.shipSaveInfo.rando.randoSaveChecks[kHost].obtained,
                   "(4) the loaded half still holds the host as eligible and not obtained");
    }
    return 0;
}

} // namespace

extern "C" int MM_ForeignHoldSurvivesQuit_Run(void) {
    printf("[TEST] %s: a held MM host of an OoT item stays eligible through a commit, a moon crash, a cycle reset and "
           "a load (#1635)\n",
           sRow);
    const GameId prevGame = Context_GetCurrentGame();
    Context_InitFrozenStates();
    ComboContext_Init();
    Context_ClearAllFrozenStates();
    Combo_ClearSharedItemOutbox();
    rsbs::SaveManager& mgr = rsbs::SaveManager::Instance();
    const int rc = RunLegs(mgr);

    // Disarm whatever the legs armed, on pass and on fail (as mm-cycle-razor-sword).
    DriveRandoLoad(false);
    mgr.DeleteSave(0);
    mgr.ResetSlotSessionState();
    mgr.SetSaveDirectory("Save");
    RsbsSave_SetActiveSlot(-1);
    Context_ClearAllFrozenStates();
    Combo_ClearSharedItemOutbox();
    ComboContext_Init();
    memset(&gSaveContext, 0, sizeof(SaveContext));
    Context_SetCurrentGame(prevGame);
    if (rc != 0) {
        printf("[TEST] FAIL: %s\n", sRow);
        return 1;
    }
    printf("[TEST] PASS: %s\n", sRow);
    return 0;
}

#endif // RSBS_SINGLE_EXECUTABLE
