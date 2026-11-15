/**
 * @file oot_int_foreign_hold_quit_drive.cpp
 * @brief The integration row IntForeignHoldQuit (int-foreign-hold-quit, #1635
 *        step 7): an Ocarina of Time host of a Majora's Mask item that is owed
 *        when the player quits is given once after the reload, through the real
 *        quit and the real file select, in one running process.
 *
 * WHAT IT PROVES. The ROM-free rows ForeignHoldSurvivesQuitOoT / ...MM call the
 * product's functions by hand. This row proves the WIRING in the running game:
 *   - a real quit to the title (the console's `reset`, which the Menu's Reset
 *     button and Ctrl+R dispatch) runs OnExitGame's exit snapshot
 *     (OoT_Combo_StageExitSnapshot, SaveManager.cpp: the current scene's flags,
 *     the pool, the shadow) and its commit;
 *   - a real load from the file select (A on file 3, A on Yes: FileChoose_LoadGame,
 *     Sram_OpenSave, the OnLoadFile seam, OnLoadGame) runs the randomizer's queue
 *     reset and with it OoT_Rando_Foreign_RestoreHolds;
 *   - the real RC-queue drain then records the host's outbox row exactly once and
 *     clears its pending bit (gComboCtx.crossingPendingOoT);
 *   - a second quit and reload does not hand it back again, and a chest recorded
 *     in the scene the player quits from comes back open (the step-0 flush);
 *   - after the paired crossing, Majora's Mask has redeemed each row exactly once.
 *
 * WHAT IT SHORTCUTS, AND WHY.
 *   - The hosts are PINNED through the debug-seed seam (Combo_Logic_TestSetForced
 *     Placements, as int-song-host-oot pins its song): MM's Lens of Truth on
 *     Mido's top-left chest (H1) and MM's Pictograph Box on Mido's top-right chest
 *     (H2), because the paired world's own placements are not predictable. Both
 *     are MM items that cross through the outbox, never a shared capacity tier
 *     credited to the pool at pickup (#1120); F0 checks it with the pool predicate
 *     the product calls (Combo_SharedPickupCreditOf).
 *   - No chest is animated. H1 is opened while Link stands in the Market: the row
 *     writes the left scene's saved chest word (what Play_SaveSceneFlags writes
 *     when the player leaves Mido's House) and fires the real scene-flag
 *     dispatcher (GameInteractor_ExecuteOnSceneFlagSet), which reaches
 *     RandomizerOnSceneFlagSetHandler, as the ROM-free row does. H2 is opened in
 *     Mido's House through the chest's own setter (OoT_Flags_SetTreasure), which
 *     sets the live flag and fires the same hook.
 *   - "Held" is met by the QUEUE window, not the full-outbox hold: H1 is queued and
 *     the quit lands in the same frame, after the drain ran. The durable state at
 *     the quit is the same pending bit either way (NotePending at the flag hook);
 *     the hold's own Set (a full 2048-row outbox) stays locked by the ROM-free row.
 *   - The title is skipped with the product's own Boot Sequence: File Select
 *     function (OnZTitleUpdateSkipToFileSelect, CustomLogoTitle.cpp), no CVar set.
 *
 * THE STAGES (each prints one [FHQ] line; every wait has a frame budget that fails
 * the row naming its stage):
 *   F0  pin both hosts before the generation; after the creation check them.
 *   F1  first leg's frames done and quiet: queue H1, quit 1 (same frame).
 *   F2  title reached, the session retired; skip to the file select.
 *   F3  file select: A on file 3, A on Yes.
 *   F4  reload 1's scene init: the restore handed H1 back, still pending.
 *   F5  the real drain delivers H1 once and clears its bit.
 *   F6  warp into Mido's House.
 *   F7  open H2 there; the drain delivers it (its bit lives only in the live word).
 *   F8  quit 2 in the same scene, without saving.
 *   F9  title 2, F10 file select 2.
 *   F11 reload 2's scene init: nothing pending, nothing restored.
 *   F12 H2 came back open, nothing recorded again; back to the Market.
 *   F13 the stock paired crossing and return, then the verdict.
 *
 * MUTATIONS THAT MUST TURN IT RED (the assertion that catches each):
 *   OoT_Rando_Foreign_RestoreHolds not called on load   F4 restore line / pending
 *   OnExitGame's exit snapshot not staged               F4 (H1's flag is not in the
 *                                                       committed half: dropped)
 *   OnExitGame's commit removed                         F4 generation, restore line
 *   the pending set cleared on load                     F4 pending
 *   the reconcile promotes a pending host to found      F4 reset count
 *   RandomizerResetQueuesForLoad's push loop removed    F5 budget (never delivered)
 *   the record's pending clear removed                  F5 pending after the record
 *   the exit snapshot's scene-flag flush removed        F12 (H2 back closed, then a
 *                                                       second record)
 *   any second delivery of a host                       every frame (outbox rows)
 *   MM receives a host twice or never                   F13
 *
 * games/oot/soh/GameExports_SingleExe.cpp calls these entry points from the
 * paired first crossing's boot, its hooks and its frame driver.
 */

#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>

#include <libultraship/libultraship.h>
#include <ship/Context.h>

#include "OTRGlobals.h"
#include "z64.h"
#include "soh/SaveManager.h"
#include "soh/cvar_prefixes.h"
#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/Enhancements/game-interactor/GameInteractor_Hooks.h"
#include "soh/Enhancements/randomizer/randomizerTypes.h"
#include "soh/Enhancements/randomizer/static_data.h"

#include "combo_logic.h"            // src/common: the debug-seed pin
#include "context.h"                // src/common: gComboCtx
#include "crossing_store.h"         // src/common: the pinned rows as stored
#include "foreign_items.h"          // src/common: the pending set, the item names
#include "game.h"                   // src/common: GameId
#include "integration_test_hooks.h" // src/common: the phase machine, the capture
#include "pairing_refusal_toast.h"  // src/common: RSBS_REFUSAL_TOAST_PREFIX
#include "save.h"                   // src/common: the slot's refusal words
#include "shared_items.h"           // src/common: the outbox rows
#include "shared_resources.h"       // src/common: the pool predicate

// Last: through global.h it pulls in functions.h, whose C++ half redefines `this`.
#include "src/overlays/gamestates/ovl_file_choose/file_choose.h"

extern "C" {
extern PlayState* OoT_gPlayState; // z_play.c
void Save_DeleteFile(int fileNum);
SaveFileMetaInfo* Save_GetSaveMetaInfo(int fileNum);
int OoT_Rando_Foreign_HostCollected(uint16_t rc);           // ForeignItemsSingleExe.cpp
int OoT_Rando_Foreign_TestQueuedCheck(void);                // hook_handlers.cpp: the give slot
void OoT_Fhq_FireDoor(uint16_t entrance, const char* what); // GameExports_SingleExe.cpp
}
// CustomLogoTitle.cpp (C++ linkage): what "Boot Sequence: File Select" runs on the title.
void OnZTitleUpdateSkipToFileSelect(void* gameState);

namespace {

enum class FhqStage {
    Off,          // not armed
    Pinned,       // F0 armed; the first leg's own frames
    Queue1,       // F1
    Title,        // F2 / F9
    Select,       // F3 / F10
    Load,         // F4 / F11 (awaiting the reloaded scene)
    Drain1,       // F5
    WarpIn,       // F6
    RecordH2,     // F7
    Quit2,        // F8
    OnceOnly,     // F12
    MarketReturn, // F12: the door to the Market fired
    MarketSettle, // F12: in the Market, before the stock crossing
    Done,         // F13: the stock crossing and return
    Finished,     // passed or failed: nothing more is driven
};

constexpr int kQuietFrames = 30;     // no record, textbox, give or cutscene: no threaded save in flight
constexpr int kQueueBudget = 600;    // F1 / F8 preconditions
constexpr int kTitleBudget = 600;    // F2 / F9, frames from the quit
constexpr int kSelectBudget = 900;   // F3 / F10
constexpr int kLoadBudget = 600;     // F4 / F11, frames from the load
constexpr int kDrainBudget = 600;    // F5 / F7
constexpr int kWarpBudget = 600;     // F6
constexpr int kOnceOnlyFrames = 300; // F12's watch
constexpr int kMarketSettle = 60;    // F12: frames in the Market before the stock door
constexpr int kRecordH2AtFrame = 20; // F7: the chest is opened on this arrival frame
constexpr int kCloseTextAfter = 60;  // a textbox nobody presses past is closed
constexpr int kPressAAfter = 120;    // and pressed past with A if it stays open
constexpr const char* kRestoreNeedle = "foreign pending: the loaded file had";
constexpr const char* kTitleNeedle = "Return to title: retiring";
constexpr const char* kHeldNeedle = "foreign pickup HELD";

struct FhqHost {
    uint16_t rc = 0;
    const char* itemName = "";
    SharedItem item = {};
    int scene = -1;
    int flag = -1;
    int rec0 = 0; // "recorded crossing copy" lines for this item before F1
    int out0 = 0; // outbox rows (origin MM, this id) before F1
    int red0 = 0; // of them redeemed
};

struct FhqState {
    FhqStage stage = FhqStage::Off;
    int round = 0; // which quit / reload (1 or 2)
    int slot = -1;
    uint8_t name[8] = {};
    bool pinned = false;
    bool baselined = false;
    FhqHost h[2];
    int stageFrames = 0; // player frames in a play stage, OoT frames in a title / file-select / load stage
    int quiet = 0;
    int textFrames = 0;
    int lastMmRows = -1;
    bool pressPlayA = false;
    bool quitRequested = false;
    bool doorFired = false;
    int recordedAt = -1; // F5 / F7: the stage frame the host's row appeared
    int restore0 = 0;
    int restoreAtLoad1 = 0;
    int titleAtQuit = 0;
    int held0 = 0;
    int refused0 = 0;
    int toast0 = 0;
    uint32_t gen0 = 0;
    uint32_t sav0 = 0;
    uint32_t gen1 = 0;
    uint32_t sav1 = 0;
    // The file select (F3 / F10): cached by its own main, used the next frame only.
    void* fileChoose = nullptr;
    uint64_t frame = 0;
    uint64_t fileChooseFrame = 0;
    int lastMenu = -1;
    int lastConfig = -1;
    int lastSelect = -1;
    int pressFrames = 0;
    bool metaChecked = false;
    int h2BitAfterReload2 = -1;
    bool reopened = false;
    std::string fails; // the failed checks of the stage being measured
};

FhqState sFhq;

const char* StageLabel(void) {
    switch (sFhq.stage) {
        case FhqStage::Off:
            return "off";
        case FhqStage::Pinned:
            return "F0 pinned";
        case FhqStage::Queue1:
            return "F1 queue H1 and quit";
        case FhqStage::Title:
            return sFhq.round == 1 ? "F2 await title" : "F9 await title";
        case FhqStage::Select:
            return sFhq.round == 1 ? "F3 file select" : "F10 file select";
        case FhqStage::Load:
            return sFhq.round == 1 ? "F4 await reload" : "F11 await reload";
        case FhqStage::Drain1:
            return "F5 reload drain";
        case FhqStage::WarpIn:
            return "F6 warp into Mido's House";
        case FhqStage::RecordH2:
            return "F7 record H2";
        case FhqStage::Quit2:
            return "F8 quit 2";
        case FhqStage::OnceOnly:
            return "F12 once only";
        case FhqStage::MarketReturn:
            return "F12 back to the Market";
        case FhqStage::MarketSettle:
            return "F12 Market settle";
        case FhqStage::Done:
            return "F13 stock crossing";
        case FhqStage::Finished:
            return "finished";
    }
    return "?";
}

const char* Label(const char* first, const char* second) {
    return sFhq.round == 1 ? first : second;
}

void Fail(const char* stage, const char* fmt, ...) {
    char detail[768];
    va_list args;
    va_start(args, fmt);
    vsnprintf(detail, sizeof(detail), fmt, args);
    va_end(args);
    char msg[1024];
    snprintf(msg, sizeof(msg), "int-foreign-hold-quit %s: %s", stage, detail);
    sFhq.stage = FhqStage::Finished;
    if (sFhq.pinned) {
        (void)Combo_Logic_TestSetForcedPlacements(NULL, 0);
        sFhq.pinned = false;
    }
    IntegrationTest_GameplayFail(msg);
}

// A stage's checks are all measured and printed before its verdict, so a red run
// shows every symptom of the stage.
void Check(const char* stage, bool ok, const char* fmt, ...) {
    if (ok) {
        return;
    }
    char detail[512];
    va_list args;
    va_start(args, fmt);
    vsnprintf(detail, sizeof(detail), fmt, args);
    va_end(args);
    fprintf(stderr, "[FHQ] CHECK FAILED %s: %s\n", stage, detail);
    fflush(stderr);
    if (!sFhq.fails.empty()) {
        sFhq.fails += "; ";
    }
    sFhq.fails += detail;
}

// True (and the run failed) when a check of the stage failed.
bool Verdict(const char* stage) {
    if (sFhq.fails.empty()) {
        return false;
    }
    const std::string fails = sFhq.fails;
    sFhq.fails.clear();
    Fail(stage, "%s", fails.c_str());
    return true;
}

void Enter(FhqStage stage) {
    sFhq.stage = stage;
    sFhq.stageFrames = 0;
    sFhq.quiet = 0;
    sFhq.textFrames = 0;
    sFhq.quitRequested = false;
    sFhq.doorFired = false;
    sFhq.recordedAt = -1;
    sFhq.pressFrames = 0;
    sFhq.lastMenu = sFhq.lastConfig = sFhq.lastSelect = -1;
    sFhq.metaChecked = false;
}

// ---- measurements -----------------------------------------------------------

// Outbox rows holding this host's item (origin MM, its id); `redeemed` counts
// only the rows MM has redeemed.
int Rows(const FhqHost& host, bool redeemedOnly) {
    int n = 0;
    for (int i = 0; i < (int)RSBS_SHARED_ITEM_LOGICAL_CAP; i++) {
        const SharedItem* row = Combo_SharedItemSlot(i);
        if (row == nullptr || row->originGame != host.item.originGame || row->id != host.item.id) {
            continue;
        }
        if (redeemedOnly && (row->flags & RSBS_SHARED_ITEM_REDEEMED) == 0) {
            continue;
        }
        n++;
    }
    return n;
}

// The drain's record lines for this host's item ("[SharedItem] recorded crossing
// copy origin=mm id=N in slot S"; the prefix and the trailing " in slot" keep one
// id from matching another).
int Records(const FhqHost& host) {
    char needle[96];
    snprintf(needle, sizeof(needle), "recorded crossing copy origin=mm id=%u in slot", (unsigned)host.item.id);
    return IntegrationTest_StderrCaptureCount(needle);
}

int RecordDelta(int i) {
    return Records(sFhq.h[i]) - sFhq.h[i].rec0;
}

int RowDelta(int i) {
    return Rows(sFhq.h[i], false) - sFhq.h[i].out0;
}

int Pending(int i) {
    return Combo_CrossingPendingOoT_Test(sFhq.h[i].rc) ? 1 : 0;
}

uint32_t SavedChestWord(int scene) {
    return (scene >= 0 && scene < SCENE_ID_MAX) ? gSaveContext.sceneFlags[scene].chest : 0u;
}

int SavedChestBit(int i) {
    return (SavedChestWord(sFhq.h[i].scene) & (1u << sFhq.h[i].flag)) != 0 ? 1 : 0;
}

uint32_t SavGeneration(void) {
    return SaveManager::Instance != nullptr ? SaveManager::Instance->ReadSavCommitGeneration(sFhq.slot) : 0u;
}

bool InItemCutscene(PlayState* play) {
    Player* player = GET_PLAYER(play);
    return player != nullptr && (player->stateFlags1 & (PLAYER_STATE1_IN_ITEM_CS | PLAYER_STATE1_GETTING_ITEM)) != 0;
}

// No record, textbox, give, item cutscene, pending host or transition: whatever
// the last record started (its threaded tracker save) has had time to finish.
bool QuietFrame(PlayState* play) {
    const int mmRows = Combo_CountSharedItems(GAME_MM, true);
    const bool rowsStable = mmRows == sFhq.lastMmRows;
    sFhq.lastMmRows = mmRows;
    return rowsStable && play->msgCtx.msgMode == MSGMODE_NONE && !InItemCutscene(play) &&
           OoT_Rando_Foreign_TestQueuedCheck() == (int)RC_UNKNOWN_CHECK && Combo_CrossingPendingOoT_Count() == 0 &&
           play->transitionTrigger == TRANS_TRIGGER_OFF && play->transitionMode == TRANS_MODE_OFF;
}

void TickQuiet(PlayState* play) {
    sFhq.quiet = QuietFrame(play) ? sFhq.quiet + 1 : 0;
}

// The get-item textbox of a delivery is closed as GpSongHostOoTFrame does; one
// that stays open is pressed past with A (applied at the next frame start).
void TickTextbox(PlayState* play) {
    if (play->msgCtx.msgMode == MSGMODE_NONE) {
        sFhq.textFrames = 0;
        return;
    }
    sFhq.textFrames++;
    if (sFhq.textFrames % kCloseTextAfter == 0) {
        OoT_Message_CloseTextbox(play);
    }
    if (sFhq.textFrames >= kPressAAfter && sFhq.textFrames % 8 == 0) {
        sFhq.pressPlayA = true;
    }
}

// Every frame once the baselines exist: a host delivered twice is red at once.
bool GuardTwice(const char* stage) {
    if (!sFhq.baselined) {
        return true;
    }
    for (int i = 0; i < 2; i++) {
        const int rows = RowDelta(i);
        if (rows > 1) {
            Fail(stage,
                 "H%d (MM %s from OoT check %u) was delivered twice: %d new outbox rows (a host handed back after "
                 "its record, or recorded again)",
                 i + 1, sFhq.h[i].itemName, (unsigned)sFhq.h[i].rc, rows);
            return false;
        }
    }
    return true;
}

void TakeBaselines(void) {
    for (int i = 0; i < 2; i++) {
        sFhq.h[i].rec0 = Records(sFhq.h[i]);
        sFhq.h[i].out0 = Rows(sFhq.h[i], false);
        sFhq.h[i].red0 = Rows(sFhq.h[i], true);
    }
    sFhq.restore0 = IntegrationTest_StderrCaptureCount(kRestoreNeedle);
    sFhq.held0 = IntegrationTest_StderrCaptureCount(kHeldNeedle);
    sFhq.refused0 = IntegrationTest_StderrCaptureCount("REFUSED");
    sFhq.toast0 = IntegrationTest_StderrCaptureCount(RSBS_REFUSAL_TOAST_PREFIX);
    sFhq.gen0 = gComboCtx.commitGeneration;
    sFhq.sav0 = SavGeneration();
    sFhq.baselined = true;
    fprintf(stderr,
            "[FHQ] F1 baselines: H1 records %d rows %d (redeemed %d), H2 records %d rows %d (redeemed %d); restore "
            "lines %d, HELD lines %d, refusal lines %d; commit generation %u, .sav generation %u\n",
            sFhq.h[0].rec0, sFhq.h[0].out0, sFhq.h[0].red0, sFhq.h[1].rec0, sFhq.h[1].out0, sFhq.h[1].red0,
            sFhq.restore0, sFhq.held0, sFhq.refused0, (unsigned)sFhq.gen0, (unsigned)sFhq.sav0);
    fflush(stderr);
}

// ---- the quit -----------------------------------------------------------------

// The player's quit: the Console window's Dispatch("reset"), the exact call the
// Menu's Reset button and Ctrl+R make (SohGui/Menu.cpp). The console's own Run is
// the fallback when no Console window exists (oot_windows_gate_test.cpp's path).
const char* DispatchReset(void) {
    std::shared_ptr<Ship::Context> ctx = Ship::Context::GetInstance();
    std::shared_ptr<Ship::GuiWindow> consoleWindow = nullptr;
    if (ctx != nullptr && ctx->GetWindow() != nullptr && ctx->GetWindow()->GetGui() != nullptr) {
        consoleWindow = ctx->GetWindow()->GetGui()->GetGuiWindow("Console");
    }
    if (consoleWindow != nullptr) {
        std::reinterpret_pointer_cast<Ship::ConsoleWindow>(consoleWindow)->Dispatch("reset");
        return "the Console window's Dispatch(\"reset\") (Menu Reset / Ctrl+R)";
    }
    std::string output;
    const int32_t rc = ctx != nullptr && ctx->GetConsole() != nullptr ? ctx->GetConsole()->Run("reset", &output) : -1;
    if (rc != 0) {
        fprintf(stderr, "[FHQ] the console's reset returned %d: \"%s\"\n", (int)rc, output.c_str());
        fflush(stderr);
        return nullptr;
    }
    return "the console's Run(\"reset\") (no Console window)";
}

// F1 / F8, at the end of a play frame (OnGameFrameUpdate, after the play update
// and so after this frame's drain).
void QuitNow(void) {
    PlayState* play = OoT_gPlayState;
    const bool first = sFhq.stage == FhqStage::Queue1;
    const char* stage = first ? "F1" : "F8";
    int pendingH1 = 0;
    int count = 0;
    int rowDelta = 0;
    int recDelta = 0;
    int heldDelta = 0;
    if (first) {
        TakeBaselines();
        FhqHost& h1 = sFhq.h[0];
        // The word Play_SaveSceneFlags writes when the player leaves Mido's House;
        // Link is in another scene, so it is the only copy of the flag.
        gSaveContext.sceneFlags[h1.scene].chest |= 1u << h1.flag;
        GameInteractor_ExecuteOnSceneFlagSet((int16_t)h1.scene, FLAG_SCENE_TREASURE, (int16_t)h1.flag);
        pendingH1 = Pending(0);
        count = Combo_CrossingPendingOoT_Count();
        rowDelta = RowDelta(0);
        recDelta = RecordDelta(0);
        heldDelta = IntegrationTest_StderrCaptureCount(kHeldNeedle) - sFhq.held0;
        Check(stage, pendingH1 == 1 && count == 1,
              "the scene-flag hook did not mark H1 pending (pending %d, count %d): NotePending at the hook "
              "(hook_handlers.cpp) regressed",
              pendingH1, count);
        Check(stage, rowDelta == 0 && recDelta == 0, "H1 was delivered before the quit (rows +%d, records +%d)",
              rowDelta, recDelta);
        Check(stage, heldDelta == 0, "H1 was HELD (+%d HELD lines): the outbox is full", heldDelta);
    } else {
        sFhq.gen1 = gComboCtx.commitGeneration;
        sFhq.sav1 = SavGeneration();
    }
    const uint32_t genBefore = gComboCtx.commitGeneration;
    sFhq.titleAtQuit = IntegrationTest_StderrCaptureCount(kTitleNeedle);
    const char* how = DispatchReset();
    const uint32_t genAfter = gComboCtx.commitGeneration;
    const int running = OoT_gGameState != nullptr ? (int)OoT_gGameState->running : -1;
    if (first) {
        fprintf(stderr,
                "[FHQ] F1 H1 queued at frame end: pending %d (count %d), outbox delta %d, record delta %d; quit 1 via "
                "%s: gamestate running %d, commit generation %u -> %u, .sav generation %u\n",
                pendingH1, count, rowDelta, recDelta, how != nullptr ? how : "(refused)", running, (unsigned)genBefore,
                (unsigned)genAfter, (unsigned)sFhq.sav0);
    } else {
        fprintf(stderr,
                "[FHQ] F8 quit 2 in scene %d without saving via %s: gamestate running %d, commit generation %u -> "
                "%u, .sav generation %u\n",
                play != nullptr ? (int)play->sceneNum : -1, how != nullptr ? how : "(refused)", running,
                (unsigned)genBefore, (unsigned)genAfter, (unsigned)sFhq.sav1);
    }
    fflush(stderr);
    Check(stage, how != nullptr, "the console refused `reset`");
    // ResetHandler (debugconsole.cpp) clears `running` in the same call; a
    // ConsoleWindow::Dispatch whose command was refused (the OoT gate) returns
    // void and leaves it set, so this is the quit's own proof that it ran.
    Check(stage, running == 0,
          "the quit did not stop the play gamestate (running %d): the console's `reset` was refused or did not run",
          running);
    // Dispatch runs the command inline (ConsoleWindow::Dispatch), so OnExitGame's
    // commit has run here; a deferred dispatch would leave it unchanged, and the
    // reload checks the generation it loads either way.
    Check(stage, genAfter == genBefore || genAfter == genBefore + 1,
          "the quit moved the commit generation by more than one commit (%u -> %u)", (unsigned)genBefore,
          (unsigned)genAfter);
    if (Verdict(stage)) {
        return;
    }
    sFhq.round = first ? 1 : 2;
    Enter(FhqStage::Title);
}

// ---- the reloads ------------------------------------------------------------------

bool ParseRestore(int* pending, int* handed, int* reset, int* dropped) {
    char line[512];
    if (!IntegrationTest_StderrCaptureLast(kRestoreNeedle, line, sizeof(line))) {
        return false;
    }
    const char* at = strstr(line, kRestoreNeedle);
    return at != nullptr &&
           sscanf(at + strlen(kRestoreNeedle),
                  " %d OoT host(s) pending; %d handed back to the drain (%d of them read found and were reset), %d no "
                  "longer pending",
                  pending, handed, reset, dropped) == 4;
}

void CheckLoadCommon(const char* stage, uint32_t expectGen, uint32_t expectSav) {
    char diff[512];
    const bool identity = IntegrationTest_PairedIdentityMatches(diff, sizeof(diff));
    Check(stage, identity, "the reloaded file is not the created world: %s", diff);
    Check(stage, Combo_ForeignPairingActive(), "the reloaded file has no live cross-game pairing");
    Check(stage, gComboCtx.commitGeneration == expectGen,
          "the reloaded .redsave generation is %u, expected %u (the quit's commit: OnExitGame's RsbsSave_Save)",
          (unsigned)gComboCtx.commitGeneration, (unsigned)expectGen);
    Check(stage, SavGeneration() == expectSav, "the .sav generation moved (%u, was %u): a quit writes no .sav base",
          (unsigned)SavGeneration(), (unsigned)expectSav);
    const int refused = IntegrationTest_StderrCaptureCount("REFUSED") - sFhq.refused0;
    const int toasts = IntegrationTest_StderrCaptureCount(RSBS_REFUSAL_TOAST_PREFIX) - sFhq.toast0;
    Check(stage, refused == 0 && toasts == 0, "the load logged %d refusal line(s) and %d refusal toast(s)", refused,
          toasts);
}

// F4: reload 1's scene init, before any player update (so before the drain).
void MeasureReload1(int16_t sceneNum, uint16_t entrance) {
    const char* stage = "F4";
    int p = -1;
    int h = -1;
    int r = -1;
    int d = -1;
    const int restoreLines = IntegrationTest_StderrCaptureCount(kRestoreNeedle) - sFhq.restore0;
    const bool parsed = restoreLines > 0 && ParseRestore(&p, &h, &r, &d);
    sFhq.restoreAtLoad1 = sFhq.restore0 + restoreLines;
    const int pendingH1 = Pending(0);
    const int count = Combo_CrossingPendingOoT_Count();
    const uint32_t word = SavedChestWord(sFhq.h[0].scene);
    const int recDelta = RecordDelta(0);
    fprintf(stderr,
            "[FHQ] F4 reload 1 at scene %d entrance 0x%04X: restore line had %d pending, %d handed back, %d reset, %d "
            "dropped (%d new restore line(s)); H1 pending %d (count %d); saved chest word 0x%08X; .redsave generation "
            "%u (quit %u), .sav generation %u (was %u); record delta %d\n",
            (int)sceneNum, (unsigned)entrance, p, h, r, d, restoreLines, pendingH1, count, (unsigned)word,
            (unsigned)gComboCtx.commitGeneration, (unsigned)(sFhq.gen0 + 1), (unsigned)SavGeneration(),
            (unsigned)sFhq.sav0, recDelta);
    fflush(stderr);
    Check(stage, restoreLines == 1 && parsed,
          "the load printed %d restore line(s), expected 1: OoT_Rando_Foreign_RestoreHolds did not run on the load "
          "with a pending host (OnLoadGame's RandomizerResetQueuesForLoad), or the commit carried no pending bit",
          restoreLines);
    Check(stage, !parsed || (p == 1 && h == 1 && d == 0),
          "the restore had %d pending, handed %d back, dropped %d; expected 1, 1, 0 (a drop means the committed OoT "
          "half lacks H1's flag: the exit snapshot was not staged)",
          p, h, d);
    Check(stage, !parsed || r == 0,
          "the restore reset %d found status(es): the load reconcile promoted the pending host to found", r);
    Check(stage, pendingH1 == 1 && count == 1,
          "H1 is not pending after the reload (pending %d, count %d): the pending set was cleared on load or not "
          "carried",
          pendingH1, count);
    Check(stage, (word & (1u << sFhq.h[0].flag)) != 0, "H1's chest flag is not in the reloaded half (word 0x%08X)",
          (unsigned)word);
    Check(stage, recDelta == 0, "H1 was recorded before the reload's drain could run (records +%d)", recDelta);
    CheckLoadCommon(stage, sFhq.gen0 + 1, sFhq.sav0);
    if (Verdict(stage)) {
        return;
    }
    Enter(FhqStage::Drain1);
}

// F11: reload 2's scene init.
void MeasureReload2(int16_t sceneNum, uint16_t entrance) {
    const char* stage = "F11";
    const int count = Combo_CrossingPendingOoT_Count();
    const int restoreLines = IntegrationTest_StderrCaptureCount(kRestoreNeedle) - sFhq.restoreAtLoad1;
    sFhq.h2BitAfterReload2 = SavedChestBit(1);
    const int rec1 = RecordDelta(0);
    const int rec2 = RecordDelta(1);
    const int out1 = RowDelta(0);
    const int out2 = RowDelta(1);
    fprintf(stderr,
            "[FHQ] F11 reload 2 at scene %d entrance 0x%04X: pending %d, restore lines +%d, H2 chest bit in the loaded "
            "half %d, record delta H1 %d H2 %d, outbox delta H1 %d H2 %d, generation %u (quit %u)\n",
            (int)sceneNum, (unsigned)entrance, count, restoreLines, sFhq.h2BitAfterReload2, rec1, rec2, out1, out2,
            (unsigned)gComboCtx.commitGeneration, (unsigned)(sFhq.gen1 + 1));
    fflush(stderr);
    Check(stage, count == 0 && restoreLines == 0,
          "reload 2 still had %d pending host(s) and printed %d restore line(s): a delivered host is handed back "
          "again (its record did not clear the pending bit)",
          count, restoreLines);
    Check(stage, rec1 == 1 && rec2 == 1 && out1 == 1 && out2 == 1,
          "after reload 2 the records are H1 %d H2 %d and the rows H1 %d H2 %d, expected 1 each", rec1, rec2, out1,
          out2);
    Check(stage, SavedChestBit(0) == 1, "H1's chest flag is not in reload 2's half");
    CheckLoadCommon(stage, sFhq.gen1 + 1, sFhq.sav1);
    if (Verdict(stage)) {
        return;
    }
    Enter(FhqStage::OnceOnly);
}

// ---- the play stages -----------------------------------------------------------------

// F5 / F7: the drain delivers host `i` once; returns true when the stage is done.
bool DrainHost(PlayState* play, int i, const char* stage) {
    const int rows = RowDelta(i);
    const int pending = Pending(i);
    const int collected = OoT_Rando_Foreign_HostCollected(sFhq.h[i].rc);
    if (rows == 1 && sFhq.recordedAt < 0) {
        sFhq.recordedAt = sFhq.stageFrames;
    }
    if (sFhq.recordedAt >= 0 && pending != 0 && sFhq.stageFrames - sFhq.recordedAt >= kQuietFrames) {
        Fail(stage,
             "H%d's outbox row was written %d frames ago but its pending bit is still set: the record no longer "
             "clears it (OoT_Foreign_RecordPickupImpl)",
             i + 1, sFhq.stageFrames - sFhq.recordedAt);
        return false;
    }
    if (rows == 1 && pending == 0 && collected == 1 && sFhq.quiet >= kQuietFrames) {
        return true;
    }
    if (sFhq.stageFrames > kDrainBudget) {
        Fail(stage,
             "H%d (MM %s) was not delivered in %d frames: rows +%d, records +%d, pending %d, collected %d, give slot "
             "%d, msgMode %d, item cutscene %d, quiet %d (the reload's queue push, the drain or its cutscene)",
             i + 1, sFhq.h[i].itemName, kDrainBudget, rows, RecordDelta(i), pending, collected,
             OoT_Rando_Foreign_TestQueuedCheck(), (int)play->msgCtx.msgMode, InItemCutscene(play) ? 1 : 0, sFhq.quiet);
    }
    return false;
}

void Drain1Frame(PlayState* play) {
    if (!DrainHost(play, 0, "F5")) {
        return;
    }
    const int rec = RecordDelta(0);
    fprintf(stderr,
            "[FHQ] F5 H1 delivered %d frames after reload 1: record delta %d, outbox delta %d, pending %d, "
            "collected %d\n",
            sFhq.recordedAt, rec, RowDelta(0), Pending(0), OoT_Rando_Foreign_HostCollected(sFhq.h[0].rc));
    fflush(stderr);
    Check("F5", rec == 1, "H1 was recorded %d times by the reload's drain, expected once", rec);
    if (Verdict("F5")) {
        return;
    }
    Enter(FhqStage::WarpIn);
}

void WarpInFrame(void) {
    if (!sFhq.doorFired) {
        sFhq.doorFired = true;
        OoT_Fhq_FireDoor(ENTR_MIDOS_HOUSE_0, "int-foreign-hold-quit F6: Mido's House");
        return;
    }
    if (sFhq.stageFrames > kWarpBudget) {
        Fail("F6", "Mido's House never loaded in %d frames after its door fired", kWarpBudget);
    }
}

void RecordH2Frame(PlayState* play) {
    FhqHost& h2 = sFhq.h[1];
    if (sFhq.stageFrames == kRecordH2AtFrame) {
        Check("F7", play->sceneNum == h2.scene, "Link is in scene %d, not H2's scene %d", (int)play->sceneNum,
              h2.scene);
        Check("F7", SavedChestBit(1) == 0,
              "fixture: H2's chest flag is already in the saved word before it is opened, so the leg cannot test the "
              "exit snapshot's flush");
        const uint32_t live = play->actorCtx.flags.chest;
        Check("F7", sFhq.h[0].scene != h2.scene || (live & (1u << sFhq.h[0].flag)) != 0,
              "H1's chest flag did not reach the live word of its scene (0x%08X)", (unsigned)live);
        Check("F7", (live & (1u << h2.flag)) == 0,
              "fixture: H2's chest is already open in the live word (0x%08X), so its setter fires no hook",
              (unsigned)live);
        if (Verdict("F7")) {
            return;
        }
        OoT_Flags_SetTreasure(play, h2.flag); // the chest's own setter: the live flag and the hook
    }
    if (sFhq.stageFrames <= kRecordH2AtFrame || !DrainHost(play, 1, "F7")) {
        return;
    }
    const uint32_t live = play->actorCtx.flags.chest;
    const uint32_t saved = SavedChestWord(h2.scene);
    const int rec = RecordDelta(1);
    fprintf(stderr,
            "[FHQ] F7 H2 recorded in its own scene: record delta %d, pending %d; live chest word 0x%08X, saved chest "
            "word 0x%08X (H2 bit only live)\n",
            rec, Pending(1), (unsigned)live, (unsigned)saved);
    fflush(stderr);
    Check("F7", rec == 1, "H2 was recorded %d times, expected once", rec);
    Check("F7", (live & (1u << h2.flag)) != 0 && (saved & (1u << h2.flag)) == 0,
          "fixture: H2's bit must be in the live chest word only (live 0x%08X, saved 0x%08X); without that the quit "
          "cannot test the exit snapshot's flush",
          (unsigned)live, (unsigned)saved);
    if (Verdict("F7")) {
        return;
    }
    Enter(FhqStage::Quit2);
}

// F1 / F8: wait for a quiet frame run, then quit at this frame's end.
void QueueFrame(PlayState* play) {
    const char* stage = sFhq.stage == FhqStage::Queue1 ? "F1" : "F8";
    TickQuiet(play);
    const int autosave = CVarGetInteger(CVAR_ENHANCEMENT("Autosave"), 0);
    const bool sceneOk = sFhq.stage != FhqStage::Queue1 || play->sceneNum != sFhq.h[0].scene;
    if (sFhq.quiet >= kQuietFrames && autosave == 0 && sceneOk) {
        sFhq.quitRequested = true;
        return;
    }
    if (sFhq.stageFrames > kQueueBudget) {
        Fail(stage,
             "preconditions never met in %d frames: quiet %d/%d (msgMode %d, item cutscene %d, give slot %d, pending "
             "%d, transition %d/%d), Autosave %d (must be 0), scene %d (H1's scene %d must not be current)",
             kQueueBudget, sFhq.quiet, kQuietFrames, (int)play->msgCtx.msgMode, InItemCutscene(play) ? 1 : 0,
             OoT_Rando_Foreign_TestQueuedCheck(), Combo_CrossingPendingOoT_Count(), (int)play->transitionTrigger,
             (int)play->transitionMode, autosave, (int)play->sceneNum, sFhq.h[0].scene);
    }
}

void OnceOnlyFrame(PlayState* play) {
    FhqHost& h2 = sFhq.h[1];
    if (sFhq.stageFrames == 1 && sFhq.h2BitAfterReload2 == 0) {
        // As the player would: H2's chest is closed again, so he opens it again.
        sFhq.reopened = true;
        fprintf(stderr, "[FHQ] F12 H2's chest came back closed; opening it again as the player would\n");
        fflush(stderr);
        if (play->sceneNum == h2.scene) {
            OoT_Flags_SetTreasure(play, h2.flag);
        } else {
            gSaveContext.sceneFlags[h2.scene].chest |= 1u << h2.flag;
            GameInteractor_ExecuteOnSceneFlagSet((int16_t)h2.scene, FLAG_SCENE_TREASURE, (int16_t)h2.flag);
        }
    }
    if (sFhq.stageFrames < kOnceOnlyFrames) {
        return;
    }
    const int rec1 = RecordDelta(0);
    const int rec2 = RecordDelta(1);
    const int count = Combo_CrossingPendingOoT_Count();
    const uint16_t market = IntegrationTest_GetGameplayConfig()->bootEntrance;
    fprintf(stderr,
            "[FHQ] F12 after reload 2: H2 chest came back %s; record delta H1 %d H2 %d, pending %d after %d frames; "
            "returning to the Market (0x%04X)\n",
            sFhq.h2BitAfterReload2 == 1 ? "open" : "CLOSED", rec1, rec2, count, sFhq.stageFrames, (unsigned)market);
    fflush(stderr);
    Check("F12", rec1 == 1 && rec2 == 1 && count == 0,
          "after reload 2 a host was recorded again or is pending (records H1 %d H2 %d, pending %d)", rec1, rec2,
          count);
    Check("F12", sFhq.h2BitAfterReload2 == 1,
          "H2's chest came back closed after a quit in its own scene: the exit snapshot did not flush the current "
          "scene's flags (OoT_Combo_FlushSceneFlagsForFreeze in OoT_Combo_StageExitSnapshot), so the reload reads "
          "the host neither open nor found%s",
          sFhq.reopened ? "; opening it again recorded it again" : "");
    if (Verdict("F12")) {
        return;
    }
    Enter(FhqStage::MarketReturn);
    OoT_Fhq_FireDoor(market, "int-foreign-hold-quit F12: back to the Market for the stock crossing");
}

} // namespace

// ============================================================================
// Entry points (games/oot/soh/GameExports_SingleExe.cpp)
// ============================================================================

/** F0, before the generation: pin both hosts. 1 armed, -1 failed the run. */
extern "C" int OoT_Fhq_Pin(int slot, const uint8_t* name) {
    sFhq = FhqState{};
    sFhq.slot = slot;
    memcpy(sFhq.name, name, sizeof(sFhq.name));
    sFhq.h[0].rc = (uint16_t)RC_KF_MIDOS_TOP_LEFT_CHEST;
    sFhq.h[0].itemName = "Lens of Truth";
    sFhq.h[1].rc = (uint16_t)RC_KF_MIDOS_TOP_RIGHT_CHEST;
    sFhq.h[1].itemName = "Pictograph Box";
    ComboLogicForcedPlacement rows[2];
    memset(rows, 0, sizeof(rows));
    for (int i = 0; i < 2; i++) {
        if (!Combo_GetForeignItemByNameFor((uint8_t)GAME_MM, sFhq.h[i].itemName, &sFhq.h[i].item)) {
            Fail("F0", "fixture: Majora's Mask names no item \"%s\"", sFhq.h[i].itemName);
            return -1;
        }
        rows[i].hostGame = (uint8_t)GAME_OOT;
        rows[i].hostCheck = sFhq.h[i].rc;
        rows[i].item = sFhq.h[i].item;
    }
    const int armed = Combo_Logic_TestSetForcedPlacements(rows, 2);
    fprintf(stderr,
            "[FHQ] F0 pin: MM %s (id %u) on OoT check %u and MM %s (id %u) on OoT check %u through the debug-seed "
            "seam: %d row(s) armed\n",
            sFhq.h[0].itemName, (unsigned)sFhq.h[0].item.id, (unsigned)sFhq.h[0].rc, sFhq.h[1].itemName,
            (unsigned)sFhq.h[1].item.id, (unsigned)sFhq.h[1].rc, armed);
    fflush(stderr);
    if (armed != 2) {
        Fail("F0", "fixture: the debug-seed seam took %d row(s), expected 2", armed);
        return -1;
    }
    sFhq.pinned = true;
    sFhq.stage = FhqStage::Pinned;
    return 1;
}

/** The creation's fill took the pin (or the creation failed): clear it. */
extern "C" void OoT_Fhq_PinClear(void) {
    if (sFhq.pinned) {
        (void)Combo_Logic_TestSetForcedPlacements(NULL, 0);
        sFhq.pinned = false;
    }
}

/** F0, after the creation: the pinned rows are what the world stores. */
extern "C" bool OoT_Fhq_CheckCreated(char* msg, size_t cap) {
    std::string fails;
    uint8_t kind[2] = { 0, 0 };
    uint16_t units[2] = { 0, 0 };
    bool credit[2] = { false, false };
    const bool sharedLens = Combo_ComboCrossItemRule(RSBS_XITEM_SHARED_LENS);
    for (int i = 0; i < 2; i++) {
        FhqHost& h = sFhq.h[i];
        const Rando::SpoilerCollectionCheck cc =
            Rando::StaticData::GetLocation((RandomizerCheck)h.rc)->GetCollectionCheck();
        h.scene = cc.type == SPOILER_CHK_CHEST ? (int)cc.scene : -1;
        h.flag = cc.type == SPOILER_CHK_CHEST ? (int)cc.flag : -1;
        const SharedItem* stored = Combo_Crossings_Lookup(GAME_OOT, h.rc);
        credit[i] = Combo_SharedPickupCreditOf(h.item, &kind[i], &units[i]);
        char line[256];
        if (h.scene < 0 || h.flag < 0 || h.flag > 31) {
            snprintf(line, sizeof(line), "H%d's check %u is not a chest (collection type %d)", i + 1, (unsigned)h.rc,
                     (int)cc.type);
        } else if (stored == nullptr || stored->originGame != h.item.originGame || stored->id != h.item.id) {
            snprintf(line, sizeof(line), "H%d's check %u does not hold the pinned MM %s (stored %s %u)", i + 1,
                     (unsigned)h.rc, h.itemName, stored != nullptr ? "id" : "nothing",
                     stored != nullptr ? (unsigned)stored->id : 0u);
        } else if (credit[i]) {
            snprintf(line, sizeof(line),
                     "H%d's MM %s is credited to the shared pool at pickup (kind %u units %u): it "
                     "writes no outbox row (#1061/#1120)",
                     i + 1, h.itemName, (unsigned)kind[i], (unsigned)units[i]);
        } else if (OoT_Rando_Foreign_HostCollected(h.rc) != 0 || Combo_CrossingPendingOoT_Test(h.rc)) {
            snprintf(line, sizeof(line), "H%d's check %u is already collected or pending at creation", i + 1,
                     (unsigned)h.rc);
        } else {
            line[0] = '\0';
        }
        if (line[0] != '\0') {
            fails += fails.empty() ? "" : "; ";
            fails += line;
        }
    }
    if (sharedLens) {
        fails += fails.empty() ? "" : "; ";
        fails += "the frozen rules share the Lens of Truth (RSBS_XITEM_SHARED_LENS): H1 would be a family, not an "
                 "outbox row";
    }
    fprintf(stderr,
            "[FHQ] F0 pinned MM %s (id %u) on OoT check %u (scene %d chest flag %d) and MM %s (id %u) on OoT check %u "
            "(scene %d chest flag %d): 2 row(s) armed; pool credit %d/%d (must be 0/0), shared lens rule %d; class "
            "kinds %u/%u, reserve kinds %u/%u\n",
            sFhq.h[0].itemName, (unsigned)sFhq.h[0].item.id, (unsigned)sFhq.h[0].rc, sFhq.h[0].scene, sFhq.h[0].flag,
            sFhq.h[1].itemName, (unsigned)sFhq.h[1].item.id, (unsigned)sFhq.h[1].rc, sFhq.h[1].scene, sFhq.h[1].flag,
            credit[0] ? 1 : 0, credit[1] ? 1 : 0, sharedLens ? 1 : 0,
            (unsigned)Combo_ItemClassSharedKind(sFhq.h[0].item), (unsigned)Combo_ItemClassSharedKind(sFhq.h[1].item),
            (unsigned)Combo_SharedSlotReserveKind(sFhq.h[0].item),
            (unsigned)Combo_SharedSlotReserveKind(sFhq.h[1].item));
    fflush(stderr);
    if (!fails.empty()) {
        snprintf(msg, cap, "int-foreign-hold-quit F0 fixture: %s", fails.c_str());
        sFhq.stage = FhqStage::Finished;
        return false;
    }
    return true;
}

/** One of the row's own hosts (the #878 chest drive leaves them alone). */
extern "C" bool OoT_Fhq_IsHost(uint16_t rc) {
    return sFhq.stage != FhqStage::Off && (rc == sFhq.h[0].rc || rc == sFhq.h[1].rc);
}

/** OnGameStateMainStart (OoT): the title, file-select and load budgets, the presses. */
extern "C" void OoT_Fhq_FrameStart(void) {
    sFhq.frame++;
    if (sFhq.pressPlayA) {
        sFhq.pressPlayA = false;
        if (OoT_gPlayState != nullptr && OoT_gGameState == &OoT_gPlayState->state) {
            OoT_gPlayState->state.input[0].press.button |= BTN_A;
        }
    }
    switch (sFhq.stage) {
        case FhqStage::Title:
            if (++sFhq.stageFrames > kTitleBudget) {
                Fail(Label("F2", "F9"),
                     "the title never ran in %d frames after quit %d (gGameState %p, gameMode %d, running %d)",
                     kTitleBudget, sFhq.round, (void*)OoT_gGameState, (int)gSaveContext.gameMode,
                     OoT_gGameState != nullptr ? (int)OoT_gGameState->running : -1);
            }
            return;
        case FhqStage::Load:
            if (++sFhq.stageFrames > kLoadBudget) {
                Fail(Label("F4", "F11"), "no scene was built in %d frames after the file select's load (gameMode %d)",
                     kLoadBudget, (int)gSaveContext.gameMode);
            }
            return;
        case FhqStage::Select:
            break;
        default:
            return;
    }
    const char* stage = Label("F3", "F10");
    if (++sFhq.stageFrames > kSelectBudget) {
        const char* words = RsbsSave_SlotRefusalWords(sFhq.slot);
        Fail(stage,
             "the file select never loaded file %d in %d frames (menuMode %d, configMode %d, selectMode %d, slot "
             "refusal \"%s\")",
             sFhq.slot + 1, kSelectBudget, sFhq.lastMenu, sFhq.lastConfig, sFhq.lastSelect,
             words != nullptr ? words : "");
        return;
    }
    // The file select's context, as its own main left it last frame; never used
    // once another gamestate runs.
    if (sFhq.fileChoose == nullptr || sFhq.fileChooseFrame + 1 != sFhq.frame ||
        (void*)OoT_gGameState != sFhq.fileChoose) {
        return;
    }
    FileChooseContext* fc = (FileChooseContext*)sFhq.fileChoose;
    if (!sFhq.metaChecked) {
        sFhq.metaChecked = true;
        const SaveFileMetaInfo* meta = Save_GetSaveMetaInfo(sFhq.slot);
        if (meta == nullptr || !meta->valid || memcmp(meta->playerName, sFhq.name, sizeof(sFhq.name)) != 0) {
            Fail(stage, "file %d is not this row's file at the file select (valid %d)", sFhq.slot + 1,
                 meta != nullptr ? (int)meta->valid : -1);
            return;
        }
    }
    const char* press = nullptr;
    if (fc->menuMode == FS_MENU_MODE_CONFIG && fc->configMode == CM_MAIN_MENU) {
        fc->buttonIndex = sFhq.slot;
        if (++sFhq.pressFrames % 8 == 0) {
            press = "A on the file";
        }
    } else if (fc->menuMode == FS_MENU_MODE_SELECT && fc->selectMode == SM_CONFIRM_FILE &&
               fc->selectedFileIndex == sFhq.slot && fc->confirmButtonIndex == FS_BTN_CONFIRM_YES &&
               fc->fileInfoAlpha[sFhq.slot] >= 200) {
        if (sFhq.pressFrames++ % 30 == 0) {
            press = "A on Yes";
        }
    }
    if (press != nullptr) {
        fc->state.input[0].press.button |= BTN_A;
    }
    if (press != nullptr || fc->menuMode != sFhq.lastMenu || fc->configMode != sFhq.lastConfig ||
        fc->selectMode != sFhq.lastSelect) {
        sFhq.lastMenu = fc->menuMode;
        sFhq.lastConfig = fc->configMode;
        sFhq.lastSelect = fc->selectMode;
        fprintf(stderr, "[FHQ] %s file select %d: menuMode %d configMode %d selectMode %d file %d -> %s\n", stage,
                sFhq.round, (int)fc->menuMode, (int)fc->configMode, (int)fc->selectMode, (int)fc->selectedFileIndex,
                press != nullptr ? press : "waiting");
        fflush(stderr);
    }
}

/** OnGameFrameUpdate (OoT), after the play update: F1 and F8 quit here. */
extern "C" void OoT_Fhq_FrameEnd(void) {
    if (!sFhq.quitRequested || (sFhq.stage != FhqStage::Queue1 && sFhq.stage != FhqStage::Quit2)) {
        return;
    }
    if (OoT_gPlayState == nullptr || OoT_gGameState != &OoT_gPlayState->state) {
        return;
    }
    sFhq.quitRequested = false;
    QuitNow();
}

/** OnZTitleUpdate (OoT, after the boot): F2 / F9. */
extern "C" void OoT_Fhq_TitleUpdate(void* gameState) {
    if (sFhq.stage != FhqStage::Title || gameState == nullptr) {
        return;
    }
    const char* stage = Label("F2", "F9");
    const int retired = IntegrationTest_StderrCaptureCount(kTitleNeedle) - sFhq.titleAtQuit;
    const int pairing = Combo_ForeignPairingActive() ? 1 : 0;
    const int pending = Combo_CrossingPendingOoT_Count();
    fprintf(
        stderr,
        "[FHQ] %s title reached %d frames after quit %d: session retired (retire lines +%d, pairing %d, RAM pending "
        "%d); skipping to the file select (Boot Sequence: File Select's function)\n",
        stage, sFhq.stageFrames, sFhq.round, retired, pairing, pending);
    fflush(stderr);
    Check(stage, retired == 1, "the return to the title retired the session %d time(s), expected once", retired);
    Check(stage, pairing == 0 && pending == 0,
          "fixture: the retired session still holds a pairing (%d) or pending hosts (%d), so the reload could not "
          "prove they came from disk",
          pairing, pending);
    if (Verdict(stage)) {
        return;
    }
    OnZTitleUpdateSkipToFileSelect(gameState);
    Enter(FhqStage::Select);
}

/** OnFileChooseMain (OoT, after the boot): cache the context for the next frame's presses. */
extern "C" void OoT_Fhq_FileChooseMain(void* gameState) {
    if (sFhq.stage != FhqStage::Select) {
        return;
    }
    sFhq.fileChoose = gameState;
    sFhq.fileChooseFrame = sFhq.frame;
}

/** OnLoadGame (OoT): the file select's load ran (order against other hooks unspecified: logs only). */
extern "C" void OoT_Fhq_OnLoadGame(int32_t fileNum) {
    if (sFhq.stage != FhqStage::Select) {
        return;
    }
    const char* stage = Label("F3", "F10");
    fprintf(stderr, "[FHQ] %s file select %d: FileChoose_LoadGame loaded file %d (OnLoadGame) after %d frames\n", stage,
            sFhq.round, (int)fileNum + 1, sFhq.stageFrames);
    fflush(stderr);
    if (fileNum != sFhq.slot) {
        Fail(stage, "the file select loaded file %d, not file %d", (int)fileNum + 1, sFhq.slot + 1);
        return;
    }
    sFhq.fileChoose = nullptr;
    Enter(FhqStage::Load);
}

/** OnSceneInit (OoT): true when the arrival is the row's own (the stock boot check must not run). */
extern "C" bool OoT_Fhq_SceneInit(int phase, int16_t sceneNum, uint16_t entrance) {
    if (phase != (int)GP_PHASE_OOT_PRE) {
        return false;
    }
    switch (sFhq.stage) {
        case FhqStage::Off:
        case FhqStage::Pinned:
        case FhqStage::Queue1:
        case FhqStage::Done:
        case FhqStage::Finished:
            return false;
        case FhqStage::MarketReturn:
            // The stock check reads it as the boot arrival (cfg->bootEntrance).
            Enter(FhqStage::MarketSettle);
            return false;
        case FhqStage::Load:
            if (sFhq.round == 1) {
                MeasureReload1(sceneNum, entrance);
            } else {
                MeasureReload2(sceneNum, entrance);
            }
            return true;
        case FhqStage::WarpIn: {
            // OnSceneInit fires from OoT_Play_SpawnScene, BEFORE Play_Init's actor
            // context init (func_800304DC) copies the saved word into the live one:
            // read the saved word here; F7 checks the live word on its first frame.
            const uint32_t saved = SavedChestWord(sFhq.h[1].scene);
            fprintf(stderr,
                    "[FHQ] F6 warped into scene %d (entrance 0x%04X) for the same-scene leg; saved chest word 0x%08X\n",
                    (int)sceneNum, (unsigned)entrance, (unsigned)saved);
            fflush(stderr);
            Check("F6", sceneNum == sFhq.h[1].scene, "the warp arrived in scene %d, not H2's scene %d", (int)sceneNum,
                  sFhq.h[1].scene);
            Check("F6", sFhq.h[0].scene != sFhq.h[1].scene || (saved & (1u << sFhq.h[0].flag)) != 0,
                  "H1's chest flag is not in the saved word its scene loads from (0x%08X)", (unsigned)saved);
            if (!Verdict("F6")) {
                Enter(FhqStage::RecordH2);
            }
            return true;
        }
        default:
            Fail(StageLabel(), "an unexpected OoT scene init (scene %d, entrance 0x%04X)", (int)sceneNum,
                 (unsigned)entrance);
            return true;
    }
}

/** The paired drive's OoT_PRE frames are done: true when the row's legs are done too. */
extern "C" bool OoT_Fhq_PlayerFrame(PlayState* play) {
    if (sFhq.stage == FhqStage::Pinned) {
        Enter(FhqStage::Queue1);
    }
    if (sFhq.stage == FhqStage::Done || sFhq.stage == FhqStage::Off) {
        return true;
    }
    if (sFhq.stage == FhqStage::Finished || play == nullptr) {
        return false;
    }
    sFhq.stageFrames++;
    TickTextbox(play);
    if (!GuardTwice(StageLabel())) {
        return false;
    }
    switch (sFhq.stage) {
        case FhqStage::Queue1:
        case FhqStage::Quit2:
            QueueFrame(play);
            break;
        case FhqStage::Drain1:
            TickQuiet(play);
            Drain1Frame(play);
            break;
        case FhqStage::WarpIn:
            WarpInFrame();
            break;
        case FhqStage::RecordH2:
            TickQuiet(play);
            RecordH2Frame(play);
            break;
        case FhqStage::OnceOnly:
            OnceOnlyFrame(play);
            break;
        case FhqStage::MarketSettle:
            if (sFhq.stageFrames >= kMarketSettle) {
                fprintf(stderr,
                        "[FHQ] F12 in the Market (scene %d) after %d frames: the stock paired crossing takes "
                        "over (deaths sentinel, the Happy Mask Shop door)\n",
                        (int)play->sceneNum, sFhq.stageFrames);
                fflush(stderr);
                Enter(FhqStage::Done);
                return true;
            }
            break;
        default:
            break; // Title / Select / Load / MarketReturn: waiting for a gamestate or a scene
    }
    return false;
}

/** F13: the return leg's verdict (GP_PHASE_OOT_RETURN, past the paired drive's frames). */
extern "C" void OoT_Fhq_ReturnVerdict(PlayState* play, int frame) {
    (void)play;
    if (sFhq.stage != FhqStage::Done || frame != 0) {
        return;
    }
    const char* stage = "F13";
    int rows[2];
    int redeemed[2];
    int recs[2];
    for (int i = 0; i < 2; i++) {
        rows[i] = RowDelta(i);
        redeemed[i] = Rows(sFhq.h[i], true) - sFhq.h[i].red0;
        recs[i] = RecordDelta(i);
    }
    const int mmOwed = Combo_CountSharedItems(GAME_MM, false);
    const int pending = Combo_CrossingPendingOoT_Count();
    const int held = IntegrationTest_StderrCaptureCount(kHeldNeedle) - sFhq.held0;
    fprintf(stderr,
            "[FHQ] F13 return leg: H1 rows +%d (redeemed +%d) records +%d; H2 rows +%d (redeemed +%d) records +%d; MM "
            "items owed %d, pending %d, HELD lines +%d\n",
            rows[0], redeemed[0], recs[0], rows[1], redeemed[1], recs[1], mmOwed, pending, held);
    fflush(stderr);
    for (int i = 0; i < 2; i++) {
        Check(stage, rows[i] == 1 && redeemed[i] == 1 && recs[i] == 1,
              "H%d (MM %s): %d new row(s), %d redeemed, %d record(s); expected exactly one each (Majora's Mask must "
              "receive it once)",
              i + 1, sFhq.h[i].itemName, rows[i], redeemed[i], recs[i]);
    }
    Check(stage, mmOwed == 0, "%d MM item(s) are still owed after Majora's Mask's arrival", mmOwed);
    Check(stage, pending == 0 && held == 0, "after the round trip %d host(s) are pending and %d HELD line(s) appeared",
          pending, held);
    if (Verdict(stage)) {
        return;
    }
    sFhq.stage = FhqStage::Finished;
    fprintf(stderr,
            "[FHQ] PASS: after two real quits and reloads and a round trip, MM item %u from check %u: 1 row "
            "(redeemed), 1 record; MM item %u from check %u: 1 row (redeemed), 1 record; pending 0\n",
            (unsigned)sFhq.h[0].item.id, (unsigned)sFhq.h[0].rc, (unsigned)sFhq.h[1].item.id, (unsigned)sFhq.h[1].rc);
    fflush(stderr);
    IntegrationTest_SetGameplayPhase(GP_PHASE_DONE);
    // A pass leaves no file behind (a failure keeps it for forensics; the next
    // paired run erases it by its name).
    Save_DeleteFile(sFhq.slot);
    fprintf(stderr, "[PFC] PASS: a foreign hold survives a quit; erased file %d\n", sFhq.slot + 1);
    fflush(stderr);
    IntegrationTest_SignalBootComplete(GAME_OOT, "foreign hold survives a quit");
}

/** The watchdog's line (plain reads only: it runs on the watchdog thread). */
extern "C" const char* OoT_Fhq_StageName(void) {
    return StageLabel();
}
