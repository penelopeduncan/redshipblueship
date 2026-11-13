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
 * (#489 cause 1). Safe without a real Ship::Window: assignment always moves
 * visibility TO the stored CVar value, so GuiWindow::SetVisibility's
 * `shouldSave` stays false.
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
 * Return the ImGui begin order the next window begun this frame will get, or -1
 * with no ImGui context.
 */
inline int MM_ActiveGated_NextImGuiBeginOrder(void) {
    const ImGuiContext* g = ImGui::GetCurrentContext();
    return g != nullptr ? g->WindowsActiveCount : -1;
}

/**
 * In-game close menu for an MM window (#897). Re-enters every top-level
 * window that Draw() began this frame and gives it a right-click Close popup.
 * Returns true when the player chose Close.
 */
inline bool MM_ActiveGated_DrawCloseMenu(int firstBeginOrder) {
    ImGuiContext* g = ImGui::GetCurrentContext();
    if (g == nullptr || firstBeginOrder < 0) {
        return false;
    }

    ImVector<ImGuiWindow*> began;
    for (ImGuiWindow* window : g->Windows) {
        if (window->LastFrameActive != g->FrameCount || window->BeginOrderWithinContext < firstBeginOrder) {
            continue;
        }
        if ((window->Flags & (ImGuiWindowFlags_Popup | ImGuiWindowFlags_Tooltip)) != 0) {
            continue;
        }
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

    std::string mVisibilityCVar;
};

} // namespace S2H

#endif // RSBS_SINGLE_EXECUTABLE
