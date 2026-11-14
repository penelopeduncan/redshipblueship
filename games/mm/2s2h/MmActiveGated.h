#pragma once

/**
 * Active-game gate for MM's native ImGui windows in the single executable
 * (#392, #897, #1238 slice 3).
 *
 * Extracted from TrackersGuiSingleExe.cpp so both MM's trackers and MM's
 * developer-tool viewers share the same wrapper:
 *  - Draw(), DrawElement(), and UpdateElement() are no-ops unless MM is the
 *    active game (MM_TrackersGui_ShouldDraw()), protecting unified gSaveContext
 *    and preventing raw ImGui calls while OoT is running.
 *  - Draw() mirrors the window's gWindows.* CVar every MM frame (#489 cause 1)
 *    and adds a right-click "Close" popup (#897) to every top-level window
 *    begun by BaseWindow::Draw(), clearing the visibility CVar and hiding it.
 */

#ifdef RSBS_SINGLE_EXECUTABLE

#include <string>
#include <utility>

#include <imgui.h>
#include <imgui_internal.h>
#include <libultraship/bridge/consolevariablebridge.h>
#include <ship/Context.h>
#include <ship/window/Window.h>
#include <ship/window/gui/Gui.h>
#include <ship/window/gui/GuiWindow.h>
#include <spdlog/spdlog.h>

#include "TrackersGuiSingleExe.h"

namespace S2H {

/**
 * Drive `window`'s visibility to whatever its visibility CVar currently says
 * (#489 cause 1). See the SyncVisibilityFromCVars doc comment in
 * TrackersGuiSingleExe.h for why this is safe without a real Ship::Window: the
 * assignment always moves visibility TO the stored CVar value, so
 * GuiWindow::SetVisibility's `shouldSave` is always false and its
 * Context::GetWindow()->GetGui() deref is never reached.
 */
inline void MM_ActiveGated_SyncVisibilityFromCVar(Ship::GuiWindow& window, const char* cvar) {
    if (cvar == nullptr || cvar[0] == '\0') {
        return;
    }
    const bool wanted = CVarGetInteger(cvar, 0) != 0;
    if (window.IsVisible() == wanted) {
        return;
    }
    if (wanted) {
        window.Show();
    } else {
        window.Hide();
    }
}

/**
 * The ImGui begin order the next window begun this frame will get, or -1 with
 * no ImGui context. MMActiveGated::Draw reads it before the wrapped Draw() so
 * MM_ActiveGated_DrawCloseMenu can tell which windows that Draw() began.
 */
inline int MM_ActiveGated_NextImGuiBeginOrder(void) {
    const ImGuiContext* g = ImGui::GetCurrentContext();
    return g != nullptr ? g->WindowsActiveCount : -1;
}

/**
 * In-game close for an MM window (#897). The vendored trackers pass a null
 * p_open to ImGui::Begin (CheckTracker.cpp, ItemTracker.cpp), so ImGui gives
 * them no close button, and the Item Tracker's default window type has no
 * title bar at all. After the wrapped Draw() has run, this re-enters each
 * top-level window that Draw() began this frame (ImGui appends to a window
 * begun twice in one frame) and gives it a right-click menu with one item,
 * Close. Covering every window the Draw() began, rather than one hard-coded
 * ImGui name, is what reaches the Item Tracker's split-group windows too, and
 * a window an old imgui.ini left docked is covered as well.
 * Returns true when the player chose Close.
 */
inline bool MM_ActiveGated_DrawCloseMenu(int firstBeginOrder) {
    ImGuiContext* g = ImGui::GetCurrentContext();
    if (g == nullptr || firstBeginOrder < 0) {
        return false;
    }

    // Collected first: the popup below creates a window, which grows g->Windows.
    ImVector<ImGuiWindow*> began;
    for (ImGuiWindow* window : g->Windows) {
        if (window->LastFrameActive != g->FrameCount || window->BeginOrderWithinContext < firstBeginOrder) {
            continue;
        }
        if ((window->Flags & (ImGuiWindowFlags_Popup | ImGuiWindowFlags_Tooltip)) != 0) {
            continue;
        }
        // A docked window carries ImGuiWindowFlags_ChildWindow too (BeginDocked
        // sets it), and its dock host was begun before Draw(), so a docked
        // window is still collected here; real child windows are not.
        if ((window->Flags & ImGuiWindowFlags_ChildWindow) != 0 && !window->DockIsActive) {
            continue;
        }
        began.push_back(window);
    }

    bool close = false;
    for (ImGuiWindow* window : began) {
        if (ImGui::Begin(window->Name)) {
            if (ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows) &&
                ImGui::IsMouseReleased(ImGuiMouseButton_Right)) {
                ImGui::OpenPopup("##RsbsMMWindowClose");
            }
            if (ImGui::BeginPopup("##RsbsMMWindowClose")) {
                if (ImGui::MenuItem("Close")) {
                    close = true;
                }
                ImGui::EndPopup();
            }
        }
        ImGui::End();
    }
    return close;
}

/**
 * Active-game gate. Draw() covers the per-frame ImGui path (both the base
 * GuiWindow::Draw used by the settings windows and the full Draw overrides in
 * CheckTracker.cpp / ItemTracker.cpp); DrawElement() covers a caller that runs
 * the body directly; UpdateElement() covers Gui's unconditional per-frame
 * Update(). The bases' bodies run only while MM is the active game. The
 * mm-trackers-gui and mm-dev-viewers-gate locks call these with OoT active and
 * no ImGui context, so an ungated path aborts the test process.
 *
 * The gate is also where the per-frame CVar->visibility re-sync lives (#489
 * cause 1). libultraship never re-reads a window's visibility CVar after the
 * ctor, and the single exe has no BenMenu to call Show(), so without this the
 * settings windows, which reach ImGui through the base GuiWindow::Draw and its
 * `if (!IsVisible()) return;`, would be permanently stuck at whatever the
 * config held on MM's first boot. Deliberately INSIDE the active-game gate:
 * syncing while OoT is active would be wasted work every frame, and the
 * ROM-free locks drive Draw() with OoT active specifically to prove nothing
 * below the gate runs then.
 *
 * Draw() also hosts the in-game close (#897, MM_ActiveGated_DrawCloseMenu
 * above): Close clears the visibility CVar BEFORE hiding, so the re-sync above
 * keeps the window shut and GuiWindow::SetVisibility sees no CVar change to
 * save (its save path dereferences the real Ship::Window, which the ROM-free
 * harness lacks); the config is saved here instead, when there is a window.
 */
template <typename BaseWindow> class MMActiveGated final : public BaseWindow {
  public:
    template <typename... Args>
    explicit MMActiveGated(const std::string& consoleVariable, Args&&... args)
        : BaseWindow(consoleVariable, std::forward<Args>(args)...), mVisibilityCVar(consoleVariable) {
    }

    void Draw() override {
        if (!MM_TrackersGui_ShouldDraw()) {
            return;
        }
        MM_ActiveGated_SyncVisibilityFromCVar(*this, mVisibilityCVar.c_str());
        const int firstBeginOrder = MM_ActiveGated_NextImGuiBeginOrder();
        BaseWindow::Draw();
        if (MM_ActiveGated_DrawCloseMenu(firstBeginOrder)) {
            Close();
        }
    }

    void DrawElement() override {
        if (!MM_TrackersGui_ShouldDraw()) {
            return;
        }
        BaseWindow::DrawElement();
    }

  protected:
    void UpdateElement() override {
        if (!MM_TrackersGui_ShouldDraw()) {
            return;
        }
        BaseWindow::UpdateElement();
    }

  private:
    void Close() {
        CVarClear(mVisibilityCVar.c_str());
        this->Hide();
        auto ctx = Ship::Context::GetInstance();
        if (ctx != nullptr && ctx->GetWindow() != nullptr && ctx->GetWindow()->GetGui() != nullptr) {
            ctx->GetWindow()->GetGui()->SaveConsoleVariablesNextFrame();
        }
        SPDLOG_INFO("[MM window] {} closed from its right-click menu; {} cleared (#897)", this->GetName(),
                    mVisibilityCVar);
    }

    // GuiWindow keeps mVisibilityConsoleVariable private, so the wrapper holds
    // its own copy of the same string it passed to the base ctor.
    std::string mVisibilityCVar;
};

} // namespace S2H

#endif // RSBS_SINGLE_EXECUTABLE
