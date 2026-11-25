/**
 * @file oot_minigame_options_test.cpp
 * ROM-free, display-free lock for backport packet #1747 (seven OoT minigame
 * enhancement commits: SoH #6965, #6975, #6991, #6517, #6982, #6476, #6778).
 * CTest label "redship", row OoTMinigameOptions in CMake/SingleExecutable.cmake,
 * dispatch "oot-minigame-options" in src/common/test_runner.cpp.
 *
 * WHAT IT DRIVES. ShipInit registrars (under "*" and each option's CVar key)
 * for IngoRaceOnce.cpp, HorsebackArchery.cpp, DivingGameTimer.cpp, and
 * RupeeDash.cpp, then the REAL GameInteractor_Should dispatch for all 7 new
 * VB ids (off -> vanilla, on -> option) with actor/state arguments, plus
 * RupeeDash.cpp's wallet-scaling OnPlayerUpdate callback.
 *
 * WHAT ONLY A PERSON IN-GAME CAN CHECK. Full Lon Lon Ranch actor transitions
 * in EnIn / EnHorseGameCheck (dialogue choices, cutscene camera, fence jump),
 * arrow target hit physics in EnYabusameMark and back-to-back reward delivery in
 * EnGe1, HUD timer rendering in EnDivingGame, and live per-frame rupee drain
 * timing / 0-rupee damage inside a loaded scene.
 */

#ifdef RSBS_SINGLE_EXECUTABLE

// At file scope, not inside an extern "C" block: z64.h includes <memory> under
// C++.
#include <z64.h>

#include "context.h"
#include "soh/ShipInit.hpp"
#include "soh/cvar_prefixes.h"
#include "soh/Enhancements/enhancementTypes.h"
#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/Enhancements/game-interactor/GameInteractor_Hooks.h"

#include <cstdio>
#include <cstring>
#include <memory>
#include <string>

extern "C" {
#include "functions.h"
#include "macros.h"
#include "variables.h"
#include "src/overlays/actors/ovl_En_Ge1/z_en_ge1.h"
extern PlayState* OoT_gPlayState;
extern void EnGe1_TalkAfterGame_Archery(EnGe1* enGe1, PlayState* play);
}

namespace {

#define MO_ASSERT(cond, msg)                                              \
    do {                                                                  \
        if (!(cond)) {                                                    \
            printf("[TEST] FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__); \
            return 1;                                                     \
        }                                                                 \
    } while (0)

const char* const kIngoRaceOnceKey = CVAR_ENHANCEMENT("IngoRaceOnce");
const char* const kHbaCustomizeKey = CVAR_ENHANCEMENT("CustomizeHorsebackArchery");
const char* const kHbaInstantWinKey = CVAR_ENHANCEMENT("InstantHorsebackArcheryWin");
const char* const kHbaAlwaysScoreKey = CVAR_ENHANCEMENT("HorsebackArcheryAlwaysScore");
const char* const kHbaAmmoKey = CVAR_ENHANCEMENT("HorsebackArcheryAmmo");
const char* const kHbaBothPrizesKey = CVAR_ENHANCEMENT("HorsebackArcheryBothPrizes");
const char* const kDivingTimeLimitKey = CVAR_ENHANCEMENT("DivingGame.TimeLimit");
const char* const kRupeeDashKey = CVAR_ENHANCEMENT("RupeeDash");
const char* const kRupeeDashScalingKey = CVAR_ENHANCEMENT("RupeeDashScaling");
const char* const kRupeeDashIntervalKey = CVAR_ENHANCEMENT("RupeeDashInterval");

const char* const kAllKeys[] = {
    kIngoRaceOnceKey,  kHbaCustomizeKey,    kHbaInstantWinKey, kHbaAlwaysScoreKey,   kHbaAmmoKey,
    kHbaBothPrizesKey, kDivingTimeLimitKey, kRupeeDashKey,     kRupeeDashScalingKey, kRupeeDashIntervalKey,
};
constexpr int kAllKeyCount = static_cast<int>(sizeof(kAllKeys) / sizeof(kAllKeys[0]));
constexpr int32_t kUnsetSentinel = -999999;

// Zero-initialized scratch structs reused across legs (static to avoid huge stack frames).
PlayState sPlayState;
EnGe1 sEnGe1;
GameInfo sTempGameInfo;

bool MatchesSite(const ShipInitEntry& entry, const char* filename) {
    const std::string file = entry.registeredAt.file_name();
    const std::string want = filename;
    return file.size() >= want.size() && file.compare(file.size() - want.size(), want.size(), want) == 0;
}

int CountRegistrars(const char* key, const char* filename) {
    int found = 0;
    for (const auto& entry : ShipInit::GetAll()[key]) {
        if (MatchesSite(entry, filename)) {
            found++;
        }
    }
    return found;
}

void RunRegistrars(const char* key, const char* filename) {
    for (const auto& entry : ShipInit::GetAll()[key]) {
        if (MatchesSite(entry, filename)) {
            entry.initFunc();
        }
    }
}

void RunAllPacketRegistrars() {
    RunRegistrars("*", "IngoRaceOnce.cpp");
    RunRegistrars("*", "HorsebackArchery.cpp");
    RunRegistrars("*", "DivingGameTimer.cpp");
    RunRegistrars("*", "RupeeDash.cpp");
}

void ClearPacketKeys() {
    for (int i = 0; i < kAllKeyCount; i++) {
        CVarClear(kAllKeys[i]);
    }
}

void ResetScratchState(GameInfo* scratchGameInfo) {
    std::memset(&gSaveContext, 0, sizeof(gSaveContext));
    std::memset(&sPlayState, 0, sizeof(sPlayState));
    std::memset(&sEnGe1, 0, sizeof(sEnGe1));
    std::memset(scratchGameInfo, 0, sizeof(*scratchGameInfo));
}

template <typename H> struct HooksAside {
    decltype(GameInteractor::RegisteredGameHooks<H>::functions) functions;
    decltype(GameInteractor::RegisteredGameHooks<H>::functionsForFilter) filters;

    HooksAside() {
        functions.swap(GameInteractor::RegisteredGameHooks<H>::functions);
        filters.swap(GameInteractor::RegisteredGameHooks<H>::functionsForFilter);
    }

    ~HooksAside() {
        functions.swap(GameInteractor::RegisteredGameHooks<H>::functions);
        filters.swap(GameInteractor::RegisteredGameHooks<H>::functionsForFilter);
    }
};

struct RuntimeStateGuard {
    GameId prevGame;
    int32_t savedVals[kAllKeyCount];
    std::unique_ptr<SaveContext> savedSave;
    PlayState* prevPlayState;
    GameInfo* prevGameInfo;
    std::unique_ptr<GameInfo> savedGameInfoCopy;

    RuntimeStateGuard()
        : prevGame(Context_GetCurrentGame()), savedSave(new SaveContext), prevPlayState(OoT_gPlayState),
          prevGameInfo(gGameInfo) {
        for (int i = 0; i < kAllKeyCount; i++) {
            savedVals[i] = CVarGetInteger(kAllKeys[i], kUnsetSentinel);
        }
        *savedSave = gSaveContext;
        if (prevGameInfo != nullptr) {
            savedGameInfoCopy.reset(new GameInfo);
            *savedGameInfoCopy = *prevGameInfo;
        }
    }

    ~RuntimeStateGuard() {
        for (int i = 0; i < kAllKeyCount; i++) {
            if (savedVals[i] != kUnsetSentinel) {
                CVarSetInteger(kAllKeys[i], savedVals[i]);
            } else {
                CVarClear(kAllKeys[i]);
            }
        }
        RunAllPacketRegistrars();
        gSaveContext = *savedSave;
        OoT_gPlayState = prevPlayState;
        gGameInfo = prevGameInfo;
        if (prevGameInfo != nullptr && savedGameInfoCopy != nullptr) {
            *prevGameInfo = *savedGameInfoCopy;
        }
        Context_SetCurrentGame(prevGame);
    }
};

int CheckRegistrarsLinked() {
    MO_ASSERT(CountRegistrars("*", "IngoRaceOnce.cpp") == 1, "IngoRaceOnce.cpp boot registrar missing");
    MO_ASSERT(CountRegistrars(kIngoRaceOnceKey, "IngoRaceOnce.cpp") == 1, "IngoRaceOnce.cpp CVar registrar missing");

    MO_ASSERT(CountRegistrars("*", "HorsebackArchery.cpp") == 4, "HorsebackArchery.cpp boot registrars != 4");
    MO_ASSERT(CountRegistrars(kHbaCustomizeKey, "HorsebackArchery.cpp") == 3,
              "HorsebackArchery.cpp CustomizeHorsebackArchery registrars != 3");
    MO_ASSERT(CountRegistrars(kHbaInstantWinKey, "HorsebackArchery.cpp") == 1,
              "HorsebackArchery.cpp InstantHorsebackArcheryWin registrar missing");
    MO_ASSERT(CountRegistrars(kHbaAlwaysScoreKey, "HorsebackArchery.cpp") == 1,
              "HorsebackArchery.cpp HorsebackArcheryAlwaysScore registrar missing");
    MO_ASSERT(CountRegistrars(kHbaAmmoKey, "HorsebackArchery.cpp") == 1,
              "HorsebackArchery.cpp HorsebackArcheryAmmo registrar missing");
    MO_ASSERT(CountRegistrars(kHbaBothPrizesKey, "HorsebackArchery.cpp") == 1,
              "HorsebackArchery.cpp HorsebackArcheryBothPrizes registrar missing");

    MO_ASSERT(CountRegistrars("*", "DivingGameTimer.cpp") == 1, "DivingGameTimer.cpp boot registrar missing");
    MO_ASSERT(CountRegistrars(kDivingTimeLimitKey, "DivingGameTimer.cpp") == 1,
              "DivingGameTimer.cpp DivingGame.TimeLimit registrar missing");

    MO_ASSERT(CountRegistrars("*", "RupeeDash.cpp") == 1, "RupeeDash.cpp boot registrar missing");
    MO_ASSERT(CountRegistrars(kRupeeDashKey, "RupeeDash.cpp") == 1, "RupeeDash.cpp RupeeDash registrar missing");
    return 0;
}

int CheckIngoRaceOptions(GameInfo* scratchGameInfo) {
    // Leg 1: VB_LINK_WIN_EPONA (off = Twice -> vanilla passthrough; Once / None -> force true).
    ClearPacketKeys();
    RunRegistrars(kIngoRaceOnceKey, "IngoRaceOnce.cpp");
    MO_ASSERT(!GameInteractor_Should(VB_LINK_WIN_EPONA, false), "VB_LINK_WIN_EPONA fired when IngoRaceOnce unset");
    MO_ASSERT(GameInteractor_Should(VB_LINK_WIN_EPONA, true),
              "VB_LINK_WIN_EPONA blocked true vanilla result when unset");

    CVarSetInteger(kIngoRaceOnceKey, INGO_RACE_ONCE);
    RunRegistrars(kIngoRaceOnceKey, "IngoRaceOnce.cpp");
    MO_ASSERT(GameInteractor_Should(VB_LINK_WIN_EPONA, false), "VB_LINK_WIN_EPONA stayed false under INGO_RACE_ONCE");

    // Leg 2: VB_RACE_INGO (Twice/Once -> vanilla; None with entranceIndex==2 & (eventInf[0]&0x12)==2 -> skip).
    ResetScratchState(scratchGameInfo);
    OoT_gPlayState = &sPlayState;
    gSaveContext.eventInf[0] = 2;
    gSaveContext.timerState = TIMER_STATE_DOWN_TICK;
    MO_ASSERT(GameInteractor_Should(VB_RACE_INGO, true, 2), "VB_RACE_INGO skipped race under INGO_RACE_ONCE");
    MO_ASSERT(sPlayState.nextEntranceIndex == 0, "VB_RACE_INGO touched nextEntranceIndex under INGO_RACE_ONCE");

    CVarSetInteger(kIngoRaceOnceKey, INGO_RACE_NONE);
    RunRegistrars(kIngoRaceOnceKey, "IngoRaceOnce.cpp");
    MO_ASSERT(GameInteractor_Should(VB_LINK_WIN_EPONA, false), "VB_LINK_WIN_EPONA stayed false under INGO_RACE_NONE");

    // Non-Epona (eventInf[0]==0), post-rematch (0x12), and non-race entrance (1) must stay vanilla.
    gSaveContext.eventInf[0] = 0;
    MO_ASSERT(GameInteractor_Should(VB_RACE_INGO, true, 2), "VB_RACE_INGO skipped with non-Epona eventInf[0]=0");
    gSaveContext.eventInf[0] = 0x12;
    MO_ASSERT(GameInteractor_Should(VB_RACE_INGO, true, 2), "VB_RACE_INGO skipped rematch with eventInf[0]=0x12");
    gSaveContext.eventInf[0] = 2;
    MO_ASSERT(GameInteractor_Should(VB_RACE_INGO, true, 1), "VB_RACE_INGO skipped for entranceIndex != 2");

    // Riding Epona entering race (entranceIndex == 2, (eventInf[0] & 0x12) == 2).
    MO_ASSERT(!GameInteractor_Should(VB_RACE_INGO, true, 2), "VB_RACE_INGO did not skip race under INGO_RACE_NONE");
    MO_ASSERT(sPlayState.nextEntranceIndex == ENTR_LON_LON_RANCH_7, "VB_RACE_INGO did not set ENTR_LON_LON_RANCH_7");
    MO_ASSERT(gSaveContext.eventInf[0] == 0x8046, "VB_RACE_INGO did not set eventInf[0] flags 0x8046");
    MO_ASSERT(sPlayState.transitionType == TRANS_TYPE_FADE_WHITE, "VB_RACE_INGO did not set fade-white transition");
    MO_ASSERT(sPlayState.transitionTrigger == TRANS_TRIGGER_START, "VB_RACE_INGO did not trigger transition");
    MO_ASSERT(gSaveContext.timerState == TIMER_STATE_OFF, "VB_RACE_INGO did not turn off race timer");
    MO_ASSERT(gSaveContext.forcedSeqId == NA_BGM_INGO, "VB_RACE_INGO did not queue NA_BGM_INGO");

    CVarClear(kIngoRaceOnceKey);
    RunRegistrars(kIngoRaceOnceKey, "IngoRaceOnce.cpp");
    MO_ASSERT(GameInteractor_Should(VB_RACE_INGO, true, 2), "VB_RACE_INGO stayed armed after clearing IngoRaceOnce");
    return 0;
}

int CheckHorsebackArcheryOptions(GameInfo* scratchGameInfo) {
    // Leg 3: VB_PLAY_HORSEBACK_ARCHERY (requires both CustomizeHorsebackArchery and InstantHorsebackArcheryWin).
    ClearPacketKeys();
    RunRegistrars("*", "HorsebackArchery.cpp");
    ResetScratchState(scratchGameInfo);
    OoT_gPlayState = &sPlayState;
    HooksAside<GameInteractor::OnFlagSet> onFlagSetGuard;

    CVarSetInteger(kHbaInstantWinKey, 1);
    RunRegistrars(kHbaInstantWinKey, "HorsebackArchery.cpp");
    MO_ASSERT(GameInteractor_Should(VB_PLAY_HORSEBACK_ARCHERY, true, &sEnGe1, &sPlayState),
              "VB_PLAY_HORSEBACK_ARCHERY fired without CustomizeHorsebackArchery");

    CVarSetInteger(kHbaCustomizeKey, 1);
    RunRegistrars(kHbaCustomizeKey, "HorsebackArchery.cpp");
    // sPlayState.msgCtx.msgLength == 0 avoids audio playback in OoT_Message_CloseTextbox.
    MO_ASSERT(!GameInteractor_Should(VB_PLAY_HORSEBACK_ARCHERY, true, &sEnGe1, &sPlayState),
              "VB_PLAY_HORSEBACK_ARCHERY did not skip game on InstantWin");
    MO_ASSERT(gSaveContext.minigameScore == 1500, "VB_PLAY_HORSEBACK_ARCHERY did not set minigameScore = 1500");
    MO_ASSERT(gSaveContext.rupeeAccumulator == -20, "VB_PLAY_HORSEBACK_ARCHERY did not charge 20 rupees");
    MO_ASSERT(OoT_Flags_GetEventChkInf(EVENTCHKINF_PLAYED_HORSEBACK_ARCHERY) != 0,
              "VB_PLAY_HORSEBACK_ARCHERY did not set played flag");
    MO_ASSERT(sEnGe1.actionFunc == EnGe1_TalkAfterGame_Archery,
              "VB_PLAY_HORSEBACK_ARCHERY did not advance EnGe1 actionFunc");

    CVarClear(kHbaInstantWinKey);
    RunRegistrars(kHbaInstantWinKey, "HorsebackArchery.cpp");
    ResetScratchState(scratchGameInfo);
    MO_ASSERT(GameInteractor_Should(VB_PLAY_HORSEBACK_ARCHERY, true, &sEnGe1, &sPlayState),
              "VB_PLAY_HORSEBACK_ARCHERY stayed armed after clearing InstantHorsebackArcheryWin");

    // Leg 4: VB_SCORE_HORSEBACK_ARCHERY_TARGET.
    s32 scoreIndex = 0;
    GameInteractor_Should(VB_SCORE_HORSEBACK_ARCHERY_TARGET, true, &scoreIndex);
    MO_ASSERT(scoreIndex == 0, "VB_SCORE_HORSEBACK_ARCHERY_TARGET modified scoreIndex when off");

    CVarSetInteger(kHbaAlwaysScoreKey, 1);
    RunRegistrars(kHbaAlwaysScoreKey, "HorsebackArchery.cpp");
    scoreIndex = 0;
    GameInteractor_Should(VB_SCORE_HORSEBACK_ARCHERY_TARGET, true, &scoreIndex);
    MO_ASSERT(scoreIndex == 2, "VB_SCORE_HORSEBACK_ARCHERY_TARGET did not force scoreIndex = 2");

    // Leg 5: VB_SET_HORSEBACK_ARCHERY_AMMO.
    sPlayState.interfaceCtx.hbaAmmo = 20;
    MO_ASSERT(GameInteractor_Should(VB_SET_HORSEBACK_ARCHERY_AMMO, true, &sPlayState.interfaceCtx),
              "VB_SET_HORSEBACK_ARCHERY_AMMO suppressed vanilla when Ammo at default 20");

    CVarSetInteger(kHbaAmmoKey, 35);
    RunRegistrars(kHbaAmmoKey, "HorsebackArchery.cpp");
    MO_ASSERT(!GameInteractor_Should(VB_SET_HORSEBACK_ARCHERY_AMMO, true, &sPlayState.interfaceCtx),
              "VB_SET_HORSEBACK_ARCHERY_AMMO did not intercept custom ammo");
    MO_ASSERT(sPlayState.interfaceCtx.hbaAmmo == 35, "VB_SET_HORSEBACK_ARCHERY_AMMO did not write hbaAmmo = 35");

    // Disabling master toggle suppresses AlwaysScore and Ammo.
    CVarSetInteger(kHbaCustomizeKey, 0);
    RunRegistrars(kHbaCustomizeKey, "HorsebackArchery.cpp");
    scoreIndex = 0;
    GameInteractor_Should(VB_SCORE_HORSEBACK_ARCHERY_TARGET, true, &scoreIndex);
    MO_ASSERT(scoreIndex == 0, "VB_SCORE_HORSEBACK_ARCHERY_TARGET ignored CustomizeHorsebackArchery == 0");
    sPlayState.interfaceCtx.hbaAmmo = 20;
    MO_ASSERT(GameInteractor_Should(VB_SET_HORSEBACK_ARCHERY_AMMO, true, &sPlayState.interfaceCtx),
              "VB_SET_HORSEBACK_ARCHERY_AMMO ignored CustomizeHorsebackArchery == 0");

    // Leg 6: VB_END_HORSEBACK_ARCHERY (independent of CustomizeHorsebackArchery).
    sEnGe1.actionFunc = nullptr;
    MO_ASSERT(GameInteractor_Should(VB_END_HORSEBACK_ARCHERY, true, &sEnGe1),
              "VB_END_HORSEBACK_ARCHERY fired when HorsebackArcheryBothPrizes == 0");

    CVarSetInteger(kHbaBothPrizesKey, 1);
    RunRegistrars(kHbaBothPrizesKey, "HorsebackArchery.cpp");
    MO_ASSERT(!GameInteractor_Should(VB_END_HORSEBACK_ARCHERY, true, &sEnGe1),
              "VB_END_HORSEBACK_ARCHERY did not intercept when HorsebackArcheryBothPrizes == 1");
    MO_ASSERT(sEnGe1.actionFunc == EnGe1_TalkAfterGame_Archery,
              "VB_END_HORSEBACK_ARCHERY did not loop EnGe1 to EnGe1_TalkAfterGame_Archery");

    ClearPacketKeys();
    RunRegistrars("*", "HorsebackArchery.cpp");
    return 0;
}

int CheckDivingGameTimer(GameInfo* scratchGameInfo) {
    // Leg 7: VB_SET_DIVING_GAME_TIME_LIMIT (default 50 -> vanilla passthrough; custom N -> BREG(2) + N).
    ClearPacketKeys();
    RunRegistrars(kDivingTimeLimitKey, "DivingGameTimer.cpp");
    ResetScratchState(scratchGameInfo);
    if (gGameInfo == nullptr) {
        gGameInfo = scratchGameInfo;
    }
    const s32 prevBreg2 = BREG(2);
    BREG(2) = 5;

    gSaveContext.timerSeconds = 0;
    gSaveContext.timerState = TIMER_STATE_OFF;
    MO_ASSERT(GameInteractor_Should(VB_SET_DIVING_GAME_TIME_LIMIT, true),
              "VB_SET_DIVING_GAME_TIME_LIMIT fired at default 50");
    MO_ASSERT(gSaveContext.timerSeconds == 0, "VB_SET_DIVING_GAME_TIME_LIMIT modified timer when off");

    CVarSetInteger(kDivingTimeLimitKey, 90);
    RunRegistrars(kDivingTimeLimitKey, "DivingGameTimer.cpp");
    MO_ASSERT(!GameInteractor_Should(VB_SET_DIVING_GAME_TIME_LIMIT, true),
              "VB_SET_DIVING_GAME_TIME_LIMIT did not intercept when set to 90");
    MO_ASSERT(gSaveContext.timerSeconds == 95, "VB_SET_DIVING_GAME_TIME_LIMIT did not apply BREG(2) + 90");
    MO_ASSERT(gSaveContext.timerState == TIMER_STATE_DOWN_INIT,
              "VB_SET_DIVING_GAME_TIME_LIMIT did not arm countdown timer");

    CVarClear(kDivingTimeLimitKey);
    RunRegistrars(kDivingTimeLimitKey, "DivingGameTimer.cpp");
    MO_ASSERT(GameInteractor_Should(VB_SET_DIVING_GAME_TIME_LIMIT, true),
              "VB_SET_DIVING_GAME_TIME_LIMIT stayed armed after clear");
    BREG(2) = prevBreg2;
    return 0;
}

int CheckRupeeDashScaling(GameInfo* scratchGameInfo) {
    // Leg 8: RupeeDashScaling toggles wallet-tier scaling vs flat -1 drain in UpdateRupeeDash().
    ClearPacketKeys();
    RunRegistrars(kRupeeDashKey, "RupeeDash.cpp");
    ResetScratchState(scratchGameInfo);
    OoT_gPlayState = nullptr;
    HooksAside<GameInteractor::OnPlayerUpdate> onPlayerUpdateGuard;

    CVarSetInteger(kRupeeDashKey, 1);
    CVarSetInteger(kRupeeDashIntervalKey, 0);
    RunRegistrars(kRupeeDashKey, "RupeeDash.cpp");

    // Tier 2 (Giant's Wallet): CUR_UPG_VALUE(UPG_WALLET) == 2 => walletSize == 3 => -3 per tick when scaling on.
    gSaveContext.inventory.upgrades = static_cast<u32>(2) << OoT_gUpgradeShifts[UPG_WALLET];
    gSaveContext.rupees = 100;
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnPlayerUpdate>();
    MO_ASSERT(gSaveContext.rupees == 97, "RupeeDash did not deduct wallet-scaled 3 rupees by default");

    CVarSetInteger(kRupeeDashScalingKey, 0);
    gSaveContext.rupees = 100;
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnPlayerUpdate>();
    MO_ASSERT(gSaveContext.rupees == 99, "RupeeDashScaling=0 did not reduce deduction to 1 rupee");

    ClearPacketKeys();
    RunRegistrars(kRupeeDashKey, "RupeeDash.cpp");
    return 0;
}

int RunLegs() {
    MO_ASSERT(CheckRegistrarsLinked() == 0, "registrar check failed");
    MO_ASSERT(CheckIngoRaceOptions(&sTempGameInfo) == 0, "IngoRaceOnce leg failed");
    MO_ASSERT(CheckHorsebackArcheryOptions(&sTempGameInfo) == 0, "HorsebackArchery leg failed");
    MO_ASSERT(CheckDivingGameTimer(&sTempGameInfo) == 0, "DivingGameTimer leg failed");
    MO_ASSERT(CheckRupeeDashScaling(&sTempGameInfo) == 0, "RupeeDashScaling leg failed");
    return 0;
}

} // namespace

extern "C" int OoT_MinigameOptions_RunHeadless(void) {
    if (GameInteractor::Instance == nullptr) {
        GameInteractor::Instance = new GameInteractor();
    }
    RuntimeStateGuard guard;
    Context_SetCurrentGame(GAME_OOT);

    const int rc = RunLegs();
    if (rc == 0) {
        printf("[TEST] oot-minigame-options: PASS\n");
    }
    return rc;
}

#endif // RSBS_SINGLE_EXECUTABLE
