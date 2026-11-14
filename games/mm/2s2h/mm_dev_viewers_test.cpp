/**
 * ROM-free, display-free lock for #1238 slice 3 (W6-DEV): MM's Event Log window
 * registers de-collided on the shared Gui, toggles/closes via gWindows.EventLog,
 * gates its Draw / DrawElement / UpdateElement on GAME_MM, drives all 10 EventLog
 * hooks (including the newly wired OnFlagUnset and OnSceneFlagUnset legs) through
 * S2H::GameHooks, and keeps MM_DevTools_WarpPointAvailable() inert outside MM
 * gameplay.
 *
 * CTest name: MMDevViewersGate (`redship --test mm-dev-viewers-gate`).
 */

#ifdef RSBS_SINGLE_EXECUTABLE

#include <cstdio>
#include <cstring>
#include <memory>
#include <string>

#include <imgui.h>
#include <imgui_internal.h>
#include <libultraship/bridge/consolevariablebridge.h>
#include <ship/Context.h>
#include <ship/window/gui/Gui.h>
#include <ship/window/gui/GuiWindow.h>

#include "2s2h/DeveloperTools/EventLog.h"
#include "2s2h/GameInteractor/GameInteractor.h"
#include "MmActiveGated.h"
#include "TrackersGuiSingleExe.h"
#include "context.h"
#include "include/mm_game_hooks.h"
#include "mm_dev_tools_bridge.h"
#include "mm_dev_viewers_bridge.h"
#include "variables.h"

namespace S2H {
namespace DevToolsGui {
void RegisterWindows(std::shared_ptr<Ship::Gui> gui);
} // namespace DevToolsGui
} // namespace S2H

extern "C" {
void MM_GameHooks_ExecuteOnFlagSet(FlagType flagType, u32 flag);
void MM_GameHooks_ExecuteOnFlagUnset(FlagType flagType, u32 flag);
void MM_GameHooks_ExecuteOnSceneFlagSet(s16 sceneId, FlagType flagType, u32 flag);
void MM_GameHooks_ExecuteOnSceneFlagUnset(s16 sceneId, FlagType flagType, u32 flag);
}

namespace {

#define DVG_ASSERT(cond, msg)                                             \
    do {                                                                  \
        if (!(cond)) {                                                    \
            printf("[TEST] FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__); \
            return 1;                                                     \
        }                                                                 \
    } while (0)

constexpr const char* kEventLogWindowName = "MM Event Log";
constexpr const char* kEventLogSohTwinName = "Event Log";
constexpr const char* kEventLogVisibilityCVar = "gWindows.EventLog";
constexpr const char* kEventLogEnabledCVar = "gEventLog.Enabled";

class StandinViewerWindow final : public Ship::GuiWindow {
  public:
    using GuiWindow::GuiWindow;

    void InitElement() override {
    }
    void DrawElement() override {
    }
    void UpdateElement() override {
    }
};

class UpdateSpy : public Ship::GuiWindow {
  public:
    UpdateSpy(const std::string& consoleVariable, const std::string& name) : Ship::GuiWindow(consoleVariable, name) {
    }

    int draws = 0;
    int drawElements = 0;
    int updateElements = 0;

    void Draw() override {
        draws++;
    }
    void DrawElement() override {
        drawElements++;
    }

  protected:
    void InitElement() override {
    }
    void UpdateElement() override {
        updateElements++;
    }
};

struct StandInPlayStateGuard {
    GameId prevGame;
    explicit StandInPlayStateGuard(GameId runAs) : prevGame(Context_GetCurrentGame()) {
        Context_SetCurrentGame(runAs);
        MM_DevTools_SetStandInPlayState(1);
    }
    ~StandInPlayStateGuard() {
        MM_DevTools_SetStandInPlayState(0);
        Context_SetCurrentGame(prevGame);
    }
};

void ResetEventLogHooksAndState() {
    CVarClear(kEventLogVisibilityCVar);
    CVarClear(kEventLogEnabledCVar);
    MM_EventLog_DrainEntryCountForTest();
    S2H::GameHooks::ResetForTest<GameInteractor::OnFlagSet>();
    S2H::GameHooks::ResetForTest<GameInteractor::OnFlagUnset>();
    S2H::GameHooks::ResetForTest<GameInteractor::OnSceneFlagSet>();
    S2H::GameHooks::ResetForTest<GameInteractor::OnSceneFlagUnset>();
    S2H::GameHooks::ResetForTest<GameInteractor::OnActorInit>();
    S2H::GameHooks::ResetForTest<GameInteractor::OnActorKill>();
    S2H::GameHooks::ResetForTest<GameInteractor::OnSceneInit>();
    S2H::GameHooks::ResetForTest<GameInteractor::OnRoomInit>();
    S2H::GameHooks::ResetForTest<GameInteractor::OnOpenText>();
    S2H::GameHooks::ResetForTest<GameInteractor::OnItemGive>();
}

// True when the log holds exactly one entry and it is of `expectedType`
// (eventLogEntryTypeNames spelling). Always drains the log.
bool DrainExactlyOneEntryOfType(const char* expectedType) {
    const char* newest = MM_EventLog_NewestEntryTypeNameForTest();
    const bool typeMatches = newest != nullptr && std::strcmp(newest, expectedType) == 0;
    const size_t count = MM_EventLog_DrainEntryCountForTest();
    if (count != 1 || !typeMatches) {
        printf("[TEST] expected one %s entry, got %zu (newest %s)\n", expectedType, count,
               newest != nullptr ? newest : "<none>");
        return false;
    }
    return true;
}

// Flush the queued removals of every hook type the log registers, so a
// CountForTest read sees them (S2H::GameHooks::Unregister only queues).
void FlushEventLogHookUnregistrations() {
    S2H::GameHooks::FlushPendingUnregistrations<GameInteractor::OnFlagSet>();
    S2H::GameHooks::FlushPendingUnregistrations<GameInteractor::OnFlagUnset>();
    S2H::GameHooks::FlushPendingUnregistrations<GameInteractor::OnSceneFlagSet>();
    S2H::GameHooks::FlushPendingUnregistrations<GameInteractor::OnSceneFlagUnset>();
    S2H::GameHooks::FlushPendingUnregistrations<GameInteractor::OnActorInit>();
    S2H::GameHooks::FlushPendingUnregistrations<GameInteractor::OnActorKill>();
    S2H::GameHooks::FlushPendingUnregistrations<GameInteractor::OnSceneInit>();
    S2H::GameHooks::FlushPendingUnregistrations<GameInteractor::OnRoomInit>();
    S2H::GameHooks::FlushPendingUnregistrations<GameInteractor::OnOpenText>();
    S2H::GameHooks::FlushPendingUnregistrations<GameInteractor::OnItemGive>();
}

} // namespace

extern "C" int MM_DevViewersGate_RunHeadless(void) {
    auto ctx = Ship::Context::GetInstance();
    DVG_ASSERT(ctx != nullptr, "Ship::Context singleton missing");
    DVG_ASSERT(ctx->GetConsoleVariables() != nullptr, "ConsoleVariables missing");
    DVG_ASSERT(ctx->GetConfig() != nullptr, "Config missing");
    DVG_ASSERT(ctx->GetConsole() != nullptr, "Console missing");

    const GameId savedGame = Context_GetCurrentGame();
    ResetEventLogHooksAndState();

    // ---- (a) Registration, SoH name de-collision, and idempotence ----
    auto gui = std::make_shared<Ship::Gui>();
    auto standin = std::make_shared<StandinViewerWindow>("", kEventLogSohTwinName);
    gui->AddGuiWindow(standin);
    DVG_ASSERT(gui->GetGuiWindow(kEventLogSohTwinName) == standin, "failed to seed SoH Event Log stand-in");

    S2H::DevToolsGui::RegisterWindows(gui);
    auto eventLog = gui->GetGuiWindow(kEventLogWindowName);
    DVG_ASSERT(eventLog != nullptr, "MM Event Log did not register on the shared Gui");
    DVG_ASSERT(gui->GetGuiWindow(kEventLogSohTwinName) == standin,
               "RegisterWindows displaced an existing \"Event Log\" window");

    // Calling both DevToolsGui and TrackersGui entry points repeatedly must be idempotent.
    S2H::DevToolsGui::RegisterWindows(gui);
    S2H::TrackersGui::RegisterWindows(gui);
    DVG_ASSERT(gui->GetGuiWindow(kEventLogWindowName) == eventLog, "RegisterWindows replaced MM Event Log");
    printf("[TEST] leg (a): MM Event Log registers de-collided beside \"%s\" and is idempotent\n",
           kEventLogSohTwinName);

    // ---- (c) Active-game gate (Draw, DrawElement, Update/UpdateElement) without ImGui context ----
    // Force gWindows.EventLog=1 so any ungated Draw() or DrawElement() reaches ImGui and aborts.
    // Also exercise MMActiveGated<UpdateSpy> across non-MM and MM because EventLogWindow::UpdateElement() is empty.
    DVG_ASSERT(ImGui::GetCurrentContext() == nullptr, "unexpected ImGui context before leg (c)");
    auto spy = std::make_shared<S2H::MMActiveGated<UpdateSpy>>("", "MMDevViewersGateSpy");
    std::shared_ptr<Ship::GuiWindow> spyWindow = spy;
    CVarSetInteger(kEventLogVisibilityCVar, 1);
    for (GameId nonMm : { GAME_OOT, GAME_NONE }) {
        Context_SetCurrentGame(nonMm);
        eventLog->Draw();
        eventLog->DrawElement();
        eventLog->Update();
        spyWindow->Draw();
        spyWindow->DrawElement();
        spyWindow->Update();
        if (spy->draws != 0 || spy->drawElements != 0 || spy->updateElements != 0) {
            CVarClear(kEventLogVisibilityCVar);
            Context_SetCurrentGame(savedGame);
            ResetEventLogHooksAndState();
            DVG_ASSERT(false, "MMActiveGated forwarded Draw/DrawElement/UpdateElement off GAME_MM");
        }
    }
    CVarClear(kEventLogVisibilityCVar);
    Context_SetCurrentGame(GAME_MM);
    spyWindow->Draw();
    spyWindow->DrawElement();
    spyWindow->Update();
    Context_SetCurrentGame(savedGame);
    DVG_ASSERT(spy->draws == 1 && spy->drawElements == 1 && spy->updateElements == 1,
               "MMActiveGated failed to forward Draw/DrawElement/UpdateElement under GAME_MM");
    printf("[TEST] leg (c): Draw, DrawElement, and UpdateElement are gated off GAME_MM and active under GAME_MM\n");

    // ---- (b) gWindows.EventLog visibility toggle & right-click Close ----
    {
        ImGuiContext* imgui = ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.IniFilename = nullptr;
        io.LogFilename = nullptr;
        io.DisplaySize = ImVec2(1280.0f, 800.0f);
        io.DeltaTime = 1.0f / 60.0f;
        io.Fonts->AddFontDefault();
        unsigned char* atlasPixels = nullptr;
        int atlasW = 0;
        int atlasH = 0;
        io.Fonts->GetTexDataAsRGBA32(&atlasPixels, &atlasW, &atlasH);

        Context_SetCurrentGame(GAME_MM);
        auto drawFrame = [&]() {
            ImGui::NewFrame();
            eventLog->Draw();
            ImGui::Render();
        };
        auto drawnWindow = [&]() -> ImGuiWindow* {
            ImGuiWindow* w = ImGui::FindWindowByName(kEventLogWindowName);
            return (w != nullptr && w->LastFrameActive == ImGui::GetFrameCount()) ? w : nullptr;
        };

        // Starts closed when CVar is unset.
        CVarClear(kEventLogVisibilityCVar);
        drawFrame();
        if (drawnWindow() != nullptr || eventLog->IsVisible()) {
            Context_SetCurrentGame(savedGame);
            ImGui::DestroyContext(imgui);
            ResetEventLogHooksAndState();
            DVG_ASSERT(false, "MM Event Log drew while gWindows.EventLog was unset");
        }

        // Setting gWindows.EventLog=1 opens the window.
        CVarSetInteger(kEventLogVisibilityCVar, 1);
        drawFrame();
        drawFrame();
        ImGuiWindow* win = drawnWindow();
        if (win == nullptr || !eventLog->IsVisible()) {
            Context_SetCurrentGame(savedGame);
            ImGui::DestroyContext(imgui);
            ResetEventLogHooksAndState();
            DVG_ASSERT(false, "gWindows.EventLog=1 did not open MM Event Log");
        }

        // Right-clicking opens MMActiveGated's Close popup; choosing Close clears CVar and hides.
        io.AddMousePosEvent(win->Pos.x + win->Size.x * 0.5f, win->Pos.y + win->Size.y * 0.5f);
        drawFrame();
        io.AddMouseButtonEvent(ImGuiMouseButton_Right, true);
        drawFrame();
        io.AddMouseButtonEvent(ImGuiMouseButton_Right, false);
        drawFrame();
        drawFrame();
        drawFrame();

        ImGuiContext& g = *ImGui::GetCurrentContext();
        if (g.OpenPopupStack.empty() || g.OpenPopupStack.back().Window == nullptr) {
            Context_SetCurrentGame(savedGame);
            ImGui::DestroyContext(imgui);
            ResetEventLogHooksAndState();
            DVG_ASSERT(false, "right-clicking MM Event Log did not open Close menu");
        }
        ImGuiWindow* menu = g.OpenPopupStack.back().Window;
        const ImGuiStyle& style = ImGui::GetStyle();
        io.AddMousePosEvent(menu->Pos.x + style.WindowPadding.x + 4.0f,
                            menu->Pos.y + style.WindowPadding.y + ImGui::GetFontSize() * 0.5f);
        drawFrame();
        io.AddMouseButtonEvent(ImGuiMouseButton_Left, true);
        drawFrame();
        io.AddMouseButtonEvent(ImGuiMouseButton_Left, false);
        drawFrame();

        const bool cvarCleared = CVarGetInteger(kEventLogVisibilityCVar, 0) == 0;
        const bool hiddenAfterClose = !eventLog->IsVisible();
        drawFrame();
        const bool stayedShut = drawnWindow() == nullptr;

        Context_SetCurrentGame(savedGame);
        ImGui::DestroyContext(imgui);

        DVG_ASSERT(cvarCleared, "Close did not clear gWindows.EventLog");
        DVG_ASSERT(hiddenAfterClose && stayedShut, "MM Event Log stayed visible after Close");
        printf("[TEST] leg (b): gWindows.EventLog=1 opens MM Event Log and right-click > Close shuts and clears it\n");
    }

    // ---- (d) EventLog hook lifecycle & OnFlagUnset / OnSceneFlagUnset dispatch ----
    {
        ResetEventLogHooksAndState();

        // Disabled by default: Init() on a fresh uninitialized instance registers no hooks.
        auto disabledEventLog = std::make_shared<S2H::MMActiveGated<EventLogWindow>>(
            kEventLogVisibilityCVar, kEventLogWindowName, ImVec2(520, 600));
        disabledEventLog->Init();
        DVG_ASSERT(S2H::GameHooks::CountForTest<GameInteractor::OnFlagSet>() == 0,
                   "OnFlagSet hook registered when gEventLog.Enabled=0");
        DVG_ASSERT(S2H::GameHooks::CountForTest<GameInteractor::OnFlagUnset>() == 0,
                   "OnFlagUnset hook registered when gEventLog.Enabled=0");
        DVG_ASSERT(S2H::GameHooks::CountForTest<GameInteractor::OnSceneFlagSet>() == 0,
                   "OnSceneFlagSet hook registered when gEventLog.Enabled=0");
        DVG_ASSERT(S2H::GameHooks::CountForTest<GameInteractor::OnSceneFlagUnset>() == 0,
                   "OnSceneFlagUnset hook registered when gEventLog.Enabled=0");

        // Enable EventLog: Init() on a fresh instance registers all 10 S2H::GameHooks entries.
        CVarSetInteger(kEventLogEnabledCVar, 1);
        auto enabledEventLog = std::make_shared<S2H::MMActiveGated<EventLogWindow>>(
            kEventLogVisibilityCVar, kEventLogWindowName, ImVec2(520, 600));
        enabledEventLog->Init();
        DVG_ASSERT(S2H::GameHooks::CountForTest<GameInteractor::OnFlagSet>() == 1, "OnFlagSet hook missing");
        DVG_ASSERT(S2H::GameHooks::CountForTest<GameInteractor::OnFlagUnset>() == 1, "OnFlagUnset hook missing");
        DVG_ASSERT(S2H::GameHooks::CountForTest<GameInteractor::OnSceneFlagSet>() == 1, "OnSceneFlagSet hook missing");
        DVG_ASSERT(S2H::GameHooks::CountForTest<GameInteractor::OnSceneFlagUnset>() == 1,
                   "OnSceneFlagUnset hook missing");
        DVG_ASSERT(S2H::GameHooks::CountForTest<GameInteractor::OnActorInit>() == 1, "OnActorInit hook missing");
        DVG_ASSERT(S2H::GameHooks::CountForTest<GameInteractor::OnActorKill>() == 1, "OnActorKill hook missing");
        DVG_ASSERT(S2H::GameHooks::CountForTest<GameInteractor::OnSceneInit>() == 1, "OnSceneInit hook missing");
        DVG_ASSERT(S2H::GameHooks::CountForTest<GameInteractor::OnRoomInit>() == 1, "OnRoomInit hook missing");
        DVG_ASSERT(S2H::GameHooks::CountForTest<GameInteractor::OnOpenText>() == 1, "OnOpenText hook missing");
        DVG_ASSERT(S2H::GameHooks::CountForTest<GameInteractor::OnItemGive>() == 1, "OnItemGive hook missing");

        // Each dispatcher on its own: exactly one entry, of its own type (an Unset
        // dispatcher that ran the Set registry would log a FlagSet entry instead).
        MM_EventLog_DrainEntryCountForTest();
        MM_GameHooks_ExecuteOnFlagSet(FLAG_WEEK_EVENT_REG, 0x0102);
        DVG_ASSERT(DrainExactlyOneEntryOfType("FlagSet"), "MM_GameHooks_ExecuteOnFlagSet did not log one FlagSet");
        MM_GameHooks_ExecuteOnFlagUnset(FLAG_WEEK_EVENT_REG, 0x0102);
        DVG_ASSERT(DrainExactlyOneEntryOfType("FlagUnset"),
                   "MM_GameHooks_ExecuteOnFlagUnset did not log one FlagUnset");
        MM_GameHooks_ExecuteOnSceneFlagSet(0, FLAG_CYCL_SCENE_SWITCH, 1);
        DVG_ASSERT(DrainExactlyOneEntryOfType("SceneFlagSet"),
                   "MM_GameHooks_ExecuteOnSceneFlagSet did not log one SceneFlagSet");
        MM_GameHooks_ExecuteOnSceneFlagUnset(0, FLAG_CYCL_SCENE_SWITCH, 1);
        DVG_ASSERT(DrainExactlyOneEntryOfType("SceneFlagUnset"),
                   "MM_GameHooks_ExecuteOnSceneFlagUnset did not log one SceneFlagUnset");

        // Drive z_actor.c's MM_Flags_SetSwitch / MM_Flags_UnsetSwitch call sites end-to-end.
        {
            StandInPlayStateGuard playGuard(GAME_MM);
            if (MM_gPlayState == nullptr) {
                ResetEventLogHooksAndState();
                DVG_ASSERT(false, "MM_DevTools_SetStandInPlayState(1) left MM_gPlayState null");
            }
            MM_Flags_SetSwitch(MM_gPlayState, 1);
            const bool setLogged = DrainExactlyOneEntryOfType("SceneFlagSet");
            MM_Flags_UnsetSwitch(MM_gPlayState, 1);
            const bool unsetLogged = DrainExactlyOneEntryOfType("SceneFlagUnset");
            if (!setLogged || !unsetLogged) {
                ResetEventLogHooksAndState();
                DVG_ASSERT(false, "MM_Flags_SetSwitch / MM_Flags_UnsetSwitch did not log through EventLog");
            }
        }

        // Disable EventLog: Init() on a fresh instance queues unregistrations that flush before next Execute runs,
        // logging 0 entries.
        CVarSetInteger(kEventLogEnabledCVar, 0);
        auto disabledAfterEnableEventLog = std::make_shared<S2H::MMActiveGated<EventLogWindow>>(
            kEventLogVisibilityCVar, kEventLogWindowName, ImVec2(520, 600));
        disabledAfterEnableEventLog->Init();
        MM_GameHooks_ExecuteOnFlagSet(FLAG_WEEK_EVENT_REG, 0x0102);
        MM_GameHooks_ExecuteOnFlagUnset(FLAG_WEEK_EVENT_REG, 0x0102);
        MM_GameHooks_ExecuteOnSceneFlagSet(0, FLAG_CYCL_SCENE_SWITCH, 1);
        MM_GameHooks_ExecuteOnSceneFlagUnset(0, FLAG_CYCL_SCENE_SWITCH, 1);
        DVG_ASSERT(MM_EventLog_DrainEntryCountForTest() == 0,
                   "EventLog still logged entries after gEventLog.Enabled=0");
        // The other six types were never executed above, so their queued removals
        // are flushed directly (no dispatch: the actor hooks would need an Actor*).
        FlushEventLogHookUnregistrations();
        DVG_ASSERT(S2H::GameHooks::CountForTest<GameInteractor::OnFlagSet>() == 0,
                   "OnFlagSet hook remained registered after disable+flush");
        DVG_ASSERT(S2H::GameHooks::CountForTest<GameInteractor::OnFlagUnset>() == 0,
                   "OnFlagUnset hook remained registered after disable+flush");
        DVG_ASSERT(S2H::GameHooks::CountForTest<GameInteractor::OnSceneFlagSet>() == 0,
                   "OnSceneFlagSet hook remained registered after disable+flush");
        DVG_ASSERT(S2H::GameHooks::CountForTest<GameInteractor::OnSceneFlagUnset>() == 0,
                   "OnSceneFlagUnset hook remained registered after disable+flush");
        DVG_ASSERT(S2H::GameHooks::CountForTest<GameInteractor::OnActorInit>() == 0,
                   "OnActorInit hook remained registered after disable+flush");
        DVG_ASSERT(S2H::GameHooks::CountForTest<GameInteractor::OnActorKill>() == 0,
                   "OnActorKill hook remained registered after disable+flush");
        DVG_ASSERT(S2H::GameHooks::CountForTest<GameInteractor::OnSceneInit>() == 0,
                   "OnSceneInit hook remained registered after disable+flush");
        DVG_ASSERT(S2H::GameHooks::CountForTest<GameInteractor::OnRoomInit>() == 0,
                   "OnRoomInit hook remained registered after disable+flush");
        DVG_ASSERT(S2H::GameHooks::CountForTest<GameInteractor::OnOpenText>() == 0,
                   "OnOpenText hook remained registered after disable+flush");
        DVG_ASSERT(S2H::GameHooks::CountForTest<GameInteractor::OnItemGive>() == 0,
                   "OnItemGive hook remained registered after disable+flush");
        printf("[TEST] leg (d): EventLog registers all 10 hooks, logs FlagSet/FlagUnset/SceneFlagSet/SceneFlagUnset, "
               "and unregisters cleanly on disable\n");
    }

    // ---- (e) Hosted row gameplay-availability predicate ----
    for (GameId game : { GAME_OOT, GAME_NONE, GAME_MM }) {
        Context_SetCurrentGame(game);
        DVG_ASSERT(MM_DevTools_WarpPointAvailable() == 0,
                   "MM_DevTools_WarpPointAvailable returned non-zero without MM gameplay state");
    }

    // ---- (f) Hosted Message Viewer (#1238 slice 1) ----
    // Always restore the process-wide current game even if an assertion returns early.
    struct CurrentGameRestoreGuard {
        GameId game;
        explicit CurrentGameRestoreGuard(GameId g) : game(g) {
        }
        ~CurrentGameRestoreGuard() {
            Context_SetCurrentGame(game);
        }
    } restoreGameGuard(savedGame);

    // (a) Precondition: hosted MM Message Viewer body must start uninitialized (never init on read).
    DVG_ASSERT(!MM_MessageViewer_HostedInitializedForTest(),
               "MM_MessageViewer_HostedInitializedForTest() returned true before first GAME_MM update");

    // (b) HostedActive tracks only GAME_MM.
    Context_SetCurrentGame(GAME_OOT);
    DVG_ASSERT(!MM_MessageViewer_HostedActive(), "MM_MessageViewer_HostedActive() returned true under GAME_OOT");
    Context_SetCurrentGame(GAME_MM);
    DVG_ASSERT(MM_MessageViewer_HostedActive(), "MM_MessageViewer_HostedActive() returned false under GAME_MM");
    Context_SetCurrentGame(GAME_NONE);
    DVG_ASSERT(!MM_MessageViewer_HostedActive(), "MM_MessageViewer_HostedActive() returned true under GAME_NONE");

    // (c) Non-MM UpdateHostedElement calls must not initialize MM's body.
    Context_SetCurrentGame(GAME_OOT);
    MM_MessageViewer_UpdateHostedElement();
    DVG_ASSERT(!MM_MessageViewer_HostedInitializedForTest(),
               "MM_MessageViewer_UpdateHostedElement() initialized MM body under GAME_OOT");
    Context_SetCurrentGame(GAME_NONE);
    MM_MessageViewer_UpdateHostedElement();
    DVG_ASSERT(!MM_MessageViewer_HostedInitializedForTest(),
               "MM_MessageViewer_UpdateHostedElement() initialized MM body under GAME_NONE");

    // (d) First GAME_MM UpdateHostedElement call initializes the body; a second call stays safe and initialized.
    Context_SetCurrentGame(GAME_MM);
    MM_MessageViewer_UpdateHostedElement();
    DVG_ASSERT(MM_MessageViewer_HostedInitializedForTest(),
               "MM_MessageViewer_UpdateHostedElement() failed to initialize MM body under GAME_MM");
    MM_MessageViewer_UpdateHostedElement();
    DVG_ASSERT(MM_MessageViewer_HostedInitializedForTest(),
               "second MM_MessageViewer_UpdateHostedElement() call left MM body uninitialized");

    Context_SetCurrentGame(savedGame);
    ResetEventLogHooksAndState();
    printf("[TEST] PASS: mm-dev-viewers-gate\n");
    return 0;
}

#endif // RSBS_SINGLE_EXECUTABLE
